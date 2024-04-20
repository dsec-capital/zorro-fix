#ifndef EXEC_REPORT_H
#define EXEC_REPORT_H

#include <string>
#include <iomanip>
#include <ostream>

namespace common {

	class ExecReport
	{
		friend std::ostream& operator<<(std::ostream&, const ExecReport&);

	public:

		ExecReport();

		ExecReport(
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
		);

		std::string symbol{};
		std::string ord_id{};
		std::string cl_ord_id{};
		std::string exec_id{};
		char exec_type{};
		char ord_type{};
		char ord_status{};
		char side{};
		double price{};
		double avg_px{};
		double order_qty{};
		double last_qty{};
		double last_px{};
		double cum_qty{};
		double leaves_qty{};
		std::string text{};

		std::string to_string() const;
	};

	std::ostream& operator<<(std::ostream&, const ExecReport&);
}

#endif
