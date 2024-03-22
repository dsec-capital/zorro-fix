#pragma warning(disable : 4996 4244 4312)

#include "pch.h"

#include "application.h"
#include "fix_thread.h"
#include "zorro_fix_plugin.h"

#include "common/market_data.h"
#include "common/exec_report.h"
#include "common/blocking_queue.h"
#include "common/time_utils.h"

#include "broker_commands.h"

#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"

#define PLUGIN_VERSION	2
#define INITIAL_PRICE	1.0
#define INITIAL_ID		1000

namespace zfix {

	using namespace common;
	using namespace std::chrono_literals;

	int maxSnaphsotWaitingIterations = 100; 
	std::chrono::milliseconds fixBlockinQueueWaitingTime = 500ms;
	std::string settingsCfgFile = "Plugin/zorro_fix_client.cfg";

	std::unique_ptr<zfix::FixThread> fixThread = nullptr;

	int s_internalOrderId = 1000;

	FIX::TimeInForce s_timeInForce = FIX::TimeInForce_GOOD_TILL_CANCEL;

	BlockingTimeoutQueue<ExecReport> s_execReportQueue;

	enum ExchangeStatus {
		Unavailable = 0,
		Closed = 1,
		Open = 2
	};

	DATE convertTime(__time32_t t32)
	{
		return (DATE)t32 / (24. * 60. * 60.) + 25569.; // 25569. = DATE(1.1.1970 00:00)
	}

	__time32_t convertTime(DATE date)
	{
		return (__time32_t)((date - 25569.) * 24. * 60. * 60.);
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
						s_execReportQueue
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
		auto clOrdId = FIX::ClOrdID(std::to_string(s_internalOrderId));
		auto side = nAmount > 0 ? FIX::Side(FIX::Side_BUY) : FIX::Side(FIX::Side_SELL);
		auto qty = FIX::OrderQty(std::abs(nAmount));
		auto limitPrice = FIX::Price(dLimit);
		auto stopPrice = FIX::StopPx(dStopDist);

		auto msg = fixThread->fixApp().newOrderSingle(
			symbol, clOrdId, side, ordType, s_timeInForce, qty, limitPrice, stopPrice
		);

		show("BrokerBuy2: newOrderSingle " + msg.toString());

		ExecReport report;
		bool success = s_execReportQueue.pop(report, std::chrono::seconds(4));
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

			return s_internalOrderId++;
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
				s_timeInForce = FIX::TimeInForce_IMMEDIATE_OR_CANCEL;
				break;
			case ORDERTYPE_GTC:
				s_timeInForce = FIX::TimeInForce_GOOD_TILL_CANCEL;
				break;
			case ORDERTYPE_FOK:
				s_timeInForce = FIX::TimeInForce_FILL_OR_KILL;
				break;
			case ORDERTYPE_DAY:
				s_timeInForce = FIX::TimeInForce_DAY;
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


