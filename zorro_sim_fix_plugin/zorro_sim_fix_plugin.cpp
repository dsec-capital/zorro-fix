#pragma warning(disable : 4996 4244 4312)

#include "pch.h"

#include "application.h"
#include "fix_thread.h"
#include "zorro_sim_fix_plugin.h"

#include "common/market_data.h"
#include "common/exec_report.h"
#include "common/blocking_queue.h"
#include "common/json.h"
#include "common/time_utils.h"

#include "zorro_common/log.h"
#include "zorro_common/utils.h"
#include "zorro_common/enums.h"
#include "zorro_common/broker_commands.h"

#include "nlohmann/json.h"
#include "httplib/httplib.h"
#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"

#define PLUGIN_VERSION 2
#define PLUGIN_NAME "SimFixPlugin"

namespace zorro {

	using namespace common;
	using namespace std::chrono_literals;

	// http://localhost:8080/bars?symbol=AUD/USD
	std::string rest_host = "http://localhost";
	int rest_port = 8080;
	httplib::Client rest_client(std::format("{}:{}", rest_host, rest_port));

	int max_snaphsot_waiting_iterations = 10; 
	std::chrono::milliseconds fix_exec_report_waiting_time = 500ms;
	std::string settings_cfg_file = "Plugin/zorro_sim_fix_client.cfg";

	int client_order_id = 0;
	int internal_order_id = 1000;

	std::string asset;
	std::string currency;
	std::string position_symbol;
	std::string order_text;
	int multiplier = 1;
	int price_type = 0;
	int vol_type = 0;
	int leverage = 1;
	double amount = 1;
	double limit = 0;
	char time_in_force = FIX::TimeInForce_GOOD_TILL_CANCEL;
	HWND window_handle;

	std::shared_ptr<spdlog::logger> spd_logger = nullptr;
	std::unique_ptr<FixThread> fix_thread = nullptr;
	BlockingTimeoutQueue<ExecReport> exec_report_queue;
	BlockingTimeoutQueue<TopOfBook> top_of_book_queue;  
	std::unordered_map<int, std::string> order_id_by_internal_order_id;
	std::unordered_map<std::string, TopOfBook> top_of_books;
	OrderTracker order_tracker("account");

	std::string order_mapping_string() {
		std::string str = "OrderMapping[\n";
		for (const auto& [internal_id, order_id] : order_id_by_internal_order_id) {
			str += std::format("  internal_id={}, order_id={}\n", internal_id, order_id);
		}
		str += "]";
		return str;
	}

	int pop_exec_reports() {
		auto n = exec_report_queue.pop_all(
			[](const ExecReport& report) { 
				auto ok = order_tracker.process(report); 
			}
		);
		return n;
	}

	int pop_top_of_books() {
		auto n = top_of_book_queue.pop_all(
			[](const TopOfBook& top) { 
				top_of_books.insert_or_assign(top.symbol, top); 
			}
		);
		return n;
	}

	int get_position_size(const std::string& symbol) {
		auto& np = order_tracker.net_position(symbol);
		return (int)np.qty;
	}

	double get_avg_entry_price(const std::string& symbol) {
		auto& np = order_tracker.net_position(symbol);
		return np.avg_px;
	}

	int get_num_orders() {
		return order_tracker.num_orders();
	}

	std::string next_client_order_id() {
		++client_order_id;
		return std::format("cl_ord_id_{}", client_order_id);
	}

	int next_internal_order_id() {
		++internal_order_id;
		return internal_order_id;
	}

	// get historical data - note time is in UTC
	// http://localhost:8080/bars?symbol=EUR/USD&from=2024-03-30 12:00:00&to=2024-03-30 16:00:00
	int get_historical_bars(const char* Asset, DATE from, DATE to, std::map<std::chrono::nanoseconds, Bar>& bars) {
		auto from_str = zorro_date_to_string(from);
		auto to_str = zorro_date_to_string(to);
		auto request = std::format("/bars?symbol={}&from={}&to={}", Asset, from_str, to_str);
		auto res = rest_client.Get(request);
		if (res->status == httplib::StatusCode::OK_200) {
			auto j = json::parse(res->body);
			from_json(j, bars);

			log::debug<4, true>(
				"get_historical_bars: Asset={} from={} to={} num bars={}",
				Asset, from_str, to_str, bars.size()
			);
		}
		else {
			log::error<true>(
				"get_historical_bars error: status={} Asset={} from={} to={} body={}", 
				res->status, Asset, from_str, to_str, res->body
			);
		}

		return res->status; 
	}

	int get_historical_bar_range(const char* Asset, std::chrono::nanoseconds& from, std::chrono::nanoseconds& to, size_t& num_bars, bool verbose=false) {
		auto request = std::format("/bar_range?symbol={}", Asset);
		auto res = rest_client.Get(request);
		if (res->status == httplib::StatusCode::OK_200) {
			auto j = json::parse(res->body);
			from = std::chrono::nanoseconds(j["from"].template get<long long>());
			to = std::chrono::nanoseconds(j["to"].template get<long long>());
			num_bars = j["num_bars"].template get<size_t>();
			auto from_str = common::to_string(from);
			auto to_str = common::to_string(to);
		
			log::debug<4, true>( 
				"get_historical_bar_range: Asset={} from={} to={} num bars={} body={}",
				Asset, from_str, to_str, num_bars, res->body
			);
		}
		else {
			log::error<true>("get_historical_bar_range error: status={} Asset={} body={}", res->status, Asset, res->body);
		}

		return res->status;
	}

	void plugin_callback(void* address) {
		show("plugin_callback called with address={}", address);
	}

	void trigger_price_quote_request() {
		PostMessage(window_handle, WM_APP + 1, 0, 0);
	}

	DLLFUNC_C void BrokerHTTP(FARPROC fp_send, FARPROC fp_status, FARPROC fp_result, FARPROC fp_free) {
		(FARPROC&)http_send = fp_send;
		(FARPROC&)http_status = fp_status;
		(FARPROC&)http_result = fp_result;
		(FARPROC&)http_free = fp_free;
	}

	DLLFUNC int BrokerLogin(char* user, char* password, char* type, char* account) {
		try {
			if (fix_thread == nullptr) {
				log::debug<1, true>("BrokerLogin: FIX thread createing...");
				fix_thread = std::unique_ptr<FixThread>(new FixThread(
					settings_cfg_file,
					exec_report_queue, 
					top_of_book_queue
				));
				log::debug<1, true>how("BrokerLogin: FIX thread created");
			}

			if (user) {
				log::debug<1, true>("BrokerLogin: FIX service starting...");
				fix_thread->start();
				log::debug<1, true>("BrokerLogin: FIX service running");

				auto count = 0;
				auto start = std::chrono::system_clock::now();
				auto logged_in = fix_thread->fix_app().is_logged_in();
				while (!logged_in && count < 50) {
					std::this_thread::sleep_for(100ms);
					logged_in = fix_thread->fix_app().is_logged_in();
				}
				auto dt = std::chrono::system_clock::now() - start;
				auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(dt).count();
				if (logged_in) {
					log::info<1, true>("BrokerLogin: FIX login after {}ms", ms);
					return BrokerLoginStatus::LoggedIn;
				}
				else {
					throw std::runtime_error(std::format("login timeout after {}ms", ms));
				}
			}
			else {
				log::debug<1, true>("BrokerLogin: FIX service stopping...");
				fix_thread->cancel();
				log::debug<1, true>("BrokerLogin: FIX service stopped");
				return BrokerLoginStatus::LoggedOut; 
			}
		}
		catch (std::exception& e) {
			log::error<true>("BrokerLogin: exception creating/starting FIX service {}", e.what());
			return BrokerLoginStatus::LoggedOut;
		}
		catch (...) {
			log::error<true>("BrokerLogin: unknown exception");
			return BrokerLoginStatus::LoggedOut;
		}
	}

	DLLFUNC int BrokerOpen(char* Name, FARPROC fpError, FARPROC fpProgress) {
		if (Name) strcpy_s(Name, 32, PLUGIN_NAME);

		(FARPROC&)BrokerError = fpError;
		(FARPROC&)BrokerProgress = fpProgress;

		std::string cwd = std::filesystem::current_path().string();
		log::info<1, true>("BrokerOpen: Sim FIX plugin opened in {}", cwd);

		try
		{
			if (!spd_logger) {
				auto postfix = timestamp_posfix();
				auto logger_name = std::format("Log/zorro_sim_fix_plugin_spdlog_{}.log", postfix);
				spd_logger = create_file_logger(logger_name);
			}
		}
		catch (const spdlog::spdlog_ex& ex)
		{
			log::error<true>("BrokerOpen: FIX plugin failed to init log: {}", ex.what());
		}

		return PLUGIN_VERSION;
	}

	DLLFUNC int BrokerTime(DATE* pTimeGMT) 
	{
		if (!fix_thread) {
			return ExchangeStatus::Unavailable;
		}

		const auto time = get_current_system_clock();
		auto n = pop_exec_reports();
		auto m = pop_top_of_books();

		log::debug<4, true>(
			"BrokerTime {} top of book processed={} exec reports processed={}\n{}\n{}", 
			common::to_string(time), m, n, order_tracker.to_string(), order_mapping_string()
		);

		return ExchangeStatus::Open;
	}

	DLLFUNC int BrokerAccount(char* Account, double* pdBalance, double* pdTradeVal, double* pdMarginVal) 
	{
		// TODO 
		double Balance = 10000;
		double TradeVal = 0;
		double MarginVal = 0;

		if (pdBalance && Balance > 0.) *pdBalance = Balance;
		if (pdTradeVal && TradeVal > 0.) *pdTradeVal = TradeVal;
		if (pdMarginVal && MarginVal > 0.) *pdMarginVal = MarginVal;
		return 1;
	}

	/*
	 * BrokerAsset
	 * 
	 * Returns
	 *	 1 when the asset is available and the returned data is valid
	 *   0 otherwise. 
	 * 
	 * An asset that returns 0 after subscription will trigger Error 053, and its trading will be disabled.
	 * 
	 */
	DLLFUNC int BrokerAsset(char* Asset, double* price, double* spread,
		double* volume, double* pip, double* pip_cost, double* min_amount,
		double* margin_cost, double* roll_long, double* roll_short) 
	{

		try {
			if (fix_thread == nullptr) {
				throw std::runtime_error("no FIX session");
			}

			auto& fix_app = fix_thread->fix_app();

			// subscribe to Asset market data
			if (!price) {  
				FIX::Symbol symbol(Asset);
				fix_app.market_data_request(
					symbol,
					FIX::MarketDepth(1),
					FIX::SubscriptionRequestType_SNAPSHOT_AND_UPDATES
				);

				log::info<1, true>("BrokerAsset: subscription request sent for symbol {}", Asset);

				TopOfBook top;
				auto timeout = 2000;
				bool success = top_of_book_queue.pop(top, std::chrono::milliseconds(timeout));
				if (!success) {
					throw std::runtime_error(std::format("failed to get snapshot in {}ms", timeout));
				}
				else {
					log::info<1, true>("BrokerAsset: subscription request obtained symbol {}", Asset);
					return 1;
				}
			}
			else {
				pop_top_of_books();
				auto it = top_of_books.find(Asset);
				if (it != top_of_books.end()) {
					const auto& top = it->second;
					if (price) *price = top.mid();
					if (spread) *spread = top.spread();

					log::debug<4, true>("BrokerAsset: top bid={:.5f} ask={:.5f} @ {}", top.bid_price, top.ask_price, common::to_string(top.timestamp));
				}
				return 1;
			}
		}
		catch (const std::exception& e) {
			log::error<true>("BrokerAsset: exception {}", e.what());
			return 0;
		}
		catch (...) {
			log::error<true>("BrokerAsset: undetermined exception");
			return 0;
		}
	}

	DLLFUNC int BrokerHistory2(char* Asset, DATE tStart, DATE tEnd, int nTickMinutes, int nTicks, T6* ticks) {
		auto seconds = nTicks * nTickMinutes * 60;
		auto tStart2 = tEnd - seconds/SECONDS_PER_DAY;
		auto from = zorro_date_to_string(tStart2);
		auto to = zorro_date_to_string(tEnd);
		
		log::debug<2, true>("BrokerHistory2 {}: requesting {} ticks bar period {} minutes from {} to {}", Asset, nTicks, nTickMinutes, from, to);

		std::map<std::chrono::nanoseconds, Bar> bars;
		auto status = get_historical_bars(Asset, tStart2, tEnd, bars);

		auto count = 0;		
		if (status == httplib::StatusCode::OK_200) {
			for (auto it = bars.rbegin(); it != bars.rend() && count <= nTicks; ++it) {
				const auto& bar = it->second;
				auto time = convert_time_chrono(bar.end);

				log::debug<5, false>(
					"[{}] {} : open={:.5f} high={:.5f} low={:.5f} close={:.5f}", 
					count, zorro_date_to_string(time), bar.open, bar.high, bar.low, bar.close
				);
				
				ticks->fOpen = static_cast<float>(bar.open);
				ticks->fClose = static_cast<float>(bar.close);
				ticks->fHigh = static_cast<float>(bar.high);
				ticks->fLow = static_cast<float>(bar.low);
				ticks->time = time;
				++ticks;
				++count;
			}
		}
		else {
			size_t num_bars;
			std::chrono::nanoseconds from, to;
			auto status = get_historical_bar_range(Asset, from, to, num_bars, false);
			log::error<true>("BrokerHistory2 {}: error status={} bar range from={} to={}", Asset, status, common::to_string(from), common::to_string(to));
		}

		return count;
	}

	/*
	 * BrokerBuy2
	 * 
	 * Sends an order to open a long or short position, either at market, or at a price limit. Also used for NFA compliant accounts to close a position by 
	 * opening a new position in the opposite direction. The order type (FOK, IOC, GTC) can be set with SET_ORDERTYPE before. Orders other than GTC are 
	 * cancelled when they are not completely filled within the wait time (usually 30 seconds).
	 * 
	 * Parameters:
     *	Asset	  Input, asset symbol for trading (see Symbols).
	 *	Amount	  Input, number of contracts, positive for a long trade and negative for a short trade. For currencies or CFDs, the number of contracts is the number of Lots multiplied with the LotAmount. 
	 *            If LotAmount is < 1 (f.i. for a CFD or a fractional share with 0.1 contracts lot size), the number of lots is given here instead of the number of contracts.
	 *	StopDist  Optional input, 'safety net' stop loss distance to the opening price when StopFactor was set, or 0 for no stop, or -1 for indicating that this function was called for closing a position. 
	 *            This is not the real stop loss, which is handled by Zorro. Can be ignored if the API is NFA compliant and does not support a stop loss in a buy/sell order.
	 *	Limit	  Optional input, fill price for limit orders, set up by OrderLimit, or 0 for market orders. Can be ignored if limit orders are not supported by the API.
	 *	pPrice	  Optional output, the average fill price if the position was partially or fully filled.
	 *	pFill	  Optional output, the fill amount, always positive.
	 * 
	 * Returns see BrokerBuyStatus
	 *  if successful 
	 *    Trade or order id 
	 *  or on error
	 *	 0 when the order was rejected or a FOK or IOC order was unfilled within the wait time (adjustable with the SET_WAIT command). The order must then be cancelled by the plugin.
	 *	   Trade or order ID number when the order was successfully placed. If the broker API does not provide trade or order IDs, the plugin should generate a unique 6-digit number, f.i. from a counter, and return it as a trade ID.
	 *	-1 when the trade or order identifier is a UUID that is then retrieved with the GET_UUID command.
	 *	-2 when the broker API did not respond at all within the wait time. The plugin must then attempt to cancel the order. Zorro will display a "possible orphan" warning.
	 *	-3 when the order was accepted, but got no ID yet. The ID is then taken from the next subsequent BrokerBuy call that returned a valid ID. This is used for combo positions that require several orders.
	 */
	DLLFUNC_C int BrokerBuy2(char* Asset, int amount, double stop, double limit, double* av_fill_price, int* fill_qty) {
		log::debug<2, true>("BrokerBuy2: {} amount={} limit={}", Asset, amount, limit);

		if (!fix_thread) {
			log::error<true>("BrokerBuy2: no FIX session");
			return BrokerError::BrokerAPITimeout;
		}

		auto symbol = FIX::Symbol(Asset);
		FIX::OrdType ord_type;
		if (limit && !stop)
			ord_type = FIX::OrdType_LIMIT;
		else if (limit && stop)
			ord_type = FIX::OrdType_STOP_LIMIT;
		else if (!limit && stop)
			ord_type = FIX::OrdType_STOP;
		else
			ord_type = FIX::OrdType_MARKET;

		auto cl_ord_id = FIX::ClOrdID(next_client_order_id());
		auto side = amount > 0 ? FIX::Side(FIX::Side_BUY) : FIX::Side(FIX::Side_SELL);
		auto qty = FIX::OrderQty(std::abs(amount));
		auto limit_price = FIX::Price(limit);
		auto stop_price = FIX::StopPx(stop);

		auto msg = fix_thread->fix_app().new_order_single(
			symbol, cl_ord_id, side, ord_type, time_in_force, qty, limit_price, stop_price
		);

		log::debug<2, true>("BrokerBuy2: NewOrderSingle {}", fix_string(msg));

		ExecReport report;
		bool success = exec_report_queue.pop(report, fix_exec_report_waiting_time);

		if (!success) {
			log::error<true>("BrokerBuy2 timeout while waiting for FIX exec report on order new!");
			return BrokerError::OrderRejectedOrTimeout;
		}
		else {
			if (report.exec_type == FIX::ExecType_REJECTED) {
				log::info<1, true>("BrokerBuy2: rejected {}", report.to_string());
				return BrokerError::OrderRejectedOrTimeout;
			}
			else if (report.cl_ord_id == cl_ord_id.getString()) { 
				auto interal_id = next_internal_order_id();
				order_id_by_internal_order_id.emplace(interal_id, report.ord_id);
				auto ok = order_tracker.process(report);

				if (report.ord_status == FIX::OrdStatus_FILLED || report.ord_status == FIX::OrdStatus_PARTIALLY_FILLED) {
					if (av_fill_price) {
						*av_fill_price = report.avg_px;
					}
				}

				if (fill_qty) {
					*fill_qty = (int)report.cum_qty;  // assuming lot size
				}

				log::debug<2, true>(
					"BrokerBuy2: interal_id={} processed={}\n{}\n{}", 
					interal_id, report.to_string(), order_tracker.to_string(), order_mapping_string()
				);

				return interal_id;
			}
			else {
				// TODO what should be done in this rare case?
				// example: two orders at almost same time, 
				// exec report of the second order arrives first 
								
				log::info<1, true>(
					"BrokerBuy2: report {} does belong to given cl_ord_id={}", 
					report.to_string(), cl_ord_id.getString()
				);

				return BrokerError::OrderRejectedOrTimeout;
			}
		}		
	}

	/* BrokerTrade
	 *
	 * Optional function that returns the order fill state (for brokers that support only orders and positions) or the trade 
	 * state (for brokers that support individual trades). Called by Zorro for any open trade when the price moved by more than 1 pip, 
	 * or when contractUpdate or contractPrice is called for an option or future trade.
	 * 
	 * Parameters:
     *	nTradeID	Input, order/trade ID as returned by BrokerBuy, or -1 when the trade UUID was set before with a SET_UUID command.
	 *	pOpen	Optional output, the average fill price if the trade was partially or fully filled. If not available by the API, Zorro will estimate the values based on last price and asset parameters.
	 *	pClose	Optional output, current bid or ask close price of the trade. If not available, Zorro will estimale the value based on current ask price and ask-bid spread.
	 *	pCost	Optional output, total rollover fee (swap fee) of the trade so far. If not available, Zorro will estimate the swap from the asset parameters.
	 *	pProfit	Optional output, current profit or loss of the trade in account currency units, without rollover and commission. If not available, Zorro will estimate the profit from the difference of current price and fill price.
	 * 
	 * Returns:
	 *	Number of contracts or lots (as in BrokerBuy2) currently filled for the trade.
	 *	  - -1 when the trade was completely closed.
	 *	  - NAY (defined in trading.h) when the order or trade state was unavailable. Zorro will then assume that the order was completely filled, and keep the trade open.
	 *	  - NAY-1 when the order was cancelled or removed by the broker. Zorro will then cancel the trade and book the profit or loss based on the current price and the last fill amount.
	 */
	DLLFUNC int BrokerTrade(int trade_id, double* open, double* close, double* cost, double* profit) {
		log::debug<1, true>("BrokerTrade: {}", trade_id);

		// pop all the exec reports and markeet data from the queue to have up to date information
		pop_top_of_books();
		pop_exec_reports();

		auto it = order_id_by_internal_order_id.find(trade_id);
		if (it != order_id_by_internal_order_id.end()) {
			auto [oit, success] = order_tracker.get_order(it->second);
			if (success) {
				if (open) {
					*open = oit->second.avg_px;
				}
				if (profit) {
					auto pit = top_of_books.find(oit->second.symbol);
					if (pit != top_of_books.end()) {
						*profit = oit->second.side == FIX::Side_BUY 
							? (pit->second.ask_price - oit->second.avg_px) * oit->second.cum_qty
							: (oit->second.avg_px - pit->second.bid_price) * oit->second.cum_qty;
						*cost = 0; // TODO
					}
				}
				auto filled = (int)oit->second.cum_qty;

				auto o = open != nullptr ? *open : 0;
				auto p = profit != nullptr ? *profit : 0;
				log::debug<2, true>("BrokerTrade: nTradeID={} avg_px={} profit={} filled={}", trade_id, o, p, filled);

				return filled;
			}
		}

		return 0;
	}

	/* BrokerSell2
	 *
	 * Optional function; closes a trade - completely or partially - at market or at a limit price.If partial closing is not supported,
	 * nAmount is ignored and the trade is completely closed.Only used for not NFA compliant accounts that support individual closing of trades.
	 * If this function is not provided or if the NFA flag is set, Zorro closes the trade by calling BrokerBuy2 with the negative amount and with StopDist at - 1.
	 * 
	 * Parameters:
	 *	nTradeID	Input, trade/order ID as returned by BrokerBuy2, or -1 for a UUID to be set before with a SET_UUID command.
	 *	nAmount	Input, number of contracts resp. lots to be closed, positive for a long trade and negative for a short trade (see BrokerBuy). If less than the original size of the trade, the trade is partially closed.
	 *	Limit	Optional input, fill price for a limit order, set up by OrderLimit, or 0 for closing at market. Can be ignored if limit orders are not supported by the API. 
	 *	pClose	Optional output, close price of the trade.
	 *	pCost	Optional output, total rollover fee (swap fee) of the trade.
	 *	pProfit	Optional output, total profit or loss of the trade in account currency units.
	 *	pFill	Optional output, the amount that was closed from the position, always positive.
	 * 
	 * Returns:
	 *	  - New trade ID when the trade was partially closed and the broker assigned a different ID to the remaining position.
	 *	  - nTradeID when the ID did not change or the trade was fully closed.
	 *	  - 0 when the trade was not found or could not be closed.
	 */
	DLLFUNC_C int BrokerSell2(int trade_id, int amount, double limit, double* close, double* cost, double* profit, int* fill) {
		log::debug<1, true>("BrokerSell2 nTradeID={} nAmount={} limit={}", trade_id, amount, limit);

		auto it = order_id_by_internal_order_id.find(trade_id);
		if (it != order_id_by_internal_order_id.end()) {
			auto [oit, success] = order_tracker.get_order(it->second);

			if (success) {
				auto& order = oit->second;

				log::debug<2, true>("BrokerSell2: found open order={}", order.to_string());

				if (order.ord_status == FIX::OrdStatus_CANCELED || order.ord_status == FIX::OrdStatus_REJECTED) {
					log::debug<2, true>("BrokerSell2: order rejected or already cancelled");
					return 0;
				}

				// trade opposite quantity for a fully filled then offset the trade - here the amount is not needed
				if (order.ord_status == FIX::OrdStatus_FILLED) {
					double close_price;
					int close_fill;
					int signed_qty = (int)(order.side == FIX::Side_BUY ? -order.cum_qty : order.cum_qty);

					log::debug<2, true>("BrokerSell2: closing filled order with trade in opposite direction signed_qty={}, limit={}", signed_qty, limit);

					auto asset = const_cast<char*>(order.symbol.c_str());
					auto trade_id_close = BrokerBuy2(asset, signed_qty, 0, limit, &close_price, &close_fill);

					if (trade_id_close) {
						auto cit = order_id_by_internal_order_id.find(trade_id_close);
						if (cit != order_id_by_internal_order_id.end()) {
							auto [coit, success] = order_tracker.get_order(cit->second);
							auto& close_order = coit->second;
							if (close) {
								*close = close_order.avg_px;
							}
							if (fill) {
								*fill = (int)close_order.cum_qty;
							}
							if (profit) {
								*profit = (close_order.avg_px - order.avg_px) * close_order.cum_qty;
							}
						}

						return trade_id;
					}
					else {
						log::error<true>("BrokerSell2: closing filled order failed");
						return 0;
					}
				}

				// if order is still working perform a cancel/replace here the amount should be always <= order_qty
				if (order.ord_status == FIX::OrdStatus_PENDING_NEW || order.ord_status == FIX::OrdStatus_NEW || order.ord_status == FIX::OrdStatus_PARTIALLY_FILLED) {
					auto symbol = FIX::Symbol(order.symbol);
					auto ord_id = FIX::OrderID(order.ord_id);
					auto orig_cl_ord_id = FIX::OrigClOrdID(order.cl_ord_id);
					auto cl_ord_id = FIX::ClOrdID(next_client_order_id());
					auto side = FIX::Side(order.side);
					auto ord_type = FIX::OrdType(order.ord_type);

					if (std::abs(amount) > order.order_qty) {
						log::error<true>("BrokerSell2: trying to cancel/replace with an amount={} > order.order_qty={}!", amount, order.order_qty);
						return 0;
					}

					if (std::abs(amount) >= order.leaves_qty) {
						log::debug<2, true>("BrokerSell2: cancel working order completely");

						auto msg = fix_thread->fix_app().order_cancel_request(
							symbol, ord_id, orig_cl_ord_id, cl_ord_id, side, FIX::OrderQty(order.leaves_qty)
						);

						ExecReport report;
						bool success = exec_report_queue.pop(report, fix_exec_report_waiting_time);

						if (!success) {
							log::error<true>("BrokerSell2: timeout while waiting for FIX exec report on order cancel!");
							return 0;
						} 
						else {
							auto ok = order_tracker.process(report);
							log::debug<2, true>(
								"BrokerSell2: exec report processed={}\n{}\n{}", 
								report.to_string(), order_tracker.to_string(), order_mapping_string()
							);
						}

						return trade_id;
					}
					else {
						int leaves_qty = (int)order.leaves_qty;
						auto target_qty = leaves_qty - std::abs(amount);

						log::debug<2, true>( 
							"BrokerSell2: cancel/replace working order from leaves_qty={} to target_qty={}",
							leaves_qty, target_qty
						);

						auto new_qty = max(target_qty, 0);
						auto msg = fix_thread->fix_app().order_cancel_replace_request(
							symbol, ord_id, orig_cl_ord_id, cl_ord_id, side, ord_type, 
							FIX::OrderQty(new_qty), FIX::Price(order.price)
						);

						ExecReport report;
						bool success = exec_report_queue.pop(report, fix_exec_report_waiting_time);

						if (!success) {
							log::error<true>("BrokerSell2: timeout while waiting for FIX exec report on order cancel/replace!");
							return 0;
						}
						else {
							auto ok = order_tracker.process(report);
							log::debug<2, true>(
								"BrokerSell2: exec report processed \n{}\n{}", 
								order_tracker.to_string(), order_mapping_string()
							);
						}

						return trade_id;
					}
				}

				log::error<true>("BrokerSell2: unexpected order status {}", ord_status_string(order.ord_status));
				return 0;
			}
			else {
				log::error<true>("BrokerSell2: could not find open order with ord_id={} ", it->second);
			}
		}
		else {
			log::error<true>("BrokerSell2: mapping not found for trade_id={} ", trade_id);
		}

		return 0;
	}

	// https://zorro-project.com/manual/en/brokercommand.htm
	DLLFUNC double BrokerCommand(int command, DWORD dw_parameter) {
		switch (command) {
			case GET_COMPLIANCE: {
				auto result = 2;
				log::debug<1, true>("BrokerCommand {}[{}] = {}", broker_command_string(command), command, result);
				return result;
			}

			case GET_BROKERZONE: {
				log::debug<1, true>("BrokerCommand {}[{}] = {}", broker_command_string(command), command, 0);
				return 0;   // historical data in UTC time
			}

			case GET_MAXTICKS: {
				auto result = 1000;
				log::debug<1, true>("BrokerCommand {}[{}] = {}", broker_command_string(command), command, result);
				return result;
			}

			case GET_MAXREQUESTS: {
				log::debug<1, true>("BrokerCommand {}[{}] = {}", broker_command_string(command), command, 30);
				return 30;
			}

			case GET_LOCK: {
				log::debug<1, true>("BrokerCommand {}[{}] = {}", broker_command_string(command), command, 1);
				return 1;
			}

			case GET_NTRADES: {
				auto result = get_num_orders();
				log::debug<1, true>("BrokerCommand {}[{}] = {}", broker_command_string(command), command, result);
				return result;
			}

			case GET_POSITION: {
				position_symbol = std::string((const char*)dw_parameter);
				auto result = get_position_size(position_symbol);
				log::debug<1, true>("BrokerCommand {}[{}] = {}", broker_command_string(command), command, result);
				return result;
			}

			case GET_AVGENTRY: {
				auto result = get_avg_entry_price(position_symbol);
				log::debug<1, true>("BrokerCommand {}[{}] = {}", broker_command_string(command), command, result);
				return result;
			}

			// Returns 1 when the order was cancelled, or 0 when the order was not found or could not be cancelled.						
			case DO_CANCEL: {
				int trade_id = (int)dw_parameter;
				log::debug<1, true>("BrokerCommand {}[{}] trade_id={}", broker_command_string(command), command, trade_id);

				auto it = order_id_by_internal_order_id.find(trade_id); if (it != order_id_by_internal_order_id.end()) {
					auto [oit, success] = order_tracker.get_order(it->second);

					if (success) {
						auto& order = oit->second;

						log::debug<2, true>("BrokerCommand[DO_CANCEL]: found open order={}", order.to_string());

						if (order.ord_status == FIX::OrdStatus_FILLED) {
							log::debug<2, true>("BrokerCommand[DO_CANCEL]: order already filled");
							return 0;
						}

						if (order.ord_status == FIX::OrdStatus_CANCELED || order.ord_status == FIX::OrdStatus_REJECTED) {
							log::debug<2, true>("BrokerCommand[DO_CANCEL]: order rejected or already cancelled");
							return 0;
						}

						auto symbol = FIX::Symbol(order.symbol);
						auto ord_id = FIX::OrderID(order.ord_id);
						auto orig_cl_ord_id = FIX::OrigClOrdID(order.cl_ord_id);
						auto cl_ord_id = FIX::ClOrdID(next_client_order_id());
						auto side = FIX::Side(order.side);
						auto ord_type = FIX::OrdType(order.ord_type);

						log::debug<2, true>("BrokerCommand[DO_CANCEL]: cancel working order");

						auto msg = fix_thread->fix_app().order_cancel_request(
							symbol, ord_id, orig_cl_ord_id, cl_ord_id, side, FIX::OrderQty(order.leaves_qty)
						);

						ExecReport report;
						bool success = exec_report_queue.pop(report, fix_exec_report_waiting_time);

						if (!success) {
							log::error<true>("BrokerCommand[DO_CANCEL] timeout while waiting for FIX exec report on order cancel!");

							return 0;
						}
						else {
							auto ok = order_tracker.process(report);
							log::debug<2, true>(
								"BrokerCommand[DO_CANCEL]: exec report processed={}\n{}\n{}",
								report.to_string(), order_tracker.to_string(), order_mapping_string()
							);

							return 1;
						}

					}
				}

				return 0;
			}

			case SET_ORDERTEXT: {
				order_text = std::string((char*)dw_parameter); 
				log::debug<1, true>("BrokerCommand {}[{}] order_text={}", broker_command_string(command), command, order_text);
				return 1;
			}

			case SET_SYMBOL: {
				asset = std::string((char*)dw_parameter);
				log::debug<1, true>("BrokerCommand {}[{}] symbol={}", broker_command_string(command), command, asset);
				return 1;
			}

			case SET_MULTIPLIER: {
				multiplier = (int)dw_parameter;
				log::debug<1, true>("BrokerCommand {}[{}] multiplier={}", broker_command_string(command), command, multiplier);
				return 1;
			}
			
			// Switch between order types and return the type if supported by the broker plugin, otherwise 0
			//	- 0 Broker default (highest fill probability)
			//	- 1 AON (all or nothing) prevents partial fills
			//	- 2 GTC (good - till - cancelled) order stays open until completely filled
			//	- 3 AON + GTC
			//  - 8 - STOP; add a stop order at distance Stop* StopFactor on NFA accounts
			case SET_ORDERTYPE: {
				auto order_type = (int)dw_parameter;
				switch (order_type) {
				case 0:
					return 0; 
				case ORDERTYPE_IOC:
					time_in_force = FIX::TimeInForce_IMMEDIATE_OR_CANCEL;
					break;
				case ORDERTYPE_GTC:
					time_in_force = FIX::TimeInForce_GOOD_TILL_CANCEL;
					break;
				case ORDERTYPE_FOK:
					time_in_force = FIX::TimeInForce_FILL_OR_KILL;
					break;
				case ORDERTYPE_DAY:
					time_in_force = FIX::TimeInForce_DAY;
					break;
				default:
					return 0; // return 0 for not supported	
				}

				// additional stop order 
				if (order_type >= 8) {
					return 0; // return 0 for not supported	
				}

				log::debug<1, true>("BrokerCommand {}[{}] order_type={}", broker_command_string(command), command, order_type);

				return 1;
			}

			case GET_PRICETYPE: {
				log::debug<1, true>("BrokerCommand {}[{}] = {}", broker_command_string(command), command, price_type);
				return price_type;
			}

			case SET_PRICETYPE: {
				price_type = (int)dw_parameter;
				log::debug<1, true>("BrokerCommand {}[{}] price_type={}", broker_command_string(command), command, price_type);
				return 1;
			}

			case GET_VOLTYPE: {
				vol_type = (int)dw_parameter;
				log::debug<1, true>("BrokerCommand {}[{}] price_type={}", broker_command_string(command), command, vol_type);
				return vol_type;
			}

			case SET_AMOUNT: {
				amount = *(double*)dw_parameter;
				log::debug<1, true>("BrokerCommand {}[{}] amount={}", broker_command_string(command), command, amount);
				return 1;
			}

			case SET_DIAGNOSTICS: {
				auto diagnostics = (int)dw_parameter;
				log::debug<1, true>("BrokerCommand {}[{}] diagnostics={}", broker_command_string(command), command, diagnostics);
				if (diagnostics == 1 || diagnostics == 0) {
					spdlog::set_level(diagnostics ? spdlog::level::debug : spdlog::level::info);
					return 1;
				}
				break;
			}

			// The window handle can be used by another application or process for triggering events or sending messages to a Zorro window. 
			// The message WM_APP+1 triggers a price quote request, WM_APP+2 triggers a script-supplied callback function, and WM_APP+3 
			// triggers a plugin-supplied callback function set by the GET_CALLBACK command.
			// The window handle is automatically sent to broker plugins with the SET_HWND command.
			// For asynchronously triggering a price quote request, send a WM_APP+1 message to the window handle received by the SET_HWND command. 
			// For triggering the callback function, send a WM_APP+2 message. This can be used for prices streamed by another thread. 
			// See https://zorro-project.com/manual/en/hwnd.htm
			case SET_HWND: {
				window_handle = (HWND)dw_parameter;
				log::debug<1, true>("BrokerCommand {}[{}] window_handle={}", broker_command_string(command), command, (long)window_handle);
				break;
			}

			case GET_CALLBACK: {
				auto ptr = (void*)plugin_callback;
				log::debug<1, true>("BrokerCommand {}[{}] = {}", broker_command_string(command), command, ptr);
				break;
			}

			case SET_CCY: {
				currency = std::string((char*)dw_parameter);
				log::debug<1, true>("BrokerCommand {}[{}] currency={}", broker_command_string(command), command, currency);
				break;
			}

			case GET_HEARTBEAT: {
				log::debug<1, true>("BrokerCommand {}[{}] = {}", broker_command_string(command), command, 500);
				return 500;
			}

			case SET_LEVERAGE: {
				leverage = (int)dw_parameter;
				log::debug<1, true>("BrokerCommand {}[{}] leverage={}", broker_command_string(command), command, leverage);
				break;
			}

			case SET_LIMIT: {
				limit = *(double*)dw_parameter;
				log::debug<1, true>("BrokerCommand {}[{}] limit={}", broker_command_string(command), command, limit);
				break;
			}

			case SET_FUNCTIONS: {
				log::debug<1, true>("BrokerCommand {}[{}]", broker_command_string(command), command);
				break;
			}

			default: {
				log::debug<1, true>("BrokerCommand {}[{}]", broker_command_string(command), command);
				break;
			}
		}

		return 0;
	}
}


