#ifndef ORDER_TRACKER_H
#define ORDER_TRACKER_H

#include <string>
#include <iomanip>
#include <ostream>
#include <unordered_map>

#include "quickfix/FixValues.h"

#include "exec_report.h"

class Order
{
	friend std::ostream& operator<<(std::ostream&, const Order&);

public:

	Order(
		const ExecReport &report
	) : symbol(report.symbol),
		clOrdId(report.clOrdId),
		orderId(report.orderId),
		ordType(report.ordType),
		ordStatus(report.ordStatus),
		side(report.side),
		price(report.price),
		avgPx(report.avgPx),
		orderQty(report.orderQty),
		cumQty(report.cumQty),
		leavesQty(report.leavesQty)
	{}

	Order(
		const std::string& symbol,
		const std::string& clOrdId,
		const std::string& orderId,
		const char ordType,
		const char ordStatus,
		const char side,
		double price,
		double avgPx,
		double orderQty,
		double cumQty,
		double leavesQty
	) : symbol(symbol),
		clOrdId(clOrdId),
		orderId(orderId),
		ordType(ordType),
		ordStatus(ordStatus),
		side(side),
		price(price),
		avgPx(avgPx),
		orderQty(orderQty),
		cumQty(cumQty),
		leavesQty(leavesQty)
	{}

	std::string symbol{};
	std::string clOrdId{};
	std::string orderId{};
	char ordType{ FIX::OrdType_MARKET };
	char ordStatus{ FIX::OrdStatus_REJECTED };
	char side{ FIX::Side_UNDISCLOSED };
	double price{ 0 };
	double avgPx{ 0 };
	double orderQty{ 0 };
	double cumQty{ 0 };
	double leavesQty{ 0 };

	std::string toString() const {
		return "symbol=" + symbol + ", "
			"clOrdId=" + clOrdId + ", "
			"orderId=" + orderId + ", "
			"ordType=" + std::to_string(ordType) + ", "
			"side=" + std::to_string(side) + ", "
			"price=" + std::to_string(price) + ", "
			"avgPx=" + std::to_string(avgPx) + ", "
			"orderQty=" + std::to_string(orderQty) + ", "
			"cumQty=" + std::to_string(cumQty) + ", "
			"leavesQty=" + std::to_string(leavesQty);
	}
};

inline std::ostream& operator<<(std::ostream& ostream, const Order& report)
{
	return ostream << report.toString();
}

class OrderTracker {
	std::unordered_map<uint32_t, Order> pendingOrdersByClOrdId;
	std::unordered_map<uint32_t, Order> openOrdersByOrdId;

public:
	OrderTracker() {}

	void process(const ExecReport& report) {

	}
};

#endif // ORDER_TRACKER_H


