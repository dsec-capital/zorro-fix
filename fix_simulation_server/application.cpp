#ifdef _MSC_VER
#pragma warning( disable : 4503 4355 4786 )
#endif

#include "application.h"

#include "common/market.h"
#include "common/market_data.h"
#include "common/time_utils.h"

#include "quickfix/config.h"
#include "quickfix/Session.h"
#include "quickfix/fix44/ExecutionReport.h"
#include "quickfix/fix44/BusinessMessageReject.h"
#include "quickfix/fix44/MarketDataRequestReject.h"
#include "quickfix/fix44/MarketDataSnapshotFullRefresh.h"
#include "quickfix/fix44/MarketDataIncrementalRefresh.h"

std::string fix_string(const FIX::Message& msg) {
	auto s = msg.toString();
	std::replace(s.begin(), s.end(), '\x1', '|');
	return s;
}

void Application::run_market_data_update() {
	while (!done) {
		std::this_thread::sleep_for(market_update_period);

		auto now = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch());

		for (auto& [symbol, market] : order_matcher.markets) {
			market.simulate_next();
			auto it = market_data_subscriptions.find(symbol);
			if (it != market_data_subscriptions.end()) {
				auto message = get_update_message(
					it->second.first, 
					it->second.second, 
					market.get_top_of_book()
				);
				if (message.has_value()) {
					FIX::Session::sendToTarget(message.value());
				}
			}
		}
	}
}

void Application::start_market_data_updates() {
	std::cout << "====> starting market data updates" << std::endl;
	thread = std::thread(&Application::run_market_data_update, this);
}

void Application::stop_market_data_updates() {
	std::cout << "====> stopping market data updates" << std::endl;
	if (!started)
		return;
	started = false;
	done = true;
	if (thread.joinable())
		thread.join();
}

void Application::subscribe_market_data(const std::string& symbol, const std::string& senderCompID, const std::string& targetCompID) {
	market_data_subscriptions.insert_or_assign(symbol, std::make_pair(senderCompID, targetCompID));
}

void Application::unsubscribe_market_data(const std::string& symbol) {
	auto it = market_data_subscriptions.find(symbol);
	if (it != market_data_subscriptions.end())
		market_data_subscriptions.erase(it);
}

bool Application::subscribed_to_market_data(const std::string& symbol) {
	auto it = market_data_subscriptions.find(symbol);
	return it != market_data_subscriptions.end();
}

void Application::onCreate(const FIX::SessionID&)
{}

void Application::onLogon(const FIX::SessionID& sessionID) 
{}

void Application::onLogout(const FIX::SessionID& sessionID) 
{}

void Application::toAdmin(FIX::Message&, const FIX::SessionID&)
{}

void Application::toApp(FIX::Message&, const FIX::SessionID&) EXCEPT(FIX::DoNotSend) 
{}

void Application::fromAdmin(
	const FIX::Message&, const FIX::SessionID&
) EXCEPT(FIX::FieldNotFound, FIX::IncorrectDataFormat, FIX::IncorrectTagValue, FIX::RejectLogon) 
{}


void Application::fromApp(
	const FIX::Message& message, const FIX::SessionID& sessionID
) EXCEPT(FIX::FieldNotFound, FIX::IncorrectDataFormat, FIX::IncorrectTagValue, FIX::UnsupportedMessageType) 
{
	crack(message, sessionID);
}

void Application::onMessage(const FIX44::NewOrderSingle& message, const FIX::SessionID&)
{
	FIX::SenderCompID senderCompID;
	FIX::TargetCompID targetCompID;
	FIX::ClOrdID clOrdID;
	FIX::Symbol symbol;
	FIX::Side side;
	FIX::OrdType ordType;
	FIX::Price price(0);
	FIX::OrderQty orderQty(0);
	FIX::TimeInForce timeInForce(FIX::TimeInForce_DAY);

	message.getHeader().get(senderCompID);
	message.getHeader().get(targetCompID);
	message.get(clOrdID);
	message.get(symbol);
	message.get(side);
	message.get(ordType);
	if (ordType == FIX::OrdType_LIMIT)
		message.get(price);
	message.get(orderQty);
	message.getFieldIfSet(timeInForce);

	try
	{
		if (timeInForce == FIX::TimeInForce_GOOD_TILL_CANCEL || timeInForce == FIX::TimeInForce_DAY) {
			Order order(clOrdID, symbol, senderCompID, targetCompID, convert(side), convert(ordType), price, (long)orderQty);

			process_order(order);
		}
		else {
			throw std::logic_error("Unsupported TIF, use Day or GTC");
		}
	}
	catch (std::exception& e)
	{
		reject_order(senderCompID, targetCompID, clOrdID, symbol, price, side, ordType, orderQty, e.what());
	}
}

void Application::onMessage(const FIX44::OrderCancelRequest& message, const FIX::SessionID&)
{
	FIX::OrigClOrdID origClOrdID;
	FIX::Symbol symbol;
	FIX::Side side;

	message.get(origClOrdID);
	message.get(symbol);
	message.get(side);

	try
	{
		process_cancel(origClOrdID, symbol, convert(side));
	}
	catch (std::exception&) {}
}

void Application::onMessage(const FIX44::MarketDataRequest& message, const FIX::SessionID&)
{
	FIX::SenderCompID senderCompID;
	FIX::TargetCompID targetCompID;
	FIX::MDReqID mdReqID;
	FIX::SubscriptionRequestType subscriptionRequestType;
	FIX::MarketDepth marketDepth;
	FIX::NoRelatedSym noRelatedSym;
	FIX44::MarketDataRequest::NoRelatedSym noRelatedSymGroup;

	message.getHeader().get(senderCompID);
	message.getHeader().get(targetCompID);
	message.get(mdReqID);
	message.get(subscriptionRequestType);
	message.get(marketDepth);
	message.get(noRelatedSym);

	if (subscriptionRequestType == FIX::SubscriptionRequestType_SNAPSHOT ||
		subscriptionRequestType == FIX::SubscriptionRequestType_SNAPSHOT_AND_UPDATES) { 

		for (int i = 1; i <= noRelatedSym; ++i)
		{
			FIX::Symbol symbol;
			message.getGroup(i, noRelatedSymGroup);
			noRelatedSymGroup.get(symbol);

			const auto& market = order_matcher.get_market(symbol.getString())->second;
			
			// flip to send back target->sender and sender->target 
			auto snapshot = get_snapshot_message(
				targetCompID.getValue(), 
				senderCompID.getValue(), 
				market.get_top_of_book().first
			);
			FIX::Session::sendToTarget(snapshot);

			if (subscriptionRequestType == FIX::SubscriptionRequestType_SNAPSHOT_AND_UPDATES) {
				subscribe_market_data(symbol.getValue(), targetCompID.getValue(), senderCompID.getValue());
			}
		}
	} 
}

FIX::Message Application::get_snapshot_message(
	const std::string& senderCompID, 
	const std::string& targetCompID, 
	const TopOfBook& top
) {
	FIX::DateTime now = FIX::DateTime::nowUtc();

	FIX44::MarketDataSnapshotFullRefresh message;
	message.set(FIX::Symbol(top.symbol));
	message.set(FIX::MDReqID(generator.genMarketDataID()));

	FIX44::MarketDataSnapshotFullRefresh::NoMDEntries group;

	group.set(FIX::MDEntryType(FIX::MDEntryType_BID));
	group.set(FIX::MDEntryPx(top.bid_price));
	group.set(FIX::MDEntrySize(top.bid_volume));
	group.set(FIX::MDEntryDate(now));
	group.set(FIX::MDEntryTime(now));
	message.addGroup(group);

	group.set(FIX::MDEntryType(FIX::MDEntryType_OFFER));
	group.set(FIX::MDEntryPx(top.ask_price));
	group.set(FIX::MDEntrySize(top.ask_volume));
	group.set(FIX::MDEntryDate(now));
	group.set(FIX::MDEntryTime(now));
	message.addGroup(group);

	auto& header = message.getHeader();
	header.setField(FIX::SenderCompID(senderCompID));
	header.setField(FIX::TargetCompID(targetCompID));

	return message;
}

std::optional<FIX::Message> Application::get_update_message(
	const std::string& senderCompID, 
	const std::string& targetCompID, 
	const std::pair<TopOfBook, TopOfBook>& topOfBookChange
) {
	const auto& [current, previous] = topOfBookChange;
	auto n = 0;

	FIX::DateTime now = FIX::DateTime::nowUtc();

	FIX44::MarketDataIncrementalRefresh message;
	message.set(FIX::MDReqID(generator.genMarketDataID()));

	FIX44::MarketDataIncrementalRefresh::NoMDEntries group;
	if (current.bid_price != previous.bid_price) {
		group.set(FIX::Symbol(current.symbol));
		group.set(FIX::MDEntryType(FIX::MDEntryType_BID));
		group.set(FIX::MDEntryPx(previous.bid_price));
		group.set(FIX::MDEntrySize(0));
		group.set(FIX::MDUpdateAction(FIX::MDUpdateAction_DELETE));
		group.set(FIX::MDEntryDate(now));
		group.set(FIX::MDEntryTime(now));
		message.addGroup(group);
		++n;

		group.set(FIX::Symbol(current.symbol));
		group.set(FIX::MDEntryType(FIX::MDEntryType_BID));
		group.set(FIX::MDEntryPx(current.bid_price));
		group.set(FIX::MDEntrySize(current.bid_volume));
		group.set(FIX::MDUpdateAction(FIX::MDUpdateAction_NEW));
		group.set(FIX::MDEntryDate(now));
		group.set(FIX::MDEntryTime(now));
		message.addGroup(group);
		++n;
	}
	else if (current.bid_volume != previous.bid_volume) {
		assert(current.bid_price == previous.bid_price);
		group.set(FIX::Symbol(current.symbol));
		group.set(FIX::MDEntryType(FIX::MDEntryType_BID));
		group.set(FIX::MDEntryPx(current.bid_price));
		group.set(FIX::MDEntrySize(current.bid_volume));
		group.set(FIX::MDUpdateAction(FIX::MDUpdateAction_CHANGE));
		group.set(FIX::MDEntryDate(now));
		group.set(FIX::MDEntryTime(now));
		message.addGroup(group);
		++n;
	}

	if (current.ask_price != previous.ask_price) {
		group.set(FIX::MDEntryType(FIX::MDEntryType_OFFER));
		group.set(FIX::Symbol(current.symbol));
		group.set(FIX::MDEntryPx(previous.ask_price));
		group.set(FIX::MDEntrySize(0));
		group.set(FIX::MDUpdateAction(FIX::MDUpdateAction_DELETE));
		group.set(FIX::MDEntryDate(now));
		group.set(FIX::MDEntryTime(now));
		message.addGroup(group);
		++n;

		group.set(FIX::Symbol(current.symbol));
		group.set(FIX::MDEntryType(FIX::MDEntryType_OFFER));
		group.set(FIX::MDEntryPx(current.ask_price));
		group.set(FIX::MDEntrySize(current.ask_volume));
		group.set(FIX::MDUpdateAction(FIX::MDUpdateAction_NEW));
		group.set(FIX::MDEntryDate(now));
		group.set(FIX::MDEntryTime(now));
		message.addGroup(group);
		++n;
	} if (current.ask_volume != previous.ask_volume) {
		assert(current.ask_price == previous.ask_price);
		group.set(FIX::Symbol(current.symbol));
		group.set(FIX::MDEntryType(FIX::MDEntryType_OFFER));
		group.set(FIX::MDEntryPx(current.ask_price));
		group.set(FIX::MDEntrySize(current.ask_volume));
		group.set(FIX::MDUpdateAction(FIX::MDUpdateAction_CHANGE));
		group.set(FIX::MDEntryDate(now));
		group.set(FIX::MDEntryTime(now));
		message.addGroup(group);
		++n;
	}

	if (n == 0) {
		return std::optional<FIX::Message>();
	}
	else {
		auto& header = message.getHeader();
		header.setField(FIX::SenderCompID(senderCompID));
		header.setField(FIX::TargetCompID(targetCompID));
		return std::optional<FIX::Message>(message);
	}
} 

void Application::update_order(const Order& order, char status, const std::string& text)
{
	FIX::TargetCompID targetCompID(order.get_owner());
	FIX::SenderCompID senderCompID(order.get_target());

	FIX44::ExecutionReport fixOrder(
		FIX::OrderID(order.get_client_id()),
		FIX::ExecID(generator.genExecutionID()),
		FIX::ExecType(status),
		FIX::OrdStatus(status),
		FIX::Side(convert(order.get_side())),
		FIX::LeavesQty(order.get_open_quantity()),
		FIX::CumQty(order.get_executed_quantity()),
		FIX::AvgPx(order.get_avg_executed_price())
	);

	fixOrder.set(FIX::Symbol(order.get_symbol())); 
	fixOrder.set(FIX::ClOrdID(order.get_client_id()));
	fixOrder.set(FIX::OrderQty(order.get_quantity()));
	fixOrder.set(FIX::OrdType(order.get_type() == Order::Type::limit ? FIX::OrdType_LIMIT : FIX::OrdType_MARKET));
	if (order.get_type() == Order::Type::limit) {
		fixOrder.set(FIX::Price(order.get_price()));
	}
	if (status == FIX::OrdStatus_FILLED || status == FIX::OrdStatus_PARTIALLY_FILLED || status == FIX::OrdStatus_NEW) {
		fixOrder.set(FIX::LastQty(order.get_last_executed_quantity()));
		fixOrder.set(FIX::LastPx(order.get_last_executed_price()));
	}
	fixOrder.set(FIX::Text(text));

	try
	{
		FIX::Session::sendToTarget(fixOrder, senderCompID, targetCompID);
	}
	catch (FIX::SessionNotFound&) {}
}

void Application::reject_order(const Order& order)
{
	update_order(order, FIX::OrdStatus_REJECTED, "");
}

void Application::accept_order(const Order& order)
{
	update_order(order, FIX::OrdStatus_NEW, "");
}

void Application::fill_order(const Order& order)
{
	update_order(
		order,
		order.isFilled() ? FIX::OrdStatus_FILLED : FIX::OrdStatus_PARTIALLY_FILLED,
		""
	); 
}

void Application::cancel_order(const Order& order)
{
	update_order(order, FIX::OrdStatus_CANCELED, "");
}

void Application::reject_order(
	const FIX::SenderCompID& sender, 
	const FIX::TargetCompID& target,
	const FIX::ClOrdID& clOrdID, 
	const FIX::Symbol& symbol,
	const FIX::Price& price,
	const FIX::Side& side, 
	const FIX::OrdType& ordType,
	const FIX::OrderQty& orderQty,
	const std::string& message)
{
	FIX::TargetCompID targetCompID(sender.getValue());
	FIX::SenderCompID senderCompID(target.getValue());

	FIX44::ExecutionReport fixOrder(
		FIX::OrderID(clOrdID.getValue()),
		FIX::ExecID(generator.genExecutionID()),
		FIX::ExecType(FIX::ExecType_REJECTED),
		FIX::OrdStatus(FIX::ExecType_REJECTED),
		side, 
		FIX::LeavesQty(0), 
		FIX::CumQty(0), 
		FIX::AvgPx(0)
	);

	fixOrder.set(symbol);
	fixOrder.set(price);
	fixOrder.set(clOrdID);
	fixOrder.set(ordType);
	fixOrder.set(orderQty);
	fixOrder.set(FIX::LastQty(0));
	fixOrder.set(FIX::Text(message));

	try
	{
		FIX::Session::sendToTarget(fixOrder, senderCompID, targetCompID);
	}
	catch (FIX::SessionNotFound&) {}
}

void Application::process_order(const Order& order)
{
	if (order_matcher.insert(order))
	{
		accept_order(order);

		std::queue<Order> orders;
		order_matcher.match(order.get_symbol(), orders);

		while (orders.size())
		{
			fill_order(orders.front());
			orders.pop();
		}
	}
	else
		reject_order(order);
}

void Application::process_cancel(const std::string& id,
	const std::string& symbol, Order::Side side)
{
	Order& order = order_matcher.find(symbol, side, id);
	order.cancel();
	cancel_order(order);
	order_matcher.erase(order);
}

Order::Side Application::convert(const FIX::Side& side)
{
	switch (side)
	{
		case FIX::Side_BUY: 
			return Order::buy;
		case FIX::Side_SELL: 
			return Order::sell;
		default: 
			throw std::logic_error("Unsupported Side, use buy or sell");
	}
}

Order::Type Application::convert(const FIX::OrdType& ordType)
{
	switch (ordType)
	{
		case FIX::OrdType_LIMIT: 
			return Order::limit;
		default: 
			throw std::logic_error("Unsupported Order Type, use limit");
	}
}

FIX::Side Application::convert(Order::Side side)
{
	switch (side)
	{
		case Order::buy: 
			return FIX::Side(FIX::Side_BUY);
		case Order::sell: 
			return FIX::Side(FIX::Side_SELL);
		default: 
			throw std::logic_error("Unsupported Side, use buy or sell");
	}
}

FIX::OrdType Application::convert(Order::Type type)
{
	switch (type)
	{
		case Order::limit: 
			return FIX::OrdType(FIX::OrdType_LIMIT);
		default: 
			throw std::logic_error("Unsupported Order Type, use limit");
	}
}

const OrderMatcher& Application::get_order_matcher() {
	return order_matcher;
}

