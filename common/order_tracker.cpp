#include "pch.h"

#include "order_tracker.h"

#include "spdlog/spdlog.h"

namespace common {

	OrderReport::OrderReport(
		const ExecReport& report
	) : symbol(report.symbol)
	  , cl_ord_id(report.cl_ord_id)
	  , order_id(report.order_id)
	  , ord_type(report.ord_type)
	  , ord_status(report.ord_status)
	  , side(report.side)
	  , price(report.price)
	  , avg_px(report.avg_px)
	  , order_qty(report.order_qty)
	  , cum_qty(report.cum_qty)
	  , leaves_qty(report.leaves_qty)
	{}

	OrderReport::OrderReport(
		const std::string& symbol,
		const std::string& cl_ord_id,
		const std::string& order_id,
		const char ord_type,
		const char ord_status,
		const char side,
		double price,
		double avg_px,
		double order_qty,
		double cum_qty,
		double leaves_qty
	) : symbol(symbol)
	  , cl_ord_id(cl_ord_id)
	  , order_id(order_id)
	  , ord_type(ord_type)
	  , ord_status(ord_status)
	  , side(side)
	  , price(price)
	  , avg_px(avg_px)
	  , order_qty(order_qty)
	  , cum_qty(cum_qty)
	  , leaves_qty(leaves_qty)
	{}

	std::string OrderReport::to_string() const {
		return "symbol=" + symbol + ", "
			"cl_ord_id=" + cl_ord_id + ", "
			"order_id=" + order_id + ", "
			"ordType=" + std::to_string(ord_type) + ", "
			"side=" + std::to_string(side) + ", "
			"price=" + std::to_string(price) + ", "
			"avg_px=" + std::to_string(avg_px) + ", "
			"order_qty=" + std::to_string(order_qty) + ", "
			"cum_qty=" + std::to_string(cum_qty) + ", "
			"leaves_qty=" + std::to_string(leaves_qty);
	}

	std::ostream& operator<<(std::ostream& ostream, const OrderReport& report)
	{
		return ostream << report.to_string();
	}

	Position::Position(
		const std::string& account,
		const std::string& symbol
	) : account(account)
		, symbol(symbol)
	{}

	std::string Position::to_string() const {
		return "account=" + account + ", "
			"symbol=" + symbol + ", "
			"avg_px=" + std::to_string(avg_px) + ", "
			"qty_long=" + std::to_string(qty_long) + ", "
			"qty_short=" + std::to_string(qty_short);
	}

	double Position::net_qty() const {
		return qty_long - qty_short;
	}

	std::ostream& operator<<(std::ostream& ostream, const Position& pos)
	{
		return ostream << pos.to_string();
	}

	NetPosition::NetPosition(
		const std::string& account,
		const std::string& symbol
	) : account(account)
		, symbol(symbol)
	{}

	std::string NetPosition::to_string() const {
		return "account=" + account + ", "
			"symbol=" + symbol + ", "
			"avg_px=" + std::to_string(avg_px) + ", "
			"qty=" + std::to_string(qty);
	}

	std::ostream& operator<<(std::ostream& ostream, const NetPosition& pos)
	{
		return ostream << pos.to_string();
	}

	OrderTracker::OrderTracker(const std::string& account) : account(account) {}

	NetPosition& OrderTracker::net_position(const std::string& symbol) {
		auto it = position_by_symbol.find(symbol);
		if (it == position_by_symbol.end()) {
			it = position_by_symbol.try_emplace(symbol, account, symbol).first;
		}
		return it->second;
	}

	std::pair<typename OrderTracker::const_iterator, bool> OrderTracker::get_pending_order(const std::string& cl_ord_id) const {
		auto it = pending_orders_by_cl_ord_id.find(cl_ord_id);
		return std::make_pair(it, it != pending_orders_by_cl_ord_id.end());
	}

	std::pair<typename OrderTracker::const_iterator, bool> OrderTracker::get_open_order(const std::string& ord_id) const {
		auto it = open_orders_by_ord_id.find(ord_id);
		return std::make_pair(it, it != open_orders_by_ord_id.end());
	}

	std::pair<typename OrderTracker::const_iterator, bool> OrderTracker::get_history_order(const std::string& ord_id) const {
		auto it = history_orders_by_ord_id.find(ord_id);
		return std::make_pair(it, it != history_orders_by_ord_id.end());
	}

	void OrderTracker::process(const ExecReport& report) {
		if (report.exec_type == 'I') {
			return;
		}

		switch (report.exec_type) {
			case FIX::ExecType_PENDING_NEW: {
				pending_orders_by_cl_ord_id.emplace(report.cl_ord_id, std::move(OrderReport(report)));
				break;
			}

			case FIX::ExecType_NEW: {
				pending_orders_by_cl_ord_id.erase(report.cl_ord_id);
				open_orders_by_ord_id.emplace(report.order_id, std::move(OrderReport(report)));
				break;
			}

			case FIX::ExecType_PARTIAL_FILL: {
				open_orders_by_ord_id.emplace(report.order_id, std::move(OrderReport(report)));
				auto position = net_position(report.symbol);
				position.qty += report.last_qty;
				position.avg_px = report.avg_px;
				break;
			}

			case FIX::ExecType_FILL: {
				open_orders_by_ord_id.erase(report.order_id);
				history_orders_by_ord_id.emplace(report.order_id, std::move(OrderReport(report)));
				auto position = net_position(report.symbol);
				position.qty += report.last_qty;
				position.avg_px = report.avg_px;
				break;
			}

			case FIX::ExecType_PENDING_CANCEL: {
				open_orders_by_ord_id.emplace(report.order_id, std::move(OrderReport(report)));
				break;
			}

			case FIX::ExecType_CANCELED: {
				open_orders_by_ord_id.erase(report.order_id);
				history_orders_by_ord_id.emplace(report.order_id, std::move(OrderReport(report)));
				break;
			}

			case FIX::ExecType_REJECTED: {
				SPDLOG_WARN("rejected {}", report.to_string());
				break;
			}
		}
	}
}



