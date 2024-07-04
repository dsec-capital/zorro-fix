#include "pch.h"

#include "order_tracker.h"

#include "spdlog/spdlog.h"

namespace common {

	OrderReport::OrderReport(
		const ExecReport& report
	) : symbol(report.symbol)
	  , ord_id(report.ord_id)
	  , cl_ord_id(report.cl_ord_id)
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
		const std::string& ord_id,
		const std::string& cl_ord_id,
		const char ord_type,
		const char ord_status,
		const char side,
		double price,
		double avg_px,
		double order_qty,
		double cum_qty,
		double leaves_qty
	) : symbol(symbol)
	  , ord_id(ord_id)
	  , cl_ord_id(cl_ord_id)
	  , ord_type(ord_type)
	  , ord_status(ord_status)
	  , side(side)
	  , price(price)
	  , avg_px(avg_px)
	  , order_qty(order_qty)
	  , cum_qty(cum_qty)
	  , leaves_qty(leaves_qty)
	{}


	bool OrderReport::is_sell() const {
		return side == FIX::Side_SELL;
	}

	bool OrderReport::is_buy() const {
		return side == FIX::Side_BUY;
	}

	bool OrderReport::is_filled() const {
		return leaves_qty == 0;
	}

	bool OrderReport::is_cancelled() const {
		return ord_status == FIX::OrdStatus_CANCELED;
	}

	std::string OrderReport::to_string() const {
		return std::string("OrderReport[") +
			"symbol=" + symbol + ", "
			"ord_id=" + ord_id + ", "
			"cl_ord_id=" + cl_ord_id + ", "
			"ord_status=" + ord_status_string(ord_status) + ", "
			"ord_type=" + ord_type_string(ord_type) + ", "
			"side=" + side_string(side) + ", "
			"price=" + std::to_string(price) + ", "
			"avg_px=" + std::to_string(avg_px) + ", "
			"order_qty=" + std::to_string(order_qty) + ", "
			"cum_qty=" + std::to_string(cum_qty) + ", "
			"leaves_qty=" + std::to_string(leaves_qty) +
			"]";
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
		return std::string("Position[") +
			"account=" + account + ", "
			"symbol=" + symbol + ", "
			"avg_px=" + std::to_string(avg_px) + ", "
			"qty_long=" + std::to_string(qty_long) + ", "
			"qty_short=" + std::to_string(qty_short) +
			"]";
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

	NetPosition& OrderTracker::get_net_position(const std::string& symbol) {
		auto it = position_by_symbol.find(symbol);
		if (it == position_by_symbol.end()) {
			it = position_by_symbol.try_emplace(symbol, account, symbol).first;
		}
		return it->second;
	}

	std::pair<typename OrderTracker::const_iterator, bool> OrderTracker::get_pending_order(const std::string& ord_id) const {
		auto it = pending_orders_by_cl_ord_id.find(ord_id);
		return std::make_pair(it, it != pending_orders_by_cl_ord_id.end());
	}

	const std::unordered_map<std::string, OrderReport>& OrderTracker::get_orders() const {
		return orders_by_ord_id;
	}

	const std::unordered_map<std::string, NetPosition>& OrderTracker::get_net_positions() const {
		return position_by_symbol;
	}

	std::pair<typename OrderTracker::const_iterator, bool> OrderTracker::get_order(const std::string& ord_id) const {
		auto it = orders_by_ord_id.find(ord_id);
		return std::make_pair(it, it != orders_by_ord_id.end());
	}

	int OrderTracker::num_order_reports() const {
		return static_cast<int>(orders_by_ord_id.size());
	}

	int OrderTracker::num_net_positions() const {
		return static_cast<int>(position_by_symbol.size());
	}

	bool OrderTracker::process(const ExecReport& report) {
		if (report.exec_type == 'I') {
			return false;
		}

		spdlog::debug("OrderTracker[{}]::process: processing report {}", account, report.to_string());

		switch (report.exec_type) {
			case FIX::ExecType_PENDING_NEW: {
				auto [_, inserted] = pending_orders_by_cl_ord_id.emplace(report.cl_ord_id, std::move(OrderReport(report)));
				if (!inserted) {
					spdlog::error(
						"OrderTracker[{}]::process[FIX::ExecType_PENDING_NEW]: failed to insert cl_ord_id={} report={}", 
						account, report.cl_ord_id, report.to_string()
					);
					return false;
				}
				return true;
			}

			case FIX::ExecType_NEW: {
				pending_orders_by_cl_ord_id.erase(report.cl_ord_id);
				auto [_, inserted] = orders_by_ord_id.emplace(report.ord_id, std::move(OrderReport(report)));
				if (!inserted) {
					spdlog::error(
						"OrderTracker[{}]::process[FIX::ExecType_NEW]: failed to insert ord_id={} report={}", 
						account, report.ord_id, report.to_string()
					);
					return false;
				}
				return true;
			}

			case FIX::ExecType_TRADE: {
				orders_by_ord_id.insert_or_assign(report.ord_id, std::move(OrderReport(report)));
				if (report.ord_status == FIX::OrdStatus_FILLED || report.ord_status == FIX::OrdStatus_PARTIALLY_FILLED) {
					auto& position = get_net_position(report.symbol);
					if (report.side == FIX::Side_BUY) {
						auto prev = position.qty;
						position.qty += report.last_qty;
						spdlog::debug(
							"OrderTracker[{}]::process[FIX::ExecType_TRADE]: buy fill updated net positon from {} to {}", 
							account, prev, position.qty
						);
					}
					else if (report.side == FIX::Side_SELL) {
						auto prev = position.qty;
						position.qty -= report.last_qty;
						spdlog::debug(
							"OrderTracker[{}]::process[FIX::ExecType_TRADE]: sell fill updated net positon from {} to {}", 
							account, prev, position.qty
						);
					}
					position.avg_px = report.avg_px;
				}
				return true;
			}

			case FIX::ExecType_PENDING_CANCEL: {
				orders_by_ord_id.insert_or_assign(report.ord_id, std::move(OrderReport(report)));
				return true;
			}

			case FIX::ExecType_REPLACED: {
				orders_by_ord_id.insert_or_assign(report.ord_id, std::move(OrderReport(report)));
				return true;
			}

			case FIX::ExecType_CANCELED: {
				orders_by_ord_id.insert_or_assign(report.ord_id, std::move(OrderReport(report)));
				return true;
			}

			case FIX::ExecType_REJECTED: {
				spdlog::warn("OrderTracker[{}]::process[FIX::ExecType_REJECTED]: rejected {}", account, report.to_string());
				return true;
			}

			default: {
				spdlog::error("OrderTracker[{}]::process: invalid FIX::ExecType {}", account, report.exec_type);
				return false;
			}
		}
	}

	void OrderTracker::set_account(const std::string& new_account) {
		account = new_account;
	}

	const std::string& OrderTracker::get_account() const {
		return account;
	}

	std::string OrderTracker::to_string() const {
		std::string rows;
		rows += std::format("OrderTracker[{}][\n", account);
		if (!pending_orders_by_cl_ord_id.empty()) {
			rows += "  pending orders:\n";
			for (auto& [cl_ord_id, order] : pending_orders_by_cl_ord_id) {
				rows += std::format("    cl_ord_id={} order={}\n", cl_ord_id, order.to_string());
			}
		}
		rows += "  orders:\n";
		for (auto& [ord_id, order] : orders_by_ord_id) {
			rows += std::format("    ord_id={} order={}\n", ord_id, order.to_string());
		}
		rows += "  positions:\n";
		for (auto& [symbol, pos] : position_by_symbol) {
			rows += std::format("    symbol={} pos={}\n", symbol, pos.to_string());
		}
		rows += "]";
		return rows;
	}
}



