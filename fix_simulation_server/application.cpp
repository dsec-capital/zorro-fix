#ifdef _MSC_VER
#pragma warning( disable : 4503 4355 4786 )
#endif

#include "quickfix/config.h"

#include "application.h"
#include "time_utils.h"

#include "quickfix/Session.h"
#include "quickfix/fix44/ExecutionReport.h"
#include "quickfix/fix44/BusinessMessageReject.h"
#include "quickfix/fix44/MarketDataRequestReject.h"
#include "quickfix/fix44/MarketDataSnapshotFullRefresh.h"
#include "quickfix/fix44/MarketDataIncrementalRefresh.h"


void Application::runMarketDataUpdate() {
	while (!done) {
		std::this_thread::sleep_for(m_marketUpdatePeriod);

		for (auto& market : m_orderMatcher.m_markets) {
			market.second.simulateNext();
			auto it = m_marketDataSubscriptions.find(market.first);
			if (it != m_marketDataSubscriptions.end()) {
				auto message = market.second.getUpdateMessage(
					it->second.first, it->second.second, m_generator.genMarketDataID()
				);
				if (message.has_value()) {
					FIX::Session::sendToTarget(message.value());
				}
			}
		}
	}
}

void Application::startMarketDataUpdates() {
	m_logger->onEvent("OrderMatcher starting market data updates");
	thread = std::thread(&Application::runMarketDataUpdate, this);
}

void Application::stopMarketDataUpdates() {
	if (!started)
		return;
	started = false;
	done = true;
	if (thread.joinable())
		thread.join();
}

void Application::marketDataSubscribe(const std::string& symbol, const std::string& senderCompID, const std::string& targetCompID) {
	m_marketDataSubscriptions.insert_or_assign(symbol, std::make_pair(senderCompID, targetCompID));
}

void Application::marketDataUnsubscribe(const std::string& symbol) {
	auto it = m_marketDataSubscriptions.find(symbol);
	if (it != m_marketDataSubscriptions.end())
		m_marketDataSubscriptions.erase(it);
}

bool Application::marketDataSubscribed(const std::string& symbol) {
	auto it = m_marketDataSubscriptions.find(symbol);
	return it != m_marketDataSubscriptions.end();
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

			processOrder(order);
		}
		else {
			throw std::logic_error("Unsupported TIF, use Day or GTC");
		}
	}
	catch (std::exception& e)
	{
		rejectOrder(senderCompID, targetCompID, clOrdID, symbol, price, side, ordType, orderQty, e.what());
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
		processCancel(origClOrdID, symbol, convert(side));
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

			auto market = m_orderMatcher.getMarket(symbol.getString());
			
			// simulate market to have up to date first values for sure
			market->second.simulateNext(); 

			// flip to send back target->sender and sender->target
			auto snapshot = market->second.getSnapshotMessage(targetCompID.getValue(), senderCompID.getValue(), mdReqID.getValue());
			FIX::Session::sendToTarget(snapshot);

			if (subscriptionRequestType == FIX::SubscriptionRequestType_SNAPSHOT_AND_UPDATES) {
				marketDataSubscribe(symbol.getValue(), targetCompID.getValue(), senderCompID.getValue());
			}
		}
	} 
}

void Application::updateOrder(const Order& order, char status)
{
	FIX::TargetCompID targetCompID(order.getOwner());
	FIX::SenderCompID senderCompID(order.getTarget());

	FIX44::ExecutionReport fixOrder(
		FIX::OrderID(order.getClientID()),
		FIX::ExecID(m_generator.genExecutionID()),
		FIX::ExecType(status),
		FIX::OrdStatus(status),
		FIX::Side(convert(order.getSide())),
		FIX::LeavesQty(order.getOpenQuantity()),
		FIX::CumQty(order.getExecutedQuantity()),
		FIX::AvgPx(order.getAvgExecutedPrice())
	);

	fixOrder.set(FIX::Symbol(order.getSymbol())); 
	fixOrder.set(FIX::ClOrdID(order.getClientID()));
	fixOrder.set(FIX::OrderQty(order.getQuantity()));
		
	if (status == FIX::OrdStatus_FILLED || status == FIX::OrdStatus_PARTIALLY_FILLED)
	{
		fixOrder.set(FIX::LastQty(order.getLastExecutedQuantity()));
		fixOrder.set(FIX::LastPx(order.getLastExecutedPrice()));
	}

	try
	{
		FIX::Session::sendToTarget(fixOrder, senderCompID, targetCompID);
	}
	catch (FIX::SessionNotFound&) {}
}

void Application::rejectOrder(const Order& order)
{
	updateOrder(order, FIX::OrdStatus_REJECTED);
}

void Application::acceptOrder(const Order& order)
{
	updateOrder(order, FIX::OrdStatus_NEW);
}

void Application::fillOrder(const Order& order)
{
	updateOrder(
		order,
		order.isFilled() ? FIX::OrdStatus_FILLED : FIX::OrdStatus_PARTIALLY_FILLED
	);
}

void Application::cancelOrder(const Order& order)
{
	updateOrder(order, FIX::OrdStatus_CANCELED);
}

void Application::rejectOrder(
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
		FIX::ExecID(m_generator.genExecutionID()),
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
	fixOrder.set(FIX::Text(message));

	try
	{
		FIX::Session::sendToTarget(fixOrder, senderCompID, targetCompID);
	}
	catch (FIX::SessionNotFound&) {}
}

void Application::processOrder(const Order& order)
{
	if (m_orderMatcher.insert(order))
	{
		acceptOrder(order);

		std::queue<Order> orders;
		m_orderMatcher.match(order.getSymbol(), orders);

		while (orders.size())
		{
			fillOrder(orders.front());
			orders.pop();
		}
	}
	else
		rejectOrder(order);
}

void Application::processCancel(const std::string& id,
	const std::string& symbol, Order::Side side)
{
	Order& order = m_orderMatcher.find(symbol, side, id);
	order.cancel();
	cancelOrder(order);
	m_orderMatcher.erase(order);
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

const OrderMatcher& Application::orderMatcher() {
	return m_orderMatcher;
}

