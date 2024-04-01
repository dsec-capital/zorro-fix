#ifdef _MSC_VER
#pragma warning(disable : 4503 4355 4786)
#endif

#include "pch.h"

#include "application.h"

#include "quickfix/config.h"
#include "quickfix/Session.h"

#include "common/time_utils.h"

#include "spdlog/spdlog.h"

namespace zfix {

	using namespace common;

	std::string fix_string(const FIX::Message& msg) {
		auto s = msg.toString();
		std::replace(s.begin(), s.end(), '\x1', '|');  
		return s;
	}

	Application::Application(
		const FIX::SessionSettings& session_settings,
		BlockingTimeoutQueue<ExecReport>& exec_report_queue
	) : session_settings(session_settings)
	  , exec_report_queue(exec_report_queue)
	  , done(false)
     , order_tracker("account")
	{}

	void Application::onCreate(const FIX::SessionID&) {}

	void Application::onLogon(const FIX::SessionID& sessionID)
	{
		SPDLOG_INFO("Application::onLogon {}", sessionID.toString());
		sender_comp_id = session_settings.get(sessionID).getString("SenderCompID");
		target_comp_id = session_settings.get(sessionID).getString("TargetCompID");
		SPDLOG_INFO("senderCompID={}, targetCompID={}", sender_comp_id, target_comp_id);
	}

	void Application::onLogout(const FIX::SessionID& sessionID)
	{
		SPDLOG_INFO("Application::onLogout {}", sessionID.toString());
	}

	void Application::fromAdmin(
		const FIX::Message&, const FIX::SessionID&
	) EXCEPT(FIX::FieldNotFound, FIX::IncorrectDataFormat, FIX::IncorrectTagValue, FIX::RejectLogon)
	{}

	void Application::toAdmin(FIX::Message&, const FIX::SessionID&) 
	{}

	void Application::fromApp(const FIX::Message& message, const FIX::SessionID& sessionID)
		EXCEPT(FIX::FieldNotFound, FIX::IncorrectDataFormat, FIX::IncorrectTagValue, FIX::UnsupportedMessageType)
	{
		SPDLOG_INFO("IN fromApp: {}", fix_string(message));
		crack(message, sessionID);
	}

	void Application::toApp(FIX::Message& message, const FIX::SessionID& sessionID)
		EXCEPT(FIX::DoNotSend)
	{
		try
		{
			FIX::PossDupFlag possDupFlag;
			message.getHeader().getField(possDupFlag);
			if (possDupFlag) throw FIX::DoNotSend();
		}
		catch (FIX::FieldNotFound&) {}

		SPDLOG_INFO("OUT toApp: {}", fix_string(message));
	}

	void Application::onMessage(const FIX44::MarketDataSnapshotFullRefresh& message, const FIX::SessionID&)
	{
		FIX::Symbol symbol;
		FIX::NoMDEntries noMDEntries;
		FIX44::MarketDataSnapshotFullRefresh::NoMDEntries noMDEntriesGroup;

		message.get(symbol);
		message.get(noMDEntries);

		auto book = books.insert_or_assign(symbol, Book()).first;

		for (int i = 1; i <= noMDEntries; ++i)
		{
			message.getGroup(i, noMDEntriesGroup);

			FIX::MDEntryType type;
			FIX::MDEntryDate date;
			FIX::MDEntryTime time;
			FIX::MDEntrySize size;
			FIX::MDEntryPx price;

			noMDEntriesGroup.get(type);
			noMDEntriesGroup.get(date);
			noMDEntriesGroup.get(time);
			noMDEntriesGroup.get(size);
			noMDEntriesGroup.get(price);

			auto is_bid = type == FIX::MDEntryType_BID;
			book->second.update_book(price, size, is_bid);
		}
	}

	void Application::onMessage(const FIX44::MarketDataIncrementalRefresh& message, const FIX::SessionID&)
	{
		FIX::NoMDEntries noMDEntries;
		FIX44::MarketDataIncrementalRefresh::NoMDEntries noMDEntriesGroup;

		message.get(noMDEntries);

		FIX::Symbol symbol;
		FIX::MDEntryType type;
		FIX::MDEntryDate date;
		FIX::MDEntryTime time;
		FIX::MDEntrySize size;
		FIX::MDEntryPx price;
		FIX::MDUpdateAction action;

		auto it = books.end();

		for (int i = 1; i <= noMDEntries; ++i)
		{
			message.getGroup(i, noMDEntriesGroup);
			noMDEntriesGroup.get(symbol);
			noMDEntriesGroup.get(type);
			noMDEntriesGroup.get(date);
			noMDEntriesGroup.get(time);
			noMDEntriesGroup.get(size);
			noMDEntriesGroup.get(price);
			noMDEntriesGroup.get(action);

			if (it == books.end() || it->first != symbol) {
				it = books.find(symbol);
				if (it == books.end()) {
					SPDLOG_ERROR("MarketDataIncrementalRefresh: no book for {} probably not subscribed? msg={}", symbol.getString(), fix_string(message));
				}
				it->second.set_timestamp(get_current_system_clock());
			}
			else {
				if (action == FIX::MDUpdateAction_DELETE) {
					assert(size == 0);
				}
				it->second.update_book(price, size, type == FIX::MDEntryType_BID);
			}
		}
	}

	void Application::onMessage(const FIX44::ExecutionReport& message, const FIX::SessionID&) 
	{
		FIX::Symbol symbol;
		FIX::ExecID exec_id;
		FIX::ExecType exec_type;
		FIX::OrderID order_id;
		FIX::ClOrdID cl_ord_id;
		FIX::OrdStatus ord_status;
		FIX::OrdType ord_type;
		FIX::Side side;
		FIX::Price price;
		FIX::OrderQty order_qty;
		FIX::LastQty last_qty;
		FIX::LastPx last_px;
		FIX::LeavesQty leaves_qty;
		FIX::CumQty cum_qty;
		FIX::AvgPx avg_px;
		FIX::Text text;

		message.get(symbol);
		message.get(exec_id);
		message.get(exec_type);
		message.get(order_id);
		message.get(cl_ord_id);
		message.get(ord_status);
		message.get(ord_type);
		message.get(side);
		message.get(price);
		message.get(avg_px);
		message.get(order_qty);
		message.get(last_qty);
		message.get(leaves_qty);
		message.get(last_px);
		message.get(cum_qty);
		message.get(text);

		ExecReport report(
			symbol.getString(),
			cl_ord_id.getString(),
			order_id.getString(),
			exec_id.getString(),
			exec_type.getValue(),
			ord_status.getValue(),
			ord_type.getValue(),
			side.getValue(),
			price.getValue(),
			avg_px.getValue(),
			order_qty.getValue(),
			last_qty.getValue(),
			last_px.getValue(),
			cum_qty.getValue(),
			leaves_qty.getValue(),
			text.getString()
		);

		order_tracker.process(report);
		exec_report_queue.push(report);
	}
	
	void Application::onMessage(const FIX44::OrderCancelReject&, const FIX::SessionID&) 
	{}

	void Application::onMessage(const FIX44::BusinessMessageReject&, const FIX::SessionID&)
	{}

	FIX::Message Application::market_data_request(
		const FIX::Symbol& symbol, 
		const FIX::MarketDepth& markeDepth,
		const FIX::SubscriptionRequestType& subscriptionRequestType
	) {
		FIX::MDReqID mdReqID(id_generator.genID());
		FIX44::MarketDataRequest request(mdReqID, subscriptionRequestType, markeDepth);
		FIX44::MarketDataRequest::NoRelatedSym noRelatedSymGroup;
		noRelatedSymGroup.set(symbol);
		request.addGroup(noRelatedSymGroup);
		FIX44::MarketDataRequest::NoMDEntryTypes noMDEntryTypesGroup;
		noMDEntryTypesGroup.set(FIX::MDEntryType_BID);
		noMDEntryTypesGroup.set(FIX::MDEntryType_OFFER);
		noMDEntryTypesGroup.set(FIX::MDEntryType_TRADE);
		request.addGroup(noMDEntryTypesGroup);

		auto& header = request.getHeader();
		header.setField(FIX::SenderCompID(sender_comp_id));
		header.setField(FIX::TargetCompID(target_comp_id));

		SPDLOG_INFO("marketDataRequest: {}", fix_string(request));

		FIX::Session::sendToTarget(request);

		return request;
	}

	FIX::Message Application::new_order_single(
		const FIX::Symbol& symbol, 
		const FIX::ClOrdID& clOrdId, 
		const FIX::Side& side, 
		const FIX::OrdType& ordType, 
		const FIX::TimeInForce& tif,
		const FIX::OrderQty& orderQty, 
		const FIX::Price& price, 
		const FIX::StopPx& stopPrice
	) const {
		FIX44::NewOrderSingle order(
			clOrdId,
			side,
			FIX::TransactTime(),
			ordType);

		order.set(FIX::HandlInst('1'));
		order.set(symbol);
		order.set(orderQty);
		order.set(tif);

		if (ordType == FIX::OrdType_LIMIT || ordType == FIX::OrdType_STOP_LIMIT)
			order.set(price);

		if (ordType == FIX::OrdType_STOP || ordType == FIX::OrdType_STOP_LIMIT)
			order.set(stopPrice);

		auto& header = order.getHeader();
		header.setField(FIX::SenderCompID(sender_comp_id));
		header.setField(FIX::TargetCompID(target_comp_id));

		SPDLOG_INFO("newOrderSingle: {}" , fix_string(order));

		FIX::Session::sendToTarget(order);

		return order;
	}	

	FIX::Message Application::order_cancel_request(
		const FIX::Symbol& symbol,
		const FIX::OrigClOrdID& origClOrdID,
		const FIX::ClOrdID& clOrdID,
		const FIX::Side& side,
		const FIX::OrderQty& orderQty		
	) const {
		FIX44::OrderCancelRequest request(
			origClOrdID,
			clOrdID, 
			side, 
			FIX::TransactTime());

		request.set(symbol);
		request.set(orderQty);

		auto& header = request.getHeader();
		header.setField(FIX::SenderCompID(sender_comp_id));
		header.setField(FIX::TargetCompID(target_comp_id));

		SPDLOG_INFO("orderCancelRequest: {}", fix_string(request));

		FIX::Session::sendToTarget(request);

		return request;
	}

	FIX::Message Application::cancel_replace_request(
		const FIX::Symbol& symbol,
		const FIX::OrigClOrdID& origClOrdID,
		const FIX::ClOrdID& clOrdID,
		const FIX::Side& side,
		const FIX::OrdType& ordType,
		const FIX::OrderQty& orderQty,
		const FIX::Price& price
	) const {
		FIX44::OrderCancelReplaceRequest request(
			origClOrdID, 
			clOrdID,
			side, 
			FIX::TransactTime(),
			ordType);

		request.set(FIX::HandlInst('1'));
		request.set(symbol);
		request.set(price);
		request.set(orderQty);

		auto& header = request.getHeader();
		header.setField(FIX::SenderCompID(sender_comp_id));
		header.setField(FIX::TargetCompID(target_comp_id));

		SPDLOG_INFO("orderCancelRequest: {}", fix_string(request));

		FIX::Session::sendToTarget(request);

		return request;
	}

	bool Application::has_book(const std::string& symbol) {
		std::unique_lock<std::mutex> mlock(mutex);
		return books.contains(symbol);
	}

	TopOfBook Application::top_of_book(const std::string& symbol) {
		std::unique_lock<std::mutex> mlock(mutex);
		auto it = books.find(symbol);
		if (it != books.end()) {
			return it->second.top(symbol);
		}
		else {
			throw std::runtime_error(std::format("symbol {} not found in books", symbol));
		}
	}
}
