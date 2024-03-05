#ifndef EXEC_REPORT_H
#define EXEC_REPORT_H

#include <string>
#include <iomanip>
#include <ostream>

#include "quickfix/FixValues.h"


class ExecReport
{
	friend std::ostream& operator<<(std::ostream&, const ExecReport&);

public:

	ExecReport() {}

	ExecReport(
		const std::string& symbol,
		const std::string& clOrdId,
		const std::string& orderId,
		const std::string& execId,
		const char execType,
		const char ordType,
		const char ordStatus,
		const char side,
		double price,
		double avgPx,
		double orderQty,
		double cumQty,
		double leavesQty,
		const std::string& text
	) : symbol(symbol),
		clOrdId(clOrdId),
		orderId(orderId),
		execId(execId),
		execType(execType),
		ordType(ordType),
		ordStatus(ordStatus),
		side(side),
		price(price),
		avgPx(avgPx),
		orderQty(orderQty),
		cumQty(cumQty),
		leavesQty(leavesQty),
		text(text)
	{}

	/*
	ExecReport(ExecReport&& other) {

	}

	ExecReport& operator=(ExecReport&& other) {

	}*/

	std::string symbol{};
	std::string clOrdId{};
	std::string orderId{};
	std::string execId{};
	char execType{ FIX::ExecType_REJECTED };
	char ordType{FIX::OrdType_MARKET };
	char ordStatus{ FIX::OrdStatus_REJECTED };
	char side{FIX::Side_UNDISCLOSED};
	double price{ 0 };
	double avgPx{ 0 };
	double orderQty{ 0 };
	double cumQty{ 0 };
	double leavesQty{ 0 };
	std::string text{};
};

inline std::ostream& operator<<(std::ostream& ostream, const ExecReport& report)
{
	return ostream
		<< "symbol=" << report.symbol
		<< " clOrdId=" << report.clOrdId
		<< " orderId=" << report.orderId
		<< " execType=" << report.execType
		<< " ordType=" << report.ordType;
}

#endif
