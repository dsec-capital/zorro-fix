#pragma warning(disable : 4996 4244 4312)

#include "pch.h"

#include "application.h"
#include "fix_thread.h"
#include "zorro_fix_plugin.h"

#include "common/market_data.h"
#include "common/exec_report.h"
#include "common/blocking_queue.h"
#include "common/json.h"
#include "common/time_utils.h"

#include "broker_commands.h"

#include "nlohmann/json.h"
#include "httplib/httplib.h"
#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"

#define PLUGIN_VERSION 2

// Days between midnight 1899-12-30 and midnight 1970-01-01 is 25569
#define DAYS_BETWEEN_1899_12_30_1979_01_01	25569.0
#define SECONDS_PER_DAY						86400.0
#define MILLIS_PER_DAY                      86400000.0 
#define MICROS_PER_DAY                      86400000000.0 

namespace zfix {

	using namespace common;
	using namespace std::chrono_literals;

	enum ExchangeStatus {
		Unavailable = 0,
		Closed = 1,
		Open = 2
	};

	enum BrokerLoginStatus {
		LoggedOut = 0,
		LoggedIn = 1
	};

	enum BrokerBuyError {
		OrderRejectedOrTimeout = 0,
		TradeOrderIdUUID = -1,
		BrokerAPITimeout = -2,
		OrderAcceptedWithoutOrderId = -3
	};

	bool log_history_bars = false;

	// http://localhost:8080/bars?symbol=AUD/USD
	std::string rest_host = "http://localhost";
	int rest_port = 8080;
	httplib::Client rest_client(std::format("{}:{}", rest_host, rest_port));

	int max_snaphsot_waiting_iterations = 10; 
	std::chrono::milliseconds fix_exec_report_waiting_time = 500ms;
	std::string settings_cfg_file = "Plugin/zorro_fix_client.cfg";

	int client_order_id = 0;
	int internal_order_id = 1000;
	auto time_in_force = FIX::TimeInForce_GOOD_TILL_CANCEL;

	std::shared_ptr<spdlog::logger> spd_logger = nullptr;
	std::unique_ptr<zfix::FixThread> fix_thread = nullptr;
	BlockingTimeoutQueue<ExecReport> exec_report_queue;
	BlockingTimeoutQueue<TopOfBook> top_of_book_queue;  
	std::unordered_map<int, std::string> order_id_by_internal_order_id;
	OrderTracker order_tracker("account");
	std::unordered_map<std::string, TopOfBook> top_of_books;

	void show(const std::string& msg) {
		if (!BrokerError) return;
		auto tmsg = "[" + now_str() + "] " + msg + "\n";
		BrokerError(tmsg.c_str());
	}

	int pop_exec_reports() {
		auto n = exec_report_queue.pop_all(
			[](const ExecReport& report) { 
				order_tracker.process(report); 
			}
		);
		return n;
	}

	int pop_top_of_books() {
		auto n = top_of_book_queue.pop_all(
			[](const TopOfBook& top) { 
				top_of_books.insert(std::make_pair(top.symbol, top)); 
			}
		);
		return n;
	}

	int get_position_size(const std::string& symbol) {
		auto np = order_tracker.net_position(symbol);
		return (int)np.qty;
	}

	std::string next_client_order_id() {
		++client_order_id;
		return std::format("cl_ord_id_{}", client_order_id);
	}

	int next_internal_order_id() {
		++internal_order_id;
		return internal_order_id;
	}

	// From Zorro documentation:
	//		If not stated otherwise, all dates and times reflect the time stamp of the Close price at the end of the bar. 
	//		Dates and times are normally UTC. Timestamps in historical data are supposed to be either UTC, or to be a date 
	//		only with no additional time part, as for daily bars. 
	//		It is possible to use historical data with local timestamps in the backtest, but this must be considered in the 
	//		script since the date and time functions will then return the local time instead of UTC, and time zone functions 
	//		cannot be used.

	// DATE is fractional time in days since midnight 1899-12-30
	// The type __time32_t is representing the time as seconds elapsed since midnight 1970-01-01
	DATE convert_time(__time32_t t32)
	{
		return (DATE)t32 / SECONDS_PER_DAY + DAYS_BETWEEN_1899_12_30_1979_01_01;  
	}

	__time32_t convert_time(DATE date)
	{
		return (__time32_t)((date - DAYS_BETWEEN_1899_12_30_1979_01_01) * SECONDS_PER_DAY);
	}

	std::chrono::nanoseconds convert_time_chrono(DATE date) {
		auto count = (long long)((date - DAYS_BETWEEN_1899_12_30_1979_01_01) * MICROS_PER_DAY);
		auto us = std::chrono::microseconds(count);
		return std::chrono::duration_cast<std::chrono::nanoseconds>(us);
	}

	DATE convert_time_chrono(const std::chrono::nanoseconds& t) {
		auto us = std::chrono::duration_cast<std::chrono::microseconds>(t).count();
		return (DATE)us / MICROS_PER_DAY + DAYS_BETWEEN_1899_12_30_1979_01_01;
	}

	std::string zorro_date_to_string(DATE date, bool millis=false) {
		auto ts = convert_time(date);
		auto ms = millis ? (long)(date * 1000) % 1000 : 0;
		return time32_to_string(ts, ms);
	}

	// get historical data - note time is in UTC
	// http://localhost:8080/bars?symbol=EUR/USD&from=2024-03-30 12:00:00&to=2024-03-30 16:00:00
	int get_historical_bars(const char* Asset, DATE from, DATE to, std::map<std::chrono::nanoseconds, Bar>& bars, bool verbose=false) {
		auto from_str = zorro_date_to_string(from);
		auto to_str = zorro_date_to_string(to);
		auto request = std::format("/bars?symbol={}&from={}&to={}", Asset, from_str, to_str);
		auto res = rest_client.Get(request);
		if (res->status == httplib::StatusCode::OK_200) {
			auto j = json::parse(res->body);
			from_json(j, bars);
			if (verbose) {
				show(std::format(
					"get_historical_bars: Asset={} from={} to={} num bars={}", 
					Asset, from_str, to_str, bars.size()
				));
			}
		}
		else {
			show(std::format(
				"get_historical_bars error: status={} Asset={} from={} to={} body={}", 
				res->status, Asset, from_str, to_str, res->body
			));
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
			if (verbose) {
				show(std::format(
					"get_historical_bar_range: Asset={} from={} to={} num bars={} body={}",
					Asset, from_str, to_str, num_bars, res->body
				));
			}
		}
		else {
			show(std::format(
				"get_historical_bar_range error: status={} Asset={} body={}", 
				res->status, Asset, res->body
			));
		}

		return res->status;
	}

	DLLFUNC_C void BrokerHTTP(FARPROC fpSend, FARPROC fpStatus, FARPROC fpResult, FARPROC fpFree) {
		(FARPROC&)http_send = fpSend;
		(FARPROC&)http_status = fpStatus;
		(FARPROC&)http_result = fpResult;
		(FARPROC&)http_free = fpFree;
	}

	DLLFUNC int BrokerLogin(char* User, char* Pwd, char* Type, char* Account) {
		try {
			if (fix_thread == nullptr) {
				show("BrokerLogin: FIX thread createing...");
				fix_thread = std::unique_ptr<FixThread>(new FixThread(
					settings_cfg_file,
					exec_report_queue, 
					top_of_book_queue
				));
				show("BrokerLogin: FIX thread created");
			}
			if (User) {
				show("BrokerLogin: FIX service starting...");
				fix_thread->start();
				show("BrokerLogin: FIX service running");
				spdlog::debug("BrokerLogin: FIX service running");

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
					show(std::format("BrokerLogin: FIX login after {}ms", ms));
					spdlog::debug("BrokerLogin: FIX login after {}ms", ms);
					return BrokerLoginStatus::LoggedIn;
				}
				else {
					throw std::runtime_error(std::format("login timeout after {}ms", ms));
				}
			}
			else {
				show("BrokerLogin: FIX service stopping...");
				fix_thread->cancel();
				show("BrokerLogin: FIX service stopped");
				spdlog::debug("BrokerLogin: FIX service stopped");
				return BrokerLoginStatus::LoggedOut; 
			}
		}
		catch (std::exception& e) {
			show(std::format("BrokerLogin: exception creating/starting FIX service {}", e.what()));
			spdlog::debug("BrokerLogin: exception creating/starting FIX service {}", e.what());
			return BrokerLoginStatus::LoggedOut;
		}
		catch (...) {
			show("BrokerLogin: unknown exception");
			spdlog::debug("BrokerLogin: unknown exception");
			return BrokerLoginStatus::LoggedOut;
		}
	}

	DLLFUNC int BrokerOpen(char* Name, FARPROC fpError, FARPROC fpProgress) {
		if (Name) strcpy_s(Name, 32, "_FixPlugin");
		(FARPROC&)BrokerError = fpError;
		(FARPROC&)BrokerProgress = fpProgress;

		std::string cwd = std::filesystem::current_path().string();
		show(std::format("BrokerOpen: FIX plugin opened in {}", cwd));

		try
		{
			if (!spd_logger) {
				auto postfix = timestamp_posfix();
				spd_logger = spdlog::basic_logger_mt(
					"standard_logger",
					std::format("Log/zorro-fix-bridge_spdlog_{}.log", postfix)
				);
				spd_logger->set_level(spdlog::level::debug);
				spdlog::set_default_logger(spd_logger);
				spdlog::set_level(spdlog::level::debug);
				spdlog::flush_every(std::chrono::seconds(2));
				spdlog::info("Logging started, level={}, cwd={}", (int)spd_logger->level(), cwd);
			}
		}
		catch (const spdlog::spdlog_ex& ex)
		{
			show(std::format("BrokerOpen: FIX plugin failed to init log: {}", ex.what()));
		}

		return PLUGIN_VERSION;
	}

	DLLFUNC int BrokerTime(DATE* pTimeGMT) 
	{
		if (!fix_thread) {
			return ExchangeStatus::Unavailable;
		}

		const auto time = get_current_system_clock();
		//show(std::format("BrokerTime {}", common::to_string(time)));

		auto n = pop_exec_reports();
		//show(std::format("BrokerTime {} exec reports processed", n));

		auto m = pop_top_of_books();
		//show(std::format("BrokerTime {} top of book processed", m));

		show(order_tracker.to_string());

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
				show(std::format("BrokerAsset: subscribing for symbol {}", Asset));

				FIX::Symbol symbol(Asset);
				fix_app.market_data_request(
					symbol,
					FIX::MarketDepth(1),
					FIX::SubscriptionRequestType_SNAPSHOT_AND_UPDATES
				);

				show(std::format("BrokerAsset: subscription request sent for symbol {}", Asset));

				TopOfBook top;
				auto timeout = 2000;
				bool success = top_of_book_queue.pop(top, std::chrono::milliseconds(timeout));
				if (!success) {
					throw std::runtime_error(std::format("failed to get snapshot in {}ms", timeout));
				}
				else {
					show(std::format("BrokerAsset: subscription request obtained symbol {}", Asset));
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
					show(std::format("BrokerAsset: top bid={:.5f} ask={:.5f} @ {}", top.bid_price, top.ask_price, common::to_string(top.timestamp)));
				}
				return 1;
			}
		}
		catch (const std::exception& e) {
			show(std::format("BrokerAsset: exception {}", e.what()));
			return 0;
		}
		catch (...) {
			show("BrokerAsset: undetermined exception");
			return 0;
		}
	}

	DLLFUNC int BrokerHistory2(char* Asset, DATE tStart, DATE tEnd, int nTickMinutes, int nTicks, T6* ticks) {
		auto seconds = nTicks * nTickMinutes * 60;
		auto tStart2 = tEnd - seconds/SECONDS_PER_DAY;
		auto from = zorro_date_to_string(tStart2);
		auto to = zorro_date_to_string(tEnd);
		
		spdlog::debug("BrokerHistory2 {}: requesting {} ticks bar period {} minutes from {} to {}", Asset, nTicks, nTickMinutes, from, to);
		show(std::format("BrokerHistory2 {}: requesting {} ticks bar period {} minutes from {} to {}", Asset, nTicks, nTickMinutes, from, to));

		std::map<std::chrono::nanoseconds, Bar> bars;
		auto status = get_historical_bars(Asset, tStart2, tEnd, bars, false);

		auto count = 0;		
		if (status == httplib::StatusCode::OK_200) {
			for (auto it = bars.rbegin(); it != bars.rend() && count <= nTicks; ++it) {
				const auto& bar = it->second;
				auto time = convert_time_chrono(bar.end);
				if (log_history_bars) {
					spdlog::debug(
						"[{}] {} : open={:.5f} high={:.5f} low={:.5f} close={:.5f}", 
						count, zorro_date_to_string(time), bar.open, bar.high, bar.low, bar.close
					);
				}
				ticks->fOpen = (float)bar.open;
				ticks->fClose = (float)bar.close;
				ticks->fHigh = (float)bar.high;
				ticks->fLow = (float)bar.low;
				ticks->time = time;
				++ticks;
				++count;
			}
		}
		else {
			size_t num_bars;
			std::chrono::nanoseconds from, to;
			auto status = get_historical_bar_range(Asset, from, to, num_bars, false);
			spdlog::debug("BrokerHistory2 {}: error status={} bar range from={} to={}", Asset, status, common::to_string(from), common::to_string(to));
			show(std::format("BrokerHistory2 {}: error status={} bar range from={} to={}", Asset, status, common::to_string(from), common::to_string(to)));
		}

		return count;
	}

	/*
	* BrokerBuy2
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
		spdlog::debug("BrokerBuy2: {} amount={} limit={}", Asset, amount, limit);
		show(std::format("BrokerBuy2: {} amount={} limit={}", Asset, amount, limit));

		if (!fix_thread) {
			show("BrokerBuy2: no FIX session");
			return BrokerBuyError::BrokerAPITimeout;
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

		spdlog::debug("BrokerBuy2: NewOrderSingle {}", fix_string(msg));
		show(std::format("BrokerBuy2: NewOrderSingle {}", fix_string(msg)));

		ExecReport report;
		bool success = exec_report_queue.pop(report, fix_exec_report_waiting_time);

		if (!success) {
			show("BrokerBuy2 timeout while waiting for FIX exec report on order new!");
			return BrokerBuyError::OrderRejectedOrTimeout;
		}
		else {
			if (report.exec_type == FIX::ExecType_REJECTED) {
				spdlog::debug("BrokerBuy2: rejected {}", report.to_string());
				show(std::format("BrokerBuy2: rejected {}", report.to_string()));
				return BrokerBuyError::OrderRejectedOrTimeout;
			}
			else if (report.cl_ord_id == cl_ord_id.getString()) {
				auto i_ord_id = next_internal_order_id();
				order_id_by_internal_order_id.emplace(i_ord_id, report.order_id); // TODO remove the mappings at some point
				order_tracker.process(report);

				if (report.ord_status == FIX::OrdStatus_FILLED || report.ord_status == FIX::OrdStatus_PARTIALLY_FILLED) {
					if (av_fill_price) {
						*av_fill_price = report.avg_px;
					}
					if (fill_qty) {
						*fill_qty = (int)report.cum_qty;  // assuming lot size
					}
				}

				spdlog::debug("BrokerBuy2: processed {} \n{}", report.to_string(), order_tracker.to_string());
				show(std::format("BrokerBuy2: processed {} \n{}", report.to_string(), order_tracker.to_string()));

				return i_ord_id;
			}
			else {
				spdlog::debug("BrokerBuy2: report {} does belong to cl {}", report.to_string(), cl_ord_id.getString());
				show(std::format("BrokerBuy2: report {} does belong to cl {}", report.to_string(), cl_ord_id.getString()));
				return BrokerBuyError::OrderRejectedOrTimeout;

				// TODO what should be done in this rare case?
				// example: two orders at almost same time, 
				// exec report of the second order arrives first 
			}
		}		
	}

	DLLFUNC int BrokerTrade(int trade_id, double* open, double* close, double* cost, double* profit) {
		spdlog::debug("BrokerTrade: {}", trade_id);
		show(std::format("BrokerTrade: trade_id={}", trade_id));

		// pop all the exec reports and markeet data from the queue to have up to date information
		pop_top_of_books();
		pop_exec_reports();

		auto it = order_id_by_internal_order_id.find(trade_id);
		if (it != order_id_by_internal_order_id.end()) {
			auto [oit, success] = order_tracker.get_open_order(it->second);
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
				return (int)oit->second.cum_qty;
			}
		}

		return 0;
	}

	DLLFUNC_C int BrokerSell2(int trade_id, int amount, double limit, double* close, double* cost, double* profit, int* fill) {
		spdlog::debug("BrokerSell2 nTradeID={} nAmount{} limit={}", trade_id, amount, limit);
		show(std::format("BrokerSell2: trade_id={}", trade_id));

		auto it = order_id_by_internal_order_id.find(trade_id);
		if (it != order_id_by_internal_order_id.end()) {
			auto [oit, success] = order_tracker.get_open_order(it->second);

			if (success) {
				auto& order = oit->second;

				spdlog::debug("BrokerSell2: found open order={}", order.to_string());
				show(std::format("BrokerSell2: found open order={}", order.to_string()));

				if (order.ord_status == FIX::OrdStatus_FILLED) {
					double close_price;
					int close_fill;
					int signed_qty = (int)(order.side == FIX::Side_BUY ? -order.cum_qty : order.cum_qty);

					spdlog::debug("BrokerSell2: closing filled order with trade in opposite direction signed_qty={}, limit={}", signed_qty, limit);
					show(std::format("BrokerSell2: closing filled order with trade in opposite direction signed_qty={}, limit={}", signed_qty, limit));

					auto asset = const_cast<char*>(order.symbol.c_str());
					auto trade_id_close = BrokerBuy2(asset, signed_qty, 0, limit, &close_price, &close_fill);

					if (trade_id_close) {
						auto cit = order_id_by_internal_order_id.find(trade_id_close);
						if (cit != order_id_by_internal_order_id.end()) {
							auto [coit, success] = order_tracker.get_open_order(cit->second);
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
						trade_id;
					}
					return 0;
				}
				else {
					auto symbol = FIX::Symbol(order.symbol);
					auto orig_cl_ord_id = FIX::OrigClOrdID(order.cl_ord_id);
					auto cl_ord_id = FIX::ClOrdID(next_client_order_id());
					auto side = FIX::Side(order.side);
					auto ord_type = FIX::OrdType(order.ord_type);

					if (std::abs(amount) >= order.order_qty) {
						spdlog::debug(std::format("BrokerSell2: cancel working order"));
						show(std::format("BrokerSell2: cancel working order"));

						auto msg = fix_thread->fix_app().order_cancel_request(
							symbol, orig_cl_ord_id, cl_ord_id, side, FIX::OrderQty(order.leaves_qty)
						);

						ExecReport report;
						bool success = exec_report_queue.pop(report, fix_exec_report_waiting_time);

						if (!success) {
							show("BrokerSell2 timeout while waiting for FIX exec report on order cancel!");
							return BrokerBuyError::OrderRejectedOrTimeout;
						} 
						else {
							order_tracker.process(report);

							show(order_tracker.to_string());
						}

						return trade_id;
					}
					else {
						int leaves_qty = (int)order.leaves_qty;
						auto target_qty = leaves_qty - std::abs(amount);

						spdlog::debug(
							"BrokerSell2: cancel/replace working order from leaves_qty={} to target_qty={}",
							leaves_qty, target_qty
						);
						show(std::format(
							"BrokerSell2: cancel/replace working order from leaves_qty={} to target_qty={}",
							leaves_qty, target_qty
						));

						auto new_qty = max(target_qty, 0);
						auto msg = fix_thread->fix_app().order_cancel_replace_request(
							symbol, orig_cl_ord_id, cl_ord_id, side, ord_type, 
							FIX::OrderQty(new_qty), FIX::Price(order.price)
						);

						ExecReport report;
						bool success = exec_report_queue.pop(report, fix_exec_report_waiting_time);

						if (!success) {
							show("BrokerSell2 timeout while waiting for FIX exec report on order cancel/replace!");
							return BrokerBuyError::OrderRejectedOrTimeout;
						}
						else {
							order_tracker.process(report);

							show(order_tracker.to_string());
						}

						return trade_id;
					}
				}
			}
		}
		else {
			show(std::format("BrokerSell2: mapping not found for trade_id={} ", trade_id));
		}

		return 0;
	}

	// https://zorro-project.com/manual/en/brokercommand.htm
	DLLFUNC double BrokerCommand(int command, DWORD dw_parameter) {
		show(std::format("BrokerCommand {}[{}]", broker_command_string(command), command));

		switch (command)
		{
		case GET_COMPLIANCE:
			return 2;

		case GET_BROKERZONE:
			return 0;   // historical data in UTC time

		case GET_MAXTICKS:
			return 1000;

		case GET_MAXREQUESTS:
			return 30;

		case GET_LOCK:
			return 1;

		case GET_POSITION: {
			return get_position_size((const char*)dw_parameter);
			break;
		}

		case SET_ORDERTEXT: {
			return dw_parameter;
		}

		case SET_SYMBOL: {
			auto s_asset = (char*)dw_parameter;
			return 1;
		}

		case SET_MULTIPLIER: {
			auto s_multiplier = (int)dw_parameter;
			return 1;
		}

		// return 0 for not supported							
		case SET_ORDERTYPE: {
			switch ((int)dw_parameter) {
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
				return 0;
			}

			// additional stop order 
			if ((int)dw_parameter >= 8) {
				return 0; 
			}

			spdlog::debug("SET_ORDERTYPE: {}", (int)dw_parameter);
			return (int)dw_parameter;
		}

		case GET_PRICETYPE:
			return 0;

		case SET_PRICETYPE: {
			auto s_priceType = (int)dw_parameter;
			spdlog::debug("SET_PRICETYPE: {}", s_priceType);
			return dw_parameter;
		}

		case GET_VOLTYPE:
			return 0;

		case SET_AMOUNT: {
			auto s_amount = *(double*)dw_parameter;
			spdlog::debug("SET_AMOUNT: {}", s_amount);
			break;
		}

		case SET_DIAGNOSTICS: {
			if ((int)dw_parameter == 1 || (int)dw_parameter == 0) {
				spdlog::set_level((int)dw_parameter ? spdlog::level::debug : spdlog::level::info);
				return dw_parameter;
			}
			break;
		}

		case SET_HWND:
		case GET_CALLBACK:
		case SET_CCY:
			break;

		case GET_HEARTBEAT:
			return 500;

		case SET_LEVERAGE: {
			spdlog::debug("BrokerCommand: SET_LEVERAGE param={}", (int)dw_parameter);
			break;
		}

		case SET_LIMIT: {
			auto limit = *(double*)dw_parameter;
			spdlog::debug("BrokerCommand: SET_LIMIT param={}", limit);
			break;
		}

		case SET_FUNCTIONS:
			spdlog::debug("BrokerCommand: SET_FUNCTIONS param={}", (int)dw_parameter);
			break;

		default:
			spdlog::debug("BrokerCommand: unhandled command {} param={}", command, dw_parameter);
			break;
		}

		return 0.;
	}
}


