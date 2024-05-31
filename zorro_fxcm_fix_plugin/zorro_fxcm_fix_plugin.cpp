#pragma warning(disable : 4996 4244 4312)

#include "pch.h"
#include "application.h"
#include "fix_thread.h"
#include "zorro_fxcm_fix_plugin.h"

#include "common/market_data.h"
#include "common/exec_report.h"
#include "common/blocking_queue.h"
#include "common/json.h"
#include "common/time_utils.h"

#include "zorro_common/log.h"
#include "zorro_common/utils.h"
#include "zorro_common/enums.h"
#include "zorro_common/broker_commands.h"

#include "fxcm_market_data/fxcm_market_data.h"

#include "nlohmann/json.h"
#include "httplib/httplib.h"
#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"

#define PLUGIN_VERSION 2
#define PLUGIN_NAME "ZorroFXCMFixPlugin"

#define BAR_DUMP_FILE_NAME "Log/bar_dump.csv"
#define BAR_DUMP_FILE_SEP ","
#define BAR_DUMP_FILE_PREC 5  // FX is point precision 

namespace zorro {

	using namespace common;
	using namespace std::chrono_literals;

	// http://localhost:8080/bars?symbol=AUD/USD
	std::string rest_host = "http://localhost";
	int rest_port = 8080;
	httplib::Client rest_client(std::format("{}:{}", rest_host, rest_port));

	int max_snaphsot_waiting_iterations = 10; 
	std::chrono::milliseconds fix_exec_report_waiting_time = 500ms;
	std::string settings_cfg_file = "Plugin/zorro_fxcm_fix_client.cfg";

	int client_order_id = 0;
	int internal_order_id = 1000;

	std::string fxcm_login;
	std::string fxcm_password;
	std::string fxcm_account;
	std::string fxcm_connection = "Demo";
	int num_fix_sessions = 2; // trading and market data

	std::string asset;
	std::string currency;
	std::string position_symbol;
	std::string order_text;
	int multiplier = 1;
	int price_type = 0;
	int vol_type = 0;
	int leverage = 1;
	double lot_amount = 1;
	double limit = 0;
	char time_in_force = FIX::TimeInForce_GOOD_TILL_CANCEL;
	HWND window_handle;
	bool dump_bars_to_file = true;

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

	void plugin_callback(void* address) {
		show("plugin_callback called with address={}", address);
	}

	void trigger_price_quote_request() {
		PostMessage(window_handle, WM_APP + 1, 0, 0);
	}

	template<typename T>
	std::string format_fp(T value, int precision)
	{
		char format[16];
		char buffer[64];

		sprintf_s(format, 16, "%%.%if", precision);
		sprintf_s(buffer, 64, format, value);
		char* point = strchr(buffer, '.');
		if (point != 0)
			*point = '.';

		return std::string(buffer);
	}

	void write_to_file(const std::string filename, const std::string& text, const std::string& header, std::ios_base::openmode mode = std::fstream::app) {
		bool skip_header = std::filesystem::exists(filename);

		std::fstream fs;
		fs.open(filename, std::fstream::out | mode);
		if (!fs.is_open())
		{
			log::error<true>("write_to_file: could not open file {}", filename);
			return;
		}

		if (!skip_header) {
			fs << header << std::endl;
		}
		fs << text << std::endl;

		fs.close();
	}

	// std::fstream::trunc
	void dump_bars(T6* ticks, int n_ticks, std::ios_base::openmode mode = std::fstream::app) {
		bool skip_header = std::filesystem::exists(BAR_DUMP_FILE_NAME);
		
		std::fstream fs;
		fs.open(BAR_DUMP_FILE_NAME, std::fstream::out | mode);
		if (!fs.is_open())
		{
			log::error<true>("dump_bars: could not open file {}", BAR_DUMP_FILE_NAME);
			return;
		}

		log::debug<1, true>("dump_bars[skip_header={}]: writing {} bars to {}", skip_header, n_ticks, BAR_DUMP_FILE_NAME);

		if (!skip_header) {
			fs << "bar_end" << BAR_DUMP_FILE_SEP
			   << "open" << BAR_DUMP_FILE_SEP
			   << "high" << BAR_DUMP_FILE_SEP
			   << "low" << BAR_DUMP_FILE_SEP
			   << "close" << BAR_DUMP_FILE_SEP
			   << "vol[volume]" << BAR_DUMP_FILE_SEP
			   << "val[spread]" 
			   << std::endl;
		}

		for (int i = 0; i < n_ticks; ++i, ++ticks) {
			fs << zorro_date_to_string(ticks->time) << BAR_DUMP_FILE_SEP
			   << format_fp(ticks->fOpen, BAR_DUMP_FILE_PREC) << BAR_DUMP_FILE_SEP
			   << format_fp(ticks->fHigh, BAR_DUMP_FILE_PREC) << BAR_DUMP_FILE_SEP
			   << format_fp(ticks->fLow, BAR_DUMP_FILE_PREC) << BAR_DUMP_FILE_SEP
			   << format_fp(ticks->fClose, BAR_DUMP_FILE_PREC) << BAR_DUMP_FILE_SEP
			   << format_fp(ticks->fVol, BAR_DUMP_FILE_PREC) << BAR_DUMP_FILE_SEP
			   << format_fp(ticks->fVal, BAR_DUMP_FILE_PREC) 
			   << std::endl;
		}

		fs.close();
	}

	DLLFUNC_C void BrokerHTTP(FARPROC fp_send, FARPROC fp_status, FARPROC fp_result, FARPROC fp_free) {
		(FARPROC&)http_send = fp_send;
		(FARPROC&)http_status = fp_status;
		(FARPROC&)http_result = fp_result;
		(FARPROC&)http_free = fp_free;
	}

	/*
	 * BrokerLogin
	 * 
	 * Login or logout to the broker's API server; called in [Trade] mode or for downloading historical price data. 
	 * If the connection to the server was lost, f.i. due to to Internet problems or server weekend maintenance, 
	 * Zorro calls this function repeatedly in regular intervals until it is logged in again. Make sure that the 
	 * function internally detects the login state and returns safely when the user was still logged in.
	 *
	 * Parameters:
	 *	User		Input, User name for logging in, or NULL for logging out.
	 *	Pwd			Input, Password for logging in.
	 *	Type		Input, account type for logging in; either "Real" or "Demo".
	 *	Accounts	Input / optional output, char[1024] array, intially filled with the account id from the account list. Can be filled with all user's account numbers as subsequent zero-terminated strings, ending with "" for the last string. When a list is returned, the first account number is used by Zorro for subsequent BrokerAccount calls.
	 * 
	 * Returns:
	 *	Login state: 1 when logged in, 0 otherwise.
	 */
	DLLFUNC int BrokerLogin(char* user, char* password, char* type, char* account) {
		try {
			if (fix_thread == nullptr) {
				log::debug<1, true>("BrokerLogin: FIX thread createing...");
				fix_thread = std::unique_ptr<FixThread>(new FixThread(
					settings_cfg_file,
					exec_report_queue, 
					top_of_book_queue
				));
				log::debug<1, true>("BrokerLogin: FIX thread created");
			}

			if (user) {
				fxcm_login = std::string(user);
				fxcm_password = std::string(password);
				fxcm_account = std::string(account);

				if (strcmp(type, "Real") == 0) {
					fxcm_connection = fxcm::real_connection;
				}
				else {
					fxcm_connection = fxcm::demo_connection;
				}

				log::info<1, true>(
					"BrokerLogin: FXCM credentials login={} password={} connection={} account={}", 
					fxcm_login, fxcm_password, fxcm_connection, fxcm_account
				);

				log::debug<1, true>("BrokerLogin: FIX service starting...");
				fix_thread->start();
				log::debug<1, true>("BrokerLogin: FIX service running");

				auto count = 0;
				auto start = std::chrono::system_clock::now();
				log::debug<1, true>("BrokerLogin: waiting for all session log in");
				auto login_count = fix_thread->fix_app().login_count();
				while (login_count < num_fix_sessions && count < 50) {
					std::this_thread::sleep_for(100ms);
					login_count = fix_thread->fix_app().login_count();
					log::debug<5, true>("BrokerLogin: waiting for all session log in - login count={}", login_count);
				}
				auto dt = std::chrono::system_clock::now() - start;
				auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(dt).count();
				if (login_count >= num_fix_sessions) {
					log::info<1, true>("BrokerLogin: FIX login after {}ms", ms);
					return BrokerLoginStatus::LoggedIn;
				}
				else {
					throw std::runtime_error(std::format("login timeout after {}ms login count={}", ms, login_count));
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
		log::info<1, true>("BrokerOpen: FXCM FIX plugin opened in {}", cwd);

		try
		{
			if (!spd_logger) {
				auto postfix = timestamp_posfix();
				auto logger_name = std::format("Log/zorro_fxcm_fix_plugin_spdlog_{}.log", postfix);
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
				fix_app.subscribe_market_data(symbol, false);

				log::info<1, true>("BrokerAsset: subscription request sent for symbol {}", Asset);

				TopOfBook top;
				auto timeout = 2000;
				bool success = top_of_book_queue.pop(top, std::chrono::milliseconds(timeout));
				if (!success) {
					throw std::runtime_error(std::format("failed to get snapshot in {}ms", timeout));
				}
				else {
					log::info<1, true>("BrokerAsset: successfully subscribed symbol {} for market data", Asset);
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

					log::debug<4, true>(
						"BrokerAsset: top bid={:.5f} ask={:.5f} @ {}", 
						top.bid_price, top.ask_price, common::to_string(top.timestamp)
					);
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

	/*
	 * BrokerHistory2 
	 * 
	 * Returns the price history of an asset. Called by Zorro's assetHistory function and at the begin of a trading session for filling the lookback period.
	 *
	 * Parameters:
	 *	Asset			Input, asset symbol for historical prices (see Symbols).
	 *	tStart			Input, UTC start date/time of the price history (see BrokerTime about the DATE format). This has only the meaning of a 
	 *                  seek-no-further date; the relevant date for the begin of the history is tEnd.
	 *	tEnd			Input, UTC end date/time of the price history. If the price history is not available in UTC time, but in the brokers's local time, 
	 *                  the plugin must convert it to UTC.
	 *	nTickMinutes	Input, time period of a tick in minutes. Usual values are 0 for single price ticks (T1 data; optional), 1 for one-minute (M1) historical data, 
	 *                  or a larger value for more quickly filling the LookBack period before starting a strategy.
	 *	nTicks			Input, maximum number of ticks to be filled; must not exceed the number returned by brokerCommand(GET_MAXTICKS,0), or 300 otherwise.
	 *	ticks			Output, array of T6 structs (defined in include\trading.h) to be filled with the ask prices, close time, and additional data if available, 
	 *                  such as historical spread and volume. See history for details. The ticks array is filled in reverse order from tEnd on until either the tick 
	 *                  time reaches tStart or the number of ticks reaches nTicks, whichever happens first. The most recent tick, closest to tEnd, is at
	 *                  the start of the array. In the case of T1 data, or when only a single price is available, all prices in a TICK struct can be set to the same value.
	 * 
	 * Important Note:
	 *	The timestamp of a bar in ForexConnect is the beginning of the bar period. 
	 *  As for Zorro the T6 format is defined 
	 * 
     *		typedef struct T6
	 *		{
	 *		  DATE  time; // UTC timestamp of the close, DATE format
	 *		  float fHigh,fLow;	
	 *		  float fOpen,fClose;	
	 * 		  float fVal,fVol; // additional data, ask-bid spread, volume etc.
	 *		} T6;
	 * 
	 *	Hence the timestamp should represent the end of the bar period.
	 * 
	 * From Zorro documentation:
	 *	If not stated otherwise, all dates and times reflect the time stamp of the Close price at the end of the bar. 
	 *	Dates and times are normally UTC. Timestamps in historical data are supposed to be either UTC, or to be a date
	 *	only with no additional time part, as for daily bars. 
	 *  It is possible to use historical data with local timestamps in the backtest, but this must be considered in the 
	 *  script since the date and time functions will then return the local time instead of UTC, and time zone functions
	 *	cannot be used.
	 */
	DLLFUNC int BrokerHistory2(char* Asset, DATE t_start, DATE t_end, int n_tick_minutes, int n_ticks, T6* ticks) {
		auto bar_seconds = n_tick_minutes * 60;
		auto t_bar = bar_seconds / SECONDS_PER_DAY;
		auto t_start2 = t_end - n_ticks * t_bar;
		auto from = zorro_date_to_string(t_start2);
		auto to = zorro_date_to_string(t_end);
		auto ticks_start = ticks;

		// todo determing proper time frame
		std::string timeframe = "m1";
		
		auto now = common::get_current_system_clock();
		auto now_zorro = zorro::convert_time_chrono(now);
		auto now_str = zorro_date_to_string(now_zorro);

		log::debug<2, true>(
			"BrokerHistory2 {}: requesting {} ticks bar period {} minutes from {}[{}] to {}[{}] at {}", 
			Asset, n_ticks, n_tick_minutes, from, t_start2, to, t_end, now_str
		);

		log::debug<2, true>("BrokerHistory2: t_start={}, t_start2={}, t_end={}, now_zorro={}", t_start, t_start2, t_end, now_zorro);

		std::vector<BidAskBar<DATE>> bars;

		auto success = fxcm::get_historical_prices(
			bars, 
			fxcm_login.c_str(),
			fxcm_password.c_str(), 
			fxcm_connection.c_str(), 
			fxcm::default_url, 
			Asset, 
			timeframe.c_str(),
			t_start2, 
			t_end
		);

		if (!success) {
			log::debug<2, true>("BrokerHistory2 {}: get_historical_prices failed - check logs", Asset);
			return 0;
		}

		int count = 0;
		for (auto it = bars.rbegin(); it != bars.rend() && count <= n_ticks; ++it) {
			const auto& bar = *it;
			DATE start = bar.timestamp;
			DATE end = start + t_bar;
			auto time = convert_time_chrono(bar.timestamp);

			if (end > t_end || end < t_start) {
				log::debug<2, true>(
					"BrokerHistory2 {}: skipping timestamp {} as it is out of bound t_start={} t_end={}", 
					Asset, zorro_date_to_string(end), zorro_date_to_string(t_start), zorro_date_to_string(t_end)
				);

				continue;
			}

			log::debug<5, false>(
				"[{}] from={} to={} open={:.5f} high={:.5f} low={:.5f} close={:.5f}",
				count, zorro_date_to_string(start), zorro_date_to_string(end), bar.ask_open, bar.ask_high, bar.ask_low, bar.ask_close
			);

			ticks->fOpen = static_cast<float>(bar.ask_open);
			ticks->fClose = static_cast<float>(bar.ask_close);
			ticks->fHigh = static_cast<float>(bar.ask_high);
			ticks->fLow = static_cast<float>(bar.ask_low);
			ticks->fVol = static_cast<float>(bar.volume);
			ticks->fVal = static_cast<float>(bar.ask_close - bar.bid_close);
			ticks->time = end;
			++ticks;
			++count;
		}

		if (dump_bars_to_file) {
			dump_bars(ticks_start, count);
			write_to_file("Log/broker_hist.csv", 
				std::format(
					"{}, {}, {}, {}, {}, {}, {}, {}",
					Asset, timeframe, zorro_date_to_string(t_start2), t_start2, zorro_date_to_string(t_end), t_end, n_ticks, count
				),
				"asset, timeframe, t_start2, t_start2[DATE], t_end, t_end[DATE], n_ticks, count"
			);
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
	DLLFUNC_C int BrokerBuy2(char* Asset, int lots, double stop, double limit, double* av_fill_price, int* fill_qty) {
		log::debug<2, true>("BrokerBuy2: {} lots={} limit={} lot_amount={}", Asset, lots, limit, lot_amount);

		if (!fix_thread) {
			log::error<true>("BrokerBuy2: no FIX session");
			return BrokerError::BrokerAPITimeout;
		}

		auto symbol = FIX::Symbol(Asset);
		FIX::OrdType ord_type;
		if (limit && !stop) {
			ord_type = FIX::OrdType_LIMIT;
		} 
		else if (limit && stop) {
			ord_type = FIX::OrdType_STOP_LIMIT;
		}
		else if (!limit && stop) {
			ord_type = FIX::OrdType_STOP;
		}
		else {
			ord_type = FIX::OrdType_MARKET;	
		}

		auto cl_ord_id = FIX::ClOrdID(next_client_order_id());
		auto side = lots > 0 ? FIX::Side(FIX::Side_BUY) : FIX::Side(FIX::Side_SELL);
		auto qty = FIX::OrderQty(std::abs(lots));
		auto limit_price = FIX::Price(limit);
		auto stop_price = FIX::StopPx(stop);

		auto msg = fix_thread->fix_app().new_order_single(
			symbol, cl_ord_id, side, ord_type, time_in_force, qty, limit_price, stop_price
		);

		if (msg.has_value()) {
			log::debug<2, true>("BrokerBuy2: NewOrderSingle {}", fix_string(msg.value()));
		}
		else {
			log::debug<2, true>("BrokerBuy2: failed to create NewOrderSingle");
		}

		ExecReport report;
		bool success = exec_report_queue.pop(report, fix_exec_report_waiting_time);

		if (!success) {
			log::error<true>("BrokerBuy2 timeout while waiting for FIX exec report on order new!");
			return BrokerError::OrderRejectedOrTimeout;
		}
		else {
			if (report.exec_type == FIX::ExecType_REJECTED || report.ord_status == FIX::OrdStatus_REJECTED) {
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
	DLLFUNC_C int BrokerSell2(int trade_id, int lot_amount, double limit, double* close, double* cost, double* profit, int* fill) {
		log::debug<1, true>("BrokerSell2 nTradeID={} nAmount={} limit={}", trade_id, lot_amount, limit);

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

					if (std::abs(lot_amount) > order.order_qty) {
						log::error<true>("BrokerSell2: trying to cancel/replace with an lots={} > order.order_qty={}!", lot_amount, order.order_qty);
						return 0;
					}

					if (std::abs(lot_amount) >= order.leaves_qty) {
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
						auto target_qty = leaves_qty - std::abs(lot_amount);

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
				lot_amount = *(double*)dw_parameter;
				log::debug<1, true>("BrokerCommand {}[{}] lots={}", broker_command_string(command), command, lot_amount);
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


