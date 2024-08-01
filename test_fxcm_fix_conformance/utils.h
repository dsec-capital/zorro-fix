#pragma once

#include "zorro_fxcm_fix_lib/fix_client.h"
#include "zorro_fxcm_fix_lib/fix_service.h"

#include "common/exec_report.h"
#include "common/blocking_queue.h"

using namespace common;
using namespace zorro;

namespace fxcm {

	BlockingTimeoutQueue<TopOfBook> top_of_book_queue;
	BlockingTimeoutQueue<ExecReport> exec_report_queue;
	BlockingTimeoutQueue<StatusExecReport> status_exec_report_queue;
	BlockingTimeoutQueue<ServiceMessage> service_message_queue;
	BlockingTimeoutQueue<FXCMPositionReport> position_report_queue;
	BlockingTimeoutQueue<FXCMPositionReports> position_snapshot_reports_queue;
	BlockingTimeoutQueue<FXCMCollateralReport> collateral_report_queue;
	BlockingTimeoutQueue<FXCMTradingSessionStatus> trading_session_status_queue;

	template<class R, class P>
	std::pair<std::vector<StatusExecReport>, bool> pop_status_exec_reports(const std::string& mass_status_req_id, const std::chrono::duration<R, P>& timeout) {
		log::debug<dl2, true>("pop_status_exec_reports: mass_status_req_id={}", mass_status_req_id);
		std::vector<StatusExecReport> reports;
		auto success = status_exec_report_queue.pop_until(
			[&](const StatusExecReport& report) {
				log::debug<dl2, true>("pop_status_exec_reports: got report={}", report.to_string());
				if (report.mass_status_req_id == mass_status_req_id) {
					reports.push_back(report);
				}
				auto done = report.ord_status == FIX::OrdStatus_REJECTED || report.last_rpt_requested;
				return done;
			}, timeout
		);
		log::debug<dl2, true>("pop_status_exec_reports: reports obtained={}", reports.size());
		return std::make_pair(reports, success);
	}

	template<class R, class P>
	std::optional<ExecReport> pop_exec_report_new(const std::string& expected_cl_ord_id, const std::chrono::duration<R, P>& timeout) {
		log::debug<dl2, true>("pop_exec_report_new: expected_cl_ord_id={}", expected_cl_ord_id);
		std::optional<ExecReport> target_report = std::optional<ExecReport>();
		auto success = exec_report_queue.pop_until(
			[&](const ExecReport& report) {
				auto is_new = report.exec_type == FIX::ExecType_NEW
					&& report.ord_status == FIX::OrdStatus_NEW
					&& report.cl_ord_id == expected_cl_ord_id;
				auto is_fill = report.exec_type == FIX::ExecType_TRADE
					&& report.ord_status == FIX::OrdStatus_FILLED
					&& report.cl_ord_id == expected_cl_ord_id;
				auto is_reject = report.exec_type == FIX::ExecType_REJECTED
					&& report.ord_status == FIX::OrdStatus_REJECTED
					&& report.cl_ord_id == expected_cl_ord_id;
				auto done = is_new || is_fill || is_reject;
				if (done) {
					target_report = std::optional<ExecReport>(report);
				}
				return done;
			}, timeout
		);
		log::debug<dl2, true>("pop_exec_report_new: report found={}", target_report.has_value());
		return target_report;
	}

	template<class R, class P>
	std::optional<ExecReport> pop_exec_report_fill(const std::string& expected_cl_ord_id, const std::chrono::duration<R, P>& timeout) {
		log::debug<dl2, true>("pop_exec_report_fill: expected_cl_ord_id={}", expected_cl_ord_id);
		std::optional<ExecReport> target_report = std::optional<ExecReport>();
		auto success = exec_report_queue.pop_until(
			[&](const ExecReport& report) {
				auto is_filled = report.exec_type == FIX::ExecType_TRADE
					&& report.ord_status == FIX::OrdStatus_FILLED
					&& report.cl_ord_id == expected_cl_ord_id;
				auto is_reject = report.exec_type == FIX::ExecType_REJECTED
					&& report.ord_status == FIX::OrdStatus_REJECTED
					&& report.cl_ord_id == expected_cl_ord_id;
				auto done = is_filled || is_reject;
				if (done) {
					target_report = std::optional<ExecReport>(report);
				}
				return done;
			}, timeout
		);
		log::debug<dl2, true>("pop_exec_report_fill: report found={}", target_report.has_value());
		return target_report;
	}

	template<class R, class P>
	std::optional<ExecReport> pop_exec_report_cancel(const std::string& expected_ord_id, const std::chrono::duration<R, P>& timeout) {
		log::debug<dl2, true>("pop_exec_report_cancel: expected_ord_id={}", expected_ord_id);
		std::optional<ExecReport> target_report = std::optional<ExecReport>();
		auto success = exec_report_queue.pop_until(
			[&](const ExecReport& report) {
				auto is_cancel = report.exec_type == FIX::ExecType_CANCELED
					&& report.ord_status == FIX::OrdStatus_CANCELED
					&& report.ord_id == expected_ord_id;
				auto is_reject = report.exec_type == FIX::ExecType_REJECTED
					&& report.ord_status == FIX::OrdStatus_REJECTED
					&& report.cl_ord_id == expected_ord_id;
				auto done = is_cancel || is_reject;
				if (done) {
					target_report = std::optional<ExecReport>(report);
				}
				return done;
			}, timeout
		);
		log::debug<dl2, true>("pop_exec_report_cancel: report found={}", target_report.has_value());
		return target_report;
	}

	template<class R, class P>
	std::optional<ExecReport> pop_exec_report_cancel_replace(const std::string& expected_ord_id, const std::chrono::duration<R, P>& timeout) {
		log::debug<dl2, true>("pop_exec_report_cancel_replace: expected_ord_id={}", expected_ord_id);
		std::optional<ExecReport> target_report = std::optional<ExecReport>();
		auto success = exec_report_queue.pop_until(
			[&](const ExecReport& report) {
				auto is_replace = report.exec_type == FIX::ExecType_REPLACED
					&& report.ord_status == FIX::OrdStatus_REPLACED
					&& report.ord_id == expected_ord_id;
				auto is_reject = report.exec_type == FIX::ExecType_REJECTED
					&& report.ord_status == FIX::OrdStatus_REJECTED
					&& report.cl_ord_id == expected_ord_id;
				auto done = is_replace || is_reject;
				if (done) {
					target_report = std::optional<ExecReport>(report);
				}
				return done;
			}, timeout
		);
		log::debug<dl2, true>("pop_exec_report_cancel_replace: report found={}", target_report.has_value());
		return target_report;
	}

	template<class R, class P>
	std::optional<ServiceMessage> pop_login_service_message(int expected_num_logins, const std::chrono::duration<R, P>& timeout) {
		log::debug<dl2, true>("pop_login_service_message: expected_num_logins={}", expected_num_logins);
		std::optional<ServiceMessage> message = std::optional<ServiceMessage>();
		auto success = service_message_queue.pop_until(
			[&](const ServiceMessage& msg) {
				bool done = false;
				const auto& msg_type = sm_get_or_else<std::string>(msg, SERVICE_MESSAGE_TYPE, "unknown");
				log::debug<dl2, true>("pop_login_service_message: service message type={}", msg_type);
				if (msg_type == SERVICE_MESSAGE_LOGON_STATUS) {
					const auto& ready = sm_get_or_else<bool>(msg, SERVICE_MESSAGE_LOGON_STATUS_READY, false);
					const auto& session_logins = sm_get_or_else<unsigned int>(msg, SERVICE_MESSAGE_LOGON_STATUS_SESSION_LOGINS, 0);
					log::debug<dl2, true>("pop_login_service_message: ready={} session_logins={}", ready, session_logins);
					done = ready && session_logins == expected_num_logins;
					if (done) {
						message = std::optional<ServiceMessage>(msg);
					}
				}
				return done;
			}, timeout
		);
		log::debug<dl2, true>("pop_login_service_message: message found={}", message.has_value());
		return message;
	}

	template<class R, class P>
	std::optional<ServiceMessage> pop_logout_service_message(const std::chrono::duration<R, P>& timeout) {
		log::debug<dl2, true>("pop_logout_service_message");
		std::optional<ServiceMessage> message = std::optional<ServiceMessage>();
		auto success = service_message_queue.pop_until(
			[&](const ServiceMessage& msg) {
				bool done = false;
				const auto& msg_type = sm_get_or_else<std::string>(msg, SERVICE_MESSAGE_TYPE, "unknown");
				log::debug<dl2, true>("pop_logout_service_message: service message type={}", msg_type);
				if (msg_type == SERVICE_MESSAGE_LOGON_STATUS) {
					const auto& session_logins = sm_get_or_else<unsigned int>(msg, SERVICE_MESSAGE_LOGON_STATUS_SESSION_LOGINS, 1); // make if non-zero to really wait for 0
					log::debug<dl2, true>("pop_logout_service_message: session_logins={}", session_logins);
					done = session_logins == 0;
					if (done) {
						message = std::optional<ServiceMessage>(msg);
					}
				}
				return done;
			}, timeout
		);
		log::debug<dl2, true>("pop_logout_service_message: message found={}", message.has_value());
		return message;
	}

	int pop_service_message() {
		auto n = service_message_queue.pop_all(
			[](const ServiceMessage& msg) {
				const auto& msg_type = sm_get_or_else<std::string>(msg, SERVICE_MESSAGE_TYPE, "unknown");
				if (msg_type == SERVICE_MESSAGE_REJECT) {
					const auto& reject_type = sm_get_or_else<std::string>(msg, SERVICE_MESSAGE_REJECT_TYPE, "unknown");
					const auto& reject_text = sm_get_or_else<std::string>(msg, SERVICE_MESSAGE_REJECT_TEXT, "unknown");
					log::error<true>("new rejection ServiceMessage obtained type={} text={}", reject_type, reject_text);
				}
			}
		);
		return n;
	}

	std::unique_ptr<FixService> create_fix_service(const std::string& settings_cfg_file) {
		auto fix_service = std::unique_ptr<FixService>(new FixService(
			settings_cfg_file,
			2,
			exec_report_queue,
			status_exec_report_queue,
			top_of_book_queue,
			service_message_queue,
			position_report_queue,
			position_snapshot_reports_queue,
			collateral_report_queue,
			trading_session_status_queue
		));
		return fix_service;
	}

}