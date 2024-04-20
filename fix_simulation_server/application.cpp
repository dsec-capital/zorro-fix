#ifdef _MSC_VER
#pragma warning( disable : 4503 4355 4786 )
#endif

#include "spdlog/spdlog.h"

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
	bool quote = true;

	while (!done) {
		std::this_thread::sleep_for(market_update_period);

		auto now = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch());

		for (auto& [symbol, market] : markets.markets) {
			spdlog::debug("Application::run_market_data_update: symbol={}", symbol);

			try {
				market.simulate_next();

				// send market data to subscribers only
				auto top = market.get_top_of_book();
				auto it = market_data_subscriptions.find(symbol);
				if (it != market_data_subscriptions.end()) {
					auto message = get_update_message(it->second.first, it->second.second, top);
					if (message) {
						FIX::Session::sendToTarget(message.value());
					}
				}

				if (quote) {
					if (top.first.bid_price != top.second.bid_price) {
						const auto& bid_order = market.get_bid_order();
						market.erase(bid_order.get_ord_id(), bid_order.get_side());
						auto order = Order(
							generate_id("quote_ord_id"),
							generate_id("quote_cl_ord_id"),
							symbol,
							OWNER_MARKET_SIMULATOR,
							"",
							Order::Side::buy,
							Order::Type::limit,
							top.first.bid_price,
							(long)top.first.bid_volume
						);
						auto result = market.quote(order);
						spdlog::debug("Application::run_market_data_update: bid side quote price={}", top.first.bid_price);

						for (const auto& fill : result.matched)
						{
							fill_order(fill);
							spdlog::debug("Application::run_market_data_update: bid side fill order={}", fill.to_string());
						}
					}

					if (top.first.ask_price != top.second.ask_price) {
						const auto& ask_order = market.get_ask_order();
						market.erase(ask_order.get_ord_id(), ask_order.get_side());
						auto order = Order(
							generate_id("quote_ord_id"),
							generate_id("quote_cl_ord_id"),
							symbol,
							OWNER_MARKET_SIMULATOR,
							"",
							Order::Side::sell,
							Order::Type::limit,
							top.first.ask_price,
							(long)top.first.ask_volume
						);
						auto result = market.quote(order);
						spdlog::debug("Application::run_market_data_update: ask side quote price={}", top.first.ask_price);

						for (const auto& fill : result.matched)
						{
							fill_order(fill);
							spdlog::debug("Application::run_market_data_update: ask side fill order={}", fill.to_string());
						}
					}
				}
			}
			catch (std::exception& e) {
				spdlog::error("Application::run_market_data_update: exception={}", e.what());
			}

			spdlog::debug("Application::run_market_data_update: completed symbol={}", symbol);
		}
	}
}

void Application::start_market_data_updates() {
	spdlog::info("====> starting market data updates");
	thread = std::thread(&Application::run_market_data_update, this);
}

void Application::stop_market_data_updates() {
	spdlog::info("====> stopping market data updates");
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
{
	try {
		auto senderCompID = sessionID.getSenderCompID().getString();
		auto targetCompID = sessionID.getTargetCompID().getString();
		auto it = market_data_subscriptions.begin();
		spdlog::info(
			"====> removing market data subscriptions for sender_comp_id = {} target_comp_id = {}",
			senderCompID, targetCompID
		);
		while (it != market_data_subscriptions.end()) {
			if (senderCompID == it->second.first && targetCompID == it->second.second) {
				spdlog::info("removing subscription for symbol={}", it->first);
				it = market_data_subscriptions.erase(it);
			}
			else {
				++it;
			}
		}
	}
	catch (std::exception& e) {
		spdlog::error("Application::onLogon: {}", e.what());
	}

}

void Application::toAdmin(FIX::Message& message, const FIX::SessionID&)
{
}

void Application::toApp(FIX::Message& message, const FIX::SessionID&)
{
}

void Application::fromAdmin(const FIX::Message& message, const FIX::SessionID&) 
{
}


void Application::fromApp(const FIX::Message& message, const FIX::SessionID& sessionID) 
{
	try {
		crack(message, sessionID);
	}
	catch (std::exception& e) {
		spdlog::error("Application::onLogon: {}", e.what());
	}
}

void Application::onMessage(const FIX44::NewOrderSingle& message, const FIX::SessionID&)
{
	FIX::SenderCompID sender_comp_id;
	FIX::TargetCompID target_comp_id;
	FIX::ClOrdID cl_ord_id;
	FIX::Symbol symbol;
	FIX::Side side;
	FIX::OrdType ord_type;
	FIX::Price price(0);
	FIX::OrderQty orderQty(0);
	FIX::TimeInForce timeInForce(FIX::TimeInForce_DAY);

	message.getHeader().get(sender_comp_id);
	message.getHeader().get(target_comp_id);
	message.get(cl_ord_id);
	message.get(symbol);
	message.get(side);
	message.get(ord_type);
	if (ord_type == FIX::OrdType_LIMIT) {
		message.get(price);
	}
	else if (ord_type == FIX::OrdType_MARKET) {
		ord_type = FIX::OrdType_LIMIT;
		double aggressive_price = side == FIX::Side_BUY ? (std::numeric_limits<double>::max)() : 0.0;
		price = FIX::Price(aggressive_price);

		spdlog::info(
			"converging market order {} to limit order with maximally aggressive price", 
			FIX::Side_BUY ? "buy" : "sell"
		);
	}
	message.get(orderQty);
	message.getFieldIfSet(timeInForce);

	try
	{
		if (timeInForce == FIX::TimeInForce_GOOD_TILL_CANCEL || timeInForce == FIX::TimeInForce_DAY) {
			Order order(generate_id("ord_id"), cl_ord_id, symbol, sender_comp_id, target_comp_id, convert(side), convert(ord_type), price, (long)orderQty);

			process_order(order);
		}
		else {
			throw std::logic_error("Unsupported TIF, use Day or GTC");
		}
	}
	catch (std::exception& e)
	{
		spdlog::error("Application::onMessage[NewOrderSingle]: {}", e.what());
		reject_order(sender_comp_id, target_comp_id, cl_ord_id, symbol, price, side, ord_type, orderQty, e.what());
	}
	
	spdlog::debug("Application::onMessage[NewOrderSingle]: completed message={}", fix_string(message));
}

void Application::onMessage(const FIX44::OrderCancelRequest& message, const FIX::SessionID&)
{
	FIX::OrderID ord_id;
	FIX::OrigClOrdID orig_cl_ord_id;
	FIX::Symbol symbol;
	FIX::Side side;

	message.get(ord_id);
	message.get(orig_cl_ord_id);
	message.get(symbol);
	message.get(side);

	try
	{
		process_cancel(ord_id, symbol, convert(side));
	}
	catch (std::exception& e) {
		spdlog::error("Application::onMessage[OrderCancelRequest]: ord_id={}, orig_cl_ord_id={} error={}", ord_id.getString(), orig_cl_ord_id.getString(), e.what());
	}

	spdlog::debug("Application::onMessage[OrderCancelRequest]: completed message={}", fix_string(message));
}

void Application::onMessage(const FIX44::MarketDataRequest& message, const FIX::SessionID&)
{
	FIX::SenderCompID sender_comp_id;
	FIX::TargetCompID target_comp_id;
	FIX::MDReqID md_req_id;
	FIX::SubscriptionRequestType subscription_request_type;
	FIX::MarketDepth market_depth;
	FIX::NoRelatedSym no_related_sym;
	FIX44::MarketDataRequest::NoRelatedSym no_related_sym_group;

	message.getHeader().get(sender_comp_id);
	message.getHeader().get(target_comp_id);
	message.get(md_req_id);
	message.get(subscription_request_type);
	message.get(market_depth);
	message.get(no_related_sym);

	if (subscription_request_type == FIX::SubscriptionRequestType_SNAPSHOT ||
		subscription_request_type == FIX::SubscriptionRequestType_SNAPSHOT_AND_UPDATES) { 

		for (int i = 1; i <= no_related_sym; ++i)
		{
			FIX::Symbol symbol;
			message.getGroup(i, no_related_sym_group);
			no_related_sym_group.get(symbol);

			auto it = markets.markets.find(symbol.getString());
			if (it != markets.markets.end()) {
				const auto& market = it->second;

				// flip to send back target->sender and sender->target 
				auto snapshot = get_snapshot_message(
					target_comp_id.getValue(),
					sender_comp_id.getValue(),
					market.get_top_of_book().first
				);
				FIX::Session::sendToTarget(snapshot);

				if (subscription_request_type == FIX::SubscriptionRequestType_SNAPSHOT_AND_UPDATES) {
					subscribe_market_data(symbol.getValue(), target_comp_id.getValue(), sender_comp_id.getValue());
				}
			}
			else {
				throw std::runtime_error(std::format("invalid market symbol={}", symbol.getString()));
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

void Application::update_order(const Order& order, char exec_status, char ord_status, const std::string& text)
{
	// do not reply back to the FIX client about the other side of 
	// the order, i.e. owned/generated by the market simulator
	if (order.get_owner() == OWNER_MARKET_SIMULATOR) {
		return;
	}

	FIX::TargetCompID targetCompID(order.get_owner());
	FIX::SenderCompID senderCompID(order.get_target());

	FIX44::ExecutionReport fixOrder(
		FIX::OrderID(order.get_ord_id()),
		FIX::ExecID(generator.genExecutionID()),
		FIX::ExecType(exec_status),
		FIX::OrdStatus(ord_status),
		FIX::Side(convert(order.get_side())),
		FIX::LeavesQty(order.get_open_quantity()),
		FIX::CumQty(order.get_executed_quantity()),
		FIX::AvgPx(order.get_avg_executed_price())
	);

	fixOrder.set(FIX::Symbol(order.get_symbol())); 
	fixOrder.set(FIX::ClOrdID(order.get_cl_ord_id()));
	fixOrder.set(FIX::OrderQty(order.get_quantity()));
	fixOrder.set(FIX::OrdType(order.get_type() == Order::Type::limit ? FIX::OrdType_LIMIT : FIX::OrdType_MARKET));
	if (order.get_type() == Order::Type::limit) {
		fixOrder.set(FIX::Price(order.get_price()));
	}
	if (ord_status == FIX::OrdStatus_FILLED || 
		ord_status == FIX::OrdStatus_PARTIALLY_FILLED || 
		ord_status == FIX::OrdStatus_NEW || 
		ord_status == FIX::OrdStatus_REPLACED ||
		ord_status == FIX::OrdStatus_CANCELED)
	{
		fixOrder.set(FIX::LastQty(order.get_last_executed_quantity()));
		fixOrder.set(FIX::LastPx(order.get_last_executed_price()));
	}
	fixOrder.set(FIX::Text(text));

	try
	{
		FIX::Session::sendToTarget(fixOrder, senderCompID, targetCompID);
	}
	catch (FIX::SessionNotFound& e) {
		spdlog::error("Application::update_order: session not found {}", e.what());
	}
}

void Application::reject_order(const Order& order)
{
	update_order(order, FIX::ExecType_REJECTED, FIX::OrdStatus_REJECTED, "");
}

void Application::accept_order(const Order& order)
{
	update_order(order, FIX::ExecType_NEW, FIX::OrdStatus_NEW, "");
}

void Application::fill_order(const Order& order)
{
	auto ord_status = order.is_filled() ? FIX::OrdStatus_FILLED : FIX::OrdStatus_PARTIALLY_FILLED;
	update_order(order, FIX::ExecType_TRADE, ord_status, "");
}

void Application::cancel_order(const Order& order)
{
	update_order(order, FIX::ExecType_CANCELED, FIX::OrdStatus_CANCELED, "");
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
	catch (FIX::SessionNotFound& e) {
		spdlog::error("Application::reject_order: session not found {}", e.what());
	}
}

void Application::process_order(const Order& order)
{
	auto result = markets.insert(order);

	if (result.error) {
		assert(result.matched.empty());
		reject_order(order);
		return;
	} 

	// we have a new or partial fill 
	if (result.resting_order) {
		accept_order(result.resting_order.value());
	}

	// handle the partial fills and full fills here
	for (const auto& fill : result.matched)
	{
		fill_order(fill);
	}
}

void Application::process_cancel(
	const std::string& ord_id,
	const std::string& symbol, 
	Order::Side side)
{
	spdlog::info("Application::process_cancel: cancelling ord_id={}\n{}", ord_id, markets.to_string(symbol));
	auto order = markets.erase(symbol, ord_id, side);
	if (order.has_value()) {
		cancel_order(order.value());
		spdlog::info("Application::process_cancel: cancelled order={}\n{}", order.value().to_string(), markets.to_string(symbol));
	}
	else {
		spdlog::error("Application::process_cancel: could not find order with ord_id={} side={}", ord_id, common::to_string(side));
	}
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
		case FIX::OrdType_MARKET:
			return Order::market;
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
		case Order::market:
			return FIX::OrdType(FIX::OrdType_MARKET);
		default: 
			throw std::logic_error("Unsupported Order Type, use limit");
	}
}

const Markets& Application::get_markets() {
	return markets;
}

std::string Application::generate_id(const std::string& label) {
	return std::format("{}_{}", label, ++ord_id);
}


