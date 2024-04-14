#ifndef APPLICATION_H
#define APPLICATION_H

#include "common/order.h"
#include "common/markets.h"
#include "common/id_generator.h"

#include <queue>
#include <iostream>
#include <thread>
#include <mutex>
#include <limits>

#include "quickfix/Application.h"
#include "quickfix/MessageCracker.h"
#include "quickfix/Values.h"
#include "quickfix/Utility.h"
#include "quickfix/Mutex.h"

#include "quickfix/fix44/NewOrderSingle.h"
#include "quickfix/fix44/OrderCancelRequest.h"
#include "quickfix/fix44/MarketDataRequest.h"

using namespace common;

class Application: public FIX::Application, public FIX::MessageCracker
{
public:
	Application(
		 std::map<std::string, Market>& market,
		 std::chrono::milliseconds market_update_period,
		 FIX::Log *logger, 
		 std::mutex& mutex
	) : logger(logger)
     , markets(market)
	  , market_update_period(market_update_period)
	  , mutex(mutex)
	{}

	void run_market_data_update();

	void start_market_data_updates();

	void stop_market_data_updates();

	const Markets& get_order_matcher();

private:

	// FIX Application overloads

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

	void onMessage(const FIX44::NewOrderSingle&, const FIX::SessionID&);
	void onMessage(const FIX44::OrderCancelRequest&, const FIX::SessionID&);
	void onMessage(const FIX44::MarketDataRequest&, const FIX::SessionID&);

	// Order functionality

	void process_order(const Order&);

	void process_cancel(const std::string& id, const std::string& symbol, Order::Side);

	void update_order(const Order&, char status, const std::string& text);

	void reject_order(const Order& order);

	void accept_order(const Order& order);

	void fill_order(const Order& order);

	void cancel_order(const Order& order);

	void reject_order(
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

	void subscribe_market_data(const std::string& symbol, const std::string& senderCompID, const std::string& targetCompID);

	void unsubscribe_market_data(const std::string& symbol);

	bool subscribed_to_market_data(const std::string& symbol);

	FIX::Message get_snapshot_message(
		const std::string& senderCompID,
		const std::string& targetCompID,
		const TopOfBook& top
	);

	std::optional<FIX::Message> get_update_message(
		const std::string& senderCompID,
		const std::string& targetCompID,
		const std::pair<TopOfBook, TopOfBook>& topOfBook
	);

	IDGenerator generator;
	Markets markets;
	std::chrono::milliseconds market_update_period;

	FIX::Log* logger;

	std::map<std::string, std::pair<std::string, std::string>> market_data_subscriptions;

	std::mutex& mutex;
	std::thread thread;
	bool started{ false };
	std::atomic_bool done{ false };
};

#endif
