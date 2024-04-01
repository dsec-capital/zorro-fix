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

#include "spdlog/spdlog.h"

#include "common/id_generator.h"
#include "common/blocking_queue.h"
#include "common/exec_report.h"
#include "common/market_data.h"
#include "common/order_tracker.h"
#include "common/book.h"

namespace zfix
{
	using namespace common;

	std::string fix_string(const FIX::Message& msg);

	class Application: public FIX::Application, public FIX::MessageCracker
	{
	public:
		Application(
			const FIX::SessionSettings& session_settings,
			BlockingTimeoutQueue<ExecReport>& exec_report_queue
		);

		bool has_book(const std::string& symbol);

		TopOfBook top_of_book(const std::string& symbol);

		FIX::Message market_data_request(
			const FIX::Symbol& symbol,
			const FIX::MarketDepth& markeDepth,
			const FIX::SubscriptionRequestType& subscriptionRequestType
		);

		FIX::Message new_order_single(
			const FIX::Symbol& symbol, 
			const FIX::ClOrdID& clOrdId, 
			const FIX::Side& side,
			const FIX::OrdType& ordType, 
			const FIX::TimeInForce& tif,
			const FIX::OrderQty& quantity, 
			const FIX::Price& price, 
			const FIX::StopPx& stopPrice
		) const;

		FIX::Message order_cancel_request(
			const FIX::Symbol& symbol,
			const FIX::OrigClOrdID& origClOrdID,
			const FIX::ClOrdID& clOrdId,
			const FIX::Side& side,
			const FIX::OrderQty& orderQty
		) const;

		FIX::Message cancel_replace_request(
			const FIX::Symbol& symbol,
			const FIX::OrigClOrdID& origClOrdID,
			const FIX::ClOrdID& clOrdId,
			const FIX::Side& side,
			const FIX::OrdType& ordType,
			const FIX::OrderQty& orderQty,
			const FIX::Price& price
		) const;

	private:
		FIX::SessionSettings session_settings;
		BlockingTimeoutQueue<ExecReport>& exec_report_queue;
		std::string sender_comp_id;
		std::string target_comp_id;
		std::atomic<bool> done;
		IDGenerator id_generator;
		std::unordered_map<std::string, Book> books;
		OrderTracker order_tracker;
		std::mutex mutex;

		// FIX Application interface

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
