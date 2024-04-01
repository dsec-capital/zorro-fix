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
#define SECONDS_PER_DAY								86400.0
#define MILLIS_PER_DAY                       86400000.0 
#define MICROS_PER_DAY                       86400000000.0 

namespace zfix {

	using namespace common;
	using namespace std::chrono_literals;

	// http://localhost:8080/bars?symbol=AUD/USD
	std::string rest_host = "http://localhost";
	int rest_port = 8080;
	httplib::Client rest_client(std::format("{}:{}", rest_host, rest_port));

	int max_snaphsot_waiting_iterations = 10; 
	std::chrono::milliseconds fix_blocking_queue_waiting_time = 500ms;
	std::string settings_cfg_file = "Plugin/zorro_fix_client.cfg";

	int internalOrderId = 1000;
	auto time_in_force = FIX::TimeInForce_GOOD_TILL_CANCEL;

	
	std::shared_ptr<spdlog::logger> spd_logger = nullptr;
	std::unique_ptr<zfix::FixThread> fix_thread = nullptr;
	BlockingTimeoutQueue<ExecReport> exec_report_queue;
	SpScQueue<TopOfBook> top_of_book_queue; // not yet used
	OrderTracker order_tracker("account");

	enum ExchangeStatus {
		Unavailable = 0,
		Closed = 1,
		Open = 2
	};

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

	void show(const std::string& msg) {
		if (!BrokerError) return;
		auto tmsg = "[" + now_str() + "] " + msg + "\n";
		BrokerError(tmsg.c_str());
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
				"get_historical_bars error: Asset={} from={} to={} status={}", 
				Asset, from_str, to_str, res->status
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
				"get_historical_bar_range error: Asset={} status={} body={}", 
				Asset, res->status, res->body
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
				return 1; // logged in status
			}
			else {
				show("BrokerLogin: FIX service stopping...");
				fix_thread->cancel();
				show("BrokerLogin: FIX service stopped");
				return 0; // logged out status
			}
		}
		catch (std::exception& e) {
			show(std::format("BrokerLogin: exception creating/starting FIX service {}", e.what()));
			return 0;
		}
		catch (...) {
			show("BrokerLogin: unknown exception");
			return 0;
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
		show(std::format("BrokerTime {}", common::to_string(time)));
		
		auto n = exec_report_queue.pop_all([](const ExecReport& report) { order_tracker.process(report); });
		show(std::format("BrokerTime {} exec reports processed", n));

		// TODO eventually pull here from the top of book queue

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
	 *	 - 1 when the asset is available and the returned data is valid
	 *  - 0 otherwise. 
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

				// we do a busy polling to wait for the market data snapshot arriving
				auto count = 0;
				auto start = std::chrono::system_clock::now();
				while (true) {
					if (count >= max_snaphsot_waiting_iterations) {
						auto now = std::chrono::system_clock::now();
						auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
						throw std::runtime_error(std::format("failed to get snapshot in {}ms", ms));
					}

					auto success = fix_app.has_book(symbol);
					if (success) {
						show(std::format("BrokerAsset: subscribed to symbol {}", Asset));
						break;
					}

					++count;
					std::this_thread::sleep_for(100ms);

					show(std::format("BrokerAsset: waiting for {} count={}", Asset, count));
				}

				show(std::format("BrokerAsset: subscription request obtained symbol {}", Asset));

				return 1;
			}
			else {
				TopOfBook top = fix_app.top_of_book(Asset);
				if (price) *price = top.mid();
				if (spread) *spread = top.spread();
				show(std::format("BrokerAsset: top bid={:.5f} ask={:.5f} @ {}", top.bid_price, top.ask_price, common::to_string(top.timestamp)));
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
		
		show(std::format("BrokerHistory2 {}: requesting {} ticks bar period {} minutes from {} to {}", Asset, nTicks, nTickMinutes, from, to));

		std::map<std::chrono::nanoseconds, Bar> bars;
		auto status = get_historical_bars(Asset, tStart2, tEnd, bars, false);

		auto count = 0;		
		if (status == httplib::StatusCode::OK_200) {
			for (auto it = bars.rbegin(); it != bars.rend() && count <= nTicks; ++it) {
				const auto& bar = it->second;
				auto time = convert_time_chrono(bar.end);
				//show(std::format(
				//	"[{}] {} : open={:.5f} high={:.5f} low={:.5f} close={:.5f}", 
				//	count, zorro_date_to_string(time), bar.open, bar.high, bar.low, bar.close
				//));
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
			auto status = get_historical_bar_range(Asset, from, to, num_bars, true);
		}

		return count;
	}

	/*
	* BrokerBuy2
	* 
	* Returns 
	*	0 when the order was rejected or a FOK or IOC order was unfilled within the wait time (adjustable with the SET_WAIT command). The order must then be cancelled by the plugin.
	*	  Trade or order ID number when the order was successfully placed. If the broker API does not provide trade or order IDs, the plugin should generate a unique 6-digit number, f.i. from a counter, and return it as a trade ID.
	*	-1 when the trade or order identifier is a UUID that is then retrieved with the GET_UUID command.
	*	-2 when the broker API did not respond at all within the wait time. The plugin must then attempt to cancel the order. Zorro will display a "possible orphan" warning.
	*	-3 when the order was accepted, but got no ID yet. The ID is then taken from the next subsequent BrokerBuy call that returned a valid ID. This is used for combo positions that require several orders.
	*/
	DLLFUNC_C int BrokerBuy2(char* Asset, int amount, double stop, double limit, double* av_fill_price, int* fill_qty) {
		spdlog::debug("BrokerBuy2: {} amount={} limit={}", Asset, amount, limit);
		show(std::format("BrokerBuy2: {} amount={} limit={}", Asset, amount, limit));

		if (!fix_thread) {
			show("BrokerBuy2: no FIX session");
			return -2;
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

		auto cl_ord_id = FIX::ClOrdID(std::to_string(internalOrderId));
		auto side = amount > 0 ? FIX::Side(FIX::Side_BUY) : FIX::Side(FIX::Side_SELL);
		auto qty = FIX::OrderQty(std::abs(amount));
		auto limit_price = FIX::Price(limit);
		auto stop_price = FIX::StopPx(stop);

		auto msg = fix_thread->fix_app().new_order_single(
			symbol, cl_ord_id, side, ord_type, time_in_force, qty, limit_price, stop_price
		);

		show(std::format("BrokerBuy2: NewOrderSingle {}", fix_string(msg)));

		ExecReport report;
		bool success = exec_report_queue.pop(report, std::chrono::milliseconds(500));

		if (!success) {
			show("BrokerBuy2 timeout while waiting for FIX exec report!");
		}
		else {

			if (report.exec_type == FIX::ExecType_REJECTED) {
				show(std::format("BrokerBuy2: rejected {}", report.to_string()));
			}
			else {
				show(std::format("BrokerBuy2: {}", report.to_string()));
			}

			order_tracker.process(report);

			if (report.ord_status == FIX::OrdStatus_FILLED || report.ord_status == FIX::OrdStatus_PARTIALLY_FILLED) {
				if (av_fill_price) {
					*av_fill_price = report.avg_px;
				}
				if (fill_qty) {
					*fill_qty = (int)report.cum_qty;  // assuming lot size
				}
			}

			return internalOrderId;
		}
			
		return 0;
	}

	DLLFUNC int BrokerTrade(int nTradeID, double* pOpen, double* pClose, double* pCost, double* pProfit) {
		spdlog::debug("BrokerTrade: {}", nTradeID);
		show(std::format("BrokerTrade: nTradeID={}", nTradeID));

		return 0;
	}

	DLLFUNC_C int BrokerSell2(int nTradeID, int nAmount, double Limit, double* pClose, double* pCost, double* pProfit, int* pFill) {
		spdlog::debug("BrokerSell2 nTradeID={} nAmount{} limit={}", nTradeID, nAmount, Limit);
		show(std::format("BrokerSell2: nTradeID={}", nTradeID));

		return 0;
	}

	// https://zorro-project.com/manual/en/brokercommand.htm
	DLLFUNC double BrokerCommand(int command, DWORD dwParameter) {
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

			return 0;
			break;
		}

		case SET_ORDERTEXT: {
			return dwParameter;
		}

		case SET_SYMBOL: {
			auto s_asset = (char*)dwParameter;
			return 1;
		}

		case SET_MULTIPLIER: {
			auto s_multiplier = (int)dwParameter;
			return 1;
		}

		// return 0 for not supported							
		case SET_ORDERTYPE: {
			switch ((int)dwParameter) {
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
			if ((int)dwParameter >= 8) {
				return 0; 
			}

			spdlog::debug("SET_ORDERTYPE: {}", (int)dwParameter);
			return (int)dwParameter;
		}

		case GET_PRICETYPE:
			return 0;

		case SET_PRICETYPE: {
			auto s_priceType = (int)dwParameter;
			spdlog::debug("SET_PRICETYPE: {}", s_priceType);
			return dwParameter;
		}

		case GET_VOLTYPE:
			return 0;

		case SET_AMOUNT: {
			auto s_amount = *(double*)dwParameter;
			spdlog::debug("SET_AMOUNT: {}", s_amount);
			break;
		}

		case SET_DIAGNOSTICS: {
			if ((int)dwParameter == 1 || (int)dwParameter == 0) {
				spdlog::set_level((int)dwParameter ? spdlog::level::debug : spdlog::level::info);
				return dwParameter;
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
			spdlog::debug("BrokerCommand: SET_LEVERAGE param={}", (int)dwParameter);
			break;
		}

		case SET_LIMIT: {
			auto limit = *(double*)dwParameter;
			spdlog::debug("BrokerCommand: SET_LIMIT param={}", limit);
			break;
		}

		case SET_FUNCTIONS:
			spdlog::debug("BrokerCommand: SET_FUNCTIONS param={}", (int)dwParameter);
			break;

		default:
			spdlog::debug("BrokerCommand: unhandled command {} param={}", command, dwParameter);
			break;
		}

		return 0.;
	}
}


