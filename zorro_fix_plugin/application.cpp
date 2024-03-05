#include "pch.h"

#ifdef _MSC_VER
#pragma warning(disable : 4503 4355 4786)
#endif

#include "quickfix/config.h"
#include "quickfix/Session.h"

#include "logger.h"
#include "application.h"
#include <iostream>

namespace zfix
{
	void Application::showMsg(const std::string& msg) const {
		brokerError(msg.c_str());
	}

	void Application::onCreate(const FIX::SessionID&) {}

	void Application::onLogon(const FIX::SessionID& sessionID)
	{
		LOG_INFO("Application::onLogon %s\n", sessionID.toString().c_str());
		senderCompID = sessionSettings.get(sessionID).getString("SenderCompID");
		targetCompID = sessionSettings.get(sessionID).getString("TargetCompID");
		LOG_INFO("senderCompID=%s, targetCompID=%s\n", senderCompID.c_str(), targetCompID.c_str());
	}

	void Application::onLogout(const FIX::SessionID& sessionID)
	{
		LOG_INFO("Application::onLogout %s\n", sessionID.toString().c_str());
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
		showMsg(message.toString());
		LOG_INFO("IN: %s\n", message.toString().c_str());
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

		showMsg(message.toString());
		LOG_INFO("OUT: %s\n", message.toString().c_str());
	}

	void Application::onMessage(const FIX44::MarketDataSnapshotFullRefresh& message, const FIX::SessionID&)
	{
	}

	void Application::onMessage(const FIX44::MarketDataIncrementalRefresh& message, const FIX::SessionID&)
	{
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
		message.get(leaves_qty);
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
			cum_qty.getValue(),
			leaves_qty.getValue(),
			text.getString()
		);

		if (exec_type == FIX::ExecType_PENDING_NEW) {

		}

		if (exec_type == FIX::ExecType_NEW) {

		}

		if (exec_type == FIX::ExecType_PARTIAL_FILL) {

		}


		if (exec_type == FIX::ExecType_FILL) {

		}

		if (exec_type == FIX::ExecType_PENDING_CANCEL) {

		}

		if (exec_type == FIX::ExecType_CANCELED) {

		}

		if (exec_type == FIX::ExecType_REJECTED) {
			showMsg("FIX::ExecType_REJECTED " + message.toString());
		}
	
		execReportQueue.push(report);
	}
	
	void Application::onMessage(const FIX44::OrderCancelReject&, const FIX::SessionID&) 
	{}

	void Application::onMessage(const FIX44::BusinessMessageReject&, const FIX::SessionID&)
	{}

	FIX::Message Application::marketDataRequest(
		const FIX::Symbol& symbol, 
		const FIX::MarketDepth& markeDepth,
		const FIX::SubscriptionRequestType& subscriptionRequestType
	) {
		FIX::MDReqID mdReqID(m_generator.genID());
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
		header.setField(FIX::SenderCompID(senderCompID));
		header.setField(FIX::TargetCompID(targetCompID));

		showMsg("marketDataRequest: " + request.toString());

		FIX::Session::sendToTarget(request);

		return request;
	}

	FIX::Message Application::newOrderSingle(
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
		header.setField(FIX::SenderCompID(senderCompID));
		header.setField(FIX::TargetCompID(targetCompID));

		showMsg("newOrderSingle: " + order.toString());

		FIX::Session::sendToTarget(order);

		return order;
	}	

	FIX::Message Application::orderCancelRequest(
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
		header.setField(FIX::SenderCompID(senderCompID));
		header.setField(FIX::TargetCompID(targetCompID));

		showMsg("orderCancelRequest: " + request.toString());

		FIX::Session::sendToTarget(request);

		return request;
	}

	FIX::Message Application::cancelReplaceRequest(
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
		header.setField(FIX::SenderCompID(senderCompID));
		header.setField(FIX::TargetCompID(targetCompID));

		showMsg("orderCancelRequest: " + request.toString());

		FIX::Session::sendToTarget(request);

		return request;
	}
}
