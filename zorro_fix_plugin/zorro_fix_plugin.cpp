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

#define PLUGIN_VERSION 1

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

	int maxSnaphsotWaitingIterations = 100; 
	std::chrono::milliseconds fixBlockinQueueWaitingTime = 500ms;
	std::string settingsCfgFile = "Plugin/zorro_fix_client.cfg";

	FIX::TimeInForce timeInForce = FIX::TimeInForce_GOOD_TILL_CANCEL;
	std::unique_ptr<zfix::FixThread> fixThread = nullptr;

	int internalOrderId = 1000;

	BlockingTimeoutQueue<ExecReport> exec_report_queue;

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

	// DATE is fractional time in seconds since midnight 1899-12-30
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

	std::string time32_to_string(const __time32_t& ts, long ms=0)
	{
		const std::tm bt = localtime_xp(ts);
		std::ostringstream oss;
		oss << std::put_time(&bt, "%H:%M:%S"); // HH:MM:SS
		if (ms > 0)
			oss << '.' << std::setfill('0') << std::setw(3) << ms;
		return oss.str();
	}

	std::string zorro_date_to_string(DATE date, bool millis=false) {
		auto ts = convert_time(date);
		auto ms = millis ? (long)(date * 1000) % 1000 : 0;
		return time32_to_string(ts, ms);
	}

	template<typename ... Args>
	inline void showf(const char* format, Args... args) {
		if (!BrokerError) return;
		static char msg[4096];
		sprintf_s(msg, format, std::forward<Args>(args)...);
		BrokerError(msg);
	}

	void show(const std::string& msg) {
		if (!BrokerError) return;
		auto tmsg = "[" + now_str() + "] " + msg;
		BrokerError(tmsg.c_str());
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
			std::map<std::chrono::nanoseconds, Bar> bars;
			from_json(j, bars);
			show(std::format("get_historical_bars for {}: number of bars {} between {} and {}", Asset, bars.size(), from_str, to_str));
		}
		else {
			show(std::format("get_historical_bars for {}: error {} for request between {} and {}", Asset, res->status, from_str, to_str));
		}

		return res->status;
	}

	int get_historical_bar_range(const char* Asset, std::chrono::nanoseconds& from, std::chrono::nanoseconds& to, bool verbose=true) {
		auto request = std::format("/bar_range?symbol={}", Asset);
		auto res = rest_client.Get(request);
		if (res->status == httplib::StatusCode::OK_200) {
			auto j = json::parse(res->body);
			from = std::chrono::nanoseconds(j["from"].template get<long long>());
			to = std::chrono::nanoseconds(j["to"].template get<long long>());
			auto from_str = common::to_string(from);
			auto to_str = common::to_string(to);
			if (verbose)
				show(std::format("get_historical_bar_range for {} from {} and {}", Asset, from_str, to_str));
		}
		else {
			show(std::format("get_historical_bars: error {} for asset {}", res->status, Asset));
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
		if (User) {
			if (fixThread != nullptr) {
				show("BrokerLogin: Zorro-Fix-Bridge - stopping fix service on repeated login...");
				fixThread->cancel();
				show("BrokerLogin: Zorro-Fix-Bridge - fix service stopped on repeated login");
				fixThread = nullptr;
			}
			show("BrokerLogin: Zorro-Fix-Bridge - starting fix service...");
			try { 
				fixThread = std::unique_ptr<FixThread>(
					new FixThread(
						settingsCfgFile, 
						exec_report_queue
					)
				);
				fixThread->start();
				show("BrokerLogin: Zorro-Fix-Bridge - fix service running");
				return 1; // logged in status
			}
			catch (std::exception& e) {
				fixThread = nullptr;
				show(std::format("BrokerLogin: exception starting fix service {}", e.what()));
				return 0;  
			}
		}
		else {
			if (fixThread != nullptr) {
				show("BrokerLogin: Zorro FIX Bridge - stopping FIX service...");
				fixThread->cancel();
				show("BrokerLogin: Zorro FIX Bridge - FIX service stopped");
				fixThread = nullptr;
			}	
			return 0; // logged out status
		}
	}

	DLLFUNC int BrokerOpen(char* Name, FARPROC fpError, FARPROC fpProgress) {
		if (Name) strcpy_s(Name, 32, "AAFixPlugin");
		(FARPROC&)BrokerError = fpError;
		(FARPROC&)BrokerProgress = fpProgress;

		std::string cwd = std::filesystem::current_path().string();
		show(std::format("BrokerOpen: Zorro Fix Bridge plugin opened in {}", cwd)); 

		try
		{
			auto postfix = timestamp_posfix();
			auto logger = spdlog::basic_logger_mt(
				"basic_logger", 
				std::format("Log/zorro-fix-bridge_{}.log", postfix)
			);
			spdlog::set_level(spdlog::level::debug);
		}
		catch (const spdlog::spdlog_ex& ex)
		{
			show(std::format("BrokerOpen: Zorro FIX Bridge failed to init log: {}", ex.what()));
		}

		SPDLOG_INFO("Logging started, cwd={}", cwd);

		return PLUGIN_VERSION;
	}

	DLLFUNC int BrokerTime(DATE* pTimeGMT) {
		if (!fixThread) {
			return ExchangeStatus::Unavailable;
		}

		const auto currentTime = std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::system_clock::now().time_since_epoch()
		).count();

		show("BrokerTime");
		return ExchangeStatus::Open;
	}

	DLLFUNC int BrokerAccount(char* Account, double* pdBalance, double* pdTradeVal, double* pdMarginVal) {
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
	 *	- 1 when the asset is available and the returned data is valid
	 *  - 0 otherwise. 
	 * 
	 * An asset that returns 0 after subscription will trigger Error 053, and its trading will be disabled.
	 * 
	 */
	DLLFUNC int BrokerAsset(char* Asset, double* pPrice, double* pSpread,
		double* pVolume, double* pPip, double* pPipCost, double* pMinAmount,
		double* pMarginCost, double* pRollLong, double* pRollShort) {

		try {
			if (fixThread == nullptr) {
				throw std::runtime_error("no FIX session");
			}

			auto& fixApp = fixThread->fixApp();

			// subscribe to Asset market data
			if (!pPrice) {  
				FIX::Symbol symbol(Asset);
				fixApp.marketDataRequest(
					symbol,
					FIX::MarketDepth(1),
					FIX::SubscriptionRequestType_SNAPSHOT_AND_UPDATES
				);

				// we do a busy polling to wait for the market data snapshot arriving
				int count = 0;
				auto start = std::chrono::system_clock::now();
				while (true) {
					if (count >= maxSnaphsotWaitingIterations) {
						auto now = std::chrono::system_clock::now();
						auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
						throw std::runtime_error(std::format("failed to get snapshot in {}ms", ms));
					}

					bool success = fixApp.hasBook(symbol);
					if (success) {
						show(std::format("BrokerAsset: subscribed to symbol {}", Asset));
						return 1;
					}

					++count;
					std::this_thread::sleep_for(100ms);
				}
			}
			else {
				TopOfBook top = fixApp.topOfBook(Asset);
				if (pPrice) *pPrice = top.mid();
				if (pSpread) *pSpread = top.spread();
				return 1;
			}
		}
		catch (const std::runtime_error& e) {
			show(std::format("BrokerAsset: Excetion {}", e.what()));
			return 0;
		}
		catch (...) {
			show("BrokerAsset: undetermined exception");
			return 0;
		}
	}

	DLLFUNC int BrokerHistory2(char* Asset, DATE tStart, DATE tEnd, int nTickMinutes, int nTicks, T6* ticks)
	{
		show(std::format("BrokerHistory2: requesting {} tick minutes history for {}", nTickMinutes, Asset));

		std::map<std::chrono::nanoseconds, Bar> bars;
		auto status = get_historical_bars(Asset, tStart, tEnd, bars);

		if (status != httplib::StatusCode::OK_200 || bars.empty()) {
			std::chrono::nanoseconds from, to;
			auto status = get_historical_bar_range(Asset, from, to, true);
		}

		for (int i = 0; i < nTicks; i++) {
			ticks->fOpen = 100;
			ticks->fClose = 100;
			ticks->fHigh = 100;
			ticks->fLow = 100;
			ticks->time = tEnd - i * nTickMinutes / 1440.0;
			ticks++;
		}
		return nTicks;
	}

	/*
	* BrokerBuy2
	* 
	* Returns 
	*	- 0 when the order was rejected or a FOK or IOC order was unfilled within the wait time (adjustable with the SET_WAIT command). The order must then be cancelled by the plugin.
	*	  Trade or order ID number when the order was successfully placed. If the broker API does not provide trade or order IDs, the plugin should generate a unique 6-digit number, f.i. from a counter, and return it as a trade ID.
	*	-1 when the trade or order identifier is a UUID that is then retrieved with the GET_UUID command.
	*	-2 when the broker API did not respond at all within the wait time. The plugin must then attempt to cancel the order. Zorro will display a "possible orphan" warning.
	*	-3 when the order was accepted, but got no ID yet. The ID is then taken from the next subsequent BrokerBuy call that returned a valid ID. This is used for combo positions that require several orders.
	*/
	DLLFUNC_C int BrokerBuy2(char* Asset, int nAmount, double dStopDist, double dLimit, double* pPrice, int* pFill)
	{
		if (!fixThread) {
			show("BrokerBuy2: no FIX session");
			return -2;
		}

		auto start = std::time(nullptr);

		FIX::OrdType ordType;
		if (dLimit && !dStopDist)
			ordType = FIX::OrdType_LIMIT;
		else if (dLimit && dStopDist)
			ordType = FIX::OrdType_STOP_LIMIT;
		else if (!dLimit && dStopDist)
			ordType = FIX::OrdType_STOP;
		else
			ordType = FIX::OrdType_MARKET;

		auto symbol = FIX::Symbol(Asset);
		auto clOrdId = FIX::ClOrdID(std::to_string(internalOrderId));
		auto side = nAmount > 0 ? FIX::Side(FIX::Side_BUY) : FIX::Side(FIX::Side_SELL);
		auto qty = FIX::OrderQty(std::abs(nAmount));
		auto limitPrice = FIX::Price(dLimit);
		auto stopPrice = FIX::StopPx(dStopDist);

		auto msg = fixThread->fixApp().newOrderSingle(
			symbol, clOrdId, side, ordType, timeInForce, qty, limitPrice, stopPrice
		);

		show("BrokerBuy2: newOrderSingle " + msg.toString());

		ExecReport report;
		bool success = exec_report_queue.pop(report, std::chrono::seconds(4));
		if (!success) {
			show("BrokerBuy2 timeout while waiting for FIX exec report!");
		}
		else {
			if (report.exec_type == FIX::ExecType_REJECTED) {
				show("BrokerBuy2: exec report " + report.to_string());

			}

			if (report.exec_type == FIX::ExecType_PENDING_NEW) {

			}

			if (report.exec_type == FIX::ExecType_NEW) {

			}

			if (report.exec_type == FIX::ExecType_PARTIAL_FILL) {

			}

			if (report.exec_type == FIX::ExecType_FILL) {

			}

			if (report.exec_type == FIX::ExecType_PENDING_CANCEL) {

			}

			if (report.exec_type == FIX::ExecType_CANCELED) {

			}


			bool fill = true;

			if (fill) {
				if (pPrice) {
					*pPrice = 0;
				}
				if (pFill) {
					*pFill = 0;
				}

				// reset s_amount, next asset might have lotAmount = 1, in that case SET_AMOUNT will not be called in advance
			}

			return internalOrderId++;
		}
			
		return 0;
	}

	DLLFUNC int BrokerTrade(int nTradeID, double* pOpen, double* pClose, double* pCost, double* pProfit) {
		SPDLOG_DEBUG("BrokerTrade: {}", nTradeID);

		return 0;
	}

	DLLFUNC_C int BrokerSell2(int nTradeID, int nAmount, double Limit, double* pClose, double* pCost, double* pProfit, int* pFill) {
		SPDLOG_DEBUG("BrokerSell2 nTradeID={} nAmount{} limit={}", nTradeID, nAmount, Limit);

		return 0;
	}

	// https://zorro-project.com/manual/en/brokercommand.htm
	DLLFUNC double BrokerCommand(int command, DWORD dwParameter) {
		show("BrokerCommand");

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
				timeInForce = FIX::TimeInForce_IMMEDIATE_OR_CANCEL;
				break;
			case ORDERTYPE_GTC:
				timeInForce = FIX::TimeInForce_GOOD_TILL_CANCEL;
				break;
			case ORDERTYPE_FOK:
				timeInForce = FIX::TimeInForce_FILL_OR_KILL;
				break;
			case ORDERTYPE_DAY:
				timeInForce = FIX::TimeInForce_DAY;
				break;
			default:
				return 0;
			}

			// additional stop order 
			if ((int)dwParameter >= 8) {
				return 0; 
			}

			SPDLOG_DEBUG("SET_ORDERTYPE: {}", (int)dwParameter);
			return (int)dwParameter;
		}

		case GET_PRICETYPE:
			return 0;

		case SET_PRICETYPE: {
			auto s_priceType = (int)dwParameter;
			SPDLOG_DEBUG("SET_PRICETYPE: {}", s_priceType);
			return dwParameter;
		}

		case GET_VOLTYPE:
			return 0;

		case SET_AMOUNT: {
			auto s_amount = *(double*)dwParameter;
			SPDLOG_DEBUG("SET_AMOUNT: {}", s_amount);
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
			SPDLOG_DEBUG("BrokerCommand: SET_LEVERAGE param={}", (int)dwParameter);
			break;
		}

		case SET_LIMIT: {
			auto limit = *(double*)dwParameter;
			SPDLOG_DEBUG("BrokerCommand: SET_LIMIT param={}", limit);
			break;
		}

		case SET_FUNCTIONS:
			SPDLOG_DEBUG("BrokerCommand: SET_FUNCTIONS param={}", (int)dwParameter);
			break;

		default:
			SPDLOG_DEBUG("BrokerCommand: unhandled command {} param={}", command, dwParameter);
			break;
		}

		return 0.;
	}
}


