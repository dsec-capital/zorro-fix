#ifndef EXEC_REPORT_H
#define EXEC_REPORT_H

#include <string>
#include <iomanip>
#include <ostream>
#include <optional>

namespace common {

	std::string exec_type_string(const char exec_type);

	std::string ord_type_string(const char ord_type);

	std::string ord_status_string(const char ord_status);

	std::string side_string(const char side);

	std::string time_in_force_string(const char tif);

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
			const char tif,
			double price,
			double avg_px,
			double order_qty,
			double last_qty,
			double last_px,
			double cum_qty,
			double leaves_qty,
			const std::string& text,
			const std::string& custom_1 = "",
			const std::string& custom_2 = "",
			const std::string& custom_3 = ""
		);

		std::string symbol{};
		std::string ord_id{};
		std::string cl_ord_id{};
		std::string exec_id{};
		char exec_type{};
		char ord_type{};
		char ord_status{};
		char side{};
		char tif{};
		double price{};
		double avg_px{};
		double order_qty{};
		double last_qty{};
		double last_px{};
		double cum_qty{};
		double leaves_qty{};
		std::string text{};
		std::string custom_1{};
		std::string custom_2{};
		std::string custom_3{};

		std::string to_string(const std::string& c1 = "", const std::string& c2 = "", const std::string& c3 = "") const;
	};

	std::ostream& operator<<(std::ostream&, const ExecReport&);

	class StatusExecReport
	{
		friend std::ostream& operator<<(std::ostream&, const StatusExecReport&);

	public:

		StatusExecReport();

		StatusExecReport(
			const std::string& symbol,
			const std::string& ord_id,
			const std::string& cl_ord_id,
			const std::string& exec_id,
			const std::string& mass_status_req_id,
			const char exec_type,
			const char ord_type,
			const char ord_status,
			const char side,
			const char tif,
			double price,
			double avg_px,
			double order_qty,
			double last_qty,
			double last_px,
			double cum_qty,
			double leaves_qty,
			const std::string& text,
			int tot_num_reports,
			bool last_rpt_requested,
			const std::string& custom_1 = "",
			const std::string& custom_2 = "",
			const std::string& custom_3 = ""
		);

		std::string symbol{};
		std::string ord_id{};
		std::string cl_ord_id{};
		std::string exec_id{};
		std::string mass_status_req_id{};
		char exec_type{};
		char ord_type{};
		char ord_status{};
		char side{};
		char tif{};
		double price{};
		double avg_px{};
		double order_qty{};
		double last_qty{};
		double last_px{};
		double cum_qty{};
		double leaves_qty{};
		std::string text{};
		int tot_num_reports{};
		bool last_rpt_requested{};
		std::string custom_1{};
		std::string custom_2{};
		std::string custom_3{};

		std::string to_string(const std::string& c1="", const std::string& c2 = "", const std::string& c3 = "") const;
	};

	std::ostream& operator<<(std::ostream&, const StatusExecReport&);
}

#endif
