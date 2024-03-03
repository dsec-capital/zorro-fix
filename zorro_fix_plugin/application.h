#ifndef APPLICATION_H
#define APPLICATION_H

#include "quickfix/Application.h"
#include "quickfix/MessageCracker.h"
#include "quickfix/Values.h"
#include "quickfix/Mutex.h"

#include "quickfix/fix40/NewOrderSingle.h"
#include "quickfix/fix40/ExecutionReport.h"
#include "quickfix/fix40/OrderCancelRequest.h"
#include "quickfix/fix40/OrderCancelReject.h"
#include "quickfix/fix40/OrderCancelReplaceRequest.h"

#include "quickfix/fix41/NewOrderSingle.h"
#include "quickfix/fix41/ExecutionReport.h"
#include "quickfix/fix41/OrderCancelRequest.h"
#include "quickfix/fix41/OrderCancelReject.h"
#include "quickfix/fix41/OrderCancelReplaceRequest.h"

#include "quickfix/fix42/NewOrderSingle.h"
#include "quickfix/fix42/ExecutionReport.h"
#include "quickfix/fix42/OrderCancelRequest.h"
#include "quickfix/fix42/OrderCancelReject.h"
#include "quickfix/fix42/OrderCancelReplaceRequest.h"

#include "quickfix/fix43/NewOrderSingle.h"
#include "quickfix/fix43/ExecutionReport.h"
#include "quickfix/fix43/OrderCancelRequest.h"
#include "quickfix/fix43/OrderCancelReject.h"
#include "quickfix/fix43/OrderCancelReplaceRequest.h"
#include "quickfix/fix43/MarketDataRequest.h"

#include "quickfix/fix44/NewOrderSingle.h"
#include "quickfix/fix44/ExecutionReport.h"
#include "quickfix/fix44/OrderCancelRequest.h"
#include "quickfix/fix44/OrderCancelReject.h"
#include "quickfix/fix44/OrderCancelReplaceRequest.h"
#include "quickfix/fix44/MarketDataRequest.h"

#include "quickfix/fix50/NewOrderSingle.h"
#include "quickfix/fix50/ExecutionReport.h"
#include "quickfix/fix50/OrderCancelRequest.h"
#include "quickfix/fix50/OrderCancelReject.h"
#include "quickfix/fix50/OrderCancelReplaceRequest.h"
#include "quickfix/fix50/MarketDataRequest.h"

#include <atomic>
#include <queue>

#include "id_generator.h"

namespace zfix
{

	class Application: public FIX::Application, public FIX::MessageCracker
	{
	public:
		Application(
			const FIX::SessionSettings & sessionSettings,
			std::function<void(const char*)> brokerError
		): 
			sessionSettings(sessionSettings),
			brokerError(brokerError),
			done(false)
		{
			brokerError("FIX application created");
		}

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

	private:
		FIX::SessionSettings sessionSettings;
		std::function<void(const char*)> brokerError;
		std::string senderCompID;
		std::string targetCompID;
		std::atomic<bool> done;
		IDGenerator m_generator;

		void showMsg(const std::string& msg) const;

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

		void onMessage(const FIX40::ExecutionReport&, const FIX::SessionID&);
		void onMessage(const FIX40::OrderCancelReject&, const FIX::SessionID&);
		void onMessage(const FIX41::ExecutionReport&, const FIX::SessionID&);
		void onMessage(const FIX41::OrderCancelReject&, const FIX::SessionID&);
		void onMessage(const FIX42::ExecutionReport&, const FIX::SessionID&);
		void onMessage(const FIX42::OrderCancelReject&, const FIX::SessionID&);
		void onMessage(const FIX43::ExecutionReport&, const FIX::SessionID&);
		void onMessage(const FIX43::OrderCancelReject&, const FIX::SessionID&);
		void onMessage(const FIX44::ExecutionReport&, const FIX::SessionID&);
		void onMessage(const FIX44::OrderCancelReject&, const FIX::SessionID&);
		void onMessage(const FIX50::ExecutionReport&, const FIX::SessionID&);
		void onMessage(const FIX50::OrderCancelReject&, const FIX::SessionID&);

		void queryEnterOrder();
		void queryCancelOrder();
		void queryReplaceOrder();
		void queryMarketDataRequest();

		FIX40::NewOrderSingle queryNewOrderSingle40();
		FIX41::NewOrderSingle queryNewOrderSingle41();
		FIX42::NewOrderSingle queryNewOrderSingle42();
		FIX43::NewOrderSingle queryNewOrderSingle43();
		FIX44::NewOrderSingle queryNewOrderSingle44();
		FIX50::NewOrderSingle queryNewOrderSingle50();
		FIX40::OrderCancelRequest queryOrderCancelRequest40();
		FIX41::OrderCancelRequest queryOrderCancelRequest41();
		FIX42::OrderCancelRequest queryOrderCancelRequest42();
		FIX43::OrderCancelRequest queryOrderCancelRequest43();
		FIX44::OrderCancelRequest queryOrderCancelRequest44();
		FIX50::OrderCancelRequest queryOrderCancelRequest50();
		FIX40::OrderCancelReplaceRequest queryCancelReplaceRequest40();
		FIX41::OrderCancelReplaceRequest queryCancelReplaceRequest41();
		FIX42::OrderCancelReplaceRequest queryCancelReplaceRequest42();
		FIX43::OrderCancelReplaceRequest queryCancelReplaceRequest43();
		FIX44::OrderCancelReplaceRequest queryCancelReplaceRequest44();
		FIX50::OrderCancelReplaceRequest queryCancelReplaceRequest50();
		FIX43::MarketDataRequest queryMarketDataRequest43();
		FIX44::MarketDataRequest queryMarketDataRequest44();
		FIX50::MarketDataRequest queryMarketDataRequest50();

		void queryHeader(FIX::Header& header);
		char queryAction();
		int queryVersion();
		bool queryConfirm(const std::string& query);

		FIX::SenderCompID querySenderCompID();
		FIX::TargetCompID queryTargetCompID();
		FIX::TargetSubID queryTargetSubID();
		FIX::ClOrdID queryClOrdID();
		FIX::OrigClOrdID queryOrigClOrdID();
		FIX::Symbol querySymbol();
		FIX::Side querySide();
		FIX::OrderQty queryOrderQty();
		FIX::OrdType queryOrdType();
		FIX::Price queryPrice();
		FIX::StopPx queryStopPx();
		FIX::TimeInForce queryTimeInForce();
	};
}

#endif
