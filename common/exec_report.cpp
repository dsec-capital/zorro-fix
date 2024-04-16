#include "pch.h"

#include "exec_report.h"

#include "quickfix/FixValues.h"

namespace common {

	ExecReport::ExecReport() {}

	ExecReport::ExecReport(
		const std::string& symbol,
		const std::string& ord_id,
		const std::string& order_id,
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
	  , order_id(order_id)
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
		return "symbol=" + symbol + ", "
			"ord_id=" + ord_id + ", "
			"order_id=" + order_id + ", "
			"exec_id=" + exec_id + ", "
			"exec_type=" + std::to_string(exec_type) + ", "
			"ord_type=" + std::to_string(ord_type) + ", "
			"ord_status=" + std::to_string(ord_status) + ", "
			"side=" + std::to_string(side) + ", "
			"price=" + std::to_string(price) + ", "
			"avg_px=" + std::to_string(avg_px) + ", "
			"order_qty=" + std::to_string(order_qty) + ", "
			"cum_qty=" + std::to_string(cum_qty) + ", "
			"leaves_qty=" + std::to_string(leaves_qty) + ", "
			"text=" + text;
	}

	std::ostream& operator<<(std::ostream& ostream, const ExecReport& report)
	{
		return ostream << report.to_string();
	}
}


