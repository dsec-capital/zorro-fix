#include "pch.h"

#include "exec_report.h"

#include "quickfix/FixValues.h"

namespace common {

	std::string exec_type_string(const char exec_type) {
		switch (exec_type)
		{
		case FIX::ExecType_PENDING_NEW:
			return "PENDING_NEW";
		case FIX::ExecType_NEW:
			return "NEW";
		case FIX::ExecType_PARTIAL_FILL:
			return "PARTIAL_FILL";
		case FIX::ExecType_FILL:
			return "FILL";
		case FIX::ExecType_PENDING_CANCEL:
			return "PENDING_CANCEL";
		case FIX::ExecType_CANCELED:
			return "CANCELED";
		case FIX::ExecType_PENDING_REPLACE:
			return "PENDING_REPLACE";
		case FIX::ExecType_REPLACED:
			return "REPLACED";
		case FIX::ExecType_TRADE:
			return "TRADE";
		case FIX::ExecType_REJECTED:
			return "REJECTED";
		default:
			return "UNKNOWN";
		}
	}

	std::string ord_type_string(const char ord_type) {
		switch (ord_type)
		{
		case FIX::OrdType_MARKET:
			return "MARKET";
		case FIX::OrdType_LIMIT:
			return "LIMIT";
		default:
			return "UNKNOWN";
		}
	}

	std::string ord_status_string(const char ord_status) {
		switch (ord_status)
		{
		case FIX::OrdStatus_PENDING_NEW:
			return "PENDING_NEW";
		case FIX::OrdStatus_NEW:
			return "NEW";
		case FIX::OrdStatus_PARTIALLY_FILLED:
			return "PARTIALLY_FILLED";
		case FIX::OrdStatus_FILLED:
			return "FILLED";
		case FIX::OrdStatus_PENDING_CANCEL:
			return "PENDING_CANCEL";
		case FIX::OrdStatus_CANCELED:
			return "CANCELED";
		case FIX::OrdStatus_PENDING_REPLACE:
			return "PENDING_REPLACE";
		case FIX::OrdStatus_REPLACED:
			return "REPLACED";
		case FIX::OrdStatus_REJECTED:
			return "REJECTED";
		default:
			return "UNKNOWN";
		}
	}

	std::string side_string(const char side) {
		switch (side)
		{
		case FIX::Side_UNDISCLOSED:
			return "UNDISCLOSED";
		case FIX::Side_BUY:
			return "BUY";
		case FIX::Side_SELL:
			return "SELL";
		default:
			return "UNKNOWN";
		}
	}

	ExecReport::ExecReport() {}

	ExecReport::ExecReport(
		const std::string& symbol,
		const std::string& ord_id,
		const std::string& cl_ord_id,
		const std::string& exec_id,
		const char exec_type,
		const char ord_type,
		const char ord_status,
		const char side,
		double price,
		double avg_px,
		double order_qty,
		double last_qty,
		double last_px,
		double cum_qty,
		double leaves_qty,
		const std::string& text
	) : symbol(symbol)
	  , ord_id(ord_id)
	  , cl_ord_id(cl_ord_id)
	  , exec_id(exec_id)
	  , exec_type(exec_type)
	  , ord_type(ord_type)
	  , ord_status(ord_status)
	  , side(side)
	  , price(price)
	  , avg_px(avg_px)
	  , order_qty(order_qty)
	  , last_qty(last_qty)
     , last_px(last_px)
	  , cum_qty(cum_qty)
	  , leaves_qty(leaves_qty)
	  , text(text)
	{}

	std::string ExecReport::to_string() const {
		return std::string("ExecReport[") +
			"symbol=" + symbol + ", "
			"ord_id=" + ord_id + ", "
			"cl_ord_id=" + cl_ord_id + ", "
			"exec_id=" + exec_id + ", "
			"exec_type=" + exec_type_string(exec_type) + ", "
			"ord_type=" + ord_type_string(ord_type) + ", "
			"ord_status=" + ord_status_string(ord_status) + ", "
			"side=" + side_string(side) + ", "
			"price=" + std::to_string(price) + ", "
			"avg_px=" + std::to_string(avg_px) + ", "
			"order_qty=" + std::to_string(order_qty) + ", "
			"cum_qty=" + std::to_string(cum_qty) + ", "
			"leaves_qty=" + std::to_string(leaves_qty) + ", "
			"text=" + text +
			"]";
	}

	std::ostream& operator<<(std::ostream& ostream, const ExecReport& report)
	{
		return ostream << report.to_string();
	}
}


