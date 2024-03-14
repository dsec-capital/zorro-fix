#ifndef APPLICATION_H
#define APPLICATION_H

#include "id_generator.h"
#include "market.h"
#include "order_matcher.h"
#include "order.h"

#include <queue>
#include <iostream>
#include <thread>

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
public:
	Application(
		std::map<std::string, Market>& market,
		std::chrono::milliseconds marketUpdatePeriod,
		FIX::Log *logger, 
		std::mutex& mutex
	) : m_logger(logger)
      , m_orderMatcher(market, logger)
	  , m_marketUpdatePeriod(marketUpdatePeriod)
	  , m_mutex(mutex)
	{}

	void runMarketDataUpdate();

	void startMarketDataUpdates();

	void stopMarketDataUpdates();

	const OrderMatcher& orderMatcher();

private:
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
		const FIX::SenderCompID&, 
		const FIX::TargetCompID&,
		const FIX::ClOrdID& clOrdID, 
		const FIX::Symbol& symbol,
		const FIX::Price& price,
		const FIX::Side& side,
		const FIX::OrdType& ordType,
		const FIX::OrderQty& orderQty,
		const std::string& message
	);

	Order::Side convert(const FIX::Side&);

	Order::Type convert(const FIX::OrdType&);

	FIX::Side convert(Order::Side);

	FIX::OrdType convert(Order::Type);


	void marketDataSubscribe(const std::string& symbol, const std::string& senderCompID, const std::string& targetCompID);

	void marketDataUnsubscribe(const std::string& symbol);

	bool marketDataSubscribed(const std::string& symbol);

	IDGenerator m_generator;
	OrderMatcher m_orderMatcher;
	std::chrono::milliseconds m_marketUpdatePeriod;

	FIX::Log* m_logger;

	std::map<std::string, std::pair<std::string, std::string>> m_marketDataSubscriptions;

	std::mutex& m_mutex;
	std::thread m_thread;
	bool m_started{ false };
	std::atomic_bool m_done{ false };

};

#endif
