#include "pch.h"

#pragma warning(disable : 4996 4244 4312)

#include "quickfix/config.h"
#include "quickfix/FileStore.h"
#include "quickfix/SocketInitiator.h"
#include "quickfix/Values.h"
#ifdef HAVE_SSL
#include "quickfix/ThreadedSSLSocketInitiator.h"
#include "quickfix/SSLSocketInitiator.h"
#endif
#include "quickfix/SessionSettings.h"
#include "quickfix/Log.h"
#include "application.h"

#include <time.h>
#include <string>
#include <string>
#include <vector>
#include <memory>
#include <fstream>
#include <sstream>
#include <thread>
#include <iostream>
#include <type_traits>
#include <filesystem>
#include <unordered_map>
#include <chrono>

#include "logger.h"
#include "exec_report.h"
#include "zorro_fix_plugin.h"
#include "broker_commands.h"
#include "blocking_queue.h"

#define PLUGIN_VERSION	2
#define INITIAL_PRICE	1.0
#define INITIAL_ID		1000

using namespace std::chrono_literals;

namespace zfix {

	class FixThread {
		bool started;
		std::string settingsCfgFile;
		FIX::SessionSettings settings;
		FIX::FileStoreFactory storeFactory;
		FIX::ScreenLogFactory logFactory;
		std::unique_ptr<FIX::Initiator> initiator;
		std::unique_ptr<zfix::Application> application;
		std::thread thread;
		std::function<void(const char*)> brokerError;

		void run() {
			initiator->start();
		}

	public:
		FixThread(
			const std::string& settingsCfgFile,
			BlockingTimeoutQueue<ExecReport>& execReportQueue,
		    std::function<void(const char*)> brokerError
		) :
			started(false),
			settingsCfgFile(settingsCfgFile),
			settings(settingsCfgFile),
			storeFactory(settings),
			logFactory(settings), 
			brokerError(brokerError)
		{
			application = std::unique_ptr<zfix::Application>(new Application(
				settings, execReportQueue, brokerError)
			);
			initiator = std::unique_ptr<FIX::Initiator>(
				new FIX::SocketInitiator(*application, storeFactory, settings, logFactory)
			);
		}

#ifdef HAVE_SSL
		FixThread(
			const std::string& settingsCfgFile, 
			std::function<void(const char*)> brokerError,
			const std::string& isSSL
		) :
			started(false),
			settingsCfgFile(settingsCfgFile),
			settings(settingsCfgFile),
			storeFactory(settings),
			logFactory(settings),
			brokerError(brokerError)
		{
			application = std::unique_ptr<zfix::Application>(new Application(settings));

			if (isSSL.compare("SSL") == 0)
				initiator = std::unique_ptr<FIX::Initiator>(
					new FIX::ThreadedSSLSocketInitiator(application, storeFactory, settings, logFactory));
			else if (isSSL.compare("SSL-ST") == 0)
				initiator = std::unique_ptr<FIX::Initiator>(
					new FIX::SSLSocketInitiator(application, storeFactory, settings, logFactory));
			else
				initiator = std::unique_ptr<FIX::Initiator>(
					new FIX::SocketInitiator(*application, storeFactory, settings, logFactory)
				);
		}
#endif

		void start() {
			started = true;
			thread = std::thread(&FixThread::run, this);
		}

		void cancel() {
			if (!started)
				return;
			started = false;
			initiator->stop(true);
			LOG_INFO("FixThread: FIX initiator and application stopped - going to join\n")
			if (thread.joinable())
				thread.join();
			LOG_INFO("FixThread: FIX initiator stopped - joined\n")
		}

		zfix::Application& fixApp() {
			return *application;
		}
	};
}

namespace zfix {
	std::string settingsCfgFile = "Plugin/zorro_fix_client.cfg";

	std::unique_ptr<zfix::FixThread> fixThread = nullptr;

	int s_internalOrderId = 1000;

	FIX::TimeInForce s_timeInForce = FIX::TimeInForce_GOOD_TILL_CANCEL;

	BlockingTimeoutQueue<ExecReport> execReportQueue;
	std::vector<ExecReport> execReportHistory;

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

	void showMsg(const char* text, const char* detail) {
		if (!BrokerError) return;
		if (!detail) detail = "";
		static char msg[1024];
		sprintf_s(msg, "%s %s", text, detail);
		BrokerError(msg);
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
				showMsg("BrokerLogin: Zorro-Fix-Bridge - stopping fix service on repeated login...");
				fixThread->cancel();
				showMsg("BrokerLogin: Zorro-Fix-Bridge - fix service stopped on repeated login");
				fixThread = nullptr;
			}
			showMsg("BrokerLogin: Zorro-Fix-Bridge - starting fix service...");
			try { 
				fixThread = std::unique_ptr<FixThread>(
					new FixThread(settingsCfgFile, execReportQueue, BrokerError)
				);
			}
			catch (std::exception& e) {
				fixThread = nullptr;
				showMsg(e.what());
				return 0;  
			}
			fixThread->start();
			showMsg("BrokerLogin: Zorro-Fix-Bridge - fix service running");
			return 1; // logged in status
		}
		else {
			if (fixThread != nullptr) {
				showMsg("BrokerLogin: Zorro-Fix-Bridge - stopping fix service...");
				fixThread->cancel();
				showMsg("BrokerLogin: Zorro-Fix-Bridge - fix service stopped");
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
		showMsg(("BrokerOpen: Zorro-Fix-Bridge plugin opened in <" + cwd + ">").c_str()); 

		Logger::instance().init("zorro-fix-bridge");
		Logger::instance().setLevel(LogLevel::L_DEBUG);
		LOG_INFO("Logging started\n");

		return PLUGIN_VERSION;
	}

	DLLFUNC int BrokerTime(DATE* pTimeGMT) {
		if (!fixThread) {
			return ExchangeStatus::Unavailable;
		}

		const auto currentTime = std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::system_clock::now().time_since_epoch()
		).count();

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

	DLLFUNC int BrokerAsset(char* Asset, double* pPrice, double* pSpread,
		double* pVolume, double* pPip, double* pPipCost, double* pMinAmount,
		double* pMarginCost, double* pRollLong, double* pRollShort) {

		try {
			if (pPip != nullptr) {

			}
			else {
				if (fixThread != nullptr) {
					FIX::Symbol symbol(Asset);
					fixThread->fixApp().marketDataRequest(
						symbol,
						FIX::MarketDepth(1),
						FIX::SubscriptionRequestType_SNAPSHOT
					);
				}
			}
			if (pPrice) *pPrice = 100;
			if (pSpread) *pSpread = 0.0005;
		}
		catch (...) {
			showMsg("Excetion in BrokerAsset");
		}
		return 1;
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


	DLLFUNC_C int BrokerBuy2(char* Asset, int nAmount, double dStopDist, double dLimit, double* pPrice, int* pFill)
	{
		if (!fixThread) {
			showMsg("FIX session not created");
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

		ExecReport report;
		bool success = execReportQueue.pop(report, std::chrono::milliseconds(10));

		LOG_DEBUG("BrokerBuy2 msg=%s\n", msg.toString().c_str());

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

	DLLFUNC int BrokerTrade(int nTradeID, double* pOpen, double* pClose, double* pCost, double* pProfit) {
		LOG_DEBUG("BrokerTrade: %d\n", nTradeID);


		return 0;
	}

	DLLFUNC_C int BrokerSell2(int nTradeID, int nAmount, double Limit, double* pClose, double* pCost, double* pProfit, int* pFill) {
		LOG_DEBUG("BrokerSell2 nTradeID=%d nAmount=%d limit=%f\n", nTradeID, nAmount, Limit);

		return 0;
	}

	// https://zorro-project.com/manual/en/brokercommand.htm
	DLLFUNC double BrokerCommand(int command, DWORD dwParameter) {
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

			LOG_DEBUG("SET_ORDERTYPE: %d\n", (int)dwParameter);
			return (int)dwParameter;
		}

		case GET_PRICETYPE:
			return 0;

		case SET_PRICETYPE: {
			auto s_priceType = (int)dwParameter;
			LOG_DEBUG("SET_PRICETYPE: %d\n", s_priceType);
			return dwParameter;
		}

		case GET_VOLTYPE:
			return 0;

		case SET_AMOUNT: {
			auto s_amount = *(double*)dwParameter;
			LOG_DEBUG("SET_AMOUNT: %.8f\n", s_amount);
			break;
		}

		case SET_DIAGNOSTICS: {
			if ((int)dwParameter == 1 || (int)dwParameter == 0) {
				Logger::instance().setLevel((int)dwParameter ? LogLevel::L_DEBUG : LogLevel::L_OFF);
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
			LOG_DEBUG("BrokerCommand: SET_LEVERAGE param=%d\n", (int)dwParameter);
			LOG_DEBUG("BrokerCommand: SET_LEVERAGE param=%f\n", *(double*)dwParameter);
			break;
		}

		case SET_LIMIT: {
			auto limit = *(double*)dwParameter;
			LOG_DEBUG("BrokerCommand: SET_LIMIT param=%f\n", limit);
			break;
		}

		case SET_FUNCTIONS:
			LOG_DEBUG("BrokerCommand: SET_FUNCTIONS param=%d\n", (int)dwParameter);
			break;

		default:
			LOG_DEBUG("BrokerCommand: unhandled command %d param=%lu\n", command, dwParameter);
			break;
		}

		return 0.;
	}
}


