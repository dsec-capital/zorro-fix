#ifdef _MSC_VER 
#pragma warning(disable : 4996 4244 4312 26444)
#endif 

#include "pch.h"

#include "zorro_fxcm_fix_lib/fix_client.h"
#include "zorro_fxcm_fix_lib/fix_service.h"
#include "zorro_fxcm_fix_plugin.h"

#include "common/market_data.h"
#include "common/exec_report.h"
#include "common/blocking_queue.h"
#include "common/json.h"
#include "common/time_utils.h"

#include "zorro_common/log.h"
#include "zorro_common/utils.h"
#include "zorro_common/enums.h"
#include "zorro_common/broker_commands.h"

#include "toml++/toml.h"
#include "nlohmann/json.h"
#include "httplib/httplib.h"
#include "magic_enum/magic_enum.hpp"

#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"

#define PLUGIN_VERSION 2
#define PLUGIN_NAME "ZorroFXCMFixPlugin"

#define BAR_DUMP_FILE_NAME "Log/bar_dump.csv"
#define BAR_DUMP_FILE_SEP ","
#define BAR_DUMP_FILE_PREC 5  // FX is point precision i.e. 10-5 = PIPS/10 

namespace zorro {

	using namespace common;
	using namespace std::chrono_literals;

	constexpr std::size_t dl0 = 0;
	constexpr std::size_t dl1 = 1;
	constexpr std::size_t dl2 = 2;
	constexpr std::size_t dl3 = 3;
	constexpr std::size_t dl4 = 4;

#if defined(WIN32) && !defined(WIN64)
	std::string settings_cfg_file = "Plugin/zorro_fxcm_fix_client.cfg";
	std::string plugin_cfg_file = "Plugin/zorro_fxcm_fix_plugin_config.toml";
#elif defined WIN64 
	std::string settings_cfg_file = "Plugin64/zorro_fxcm_fix_client.cfg";
	std::string plugin_cfg_file = "Plugin64/zorro_fxcm_fix_plugin_config.toml";
#else
	std::string settings_cfg_file = "Plugin/zorro_fxcm_fix_client.cfg";
	std::string plugin_cfg_file = "Plugin/zorro_fxcm_fix_plugin_config.toml";
#endif

	// http://localhost:8080/bars?symbol=AUD/USD
	auto rest_host_env = common::get_env("FXCM_MAKRET_DATA_SERVER_HOST").value_or("http://localhost");
	auto rest_port_env = common::get_env("FXCM_MAKRET_DATA_SERVER_PORT").value_or("8083");

	auto zorro_install_dir = common::get_env("ZorroInstallDir").value_or("C:/temp");
	auto zorro_log_dir = std::format("{}/Log", zorro_install_dir);

	toml::table tbl = toml::parse_file(plugin_cfg_file);
	auto fix_cfg = tbl["fix"];
	auto log_cfg = tbl["log"];
	auto zorro_cfg = tbl["zorro"];
	auto fxcm_cfg = tbl["fxcm"];

	auto rest_host = fxcm_cfg["market_data_server_host"].value<std::string>().value_or(rest_host_env);
	auto rest_port = std::atoi(fxcm_cfg["market_data_server_port"].value<std::string>().value_or(rest_port_env).c_str());

	httplib::Client rest_client(std::format("{}:{}", rest_host, rest_port));

	std::chrono::milliseconds fix_waiting_time = std::chrono::milliseconds(fix_cfg["waiting_time_ms"].value<int>().value_or(2000));
	std::chrono::milliseconds fix_login_waiting_time = std::chrono::milliseconds(fix_cfg["login_waiting_time_ms"].value<int>().value_or(4000));  
	std::chrono::milliseconds fix_exec_report_waiting_time = std::chrono::milliseconds(fix_cfg["exec_report_waiting_time_ms"].value<int>().value_or(1000));
	std::chrono::milliseconds fix_termination_waiting_time = std::chrono::milliseconds(fix_cfg["termination_waiting_time_ms"].value<int>().value_or(2000));  
	unsigned int fix_num_required_session_logins = fix_cfg["num_required_session_logins"].value<int>().value_or(2); // trading and market data

	auto dbg_level = log_cfg["spdlog_level"].value<std::string>().value_or("debug");
	auto spdlog_flush_interval = std::chrono::seconds(log_cfg["spdlog_flush_interval_s"].value<int>().value_or(2));
	auto spdlog_level = dbg_level == "debug" ? spdlog::level::debug : spdlog::level::info;
	auto spdlog_logging_verbosity = log_cfg["spdlog_logging_verbosity"].value<int>().value_or(dl1);
	
	namespace log {
		std::size_t logging_verbosity = spdlog_logging_verbosity;
	}

	int client_order_id = 0;
	int internal_order_id = zorro_cfg["internal_order_id_start"].value<int>().value_or(1000);;
	bool dump_bars_to_file = zorro_cfg["dump_bars_to_file"].value<bool>().value_or(true);

	// these come from Zorro when the plugin is started  
	std::string fxcm_login;
	std::string fxcm_password;
	std::string fxcm_account;
	std::string fxcm_connection = fxcm_cfg["connection"].value<std::string>().value_or("Demo");

	std::string asset;
	std::string currency;
	std::string position_symbol;
	std::string order_text;
	int multiplier = 1;
	int price_type = 0;
	int vol_type = 0;
	int leverage = 1;
	double lot_amount = 1;
	double limit = 0;
	char time_in_force = FIX::TimeInForce_GOOD_TILL_CANCEL;
	HWND window_handle;
	char current_position_id[1024];

	std::shared_ptr<spdlog::logger> spd_logger = nullptr;
	std::unique_ptr<FixService> fix_service = nullptr;
	BlockingTimeoutQueue<TopOfBook> top_of_book_queue;
	BlockingTimeoutQueue<ExecReport> exec_report_queue;
	BlockingTimeoutQueue<StatusExecReport> status_exec_report_queue;
	BlockingTimeoutQueue<ServiceMessage> service_message_queue;
	BlockingTimeoutQueue<FXCMPositionReport> position_report_queue;
	BlockingTimeoutQueue<FXCMPositionReports> position_snapshot_reports_queue;
	BlockingTimeoutQueue<FXCMCollateralReport> collateral_report_queue;
	BlockingTimeoutQueue<FXCMTradingSessionStatus> trading_session_status_queue;
	std::unordered_map<int, std::string> order_id_by_internal_order_id;
	std::unordered_map<std::string, TopOfBook> top_of_books;
	std::map<std::string, FXCMCollateralReport> collateral_reports;
	std::map<std::string, FXCMPositionReport> position_reports;
	FXCMTradingSessionStatus trading_session_status;
	OrderTracker order_tracker("unknown-account");
	std::vector<ServiceMessage> service_message_history;

	// to share with Zorro strategy scripts
	std::vector<CFXCMPositionReport> c_open_position_reports;
	std::vector<CFXCMPositionReport> c_closed_position_reports;
	std::vector<CStatusExecReport> c_order_mass_status_reports;
	std::vector<COrderReport> c_order_tracker_order_reports;
	std::vector<CNetPosition> c_order_tracker_net_positions;

	void show(const std::string& msg) {
		if (!BrokerError) return;
		auto tmsg = "[" + common::now_str() + "] " + msg + "\n";
		BrokerError(tmsg.c_str());
	}

	std::chrono::milliseconds elappsed(const std::chrono::system_clock::time_point& reference) {
		return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - reference);
	}

	std::string order_mapping_string() {
		std::string str = "OrderMapping[\n";
		for (const auto& [internal_id, order_id] : order_id_by_internal_order_id) {
			str += std::format("  internal_id={}, order_id={}\n", internal_id, order_id);
		}
		str += "]";
		return str;
	}

	int pop_exec_reports() {
		auto n = exec_report_queue.pop_all(
			[](const ExecReport& report) { 
				order_tracker.process(report); 
			}
		);
		return n;
	}

	int pop_position_reports() {
		auto n = position_report_queue.pop_all(
			[](const FXCMPositionReport& report) {
				position_reports[report.position_id] = report;
				log::debug<dl1, true>("pop_position_reports: report changed={}", report.to_string());
			}
		);
		return n;
	}

	int pop_collateral_reports() {
		auto n = collateral_report_queue.pop_all(
			[](const FXCMCollateralReport& report) {
				collateral_reports[report.account] = report;
				log::debug<dl1, true>("collateral_report_queue: report changed={}", report.to_string());
			}
		);
		return n;
	}

	int pop_top_of_books() {
		auto n = top_of_book_queue.pop_all(
			[](const TopOfBook& top) { 
				top_of_books.insert_or_assign(top.symbol, top); 
			}
		);
		return n;
	}

	template<class T>
	const std::string& get_position_id(const T& order_or_exec_report) {
		return order_or_exec_report.custom_1;
	}

	template<class T>
	std::optional<std::string> get_position_id_opt(const T& order_or_exec_report) {
		if (!order_or_exec_report.custom_1.empty())
			return std::make_optional(order_or_exec_report.custom_1);
		else 
			return std::optional<std::string>();
	}

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
				order_tracker.process(report);
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
				order_tracker.process(report);
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
				order_tracker.process(report);
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
				order_tracker.process(report);
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
				service_message_history.emplace_back(msg);
			}
		);
		return n;
	}

	double get_net_position_size(const std::string& symbol) {
		auto& np = order_tracker.get_net_position(symbol);
		return np.qty;
	}

	double get_avg_entry_price(const std::string& symbol) {
		auto& np = order_tracker.get_net_position(symbol);
		return np.avg_px;
	}

	int get_num_order_reports() {
		return order_tracker.num_order_reports();
	}

	std::string next_client_order_id() {
		++client_order_id;
		auto ts = common::get_current_system_clock().count() / 1000000;
		return std::format("coid_{}_{}", ts, client_order_id);
	}

	int next_internal_order_id() {
		++internal_order_id;
		return internal_order_id;
	}

	void plugin_callback(void* address) {
		show("plugin_callback called with address={}", address);
	}

	void trigger_price_quote_request() {
		PostMessage(window_handle, WM_APP + 1, 0, 0);
	}

	template<typename T>
	std::string format_fp(T value, int precision)
	{
		char format[16];
		char buffer[64];

		sprintf_s(format, 16, "%%.%if", precision);
		sprintf_s(buffer, 64, format, value);
		char* point = strchr(buffer, '.');
		if (point != 0)
			*point = '.';

		return std::string(buffer);
	}

	void get_c_position_reports(std::vector<CFXCMPositionReport>& c_pos_reports, bool is_open) {
		c_pos_reports.clear();
		for (const auto& kv : position_reports) {
			const auto& e = kv.second;
			if (e.is_open == is_open) {
				CFXCMPositionReport pos_report;
				strncpy_s(pos_report.account, e.account.c_str(), sizeof(CFXCMPositionReport::account));
				strncpy_s(pos_report.symbol, e.symbol.c_str(), sizeof(CFXCMPositionReport::symbol));
				strncpy_s(pos_report.currency, e.currency.c_str(), sizeof(CFXCMPositionReport::currency));
				strncpy_s(pos_report.position_id, e.position_id.c_str(), sizeof(CFXCMPositionReport::position_id));
				if (e.close_order_id.has_value())
					strncpy_s(pos_report.close_order_id, e.close_order_id.value().c_str(), sizeof(CFXCMPositionReport::close_order_id));
				else
					pos_report.close_order_id[0] = '\0';
				if (e.close_cl_ord_id.has_value())
					strncpy_s(pos_report.close_cl_ord_id, e.close_cl_ord_id.value().c_str(), sizeof(CFXCMPositionReport::close_cl_ord_id));
				else
					pos_report.close_cl_ord_id[0] = '\0';
				pos_report.settle_price = e.settle_price;
				pos_report.is_open = e.is_open ? 1 : 0;
				pos_report.interest = e.interest;
				pos_report.commission = e.commission;
				pos_report.open_time = common::nanos_to_date(e.open_time);
				pos_report.used_margin = e.used_margin.value_or(0);
				pos_report.close_pnl = e.close_pnl.value_or(0);
				pos_report.close_settle_price = e.close_settle_price.value_or(0);
				pos_report.close_time = common::nanos_to_date(e.close_time.value_or(0ns));
				c_pos_reports.emplace_back(pos_report);
			}
		}
	}

	void write_to_file(const std::string filename, const std::string& text, const std::string& header, std::ios_base::openmode mode = std::fstream::app) {
		bool skip_header = std::filesystem::exists(filename);

		std::fstream fs;
		fs.open(filename, std::fstream::out | mode);
		if (!fs.is_open())
		{
			log::error<true>("write_to_file: could not open file {}", filename);
			return;
		}

		if (!skip_header || mode == std::fstream::trunc) {
			fs << header << std::endl;
		}
		fs << text << std::endl;

		fs.close();
	}

	void write_bars(T6* ticks, int n_ticks, std::ios_base::openmode mode = std::fstream::app) {
		bool skip_header = std::filesystem::exists(BAR_DUMP_FILE_NAME);
		
		std::fstream fs;
		fs.open(BAR_DUMP_FILE_NAME, std::fstream::out | mode);
		if (!fs.is_open())
		{
			log::error<true>("write_bars: could not open file {}", BAR_DUMP_FILE_NAME);
			return;
		}

		log::debug<1, true>("write_bars[skip_header={}]: writing {} bars to {}", skip_header, n_ticks, BAR_DUMP_FILE_NAME);

		if (!skip_header) {
			fs << "bar_end" << BAR_DUMP_FILE_SEP
			   << "open" << BAR_DUMP_FILE_SEP
			   << "high" << BAR_DUMP_FILE_SEP
			   << "low" << BAR_DUMP_FILE_SEP
			   << "close" << BAR_DUMP_FILE_SEP
			   << "vol[volume]" << BAR_DUMP_FILE_SEP
			   << "val[spread]" 
			   << std::endl;
		}

		for (int i = 0; i < n_ticks; ++i, ++ticks) {
			fs << zorro_date_to_string(ticks->time) << BAR_DUMP_FILE_SEP
			   << format_fp(ticks->fOpen, BAR_DUMP_FILE_PREC) << BAR_DUMP_FILE_SEP
			   << format_fp(ticks->fHigh, BAR_DUMP_FILE_PREC) << BAR_DUMP_FILE_SEP
			   << format_fp(ticks->fLow, BAR_DUMP_FILE_PREC) << BAR_DUMP_FILE_SEP
			   << format_fp(ticks->fClose, BAR_DUMP_FILE_PREC) << BAR_DUMP_FILE_SEP
			   << format_fp(ticks->fVol, BAR_DUMP_FILE_PREC) << BAR_DUMP_FILE_SEP
			   << format_fp(ticks->fVal, BAR_DUMP_FILE_PREC) 
			   << std::endl;
		}

		fs.close();
	}

	void write_position_reports(const FXCMPositionReports& reports, const std::string& filename) {
		std::stringstream headers, rows;
		headers
			<< "account" << ", "
			<< "symbol" << ", "
			<< "currency" ", "
			<< "position_id" << ", "
			<< "settle_price" << ", "
			<< "is_open" << ", "
			<< "interest" << ", "
			<< "commission" << ", "
			<< "open_time" << ", "
			<< "used_margin" << ", "
			<< "close_pnl" << ", "
			<< "close_settle_price" << ", "
			<< "close_time" << ", "
			<< "close_order_id" << ", "
			<< "close_cl_ord_id";
		for (const auto& report : reports.reports) {
			rows
				<< report.account << ", "
				<< report.symbol << ", "
				<< report.currency << ", "
				<< report.position_id << ", "
				<< report.settle_price << ", "
				<< report.is_open << ", "
				<< report.interest << ", "
				<< report.commission << ", "
				<< common::to_string(report.open_time) << ", "
				<< (report.used_margin.has_value() ? std::to_string(report.used_margin.value()) : "N/A") << ", "
				<< (report.close_pnl.has_value() ? std::to_string(report.close_pnl.value()) : "N/A") << ", "
				<< (report.close_settle_price.has_value() ? std::to_string(report.close_settle_price.value()) : "N/A") << ", "
				<< (report.close_settle_price.has_value() ? common::to_string(report.close_time.value()) : "N/A") << ", "
				<< (report.close_order_id.has_value() ? report.close_order_id.value() : "N/A") << ", "
				<< (report.close_cl_ord_id.has_value() ? report.close_cl_ord_id.value() : "N/A")
				<< std::endl;
		}
		write_to_file(filename, rows.str(), headers.str(), std::fstream::trunc);
	}

	std::optional<std::pair<bool, std::string>> fxcm_proxy_server_status() {
		auto request = "/status";
		auto res = rest_client.Get(request);
		if (res->status == httplib::StatusCode::OK_200) {
			auto j = json::parse(res->body);
			auto ready = j["ready"].get<bool>();
			auto started = j["started"].get<std::string>();
			return std::make_optional(std::make_pair(ready, started));
		}
		else {
			return std::optional<std::pair<bool, std::string>>();
		}
	}

	// get historical data - note time is in UTC
	// http://localhost:8080/bars?symbol=EUR/USD&from=2024-03-30 12:00:00&to=2024-03-30 16:00:00
	int get_historical_bars(const char* Asset, const std::string& timeframe, DATE from, DATE to, std::vector<BidAskBar<DATE>>& bars) {
		auto from_str = zorro_date_to_string(from);
		auto to_str = zorro_date_to_string(to);
		auto request = std::format("/bars?symbol={}&timeframe={}&from={}&to={}", Asset, timeframe, from_str, to_str);
		auto res = rest_client.Get(request);
		if (res->status == httplib::StatusCode::OK_200) {
			auto j = json::parse(res->body);
			from_json(j, bars);

			log::debug<4, true>(
				"get_historical_bars: Asset={} from={} to={} num bars={}",
				Asset, from_str, to_str, bars.size()
			);
		}

		return res->status;
	}

	// get historical data - note time is in UTC
	// http://localhost:8080/ticks?symbol=EUR/USD&from=2024-06-27 00:00:00 
	// http://localhost:8080/ticks?symbol=EUR/USD&count=1000 
	int get_historical_ticks(const char* Asset, DATE from, DATE to, int count, std::vector<Quote<DATE>>& quotes) {
		std::stringstream ss;
		ss << "/ticks?symbol=" << Asset;
		if (from > 0) {
			auto from_str = zorro_date_to_string(from);
			ss << "&from=" << from_str;
		}
		if (to > 0) {
			auto to_str = zorro_date_to_string(to);
			ss << "&to=" << to_str;
		}
		if (count > 0) {
			ss << "&count=" << count;
		}
		auto res = rest_client.Get(ss.str());
		if (res->status == httplib::StatusCode::OK_200) {
			auto j = json::parse(res->body);
			from_json(j, quotes);

			log::debug<4, true>(
				"get_historical_bars: Asset={} from={} to={} count={} num ticks={}",
				Asset, from, to, count, quotes.size()
			);
		}

		return res->status;
	}

	DLLFUNC_C void BrokerHTTP(FARPROC fp_send, FARPROC fp_status, FARPROC fp_result, FARPROC fp_free) {
		(FARPROC&)http_send = fp_send;
		(FARPROC&)http_status = fp_status;
		(FARPROC&)http_result = fp_result;
		(FARPROC&)http_free = fp_free;
	}

	/*
	 * BrokerOpen
	 *
	 * First function to be called to start the plugin.
	 */
	DLLFUNC int BrokerOpen(char* Name, FARPROC fpError, FARPROC fpProgress) {
		if (Name) {
			strcpy_s(Name, 32, PLUGIN_NAME);
		}

		(FARPROC&)BrokerError = fpError;
		(FARPROC&)BrokerProgress = fpProgress;

		std::string cwd = std::filesystem::current_path().string();
		log::info<1, true>("BrokerOpen: FXCM FIX plugin opened in {}", cwd);

		try
		{
			if (!spd_logger) {
				auto postfix = timestamp_postfix();
				auto logger_name = std::format("{}/zorro_fxcm_fix_plugin_spdlog_{}.log", zorro_log_dir, postfix);
				spd_logger = create_file_logger(logger_name, spdlog_level, spdlog_flush_interval);
			}
		}
		catch (const spdlog::spdlog_ex& ex)
		{
			log::error<true>("BrokerOpen: FIX plugin failed to init log: {}", ex.what());
		}

		return PLUGIN_VERSION;
	}

	/*
	 * BrokerLogin
	 * 
	 * Login or logout to the broker's API server; called in [Trade] mode or for downloading historical price data. 
	 * If the connection to the server was lost, f.i. due to to Internet problems or server weekend maintenance, 
	 * Zorro calls this function repeatedly in regular intervals until it is logged in again. Make sure that the 
	 * function internally detects the login state and returns safely when the user was still logged in.
	 *
	 * Parameters:
	 *	User		Input, User name for logging in, or NULL for logging out.
	 *	Pwd			Input, Password for logging in.
	 *	Type		Input, account type for logging in; either "Real" or "Demo".
	 *	Accounts	Input / optional output, char[1024] array, intially filled with the account id from the account list. 
	 *              Can be filled with all user's account numbers as subsequent zero-terminated strings, ending with "" for 
	 *              the last string. When a list is returned, the first account number is used by Zorro for subsequent BrokerAccount calls.
	 * 
	 * Returns:
	 *	Login state: 1 when logged in, 0 otherwise.
	 */
	DLLFUNC int BrokerLogin(char* user, char* password, char* type, char* account) {
		try {
			if (fix_service == nullptr) {
				log::debug<dl1, true>("BrokerLogin: FIX thread createing...");
				fix_service = std::unique_ptr<FixService>(new FixService(
					settings_cfg_file,
					fix_num_required_session_logins,
					exec_report_queue,
					status_exec_report_queue,
					top_of_book_queue,
					service_message_queue,
					position_report_queue,
					position_snapshot_reports_queue,
					collateral_report_queue,
					trading_session_status_queue
				));
				log::debug<dl1, true>("BrokerLogin: FIX thread created");
			}

			if (user) {
				auto fxcm_proxy_ready = fxcm_proxy_server_status();
				if (fxcm_proxy_ready.has_value() && fxcm_proxy_ready.value().first) {
					log::info<dl1, true>(
						"BrokerLogin: FXCM proxy server ready started at={}", fxcm_proxy_ready.value().second
					);
				}
				else {
					log::error<true>("BrokerLogin: FXCM proxy server not started or not properly logged in");
					return 0; // not ready
				}

				fxcm_login = std::string(user);
				fxcm_password = std::string(password);
				fxcm_account = std::string(account);
				order_tracker.set_account(fxcm_account);

				log::info<dl1, true>(
					"BrokerLogin: FXCM credentials login={} password={} connection={} account={}", 
					fxcm_login, fxcm_password, fxcm_connection, fxcm_account
				);

				auto start = std::chrono::system_clock::now();

				log::debug<dl1, true>("BrokerLogin: FIX service starting...");
				fix_service->start();
				log::debug<dl1, true>("BrokerLogin: FIX service running");

				log::debug<1, true>("BrokerLogin: waiting for FIX login...");
				auto fix_login_msg = pop_login_service_message(fix_num_required_session_logins, fix_login_waiting_time);

				if (!fix_login_msg) {
					fix_service->cancel();
					throw std::runtime_error(std::format("FIX login timeout after {}", elappsed(start)));
				}
				else {
					log::info<1, true>("BrokerLogin: FIX login after {}", elappsed(start));
				}
				
				// initial requests 
				fix_service->client().trading_session_status_request();
				fix_service->client().collateral_inquiry(FIX::SubscriptionRequestType_SNAPSHOT_AND_UPDATES);   

				// position subscriptions
				fix_service->client().request_for_positions(fxcm_account, 0, FIX::SubscriptionRequestType_SNAPSHOT_AND_UPDATES);
				fix_service->client().request_for_positions(fxcm_account, 1, FIX::SubscriptionRequestType_SNAPSHOT_AND_UPDATES);

				bool success = trading_session_status_queue.pop(trading_session_status, fix_waiting_time);
				if (!success) {
					fix_service->cancel();
					throw std::runtime_error("failed to obtain trading session status");
				}
				else {
					log::info<1, true>(
						"BrokerLogin: trading session status obtained server_timezone={} num securities={}",
						trading_session_status.server_timezone_name, trading_session_status.security_informations.size()
					);
				}

				FXCMCollateralReport collateral_report;
				success = collateral_report_queue.pop(collateral_report, fix_waiting_time);
				if (!success) {
					fix_service->cancel();
					throw std::runtime_error("failed to obtain initial collateral report");
				}
				else {
					collateral_reports.emplace(collateral_report.account, collateral_report);
					log::info<dl1, true>("BrokerLogin: collateral report={}", collateral_report.to_string());
				}

				FXCMPositionReports pos_reports;
				success = position_snapshot_reports_queue.pop(pos_reports, fix_waiting_time);
				if (!success) {
					fix_service->cancel();
					throw std::runtime_error("failed to open/closed position report");
				}
				else {
					for (const auto& report : pos_reports.reports) {
						position_reports.emplace(report.position_id, report);
						log::info<dl1, true>("BrokerLogin: position report={}", report.to_string());
					}
				}

				success = position_snapshot_reports_queue.pop(pos_reports, fix_waiting_time);
				if (!success) {
					fix_service->cancel();
					throw std::runtime_error("failed to open/closed position report");
				}
				else {
					for (const auto& report : pos_reports.reports) {
						position_reports.emplace(report.position_id, report);
						log::info<dl1, true>("BrokerLogin: position report={}", report.to_string());
					}
				}

				return BrokerLoginStatus::LoggedIn;
			}
			else {
				log::debug<dl1, true>("BrokerLogin: waiting {} before stopping FIX service...", fix_termination_waiting_time);
				
				std::this_thread::sleep_for(fix_termination_waiting_time);
				auto n = pop_exec_reports();
				
				log::debug<dl1, true>(
					"BrokerLogin: processed {} final exec reports\n{}\n{}\nFIX service stopping...", 
					n, order_tracker.to_string("position_id"), order_mapping_string()
				);

				fix_service->cancel();
				log::debug<dl1, true>("BrokerLogin: FIX service stopped");

				return BrokerLoginStatus::LoggedOut; 
			}
		}
		catch (std::exception& e) {
			log::error<true>("BrokerLogin: exception creating/starting FIX service {}", e.what());
			return BrokerLoginStatus::LoggedOut;
		}
		catch (...) {
			log::error<true>("BrokerLogin: unknown exception");
			return BrokerLoginStatus::LoggedOut;
		}
	}

	DLLFUNC int BrokerTime(DATE* pTimeGMT) 
	{
		if (!fix_service) {
			return ExchangeStatus::Unavailable;
		}

		const auto time = get_current_system_clock();

		auto n = pop_exec_reports();

		if (n > 0) {
			log::debug<dl1, true>(
				"BrokerTime {} exec reports processed={}\n{}\n{}",
				common::to_string(time), n, order_tracker.to_string("position_id"), order_mapping_string()
			);
		}

		pop_top_of_books();
		pop_position_reports();
		pop_collateral_reports();
		pop_service_message();

		return ExchangeStatus::Open;
	}
	
	/*
	 * BrokerAccount
	 *
	 * Optional function. Is called by Zorro in regular intervals and returns the current account status. 
	 * Is also used to change the account if multiple accounts are supported. If the BrokerAccount function 
	 * is not provided, f.i. when using a FIX API, Zorro estimates balance, equity, and margin from initial 
	 * values and trade results.
	 * 
	 * Parameters:
	 *	Account		Input, new account name or number, or NULL for using the current account.
	 *	pBalance	Optional output, current balance on the account.
	 *	pTradeVal	Optional output, current value of all open trades; the difference between account 
	 *              equity and returned balance value. If not available, Zorro estimes the equity from balance 
	 *              and value of all open trades. If no balance was returned, the account equity can be returned in pTradeVal.
	 *	pMarginVal	Optional output, current total margin bound by all open trades. If not
	 * 
	 * Returns:
	 *	1 when the account is available and the returned data is valid
	 *  0 when a wrong account was given or the account was not found.
	 */
	DLLFUNC int BrokerAccount(char* account, double* balance, double* pdTradeVal, double* margin_val) 
	{
		log::debug<dl2, true>("BrokerAccount: requesting account details for account {}", account);

		auto it = collateral_reports.find(account);

		if (it != collateral_reports.end()) {
			*balance = it->second.balance;
			*margin_val = it->second.margin;
			return 1;
		}
		else {
			fix_service->client().collateral_inquiry();

			FXCMCollateralReport collateral_report;
			auto success = collateral_report_queue.pop(collateral_report, fix_waiting_time);
			if (!success) {
				log::error<true>("failed to obtain collateral report for account {}", account);
				return 0;
			} 
			else {
				collateral_reports.emplace(collateral_report.account, collateral_report);

				if (collateral_report.account != account) {
					log::error<true>(
						"BrokerAccount error: wrong account, expected account {} in {}", 
						 account, collateral_report.to_string());
					return 0;
				}
				else {
					log::debug<dl1, true>("BrokerAccount: collateral report={}", collateral_report.to_string());
					*balance = collateral_report.balance;
					*margin_val = collateral_report.margin;
					return 1;
				}
			}
		}
	}

	/*
	 * BrokerAsset
	 * 
	 * Subscribes an asset, and/or returns information about it. Zorro subscribes all used assets at the begin 
	 * of the trading session. Price and spread for all assets are retrieved in TickTime intervals or when 
	 * BrokerProgress was preciously called by the plugin. Other asset data is retrieved once per bar.
	 * 
	 * Parameters:
     *	Asset		Input, asset symbol for live prices (see Symbols).
     *	pPrice		Optional output, current ask price of the asset, or NULL for subscribing the asset. An asset must be 
	 *              subscribed before any information about it can be retrieved.
     *	pSpread		Optional output, the current difference of ask and bid price of the asset.
     *	pVolume		Optional output, a parameter reflecting the current supply and demand of the asset. 
	 *              Such as trade volume per minute, accumulated daily trade volume, open interest, ask/bid volume, 
	 *              or tick frequency. If a value is returned, it should be consistent with the fVol content of the 
	 *              T6 struct in BrokerHistory2 (see below)..
     *	pPip		Optional output, size of 1 PIP, f.i. 0.0001 for EUR/USD.
     *	pPipCost	Optional output, cost of 1 PIP profit or loss per lot, in units of the account currency. If not directly supported, 
	 *			    calculate it as decribed under asset list.
     *	pLotAmount	Optional output, minimum order size, i.e. number of contracts for 1 lot of the asset. For currencies it's usually 
	 *			    10000 with mini lot accounts and 1000 with micro lot accounts. For CFDs it's usually 1, but can also be a fraction 
	 *				of a contract, like 0.1.
     *	pMargin		Optional output, either initial margin cost for buying 1 lot of the asset in units of the account currency 
	 *			    or the leverage of the asset when negative (f.i. -50 for 50:1 leverage).
     *	pRollLong	Optional output, rollover fee for long trades, i.e. interest that is added to or subtracted from the account 
	 *		        for holding positions overnight. The returned value is the daily fee per 10,000 contracts for currencies, 
	 *			    and per contract for all other assets, in units of the account currency.
     *	pRollShort	Optional output, rollover fee for short trades.
     *	pCommission	Optional output, roundturn commission per 10,000 contracts for currencies, per contract for all other assets, 
	 *			    in units of the account currency.
	 * 
     * Returns:
     *	1 when the asset is available and the returned data is valid, 
	 *  0 otherwise. An asset that returns 0 after subscription will trigger Error 053, and its trading will be disabled.
	 * 
	 * Remarks:
	 *	If parameters are not supplied by the broker API, they can be left unchanged. Zorro will then use default values from 
	 *	the asset list. Only price and spread must always be returned when the pPrice and pSpread parameters are nonzero.
	 * 
	 *  For receiving streaming price data, get the Zorro window handle from the SET_HWND command for sending messages to a 
	 *	Zorro window. The message WM_APP+1 triggers a price quote request.
	 * 
	 *	Dependent on the broker API, some asset parameters might require unit conversions because lots and pips can have 
	 *  special meanings. In most APIs, such as the FXCM API, the parameters are directly available. A more complicated example 
	 *	is the MT4™ API where the parameters must be corrected by the different scales of "Lot" and "Point"
	 * 
	 * Asset parameters can be different dependent on when they are requested. For instance, some brokers charge a three 
	 * times higher rollover fee on Wednesday for compensating the weekend. Spreads are usually higher when the market is 
	 * closed or illiquid.
	 * 
	 * If the broker API can not subscribe an asset, it must be manually subscribed in the broker platform or website.
	 */
	DLLFUNC int BrokerAsset(char* asset, double* price, double* spread,
		double* volume, double* pip, double* pip_cost, double* min_amount,
		double* margin_cost, double* roll_long, double* roll_short) 
	{
		try {
			auto& client = fix_service->client();

			// subscribe to Asset market data
			if (!price) {  
				FIX::Symbol symbol(asset);
				client.subscribe_market_data(symbol, false);

				log::info<dl1, true>("BrokerAsset: subscription request sent for symbol {}", asset);

				TopOfBook top;
				bool success = top_of_book_queue.pop(top, fix_waiting_time);
				if (!success) {
					throw std::runtime_error(std::format("failed to get snapshot in {}ms", fix_waiting_time));
				}

				log::info<dl1, true>("BrokerAsset: successfully subscribed symbol {} for market data", asset);

				auto it = trading_session_status.security_informations.find(asset);
				
				if (it == trading_session_status.security_informations.end()) {
					log::error<true>("BrokerAsset: could not find security information for asset {}", asset);
				}
				
				log::info<1, true>("BrokerAsset: found security information={}", it->second.to_string());

				return 1;
			}
			else {
				pop_top_of_books();
				auto it = top_of_books.find(asset);
				if (it != top_of_books.end()) {
					const auto& top = it->second;
					if (price) *price = top.mid();
					if (spread) *spread = top.spread();

					log::debug<dl4, true>(
						"BrokerAsset: top bid={:.5f} ask={:.5f} @ {}", 
						top.bid_price, top.ask_price, common::to_string(top.timestamp)
					);
				}
				return 1;
			}
		}
		catch (const std::exception& e) {
			log::error<true>("BrokerAsset: exception {}", e.what());
			return 0;
		}
		catch (...) {
			log::error<true>("BrokerAsset: undetermined exception");
			return 0;
		}
	}

	// https://fxcm-api.readthedocs.io/en/latest/restapi.html#candle-request-limit
	std::string get_timeframe(int n_tick_minutes) {
		switch (n_tick_minutes) {
		case 1:
			return "m1";
		case 5:
			return "m5";
		case 15:
			return "m15";
		case 30:
			return "m30";
		case 60:
			return "h1";
		case 120:
			return "h2";
		case 180:
			return "h8";
		case 240:
			return "h4";
		case 360:
			return "h6";
		case 480:
			return "h8";
		case 1440:
			return "D1";
		default:
			return "";
		}
	}

	/*
	 * BrokerHistory2 
	 * 
	 * Returns the price history of an asset. Called by Zorro's assetHistory function and at the begin of a trading session for filling the lookback period.
	 *
	 * Parameters:
	 *	Asset			Input, asset symbol for historical prices (see Symbols).
	 *	tStart			Input, UTC start date/time of the price history (see BrokerTime about the DATE format). This has only the meaning of a 
	 *                  seek-no-further date; the relevant date for the begin of the history is tEnd.
	 *	tEnd			Input, UTC end date/time of the price history. If the price history is not available in UTC time, but in the brokers's local time, 
	 *                  the plugin must convert it to UTC.
	 *	nTickMinutes	Input, time period of a tick in minutes. Usual values are 0 for single price ticks (T1 data; optional), 1 for one-minute (M1) historical data, 
	 *                  or a larger value for more quickly filling the LookBack period before starting a strategy.
	 *	nTicks			Input, maximum number of ticks to be filled; must not exceed the number returned by brokerCommand(GET_MAXTICKS,0), or 300 otherwise.
	 *	ticks			Output, array of T6 structs (defined in include\trading.h) to be filled with the ask prices, close time, and additional data if available, 
	 *                  such as historical spread and volume. See history for details. The ticks array is filled in reverse order from tEnd on until either the tick 
	 *                  time reaches tStart or the number of ticks reaches nTicks, whichever happens first. The most recent tick, closest to tEnd, is at
	 *                  the start of the array. In the case of T1 data, or when only a single price is available, all prices in a TICK struct can be set to the same value.
	 * 
	 * Important Note:
	 *	The timestamp of a bar in ForexConnect is the beginning of the bar period. 
	 *  As for Zorro the T6 format is defined 
	 * 
     *		typedef struct T6
	 *		{
	 *		  DATE  time; // UTC timestamp of the close, DATE format
	 *		  float fHigh,fLow;	
	 *		  float fOpen,fClose;	
	 * 		  float fVal,fVol; // additional data, ask-bid spread, volume etc.
	 *		} T6;
	 * 
	 *	Hence the timestamp should represent the end of the bar period.
	 * 
	 * From Zorro documentation:
	 *	If not stated otherwise, all dates and times reflect the time stamp of the Close price at the end of the bar. 
	 *	Dates and times are normally UTC. Timestamps in historical data are supposed to be either UTC, or to be a date
	 *	only with no additional time part, as for daily bars. 
	 *  It is possible to use historical data with local timestamps in the backtest, but this must be considered in the 
	 *  script since the date and time functions will then return the local time instead of UTC, and time zone functions
	 *	cannot be used.
	 */
	DLLFUNC int BrokerHistory2(char* asset, DATE t_start, DATE t_end, int n_tick_minutes, int n_ticks, T6* ticks) {
		if (n_tick_minutes > 0) {
			auto bar_seconds = n_tick_minutes * 60;
			auto t_bar = bar_seconds / SECONDS_PER_DAY;
			auto t_start2 = t_end - n_ticks * t_bar;
			auto from = zorro_date_to_string(t_start2);
			auto to = zorro_date_to_string(t_end);
			auto ticks_start = ticks;

			std::string timeframe = get_timeframe(n_tick_minutes);
			if (timeframe == "") {
				log::error<true>("BrokerHistory2 {}: invalid time frame for n_tick_minutes={}", asset, n_tick_minutes);
				return 0;
			}

			auto now = common::get_current_system_clock();
			auto now_zorro = zorro::convert_time_chrono(now);
			auto now_str = zorro_date_to_string(now_zorro);

			log::debug<dl2, true>(
				"BrokerHistory2 {}: requesting {} ticks bar period {} minutes from {}[{}] to {}[{}] at {}",
				asset, n_ticks, n_tick_minutes, from, t_start2, to, t_end, now_str
			);

			log::debug<dl2, true>("BrokerHistory2: t_start={}, t_start2={}, t_end={}, now_zorro={}", t_start, t_start2, t_end, now_zorro);

			std::vector<BidAskBar<DATE>> bars;
			auto status = get_historical_bars(asset, timeframe, t_start2, t_end, bars);
			auto success = status == httplib::StatusCode::OK_200;

			if (!success) {
				log::error<true>(
					"BrokerHistory2: get_historical_prices failed status={} Asset={} timeframe={} from={} to={}",
					status, asset, timeframe, t_start2, t_end);
				return 0;
			}

			int count = 0;
			for (auto it = bars.rbegin(); it != bars.rend() && count <= n_ticks; ++it) {
				const auto& bar = *it;
				DATE start = bar.timestamp;
				DATE end = start + t_bar;
				auto time = convert_time_chrono(bar.timestamp);

				if (end > t_end || end < t_start) {
					log::debug<dl2, true>(
						"BrokerHistory2 {}: skipping timestamp {} as it is out of bound t_start={} t_end={}",
						asset, zorro_date_to_string(end), zorro_date_to_string(t_start), zorro_date_to_string(t_end)
					);

					continue;
				}

				log::debug<5, false>(
					"[{}] from={} to={} open={:.5f} high={:.5f} low={:.5f} close={:.5f}",
					count, zorro_date_to_string(start), zorro_date_to_string(end), bar.ask_open, bar.ask_high, bar.ask_low, bar.ask_close
				);

				ticks->fOpen = static_cast<float>(bar.ask_open);
				ticks->fClose = static_cast<float>(bar.ask_close);
				ticks->fHigh = static_cast<float>(bar.ask_high);
				ticks->fLow = static_cast<float>(bar.ask_low);
				ticks->fVol = static_cast<float>(bar.volume);
				ticks->fVal = static_cast<float>(bar.ask_close - bar.bid_close);
				ticks->time = end;
				++ticks;
				++count;
			}

			if (dump_bars_to_file) {
				write_bars(ticks_start, count);
				write_to_file("Log/broker_hist.csv",
					std::format(
						"{}, {}, {}, {}, {}, {}, {}, {}",
						asset, timeframe, zorro_date_to_string(t_start2), t_start2, zorro_date_to_string(t_end), t_end, n_ticks, count
					),
					"asset, timeframe, t_start2, t_start2[DATE], t_end, t_end[DATE], n_ticks, count"
				);
			}

			return count;
		}
		else {
			log::error<true>("BrokerHistory2: called with n_tick_minutes=0 but tick data aka quotes not yet integrated");
			return 0;
		}
	}

	/*
	 * BrokerBuy2
	 * 
	 * Sends an order to open a long or short position, either at market, or at a price limit. Also used for NFA compliant accounts to close a position by 
	 * opening a new position in the opposite direction. The order type (FOK, IOC, GTC) can be set with SET_ORDERTYPE before. Orders other than GTC are 
	 * cancelled when they are not completely filled within the wait time (usually 30 seconds).
	 * 
	 * Parameters:
     *	Asset		Input, asset symbol for trading (see Symbols).
	 *	Amount		Input, number of contracts, positive for a long trade and negative for a short trade. For currencies or CFDs, the number of contracts is the number of Lots multiplied with the LotAmount. 
	 *				If LotAmount is < 1 (f.i. for a CFD or a fractional share with 0.1 contracts lot size), the number of lots is given here instead of the number of contracts.
	 *	StopDist	Optional input, 'safety net' stop loss distance to the opening price when StopFactor was set, or 0 for no stop, or -1 for indicating that this function was called for closing a position. 
	 *				This is not the real stop loss, which is handled by Zorro. Can be ignored if the API is NFA compliant and does not support a stop loss in a buy/sell order.
	 *	Limit		Optional input, fill price for limit orders, set up by OrderLimit, or 0 for market orders. Can be ignored if limit orders are not supported by the API.
	 *	pPrice		Optional output, the average fill price if the position was partially or fully filled.
	 *	pFill		Optional output, the fill amount, always positive.
	 * 
	 * Returns see BrokerBuyStatus
	 *  if successful 
	 *    Trade or order id 
	 *  or on error
	 *	 0 when the order was rejected or a FOK or IOC order was unfilled within the wait time (adjustable with the SET_WAIT command). The order must then be cancelled by the plugin.
	 *	   Trade or order ID number when the order was successfully placed. If the broker API does not provide trade or order IDs, the plugin should generate a unique 6-digit number, f.i. from a counter, and return it as a trade ID.
	 *	-1 when the trade or order identifier is a UUID that is then retrieved with the GET_UUID command.
	 *	-2 when the broker API did not respond at all within the wait time. The plugin must then attempt to cancel the order. Zorro will display a "possible orphan" warning.
	 *	-3 when the order was accepted, but got no ID yet. The ID is then taken from the next subsequent BrokerBuy call that returned a valid ID. This is used for combo positions that require several orders.
	 */
	DLLFUNC_C int BrokerBuy2(char* asset, int amount, double stop, double limit, double* av_fill_price, int* fill_qty) {
		log::debug<dl1, true>("BrokerBuy2 {}: amount={}, lot_amount={}, limit={}, stop={}", asset, amount, lot_amount, limit, stop);

		auto symbol = FIX::Symbol(asset);
		FIX::OrdType ord_type;
		if (limit && !stop) {
			ord_type = FIX::OrdType(FIX::OrdType_LIMIT);
		} 
		else if (limit && stop && stop != -1) {
			ord_type = FIX::OrdType(FIX::OrdType_STOP_LIMIT);
		}
		else if (!limit && stop && stop != -1) {
			ord_type = FIX::OrdType(FIX::OrdType_STOP);
		}
		else {
			ord_type = FIX::OrdType(FIX::OrdType_MARKET);
		}

		// currently FIX::OrdType_STOP_LIMIT and FIX::OrdType_STOP not supported!
		if (ord_type == FIX::OrdType_STOP || ord_type == FIX::OrdType_STOP_LIMIT) {
			log::error<true>("BrokerBuy2 {}: FIX::OrdType_STOP and FIX::OrdType_STOP_LIMIT not yet supported", asset);
			return BrokerError::OrderRejectedOrTimeout;
		}

		if (amount == 0 && stop == -1) {
			const auto& net_pos = order_tracker.get_net_position(asset);
			amount = -static_cast<int>(net_pos.qty);
			ord_type = FIX::OrdType(FIX::OrdType_MARKET);
			stop = 0;
			log::debug<dl1, true>("BrokerBuy2 {}: setting amount={} from open net position net_pos.qty={} on stop == -1", asset, amount, net_pos.qty);
		}

		if (stop == -1) {
			log::debug<dl1, true>("BrokerBuy2 {}: close out order with amount={}", asset, amount);
			stop = 0;
		}

		auto cl_ord_id = FIX::ClOrdID(next_client_order_id());
		auto side = amount > 0 ? FIX::Side(FIX::Side_BUY) : FIX::Side(FIX::Side_SELL);
		auto qty = FIX::OrderQty(std::abs(amount));
		auto limit_price = FIX::Price(limit);
		auto stop_price = FIX::StopPx(stop);

		auto msg = fix_service->client().new_order_single(
			symbol, cl_ord_id, side, ord_type, time_in_force, qty, limit_price, stop_price
		);

		if (!msg.has_value()) {
			log::debug<dl1, true>("BrokerBuy2 {}: failed to create NewOrderSingle", asset);
			return BrokerError::OrderRejectedOrTimeout;
		}

		log::debug<dl2, true>("BrokerBuy2 {}: NewOrderSingle {}", asset, fix_string(msg.value()));

		if (ord_type.getValue() == FIX::OrdType_LIMIT || ord_type.getValue() == FIX::OrdType_MARKET) {

			// wait for a order status exec report NEW for limit orders and a FILL for market orders
			auto report = ord_type == FIX::OrdType_LIMIT 
				? pop_exec_report_new(cl_ord_id.getString(), fix_exec_report_waiting_time)
			    : pop_exec_report_fill(cl_ord_id.getString(), fix_exec_report_waiting_time);

			if (!report.has_value()) {
				log::error<true>(
					"BrokerBuy2 {}: timeout after {} while waiting for FIX exec report on new market order",
					asset, fix_exec_report_waiting_time
				);
				return BrokerError::OrderRejectedOrTimeout;
			}

			if (report.value().exec_type == FIX::ExecType_REJECTED && report.value().ord_status == FIX::OrdStatus_REJECTED) {
				log::error<true>(
					"BrokerBuy2 {}: rejected order cl_ord_id{} reason={}", asset, report.value().cl_ord_id, report.value().text
				);
				return BrokerError::OrderRejectedOrTimeout;
			}

			*av_fill_price = report.value().avg_px;
			*fill_qty = static_cast<int>(report.value().cum_qty);

			log::debug<dl1, true>(
				"BrokerBuy2 {}: {} order fill update av_fill_price={}, fill_qty={}",
				asset, ord_type.getValue() == FIX::OrdType_MARKET ? "market" : "limit",
				report.value().avg_px, static_cast<int>(report.value().cum_qty)
			);

			auto interal_id = next_internal_order_id();
			order_id_by_internal_order_id.emplace(interal_id, report.value().ord_id);
			return interal_id;
		}
		else {
			return BrokerError::OrderRejectedOrTimeout;
		}		
	}

	/* BrokerTrade
	 *
	 * Optional function that returns the order fill state (for brokers that support only orders and positions) or the trade 
	 * state (for brokers that support individual trades). Called by Zorro for any open trade when the price moved by more than 1 pip, 
	 * or when contractUpdate or contractPrice is called for an option or future trade.
	 * 
	 * Parameters:
     *	nTradeID	Input, order/trade ID as returned by BrokerBuy, or -1 when the trade UUID was set before with a SET_UUID command.
	 *	pOpen		Optional output, the average fill price if the trade was partially or fully filled. If not available by the API, 
	 *				Zorro will estimate the values based on last price and asset parameters.
	 *	pClose		Optional output, current bid or ask close price of the trade. If not available, Zorro will estimale the value based 
	 *				on current ask price and ask-bid spread.
	 *	pCost		Optional output, total rollover fee (swap fee) of the trade so far. If not available, Zorro will estimate the swap from the asset parameters.
	 *	pProfit		Optional output, current profit or loss of the trade in account currency units, without rollover and commission. If not available, 
	 *				Zorro will estimate the profit from the difference of current price and fill price.
	 * 
	 * Returns:
	 *	  - Number of contracts or lots (as in BrokerBuy2) currently filled for the trade.
	 *	  - -1 when the trade was completely closed.
	 *	  - NAY (defined in trading.h) when the order or trade state was unavailable. Zorro will then assume that the order was completely filled, and keep the trade open.
	 *	  - NAY-1 when the order was cancelled or removed by the broker. Zorro will then cancel the trade and book the profit or loss based on the current price and the 
	 *		last fill amount.
	 */
	DLLFUNC int BrokerTrade(int trade_id, double* open, double* close, double* cost, double* profit) {
		// pop all the exec reports and markeet data from the queue to have up to date information
		pop_top_of_books();
		pop_exec_reports();

		auto it = order_id_by_internal_order_id.find(trade_id);
		if (it != order_id_by_internal_order_id.end()) {
			auto [oit, success] = order_tracker.get_order(it->second);
			if (success) {
				const auto& order = oit->second;
				auto pit = top_of_books.find(order.symbol);
				auto filled = static_cast<int>(order.cum_qty);

				if (order.ord_status == FIX::OrdStatus_CANCELED) {
					return NAY - 1; // ASK ZORRO team: do we have to set open and close and profit?
				}

				*cost = 0; // TODO
				*open = order.avg_px;

				if (pit != top_of_books.end()) {
					*close = order.is_buy() ? pit->second.bid_price : pit->second.ask_price;
				}

				// set pnl only if it can be calculated and we have a fill, otherwise set it to zero
				if (filled && pit != top_of_books.end()) {
					*profit = order.is_buy()
						? (pit->second.bid_price - order.avg_px) * order.cum_qty
						: (order.avg_px - pit->second.ask_price) * order.cum_qty;
				}
				else {
					*profit = 0;
				}

				log::debug<dl2, true>(
					"BrokerTrade: nTradeID={} side={} avg_px={} profit={} filled={}", 
					trade_id, order.is_buy() ? "buy" : "sell", *open, *profit, filled
				);

				return filled;
			}
		} 

		return NAY;
	}

	/* BrokerSell2
	 *
	 * Optional function; closes a trade - completely or partially - at market or at a limit price. If partial closing is not supported,
	 * the `amount` is ignored and the trade is completely closed. Only used for not NFA compliant accounts that support individual closing of trades.
	 * If this function is not provided or if the NFA flag is set, Zorro closes the trade by calling BrokerBuy2 with the negative amount and with StopDist at -1.
	 * 
	 * Parameters:
	 *	trade_id	Input, trade/order ID as returned by BrokerBuy2, or -1 for a UUID to be set before with a SET_UUID command.
	 *	amount		Input, number of contracts resp. lots to be closed, positive for a long trade and negative for a short trade (see BrokerBuy). 
	 *              If less than the original size of the trade, the trade is partially closed.
	 *	limit		Optional input, fill price for a limit order, set up by OrderLimit, or 0 for closing at market. Can be ignored if limit orders 
	 *				are not supported by the API. 
	 *	close		Optional output, close price of the trade.
	 *	cost		Optional output, total rollover fee (swap fee) of the trade.
	 *	profit		Optional output, total profit or loss of the trade in account currency units.
	 *	till		Optional output, the amount that was closed from the position, always positive.
	 * 
	 * Returns:
	 *	  - New trade ID when the trade was partially closed and the broker assigned a different ID to the remaining position.
	 *	  - nTradeID when the ID did not change or the trade was fully closed.
	 *	  - 0 when the trade was not found or could not be closed.
	 */
	DLLFUNC_C int BrokerSell2(int trade_id, int amount, double limit, double* close, double* cost, double* profit, int* fill) {
		log::debug<dl1, true>("BrokerSell2: nTradeID={}, amount={}, lot_amount={}, limit={}", trade_id, amount, lot_amount, limit);

		auto it = order_id_by_internal_order_id.find(trade_id);
		if (it != order_id_by_internal_order_id.end()) {
			auto [oit, success] = order_tracker.get_order(it->second);

			if (success) {
				auto& order = oit->second;

				log::debug<dl1, true>("BrokerSell2: found open order={}", order.to_string());

				if (order.ord_status == FIX::OrdStatus_CANCELED || order.ord_status == FIX::OrdStatus_REJECTED) {
					log::debug<dl1, true>("BrokerSell2: order rejected or already cancelled - nothing to cancel");
					return 0;
				}

				auto symbol = FIX::Symbol(order.symbol);
				auto ord_id = FIX::OrderID(order.ord_id);
				auto cl_ord_id = FIX::ClOrdID(next_client_order_id());
				auto orig_cl_ord_id = FIX::OrigClOrdID(order.cl_ord_id);
				auto position_id = get_position_id_opt(order);

				if (order.ord_status == FIX::OrdStatus_FILLED) 
				{
					// trade opposite quantity for a fully filled order to offset the trade - here the amount is not needed
					int signed_qty = static_cast<int>(order.side == FIX::Side_BUY ? -order.cum_qty : order.cum_qty);

					log::debug<dl2, true>(
						"BrokerSell2[OrdStatus_FILLED]: closing filled order with {} order in opposite direction signed_qty={}", 
						(limit == 0 ? "market" : "limit"), signed_qty
					);

					auto ord_type = limit == 0 ? FIX::OrdType(FIX::OrdType_MARKET) : FIX::OrdType(FIX::OrdType_LIMIT);
					auto side = signed_qty > 0 ? FIX::Side(FIX::Side_BUY) : FIX::Side(FIX::Side_SELL);
					auto qty = FIX::OrderQty(std::abs(signed_qty));

					auto msg = fix_service->client().new_order_single(
						symbol, cl_ord_id, side, ord_type, time_in_force, qty,
						FIX::Price(0), FIX::StopPx(0), position_id
					);

					if (!msg.has_value()) {
						log::debug<dl2, true>("BrokerSell2[OrdStatus_FILLED]: failed to create NewOrderSingle");
						return 0;
					}

					auto report = limit == 0
						? pop_exec_report_fill(cl_ord_id.getString(), fix_exec_report_waiting_time)
						: pop_exec_report_new(cl_ord_id.getString(), fix_exec_report_waiting_time);

					if (!report.has_value()) {
						log::error<true>(
							"BrokerSell2[OrdStatus_FILLED]: timeout after {} while waiting for FIX exec report on new market order",
							fix_exec_report_waiting_time
						);
						return 0;
					}

					if (report.value().exec_type == FIX::ExecType_REJECTED && report.value().ord_status == FIX::OrdStatus_REJECTED) {
						log::error<true>(
							"BrokerSell2[OrdStatus_FILLED]: rejected order cl_ord_id{} reason={}", report.value().cl_ord_id, report.value().text
						);
						return 0;
					}

					auto [coit, success] = order_tracker.get_order(report.value().ord_id);

					if (!success) {
						log::error<true>(
							"BrokerSell2[OrdStatus_FILLED]: could not find closing order in order tracker with order id={}", 
							report.value().ord_id
						);
						return 0;
					}

					auto& close_order = coit->second;
					*close = close_order.avg_px;
					*fill = (int)close_order.cum_qty;
					*profit = order.is_buy() 
						? (close_order.avg_px - order.avg_px) * close_order.cum_qty
						: (order.avg_px - close_order.avg_px) * close_order.cum_qty;

					return trade_id;
				}
				else {
					// it is possible that leaves_qty < amount <= order_qty
					// in this case we have to
					//   1) cancel existing order completely, i.e cancel remaining leaves_qty  
					//   2) offset the filled part with a offsetting closing order of quantity = amount - leaves_qty

					auto ord_type = FIX::OrdType(order.ord_type);
					auto side = FIX::Side(order.side);
					auto tif = FIX::TimeInForce(order.tif);
					auto price = FIX::Price(order.price);

					if (std::abs(amount) == order.leaves_qty) { // cancel all
						log::debug<dl1, true>("BrokerSell2: cancel order_qty {}", order.leaves_qty);

						auto msg = fix_service->client().order_cancel_request(
							symbol, ord_id, orig_cl_ord_id, cl_ord_id, side, FIX::OrderQty(order.leaves_qty), position_id
						);

						if (!msg.has_value()) {
							log::error<true>("BrokerSell2: failed to create OrderCancelRequest");
							return 0;
						}

						auto report = pop_exec_report_cancel(ord_id.getString(), fix_exec_report_waiting_time);

						if (!report.has_value()) {
							log::error<true>(
								"BrokerSell2 timeout after {} while waiting for FIX exec report on cancel order",
								fix_exec_report_waiting_time
							);
							return 0;
						}

						if (report.value().exec_type == FIX::ExecType_REJECTED && report.value().ord_status == FIX::OrdStatus_REJECTED) {
							log::error<true>(
								"BrokerSell2: rejected order cl_ord_id{} reason={}", 
								report.value().cl_ord_id, report.value().text
							);
							return 0;
						}
					}
					else if (std::abs(amount) < order.leaves_qty) { // cancel replace - Zorro bug, amount not properly forwarded to BrokerSell2
						auto new_qty = max(order.leaves_qty - std::abs(amount), 0);

						log::debug<dl1, true>(
							"BrokerSell2: cancel/replace remaining quantity {} to new quantity {}", 
							order.leaves_qty, new_qty
						);

						auto msg = fix_service->client().order_cancel_replace_request(
							symbol, ord_id, orig_cl_ord_id, cl_ord_id, side, ord_type, FIX::OrderQty(new_qty), price, tif, position_id
						);

						if (!msg.has_value()) {
							log::error<true>("BrokerSell2: failed to create OrderCancelReplaceRequest");
							return 0;
						}

						auto report = pop_exec_report_cancel_replace(ord_id.getString(), fix_exec_report_waiting_time);

						if (!report.has_value()) {
							log::error<true>(
								"BrokerSell2: timeout after {} while waiting for FIX exec report on cancel/replace order",
								fix_exec_report_waiting_time
							);
							return 0;
						}

						if (report.value().exec_type == FIX::ExecType_REJECTED && report.value().ord_status == FIX::OrdStatus_REJECTED) {
							log::error<true>(
								"BrokerSell2: rejected order cl_ord_id{} reason={}", report.value().cl_ord_id, report.value().text
							);
							return 0;
						}
					}
					else { // std::abs(amount) > order.leaves_qty - Zorro bug, amount not properly forwarded to BrokerSell2
						auto msg = fix_service->client().order_cancel_request(
							symbol, ord_id, orig_cl_ord_id, cl_ord_id, side, FIX::OrderQty(order.leaves_qty), position_id
						);

						if (!msg.has_value()) {
							log::error<true>("BrokerSell2: failed to create OrderCancelRequest");
							return 0;
						}

						auto report = pop_exec_report_cancel(ord_id.getString(), fix_exec_report_waiting_time);

						if (!report.has_value()) {
							log::error<true>(
								"BrokerSell2 timeout after {} while waiting for FIX exec report on cancel order",
								fix_exec_report_waiting_time
							);
							return 0;
						}

						if (report.value().exec_type == FIX::ExecType_REJECTED && report.value().ord_status == FIX::OrdStatus_REJECTED) {
							log::error<true>(
								"BrokerSell2: rejected order cl_ord_id{} reason={}",
								report.value().cl_ord_id, report.value().text
							);
							return 0;
						}

						// offset 
						auto excess_qty = max(std::abs(amount), order.order_qty) - order.leaves_qty;
						int signed_qty = static_cast<int>(order.side == FIX::Side_BUY ? -excess_qty : excess_qty);

						log::debug<dl2, true>(
							"BrokerSell2: closing filled order with {} order in opposite direction signed_qty={}",
							(limit == 0 ? "market" : "limit"), signed_qty
						);

						auto ord_type = limit == 0 ? FIX::OrdType(FIX::OrdType_MARKET) : FIX::OrdType(FIX::OrdType_LIMIT);
						auto side = signed_qty > 0 ? FIX::Side(FIX::Side_BUY) : FIX::Side(FIX::Side_SELL);
						auto qty = FIX::OrderQty(std::abs(signed_qty));

						msg = fix_service->client().new_order_single(
							symbol, cl_ord_id, side, ord_type, time_in_force, qty,
							FIX::Price(0), FIX::StopPx(0), position_id
						);

						if (!msg.has_value()) {
							log::debug<dl2, true>("BrokerSell2: failed to create NewOrderSingle");
							return 0;
						}

						report = limit == 0
							? pop_exec_report_fill(cl_ord_id.getString(), fix_exec_report_waiting_time)
							: pop_exec_report_new(cl_ord_id.getString(), fix_exec_report_waiting_time);

						if (!report.has_value()) {
							log::error<true>(
								"BrokerSell2: timeout after {} while waiting for FIX exec report on new market order",
								fix_exec_report_waiting_time
							);
							return 0;
						}

						if (report.value().exec_type == FIX::ExecType_REJECTED && report.value().ord_status == FIX::OrdStatus_REJECTED) {
							log::error<true>(
								"BrokerSell2: rejected order cl_ord_id{} reason={}", report.value().cl_ord_id, report.value().text
							);
							return 0;
						}

						auto [coit, success] = order_tracker.get_order(report.value().ord_id);

						if (!success) {
							log::error<true>(
								"BrokerSell2: could not find closing order in order tracker with order id={}",
								report.value().ord_id
							);
							return 0;
						}

						auto& close_order = coit->second;
						*close = close_order.avg_px;
						*fill = (int)close_order.cum_qty;
						*profit = order.is_buy()
							? (close_order.avg_px - order.avg_px) * close_order.cum_qty
							: (order.avg_px - close_order.avg_px) * close_order.cum_qty;
					}

					return trade_id;
				}
			}
			else {
				log::error<true>("BrokerSell2: could not find open order with ord_id={} ", it->second);
			}
		}
		else {
			log::error<true>("BrokerSell2: mapping not found for trade_id={} ", trade_id);
		}

		return 0;
	}

	bool no_orders(const StatusExecReport& report) {
		return report.ord_status == FIX::OrdStatus_REJECTED && report.text.starts_with("no order(s)");
	}

	int get_fxcm_order_mass_status(bool print) {
		auto message = fix_service->client().order_mass_status_request();
		FIX::MassStatusReqID mass_status_req_id;
		message.getField(mass_status_req_id);

		auto result = pop_status_exec_reports(mass_status_req_id.getString(), 2 * fix_exec_report_waiting_time);

		c_order_mass_status_reports.clear();

		if (result.second) {
			std::stringstream ss;

			for (const auto& e : result.first) {
				CStatusExecReport c_report;
				strncpy_s(c_report.symbol, e.symbol.c_str(), sizeof(CStatusExecReport::symbol));
				strncpy_s(c_report.ord_id, e.ord_id.c_str(), sizeof(CStatusExecReport::ord_id));
				strncpy_s(c_report.cl_ord_id, e.cl_ord_id.c_str(), sizeof(CStatusExecReport::cl_ord_id));
				strncpy_s(c_report.exec_id, e.exec_id.c_str(), sizeof(CStatusExecReport::exec_id));
				strncpy_s(c_report.mass_status_req_id, e.mass_status_req_id.c_str(), sizeof(CStatusExecReport::mass_status_req_id));
				strncpy_s(c_report.text, e.text.c_str(), sizeof(CStatusExecReport::text));
				c_report.exec_type = e.exec_type;
				c_report.ord_type = e.ord_type;
				c_report.ord_status = e.ord_status;
				c_report.side = e.side;
				c_report.price = e.price;
				c_report.avg_px = e.avg_px;
				c_report.order_qty = e.order_qty;
				c_report.last_qty = e.last_qty;
				c_report.last_px = e.last_px;
				c_report.cum_qty = e.cum_qty;
				c_report.leaves_qty = e.leaves_qty;
				c_report.tot_num_reports = e.tot_num_reports;
				c_report.last_rpt_requested = e.last_rpt_requested ? 1 : 0;
				strncpy_s(c_report.position_id, get_position_id(e).c_str(), sizeof(CStatusExecReport::position_id));
				c_order_mass_status_reports.emplace_back(c_report);
				
				if (print) {
					ss << "\n  ";
					ss << (no_orders(e) ? "StatusExecReport[no_orders]" : e.to_string());
				}
			}

			if (print) {
				log::info<dl0, true>(
					"order_mass_status_request returned {} status exec reports{}",
					c_order_mass_status_reports.size(), ss.str()
				);
			}

			return c_order_mass_status_reports.size();
		}
		else {
			log::debug<dl0, true>("order_mass_status_request timed out in {}", 2 * fix_exec_report_waiting_time);
		}

		return 0;
	}

	// this should not be needed anymore if subscirbed with updates
	int get_fxcm_position_reports(std::vector<FXCMPositionReport>& pos_reports, bool print, int pos_req_type) {
		fix_service->client().request_for_positions(fxcm_account, pos_req_type, FIX::SubscriptionRequestType_SNAPSHOT);

		pos_reports.clear();

		FXCMPositionReports reports;
		bool success = position_snapshot_reports_queue.pop(reports, 2 * fix_waiting_time);
		if (success) {
			auto n = reports.reports.size();
			if (print) {
				log::info<dl0, true>(
					"request_for_positions({}) request returned {} position reports{}",
					pos_req_type, n, (n > 0 ? "\n" + reports.to_string() : "")
				);
			}
			if (pos_req_type == 0)
				pos_reports = std::move(reports.reports);
			else if (pos_req_type == 1)
				pos_reports = std::move(reports.reports);
			return n;
		}
		else {
			log::debug<dl0, true>("request_for_positions for type {} timed out in {}", pos_req_type, 2 * fix_exec_report_waiting_time);
		}

		return 0;
	}

	/*
	 * BrokerCommand
	 * 
	 * Details can be found here
	 *  - https://zorro-project.com/manual/en/brokercommand.htm
	 */
	DLLFUNC double BrokerCommand(int command, intptr_t dw_parameter) {
		switch (command) {
			case GET_EXCHANGES: {
				log::debug<dl3, true>("BrokerCommand {}[{}]() = {}", broker_command_string(command), command, "not implemented");
				break;
			}

			case GET_COMPLIANCE: {
				auto result = 2;
				log::debug<dl3, true>("BrokerCommand {}[{}]() = {}", broker_command_string(command), command, result);
				return result;
			}

			case GET_BROKERZONE: {
				log::debug<dl3, true>("BrokerCommand {}[{}]() = {}", broker_command_string(command), command, 0);
				return trading_session_status.server_timezone;  // initialized to 0 = UTC 
			}

			case GET_MAXTICKS: {
				auto result = 1440/4;
				log::debug<dl3, true>("BrokerCommand {}[{}]() = {}", broker_command_string(command), command, result);
				return result;
			}

			case GET_MAXREQUESTS: {
				log::debug<dl3, true>("BrokerCommand {}[{}]() = {}", broker_command_string(command), command, 30);
				return 30;
			}

			case GET_LOCK: {
				log::debug<dl3, true>("BrokerCommand {}[{}]() = {}", broker_command_string(command), command, 1);
				return 1;
			}

			case GET_NTRADES: {
				auto result = get_num_order_reports();
				log::debug<dl2, true>("BrokerCommand {}[{}]() = {}", broker_command_string(command), command, result);
				return result;
			}

			case GET_POSITION: {
				position_symbol = std::string((const char*)(dw_parameter));
				auto result = get_net_position_size(position_symbol);
				log::debug<dl2, true>("BrokerCommand {}[{}](position_symbol={}) = {}", broker_command_string(command), command, position_symbol, result);
				return result;
			}

			case GET_AVGENTRY: {
				auto result = get_avg_entry_price(position_symbol);
				log::debug<dl2, true>("BrokerCommand {}[{}](position_symbol={}) = {}", broker_command_string(command), command, position_symbol, result);
				return result;
			}

			// Returns 1 when the order was cancelled, or 0 when the order was not found or could not be cancelled.	
			// Note that it takes a struct pointer to CancelReplaceArg, which is more general than in the Zorro doc. 					
			case DO_CANCEL: {
				auto arg = (CancelReplaceArg*)dw_parameter;
				int trade_id = arg->trade_id;
				int cancel_replace_lot_amount = arg->amount;
				log::debug<dl0, true>("BrokerCommand {}[{}](trade_id={})", broker_command_string(command), command, trade_id);

				auto it = order_id_by_internal_order_id.find(trade_id); 
				if (it != order_id_by_internal_order_id.end()) {
					auto [oit, success] = order_tracker.get_order(it->second);

					if (success) {
						auto& order = oit->second;

						log::debug<dl0, true>("BrokerCommand[DO_CANCEL]: found open order={}", order.to_string());

						if (order.ord_status == FIX::OrdStatus_FILLED) {
							log::debug<dl0, true>("BrokerCommand[DO_CANCEL]: order already filled - nothing to cancel");
							return 0;
						}

						if (order.ord_status == FIX::OrdStatus_CANCELED || order.ord_status == FIX::OrdStatus_REJECTED) {
							log::debug<dl0, true>("BrokerCommand[DO_CANCEL]: order rejected or already cancelled - nothing to cancel");
							return 0;
						}

						auto symbol = FIX::Symbol(order.symbol);
						auto ord_id = FIX::OrderID(order.ord_id);
						auto orig_cl_ord_id = FIX::OrigClOrdID(order.cl_ord_id);
						auto cl_ord_id = FIX::ClOrdID(next_client_order_id());
						auto side = FIX::Side(order.side);
						auto tif = FIX::TimeInForce(order.tif);
						auto ord_type = FIX::OrdType(order.ord_type);
						auto price = FIX::Price(order.price);
						
						std::string op;
						std::optional<ExecReport> report;

						if (cancel_replace_lot_amount == 0) {
							op = "order cancel";
							
							log::debug<dl0, true>("BrokerCommand[DO_CANCEL]: executing {} - cancel completely", op);

							auto msg = fix_service->client().order_cancel_request(
								symbol, ord_id, orig_cl_ord_id, cl_ord_id, side, FIX::OrderQty(order.leaves_qty)
							);

							if (!msg.has_value()) {
								log::error<true>("BrokerCommand[DO_CANCEL]: failed to create request for {}", op);
								return 0;
							}

							report = pop_exec_report_cancel(order.ord_id, fix_exec_report_waiting_time);
						}
						else {
							op = "order cancel/replace";
							auto new_qty = cancel_replace_lot_amount * lot_amount;

							log::debug<dl0, true>("BrokerCommand[DO_CANCEL]: executing {} - new_qty={}", op, new_qty);

							auto msg = fix_service->client().order_cancel_replace_request(
								symbol, ord_id, orig_cl_ord_id, cl_ord_id, side, ord_type, FIX::OrderQty(new_qty), price, tif
							);

							if (!msg.has_value()) {
								log::error<true>("BrokerCommand[DO_CANCEL]: failed to create request for {}", op);
								return 0;
							}

							report = pop_exec_report_cancel_replace(order.ord_id, fix_exec_report_waiting_time);
						}

						if (!report.has_value()) {
							log::error<true>(
								"BrokerCommand[DO_CANCEL]: timeout while waiting for FIX exec report on {} after {}",
								op, fix_exec_report_waiting_time
							);
							return 0;							
						}

						if (report.value().exec_type == FIX::ExecType_REJECTED && report.value().ord_status == FIX::OrdStatus_REJECTED) {
							log::error<true>(
								"BrokerCommand[DO_CANCEL]: rejected {} cl_ord_id{} reason={}", op, report.value().cl_ord_id, report.value().text
							);
							return 0;
						}

						log::debug<dl0, true>(
							"BrokerCommand[DO_CANCEL]: exec report processed={}\n{}\n{}",
							report.value().to_string(), order_tracker.to_string("position_id"), order_mapping_string()
						);

						return 1;
					}
					else {
						log::error<true>(
							"BrokerCommand[DO_CANCEL]: could not find order for order id {}", it->second
						);
						return 0;
					}
				}
				else {
					log::error<true>(
						"BrokerCommand[DO_CANCEL]: could not find order mapping for trade id {}", trade_id
					);
					return 0;
				}
			}

			case SET_ORDERTEXT: {
				order_text = std::string((char*)dw_parameter); 
				log::debug<dl3, true>("BrokerCommand {}[{}](order_text={})", broker_command_string(command), command, order_text);
				return 1;
			}

			case SET_SYMBOL: {
				asset = std::string((char*)dw_parameter);
				log::debug<dl3, true>("BrokerCommand {}[{}](symbol={})", broker_command_string(command), command, asset);
				return 1;
			}

			case SET_MULTIPLIER: {
				multiplier = (int)dw_parameter;
				log::debug<dl3, true>("BrokerCommand {}[{}](multiplier={})", broker_command_string(command), command, multiplier);
				return 1;
			}
			
			// Switch between order types and return the type if supported by the broker plugin, otherwise 0
			//	- 0 Broker default (highest fill probability)
			//	- 1 AON (all or nothing) prevents partial fills
			//	- 2 GTC (good - till - cancelled) order stays open until completely filled
			//	- 3 AON + GTC
			//  - 8 - STOP; add a stop order at distance Stop* StopFactor on NFA accounts
			case SET_ORDERTYPE: {
				auto order_type = (int)dw_parameter;
				log::debug<dl3, true>("BrokerCommand {}[{}](order_type={})", broker_command_string(command), command, order_type);
				switch (order_type) {
				case 0:
					return 0; 
				case ORDERTYPE_IOC:
					time_in_force = FIX::TimeInForce_IMMEDIATE_OR_CANCEL;
					break;
				case ORDERTYPE_GTC:
					time_in_force = FIX::TimeInForce_GOOD_TILL_CANCEL;
					break;
				case ORDERTYPE_FOK:
					time_in_force = FIX::TimeInForce_FILL_OR_KILL;
					break;
				case ORDERTYPE_DAY:
					time_in_force = FIX::TimeInForce_DAY;
					break;
				default:
					return 0; // return 0 for not supported	
				}

				// additional stop order 
				if (order_type >= 8) {
					return 0; // return 0 for not supported	
				}

				return 1;
			}

			case GET_PRICETYPE: {
				log::debug<dl3, true>("BrokerCommand {}[{}]() = {}", broker_command_string(command), command, price_type);
				return price_type;
			}

			case SET_PRICETYPE: {
				price_type = (int)dw_parameter;
				log::debug<dl3, true>("BrokerCommand {}[{}](price_type={})", broker_command_string(command), command, price_type);
				return 1;
			}

			case GET_VOLTYPE: {
				vol_type = (int)dw_parameter;
				log::debug<dl3, true>("BrokerCommand {}[{}](vol_type={})", broker_command_string(command), command, vol_type);
				return vol_type;
			}

			case SET_AMOUNT: {
				lot_amount = *(double*)dw_parameter;
				log::debug<dl3, true>("BrokerCommand {}[{}](lot_amount={})", broker_command_string(command), command, lot_amount);
				return 1;
			}

			case SET_DIAGNOSTICS: {
				auto diagnostics = (int)dw_parameter;
				log::debug<dl3, true>("BrokerCommand {}[{}](diagnostics={})", broker_command_string(command), command, diagnostics);
				if (diagnostics == 1 || diagnostics == 0) {
					spdlog::set_level(diagnostics ? spdlog::level::debug : spdlog::level::info);
					return 1;
				}
				break;
			}

			// The window handle can be used by another application or process for triggering events or sending messages to a Zorro window. 
			// The message WM_APP+1 triggers a price quote request, WM_APP+2 triggers a script-supplied callback function, and WM_APP+3 
			// triggers a plugin-supplied callback function set by the GET_CALLBACK command.
			// The window handle is automatically sent to broker plugins with the SET_HWND command.
			// For asynchronously triggering a price quote request, send a WM_APP+1 message to the window handle received by the SET_HWND command. 
			// For triggering the callback function, send a WM_APP+2 message. This can be used for prices streamed by another thread. 
			// See https://zorro-project.com/manual/en/hwnd.htm
			case SET_HWND: {
				window_handle = (HWND)dw_parameter;
				log::debug<dl3, true>("BrokerCommand {}[{}](window_handle={})", broker_command_string(command), command, (long)window_handle);
				break;
			}

			case GET_CALLBACK: {
				auto ptr = (void*)plugin_callback;
				log::debug<dl3, true>("BrokerCommand {}[{}]() = {}", broker_command_string(command), command, ptr);
				break;
			}

			case SET_CCY: {
				currency = std::string((char*)dw_parameter);
				log::debug<dl3, true>("BrokerCommand {}[{}](currency={})", broker_command_string(command), command, currency);
				break;
			}

			case GET_HEARTBEAT: {
				log::debug<dl3, true>("BrokerCommand {}[{}]() = {}", broker_command_string(command), command, 500);
				return 500;
			}

			case SET_LEVERAGE: {
				leverage = (int)dw_parameter;
				log::debug<dl3, true>("BrokerCommand {}[{}](leverage={})", broker_command_string(command), command, leverage);
				break;
			}

			case SET_LIMIT: {
				limit = *(double*)dw_parameter;
				log::debug<dl3, true>("BrokerCommand {}[{}](limit={})", broker_command_string(command), command, limit);
				break;
			}

			case SET_FUNCTIONS: {
				log::debug<dl3, true>("BrokerCommand {}[{}]()", broker_command_string(command), command);
				break;
			}

			case BROKER_CMD_CREATE_ASSET_LIST_FILE: {
				auto filename = std::string((const char*)dw_parameter);
				log::debug<dl3, true>(
					"BrokerCommand {}[{}](filename={})", 
					"BROKER_CMD_CREATE_ASSET_LIST_FILE", BROKER_CMD_CREATE_ASSET_LIST_FILE, filename
				);
				std::stringstream headers, rows;
				headers << "Name, Price, Spread, RollLong, RollShort, PIP, PIPCost, MarginCost, Leverage, LotAmount, Commission, Symbol, Type, Description";
				for (const auto& [symbol, info] : trading_session_status.security_informations) {
					double price = 0, spread = 0, pip_cost = 0, margin_cost = 0, leverage = 0, commission = 0.6;
					auto it = top_of_books.find(info.symbol);
					if (it != top_of_books.end()) {
						price = it->second.ask_price;
						spread = it->second.ask_price - it->second.bid_price;
					}

					// how should be the mapping?
					// FXCM_SYM_INTEREST_BUY = 9003,		// Interest Rate for sell side open positions
					// FXCM_SYM_INTEREST_SELL = 9004,		// Interest Rate for buy side open position 

					rows
						<< info.symbol << ", "
						<< price << ", "
						<< spread << ", "
						<< info.interest_buy << ", "
						<< info.interest_sell << ", "
						<< info.point_size << ", "
						<< pip_cost << ", "
						<< margin_cost << ", "
						<< leverage << ", "
						<< info.round_lots << ", "
						<< commission << ", "
						<< info.symbol << ", "
						<< magic_enum::enum_name(info.prod_id) << ", "
						<< info.symbol << "[" << info.currency << "]"
						<< std::endl;
				}
				write_to_file(filename, rows.str(), headers.str(), std::fstream::trunc);
				return trading_session_status.security_informations.size();
			}

			case BROKER_CMD_CREATE_SECURITY_INFO_FILE: {
				auto filename = std::string((const char*)dw_parameter);
				log::debug<dl3, true>(
					"BrokerCommand {}[{}](filename={})",
					"BROKER_CMD_CREATE_SECURITY_INFO_FILE", BROKER_CMD_CREATE_SECURITY_INFO_FILE, filename
				);

				std::stringstream headers, rows;
				headers
					<< "symbol" << ", "
					<< "currency" << ", "
					<< "product" << ", "
					<< "pip_size" << ", "
					<< "point_size" << ", "
					<< "max_quanity" << ", "
					<< "min_quantity" << ", "
					<< "round_lots" << ", "
					<< "factor" << ", "
					<< "contract_multiplier" << ", "
					<< "prod_id" << ", "
					<< "interest_buy" << ", "
					<< "interest_sell" << ", "
					<< "subscription_status" << ", "
					<< "sort_order" << ", "
					<< "cond_dist_stop" << ", "
					<< "cond_dist_limit" << ", "
					<< "cond_dist_entry_stop" << ", "
					<< "cond_dist_entry_limit" << ", "
					<< "fxcm_trading_status";
				for (const auto& [symbol, info] : trading_session_status.security_informations) {
					rows
						<< info.symbol << ", "
						<< info.currency << ", "
						<< info.product << ", "
						<< info.pip_size << ", "
						<< info.point_size << ", "
						<< info.max_quanity << ", "
						<< info.min_quantity << ", "
						<< info.round_lots << ", "
						<< info.factor << ", "
						<< info.contract_multiplier << ", "
						<< magic_enum::enum_name(info.prod_id) << ", "
						<< info.interest_buy << ", "
						<< info.interest_sell << ", "
						<< info.subscription_status << ", "
						<< info.sort_order << ", "
						<< info.cond_dist_stop << ", "
						<< info.cond_dist_limit << ", "
						<< info.cond_dist_entry_stop << ", "
						<< info.cond_dist_entry_limit << ", "
						<< magic_enum::enum_name(info.fxcm_trading_status)
						<< std::endl;
				}
				write_to_file(filename, rows.str(), headers.str(), std::fstream::trunc);
				return trading_session_status.security_informations.size();
			}

			case BROKER_CMD_PRINT_ORDER_TRACKER: {
				show(std::format("\n{}\n{}", order_tracker.to_string("position_id"), order_mapping_string()));
				return 0;
			}

			case BROKER_CMD_PRINT_POSIITION_REPORTS: {
				int flags = (int)dw_parameter;
				log::debug<dl0, true>("BrokerCommand {}[{}]: flags={}", BROKER_CMD_PRINT_POSIITION_REPORTS, command, flags);

				std::stringstream ss;

				if (flags & 1) {
					ss << "\nFXCMPositionReports[Open][";
					for (const auto& kv : position_reports) {
						if (kv.second.is_open)
							ss << "\n  " << kv.second.to_string();
					}
					ss << "\n]";
				}

				if (flags & 2) {
					ss << "\nFXCMPositionReports[Closed][";
					for (const auto& kv : position_reports) {
						if (!kv.second.is_open)
							ss << "\n  " << kv.second.to_string();
					}
					ss << "\n]\n";
				}
				
				show(ss.str());
				return 0;
			}

			case BROKER_CMD_PRINT_COLLATERAL_REPORTS: {
				std::stringstream ss;

				ss << "\nFXCMCollateralReports[";
				for (const auto& kv : collateral_reports) {
					ss << "\n  " << kv.second.to_string();
				}
				ss << "\n]\n";

				show(ss.str());
				return 0;
			}

			case BROKER_CMD_GET_ORDER_POSITION_ID: {
				auto arg = (GetOrderPositionIdArg*)dw_parameter;
				auto it = order_id_by_internal_order_id.find(arg->trade_id);
				if (it != order_id_by_internal_order_id.end())
				{
					auto [oit, success] = order_tracker.get_order(it->second);

					if (success) {
						const auto& order = oit->second;
						const auto& position_id = get_position_id(order);
						if (!position_id.empty()) {
							strncpy_s(arg->position_id, position_id.c_str(), sizeof(GetOrderPositionIdArg::position_id));
							arg->has_open_position = 1;
						}
						else {
							arg->has_open_position = 0;
						}
						arg->trade_not_found = 1;
						return 0;
					} 
				}

				arg->position_id[0] = '\0';
				arg->has_open_position = 0;
				arg->trade_not_found = 1;
				return 0;
			}

			case BROKER_CMD_GET_ORDER_TRACKER_ORDER_REPORTS: {
				auto* arg = (GetOrderTrackerOrderReportsArg*)dw_parameter;
				c_order_tracker_order_reports.clear();
				for (const auto& kv : order_tracker.get_orders()) {
					COrderReport order_report;
					strncpy_s(order_report.symbol, kv.second.symbol.c_str(), sizeof(COrderReport::symbol));
					strncpy_s(order_report.ord_id, kv.second.ord_id.c_str(), sizeof(COrderReport::ord_id));
					strncpy_s(order_report.cl_ord_id, kv.second.cl_ord_id.c_str(), sizeof(COrderReport::cl_ord_id));
					order_report.ord_type = kv.second.ord_type;
					order_report.ord_status = kv.second.ord_status;
					order_report.side = kv.second.side;
					order_report.price = kv.second.price;
					order_report.avg_px = kv.second.avg_px;
					order_report.order_qty = kv.second.order_qty;
					order_report.cum_qty = kv.second.cum_qty;
					order_report.leaves_qty = kv.second.leaves_qty;
					strncpy_s(order_report.position_id, get_position_id(kv.second).c_str(), sizeof(COrderReport::position_id));
					c_order_tracker_order_reports.emplace_back(order_report);
				}
				arg->num_reports = c_order_tracker_order_reports.size();
				arg->reports = c_order_tracker_order_reports.size() > 0 ? c_order_tracker_order_reports.data() : nullptr;
				return c_order_tracker_order_reports.size();
			}

			case BROKER_CMD_GET_ORDER_TRACKER_NET_POSITIONS: {
				auto* arg = (GetOrderTrackerNetPositionsArg*)dw_parameter;
				c_order_tracker_net_positions.clear();
				for (const auto& kv : order_tracker.get_net_positions()) {
					CNetPosition net_pos;
					strncpy_s(net_pos.symbol, kv.second.symbol.c_str(), sizeof(CNetPosition::symbol));
					strncpy_s(net_pos.account, kv.second.account.c_str(), sizeof(CNetPosition::account));
					net_pos.qty = kv.second.qty;
					net_pos.avg_px = kv.second.avg_px;
					c_order_tracker_net_positions.emplace_back(net_pos);
				}
				arg->num_reports = c_order_tracker_order_reports.size();
				arg->reports = c_order_tracker_net_positions.size() > 0 ? c_order_tracker_net_positions.data() : nullptr;
				return c_order_tracker_order_reports.size();
			}

			case BROKER_CMD_GET_ORDER_MASS_STATUS: {
				auto arg = (GetOrderMassStatusArg*)dw_parameter;
				get_fxcm_order_mass_status(arg->print > 0);
				arg->num_reports = c_order_mass_status_reports.size();
				arg->reports = c_order_mass_status_reports.size() > 0 ? c_order_mass_status_reports.data() : nullptr;
				return c_order_mass_status_reports.size();
			}

			case BROKER_CMD_GET_OPEN_POSITION_REPORTS: {
				auto* arg = (GetPositionReportArg*)dw_parameter;
				get_c_position_reports(c_open_position_reports, true);
				arg->num_reports = c_open_position_reports.size();
				arg->reports = c_open_position_reports.size() > 0 ? c_open_position_reports.data() : nullptr;
				return c_open_position_reports.size();
			}

			case BROKER_CMD_GET_CLOSED_POSITION_REPORTS: {
				auto* arg = (GetPositionReportArg*)dw_parameter;
				get_c_position_reports(c_closed_position_reports, false);
				arg->reports = c_closed_position_reports.size() > 0 ? c_closed_position_reports.data() : nullptr;
				arg->num_reports = c_closed_position_reports.size();
				return c_closed_position_reports.size();
			}

			default: {
				log::debug<dl0, true>("BrokerCommand {}[{}] unknown command", broker_command_string(command), command);
				break;
			}
		}

		return 0;
	}
}


