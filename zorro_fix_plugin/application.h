#ifndef APPLICATION_H
#define APPLICATION_H

#include "quickfix/Application.h"
#include "quickfix/MessageCracker.h"
#include "quickfix/Values.h"
#include "quickfix/Mutex.h"

#include "quickfix/fix44/NewOrderSingle.h"
#include "quickfix/fix44/ExecutionReport.h"
#include "quickfix/fix44/OrderCancelRequest.h"
#include "quickfix/fix44/OrderCancelReject.h"
#include "quickfix/fix44/OrderCancelReplaceRequest.h"
#include "quickfix/fix44/MarketDataRequest.h"
#include "quickfix/fix44/MarketDataSnapshotFullRefresh.h"
#include "quickfix/fix44/MarketDataIncrementalRefresh.h"

#include <mutex>
#include <atomic>
#include <queue>

#include "id_generator.h"
#include "blocking_queue.h"
#include "exec_report.h"
#include "market_data.h"
#include "order_tracker.h"
#include "book.h"


namespace zfix
{

	class Application: public FIX::Application, public FIX::MessageCracker
	{
	public:
		Application(
			const FIX::SessionSettings& sessionSettings,
			BlockingTimeoutQueue<ExecReport>& execReportQueue
		);

		bool hasBook(const std::string& symbol);

		TopOfBook topOfBook(const std::string& symbol);

		FIX::Message marketDataRequest(
			const FIX::Symbol& symbol,
			const FIX::MarketDepth& markeDepth,
			const FIX::SubscriptionRequestType& subscriptionRequestType
		);

		FIX::Message newOrderSingle(
			const FIX::Symbol& symbol, 
			const FIX::ClOrdID& clOrdId, 
			const FIX::Side& side,
			const FIX::OrdType& ordType, 
			const FIX::TimeInForce& tif,
			const FIX::OrderQty& quantity, 
			const FIX::Price& price, 
			const FIX::StopPx& stopPrice
		) const;

		FIX::Message orderCancelRequest(
			const FIX::Symbol& symbol,
			const FIX::OrigClOrdID& origClOrdID,
			const FIX::ClOrdID& clOrdId,
			const FIX::Side& side,
			const FIX::OrderQty& orderQty
		) const;

		FIX::Message cancelReplaceRequest(
			const FIX::Symbol& symbol,
			const FIX::OrigClOrdID& origClOrdID,
			const FIX::ClOrdID& clOrdId,
			const FIX::Side& side,
			const FIX::OrdType& ordType,
			const FIX::OrderQty& orderQty,
			const FIX::Price& price
		) const;

	private:
		FIX::SessionSettings sessionSettings;
		BlockingTimeoutQueue<ExecReport>& execReportQueue;
		std::deque<ExecReport> execReportStorageQueue;
		std::string senderCompID;
		std::string targetCompID;
		std::atomic<bool> done;
		IDGenerator idGenerator;
		std::unordered_map<std::string, Book> books;
		OrderTracker orderTracker;
		std::mutex mutex;

		void onCreate(const FIX::SessionID&);

		void onLogon(const FIX::SessionID& sessionID);

		void onLogout(const FIX::SessionID& sessionID);

		void toAdmin(FIX::Message&, const FIX::SessionID&);
		
		void toApp(
			FIX::Message&, const FIX::SessionID&
		) EXCEPT(FIX::DoNotSend);
		
		void fromAdmin(
			const FIX::Message&, const FIX::SessionID&
		) EXCEPT(FIX::FieldNotFound, FIX::IncorrectDataFormat, FIX::IncorrectTagValue, FIX::RejectLogon);
		
		void fromApp(
			const FIX::Message& message, const FIX::SessionID& sessionID
		) EXCEPT(FIX::FieldNotFound, FIX::IncorrectDataFormat, FIX::IncorrectTagValue, FIX::UnsupportedMessageType);

		void onMessage(const FIX44::ExecutionReport&, const FIX::SessionID&);
		void onMessage(const FIX44::OrderCancelReject&, const FIX::SessionID&);
		void onMessage(const FIX44::BusinessMessageReject&, const FIX::SessionID&);
		void onMessage(const FIX44::MarketDataSnapshotFullRefresh&, const FIX::SessionID&);
		void onMessage(const FIX44::MarketDataIncrementalRefresh&, const FIX::SessionID&);
	};
}

#endif
