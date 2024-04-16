#ifndef ORDER_TRACKER_H
#define ORDER_TRACKER_H

#include <string>
#include <iomanip>
#include <ostream>
#include <unordered_map>

#include "quickfix/FixValues.h"

#include "exec_report.h"

namespace common {

	class OrderReport
	{
		friend std::ostream& operator<<(std::ostream&, const OrderReport&);

	public:

		OrderReport(
			const ExecReport& report
		);

		OrderReport(
			const std::string& symbol,
			const std::string& ord_id,
			const std::string& order_id,
			const char ord_type,
			const char ord_status,
			const char side,
			double price,
			double avg_px,
			double order_qty,
			double cum_qty,
			double leaves_qty
		);

		std::string symbol{};
		std::string ord_id{};
		std::string order_id{};
		char ord_type{ FIX::OrdType_MARKET };
		char ord_status{ FIX::OrdStatus_REJECTED };
		char side{ FIX::Side_UNDISCLOSED };
		double price{ 0 };
		double avg_px{ 0 };
		double order_qty{ 0 };
		double cum_qty{ 0 };
		double leaves_qty{ 0 };

		std::string to_string() const;
	};

	std::ostream& operator<<(std::ostream&, const OrderReport&);

	class Position {
		friend std::ostream& operator<<(std::ostream&, const Position&);

	public:

		std::string account{};
		std::string symbol{};
		double avg_px{ 0 };
		double qty_long{ 0 };
		double qty_short{ 0 };

		Position(
			const std::string& account,
			const std::string& symbol
		);

		std::string to_string() const;

		double net_qty() const;
	};

	class NetPosition {
		friend std::ostream& operator<<(std::ostream&, const NetPosition&);

	public:

		std::string account{};
		std::string symbol{};
		double avg_px{ 0 };
		double qty{ 0 };

		NetPosition(
			const std::string& account,
			const std::string& symbol
		);

		std::string to_string() const;
	};

	std::ostream& operator<<(std::ostream&, const Position&);

	class OrderTracker {
		std::string account;
		std::unordered_map<std::string, NetPosition> position_by_symbol;
		std::unordered_map<std::string, OrderReport> pending_orders_by_cl_ord_id;
		std::unordered_map<std::string, OrderReport> open_orders_by_ord_id;
		std::unordered_map<std::string, OrderReport> history_orders_by_ord_id;

	public:
		typedef typename std::unordered_map<std::string, OrderReport>::const_iterator const_iterator;

		OrderTracker(const std::string& account);

		NetPosition& net_position(const std::string& symbol);

		std::pair<typename OrderTracker::const_iterator, bool> get_pending_order(const std::string& ord_id) const;

		std::pair<typename OrderTracker::const_iterator, bool> get_open_order(const std::string& ord_id) const;

		std::pair<typename OrderTracker::const_iterator, bool> get_history_order(const std::string& ord_id) const;

		void process(const ExecReport& report);

		std::string to_string() const;
	};

}

#endif 


