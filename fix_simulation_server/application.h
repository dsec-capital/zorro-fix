#ifndef APPLICATION_H
#define APPLICATION_H

#include "id_generator.h"
#include "order_matcher.h"
#include "order.h"
#include <queue>
#include <iostream>

#include "quickfix/Application.h"
#include "quickfix/MessageCracker.h"
#include "quickfix/Values.h"
#include "quickfix/Utility.h"
#include "quickfix/Mutex.h"

#include "quickfix/fix44/NewOrderSingle.h"
#include "quickfix/fix44/OrderCancelRequest.h"
#include "quickfix/fix44/MarketDataRequest.h"

class Application: public FIX::Application, public FIX::MessageCracker
{
	// Application overloads

	void onCreate(const FIX::SessionID&);

	void onLogon(const FIX::SessionID& sessionID);

	void onLogout(const FIX::SessionID& sessionID);

	void toAdmin(FIX::Message&, const FIX::SessionID&);

	void toApp(FIX::Message&, const FIX::SessionID&) EXCEPT(FIX::DoNotSend);

	void fromAdmin(
		const FIX::Message&, const FIX::SessionID&
	) EXCEPT(FIX::FieldNotFound, FIX::IncorrectDataFormat, FIX::IncorrectTagValue, FIX::RejectLogon);

	void fromApp(
		const FIX::Message& message, const FIX::SessionID& sessionID
	) EXCEPT(FIX::FieldNotFound, FIX::IncorrectDataFormat, FIX::IncorrectTagValue, FIX::UnsupportedMessageType);

	// MessageCracker overloads
	void onMessage(const FIX44::NewOrderSingle&, const FIX::SessionID&);
	void onMessage(const FIX44::OrderCancelRequest&, const FIX::SessionID&);
	void onMessage(const FIX44::MarketDataRequest&, const FIX::SessionID&);

	// Order functionality

	void processOrder(const Order&);

	void processCancel(const std::string& id, const std::string& symbol, Order::Side);

	void updateOrder(const Order&, char status);

	void rejectOrder(const Order& order);

	void acceptOrder(const Order& order);

	void fillOrder(const Order& order);

	void cancelOrder(const Order& order);

	void rejectOrder(
		const FIX::SenderCompID&, const FIX::TargetCompID&,
		const FIX::ClOrdID& clOrdID, const FIX::Symbol& symbol,
		const FIX::Side& side, const std::string& message
	);

	// Type conversions
	Order::Side convert(const FIX::Side&);
	Order::Type convert(const FIX::OrdType&);
	FIX::Side convert(Order::Side);
	FIX::OrdType convert(Order::Type);

	OrderMatcher m_orderMatcher;
	IDGenerator m_generator;

public:
	const OrderMatcher& orderMatcher();
};

#endif
