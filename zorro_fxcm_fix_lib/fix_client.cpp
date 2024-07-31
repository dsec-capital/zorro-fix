#ifdef _MSC_VER 
#pragma warning(disable : 4503 4355 4786 26444)
#endif

#include "pch.h"

#include "fix_client.h"

#include "quickfix/config.h"
#include "quickfix/Session.h"
#include "magic_enum/magic_enum.hpp"

#include "spdlog/spdlog.h"

#include "common/time_utils.h"
#include "zorro_common/log.h"

//#define FIX_MSG_TRACE_LOG

namespace zorro {

	using namespace common;

	constexpr int dl0 = 0;
	constexpr int dl1 = 1;
	constexpr int dl2 = 2;
	constexpr int dl3 = 3;
	constexpr int dl4 = 4;
	constexpr int dl5 = 5;

#ifdef FIX_MSG_TRACE_LOG
	std::shared_ptr<spdlog::logger> fix_msg_trace_log;
#endif

	std::string fix_string(const FIX::Message& msg) {
		auto s = msg.toString();
		std::replace(s.begin(), s.end(), '\x1', '|');  
		return s;
	}

	std::string fix_string_masked(const FIX::Message& msg) {
		FIX::Message msg_copy = msg;
		FIX::SenderCompID sender_comp_id;
		auto is_data = msg_copy.getHeader().getField(sender_comp_id).getString().starts_with("MD");
		auto masked_sender_comp_id = is_data ? "MDataSenderCompID_Masked" : "TExecSenderCompID_Masked";
		msg_copy.getHeader().setField(FIX::SenderCompID(masked_sender_comp_id));
		msg_copy.setField(FIX::Account("Account_Masked"));
		auto s = msg_copy.toString();
		std::replace(s.begin(), s.end(), '\x1', '|');
		return s;
	}
	
	template<class T>
	inline T get_or_else(const ServiceMessage& map, const std::string_view& key, const T& other) {
		auto it = map.find(std::string(key));
		if (it != map.end()) {
			return std::get<T>(it->second);
		}
		else {
			return other;
		}
	}

	template<typename G>
	inline std::chrono::nanoseconds parse_date_and_time(const G& g, int date_field = FIX::FIELD::MDEntryDate, int time_field = FIX::FIELD::MDEntryTime) {
		std::stringstream in;
		in << g.getField(date_field) << "-" << g.getField(time_field);
		std::chrono::time_point<std::chrono::system_clock>  tp;
		in >> std::chrono::parse("%Y%m%d-%T", tp);
		return std::chrono::duration_cast<std::chrono::nanoseconds>(tp.time_since_epoch());
	}

	inline std::chrono::nanoseconds parse_datetime(const std::string& datetime) {
		std::stringstream in; in << datetime;
		std::chrono::time_point<std::chrono::system_clock>  tp;
		in >> std::chrono::parse("%Y%m%d-%T", tp);
		return std::chrono::duration_cast<std::chrono::nanoseconds>(tp.time_since_epoch());
	}

	FXCMMarginCallStatus parse_fxcm_margin_call_status(const std::string& status) {
		if (status == "N")
			return FXCMMarginCallStatus::MarginCallStatus_Fine;
		if (status == "W")
			return FXCMMarginCallStatus::MarginCallStatus_MaintenanceMarginAleart;
		if (status == "Y")
			return FXCMMarginCallStatus::MarginCallStatus_LiquidationReached;
		if (status == "A")
			return FXCMMarginCallStatus::MarginCallStatus_EquityAlert;
		if (status == "Q")
			return FXCMMarginCallStatus::MarginCallStatus_EquityStop;
		throw std::runtime_error("unknown margin call status " + status);
	}

	std::string FXCMSecurityInformation::to_string() const {
		std::stringstream ss;
		ss << "FXCMSecurityInformation["
	       << "symbol=" << symbol << ", "
	       << "currency=" << currency << ", "
	       << "product=" << product << ", "
	       << "pip_size=" << pip_size << ", "
	       << "point_size=" << point_size << ", "
	       << "max_quanity=" << max_quanity << ", "
	       << "min_quantity=" << min_quantity << ", "
	       << "round_lots=" << round_lots << ", "
	       << "factor=" << factor << ", "
	       << "contract_multiplier=" << contract_multiplier << ", "
	       << "prod_id=" << magic_enum::enum_name(prod_id) << ", "
	       << "interest_buy=" << interest_buy << ", "
	       << "interest_sell=" << interest_sell << ", "
	       << "subscription_status=" << subscription_status << ", "
	       << "sort_order=" << sort_order << ", "
	       << "cond_dist_stop=" << cond_dist_stop << ", "
	       << "cond_dist_limit=" << cond_dist_limit << ", "
	       << "cond_dist_entry_stop=" << cond_dist_entry_stop << ", "
	       << "cond_dist_entry_limit=" << cond_dist_entry_limit << ", "
	       << "fxcm_trading_status=" << magic_enum::enum_name(fxcm_trading_status) 
	       << "]";
		return ss.str();
	}

	std::string FXCMCollateralReport::to_string() const {
		std::stringstream ss;
		ss << "FXCMCollateralReport["
		   << "balance=" << balance << ", "
		   << "start_cash=" << start_cash << ", "
		   << "end_cash=" << end_cash << ", "
		   << "margin_ratio=" << margin_ratio << ", "
		   << "margin=" << margin << ", "
		   << "margin=" << margin << ", "
		   << "margin_call_status=" << magic_enum::enum_name(margin_call_status) << ", "
		   << "sending_time" << common::to_string(sending_time) << ", "
		   << "party_sub_ids=[";
		bool first = false;
		for (const auto& psid : party_sub_ids) {
			if (first) ss << ", ";
			ss << "type=" << psid.first << ", " << "party_sub_id=" << psid.second;
			first = true;
		}
		ss << "]]";
		return ss.str();
	}

	std::string FXCMPositionReport::to_string() const {
		std::stringstream ss;
		ss << "FXCMPositionReport["
		   << "account=" << account << ", "
		   << "symbol=" << symbol << ", "
		   << "currency=" << currency << ", "
		   << "position_id=" << position_id << ", "
		   << "settle_price=" << settle_price << ", "
		   << "is_open=" << is_open << ", "
		   << "interest=" << interest << ", "
		   << "commission=" << commission << ", "
		   << "open_time=" << common::to_string(open_time) << ", "
		   << "used_margin=" << (used_margin.has_value() ? std::to_string(used_margin.value()) : "N/A") << ", "
		   << "close_pnl=" << (close_pnl.has_value() ? std::to_string(close_pnl.value()) : "N/A") << ", "
		   << "close_settle_price=" << (close_settle_price.has_value() ? std::to_string(close_settle_price.value()) : "N/A") << ", "
		   << "close_time=" << (close_settle_price.has_value() ? common::to_string(close_time.value()) : "N/A") << ", "
		   << "close_order_id=" << (close_order_id.has_value() ? close_order_id.value() : "N/A") << ", "
		   << "close_cl_ord_id=" << (close_cl_ord_id.has_value() ? close_cl_ord_id.value() : "N/A")
		   << "]";
		return ss.str();
	}

	std::string FXCMPositionReports::to_string() const {
		std::stringstream ss;
		ss << "FXCMPositionReports[";
		bool first = false;
		for (const auto& report : reports) {
			if (first) ss << ",";
			ss << "\n  ";
			ss << report.to_string();
			first = true;
		}
		ss << "\n]";
		return ss.str();
	}

	std::string FXCMTradingSessionStatus::to_string() const {
		std::stringstream ss;
		ss << "FXCMTradingSessionStatus["
		   << "security_informations=[";
		bool first_s = false;
		for (const auto& infos : security_informations) {
			if (first_s) ss << ", ";
			ss << infos.first << "=" << infos.second.to_string();
			first_s = true;
		}
		ss << "], ";
		ss
		   << "trade_session_status=" << magic_enum::enum_name(trade_session_status) << ", "
		   << "server_timezone_name=" << server_timezone_name << ", "
		   << "server_timezone=" << server_timezone << ", "
		   << "parameters=[";
		bool first_p = false;
		for (const auto& param : session_parameters) {
			if (first_p) ss << ", ";
			ss << param.first << "=" << param.second;
			first_p = true;
		}
		ss << "]]";
		return ss.str();
	}

	FixClient::FixClient(
		const FIX::SessionSettings& session_settings,
		unsigned int num_required_session_logins,
		BlockingTimeoutQueue<ExecReport>& exec_report_queue,
		BlockingTimeoutQueue<StatusExecReport>& status_exec_report_queue,
		BlockingTimeoutQueue<TopOfBook>& top_of_book_queue,
		BlockingTimeoutQueue<ServiceMessage>& service_message_queue,
		BlockingTimeoutQueue<FXCMPositionReport>& position_report_queue, 
		BlockingTimeoutQueue<FXCMPositionReports>& position_snapshot_reports_queue,
		BlockingTimeoutQueue<FXCMCollateralReport>& collateral_report_queue,
		BlockingTimeoutQueue<FXCMTradingSessionStatus>& trading_session_status_queue
	) : session_settings(session_settings)
      , num_required_session_logins(num_required_session_logins)
	  , exec_report_queue(exec_report_queue)
	  , status_exec_report_queue(status_exec_report_queue)
	  , top_of_book_queue(top_of_book_queue)
      , service_message_queue(service_message_queue)
	  , position_report_queue(position_report_queue)
      , position_snapshot_reports_queue(position_snapshot_reports_queue)
      , collateral_report_queue(collateral_report_queue)
      , trading_session_status_queue(trading_session_status_queue)
	  , done(false)
	{
		if (session_settings.get().has("AccountId")) {
			auto account = session_settings.get().getString("AccountId");
			account_ids.insert(account);
			log::debug<dl4, false>(
				"FixClient::FixClient account id from session settings={}", account
			);
		}

#ifdef FIX_MSG_TRACE_LOG
		fix_msg_trace_log = spdlog::basic_logger_mt("fix", std::format("fix_message_tracer_{}.log", common::timestamp_postfix()), true);
		fix_msg_trace_log->set_level(spdlog::level::debug);
#endif
	}

	std::future<bool> FixClient::login_state() {
		return login_promise.get_future();
	}

	unsigned int FixClient::get_session_logins() {
		std::unique_lock<std::mutex> ul(mutex);
		unsigned int count = 0;
		for (const auto& [sess_id, logged_in] : session_login_status) {
			count += logged_in ? 1 : 0;
		}
		return count;
	}

	std::set<std::string> FixClient::get_account_ids() {
		std::unique_lock<std::mutex> ul(mutex);
		return account_ids;
	}

	bool FixClient::is_trading_session(const FIX::SessionID& sess_id) const {
		return !sess_id.getSenderCompID().getString().starts_with("MD_");
	}

	bool FixClient::is_market_data_session(const FIX::SessionID& sess_id) const {
		return sess_id.getSenderCompID().getString().starts_with("MD_");
	}

	void FixClient::onCreate(const FIX::SessionID& sess_id) {
		// FIX Session created - QuickFIX will automatically send the Logon(A) message
		bool is_trading;
		if (is_market_data_session(sess_id)) {
			market_data_session_id = sess_id;
			is_trading = false;
		}
		else {
			trading_session_id = sess_id;
			is_trading = true;
		}
		session_login_status.insert_or_assign(sess_id.toString(), false);
		log::debug<dl2, false>(
			"FixClient::onCreate is trading={}, sessionID={}", is_trading, sess_id.toString()
		);
	}
	
	ServiceMessage FixClient::logon_service_message() const {
		ServiceMessage service_msg{
			{std::string(SERVICE_MESSAGE_TYPE), std::string(SERVICE_MESSAGE_LOGON_STATUS)}
		};

		unsigned int session_logins = 0;
		for (const auto& [sess_id, logged_in] : session_login_status) {
			session_logins += logged_in ? 1 : 0;
			service_msg.emplace(sess_id, logged_in);
		}

		service_msg.emplace(SERVICE_MESSAGE_LOGON_STATUS_SESSION_LOGINS, session_logins);
		service_msg.emplace(SERVICE_MESSAGE_LOGON_STATUS_READY, session_logins == num_required_session_logins);

		return service_msg;
	}

	ServiceMessage FixClient::reject_service_message(const std::string& reject_type, const std::string& reject_text, const std::string& raw) const {
		ServiceMessage service_msg{
			{std::string(SERVICE_MESSAGE_TYPE), std::string(SERVICE_MESSAGE_REJECT)}
		};

		service_msg.emplace(SERVICE_MESSAGE_REJECT_TYPE, reject_type);
		service_msg.emplace(SERVICE_MESSAGE_REJECT_TEXT, reject_text);
		service_msg.emplace(SERVICE_MESSAGE_REJECT_RAW_MESSAGE, raw);

		return service_msg;
	}

	void FixClient::onLogon(const FIX::SessionID& sess_id)
	{
		std::unique_lock<std::mutex> ul(mutex);
		
		session_login_status.insert_or_assign(sess_id.toString(), true);
		
		auto service_msg = logon_service_message();
		auto session_logins = sm_get_or_else<unsigned int>(service_msg, SERVICE_MESSAGE_LOGON_STATUS_SESSION_LOGINS, 0);

		log::debug<dl0, false>(
			"FixClient::onLogon sessionID={} login count={} status={}", 
			sess_id.toString(), session_logins, session_logins == num_required_session_logins ? "ready" : "waiting" 
		);

		service_message_queue.push(service_msg);
	}

	void FixClient::onLogout(const FIX::SessionID& sess_id)
	{
		std::unique_lock<std::mutex> ul(mutex);
		session_login_status.insert_or_assign(sess_id.toString(), false);
		log::debug<dl0, false>("FixClient::onLogout sessionID={}", sess_id.toString());

		auto service_msg = logon_service_message();
		service_message_queue.push(service_msg);
	}

	void FixClient::fromAdmin(
		const FIX::Message& message, const FIX::SessionID& sessionID
	) EXCEPT(FIX::FieldNotFound, FIX::IncorrectDataFormat, FIX::IncorrectTagValue, FIX::RejectLogon)
	{
		log::debug<dl2, false>("FixClient::fromAdmin IN <{}> {}", sessionID.toString(), fix_string(message));

#ifdef FIX_MSG_TRACE_LOG
		fix_msg_trace_log->debug("Admin IN  {}", fix_string_masked(message));
#endif
	}

	void FixClient::toAdmin(FIX::Message& message, const FIX::SessionID& sessionID) 
	{
		// Logon (A) requires to set the Username and Password fields
		const auto& msg_type = message.getHeader().getField(FIX::FIELD::MsgType);
		if (msg_type == FIX::MsgType_Logon) {
			// get both username and password from settings file
			auto user = session_settings.get().getString("Username");
			auto pass = session_settings.get().getString("Password");
			message.setField(FIX::Username(user));
			message.setField(FIX::Password(pass));
		}

		// all messages sent to FXCM must contain the TargetSubID field (both Administrative and Application messages) 
		auto sub_id = session_settings.get().getString("TargetSubID");
		message.getHeader().setField(FIX::TargetSubID(sub_id));

		log::debug<dl2, false>("Admin OUT <{}> {}", sessionID.toString(), fix_string(message));

#ifdef FIX_MSG_TRACE_LOG
		fix_msg_trace_log->debug("Admin OUT {}", fix_string_masked(message));
#endif
	}

	void FixClient::fromApp(const FIX::Message& message, const FIX::SessionID& sessionID)
		EXCEPT(FIX::FieldNotFound, FIX::IncorrectDataFormat, FIX::IncorrectTagValue, FIX::UnsupportedMessageType)
	{
		auto mkt = fix::is_market_data_message(message);
		auto exec = fix::is_exec_report_message(message);
		if (exec) {
			log::debug<dl0, false>("FixClient::fromApp IN <{}> {}", sessionID.toString(), fix_string(message));
		} 
		else if (mkt) {
			log::debug<dl5, false>("FixClient::fromApp IN <{}> {}", sessionID.toString(), fix_string(message));
		} 
		else {
			log::debug<dl1, false>("FixClient::fromApp IN <{}> {}", sessionID.toString(), fix_string(message));
		}

#ifdef FIX_MSG_TRACE_LOG
		fix_msg_trace_log->debug("App   IN  {}", fix_string_masked(message));
#endif

		crack(message, sessionID);
	}

	void FixClient::toApp(FIX::Message& message, const FIX::SessionID& sessionID) EXCEPT(FIX::DoNotSend)
	{
		try
		{
			FIX::PossDupFlag possDupFlag;
			message.getHeader().getField(possDupFlag);
			if (possDupFlag) throw FIX::DoNotSend();
		}
		catch (FIX::FieldNotFound&) {}

		auto sub_ID = session_settings.get().getString("TargetSubID");
		message.getHeader().setField(FIX::TargetSubID(sub_ID));

		log::debug<dl0, false>("FixClient::toApp OUT <{}> {}", sessionID.toString(), fix_string(message));

#ifdef FIX_MSG_TRACE_LOG
		fix_msg_trace_log->debug("App   OUT {}", fix_string_masked(message));
#endif
	}

	// The TradingSessionStatus message is used to provide an update on the status of the market. Furthermore, 
	// this message contains useful system parameters as well as information about each trading security (embedded SecurityList).
	// TradingSessionStatus should be requested upon successful Logon and subscribed to. The contents of the
	// TradingSessionStatus message, specifically the SecurityList and system parameters, should dictate how fields
	// are set when sending messages to FXCM.
	// ** Note on Text(58) ** 
	// You will notice that Text(58) field is always set to "Market is closed. Any trading
	// functionality is not available." This field is always set to this value; therefore, do not 
	// use this field value to determine if the trading desk is open. As stated above, use TradSesStatus for this purpose
	void FixClient::onMessage(const FIX44::TradingSessionStatus& message, const FIX::SessionID& session_ID)
	{
		try {
			FXCMTradingSessionStatus status;

			// parse TradingSessionStatus message embeded in SecurityList
			int num_symbols = FIX::IntConvertor::convert(message.getField(FIX::FIELD::NoRelatedSym));
			for (int i = 1; i <= num_symbols; i++) {
				// 55=LINK/USD|460=12|228=1|231=1|9001=3|9002=0.01|9005=8027|9080=9|15=USD|561=1|
				// 9003=-0.027|9004=0|9076=D|9090=0|9091=0|9092=0|9093=0|9094=1000|9095=1|9096=O|
				FIX44::SecurityList::NoRelatedSym group;
				message.getGroup(i, group);

				try {
					const auto& symbol = group.getField(FIX::FIELD::Symbol);
					int product = FIX::IntConvertor::convert(group.getField(FIX::FIELD::Product));
					int factor = FIX::IntConvertor::convert(group.getField(FIX::FIELD::Factor));
					double contract_multiplier = FIX::DoubleConvertor::convert(group.getField(FIX::FIELD::ContractMultiplier));
					int pip_size = FIX::IntConvertor::convert(group.getField(FXCM_SYM_PRECISION));
					double point_size = FIX::DoubleConvertor::convert(group.getField(FXCM_SYM_POINT_SIZE));
					FXCMProductId prod_id = static_cast<FXCMProductId>(FIX::IntConvertor::convert(group.getField(FXCM_PRODUCT_ID)));
					const auto& currency = group.getField(FIX::FIELD::Currency);
					int round_lots = FIX::IntConvertor::convert(group.getField(FIX::FIELD::RoundLot));
					double interest_buy = FIX::DoubleConvertor::convert(group.getField(FXCM_SYM_INTEREST_BUY));
					double interest_sell = FIX::DoubleConvertor::convert(group.getField(FXCM_SYM_INTEREST_SELL));
					const auto& subscription_status = group.getField(FXCM_SUBSCRIPTION_STATUS);
					int sort_order = FIX::IntConvertor::convert(group.getField(FXCM_SYM_SORT_ORDER));
					double cond_dist_stop = FIX::DoubleConvertor::convert(group.getField(FXCM_COND_DIST_STOP));
					double cond_dist_limit = FIX::DoubleConvertor::convert(group.getField(FXCM_COND_DIST_LIMIT));
					double cond_dist_entry_stop = FIX::DoubleConvertor::convert(group.getField(FXCM_COND_DIST_ENTRY_STOP));
					double cond_dist_entry_limit = FIX::DoubleConvertor::convert(group.getField(FXCM_COND_DIST_ENTRY_LIMIT));
					double max_quanity = FIX::DoubleConvertor::convert(group.getField(FXCM_MAX_QUANTITY));
					double min_quantity = FIX::DoubleConvertor::convert(group.getField(FXCM_MIN_QUANTITY));
					const auto& fxcm_trading_status = group.getField(FXCM_TRADING_STATUS);	
					auto trade_status = FXCMTradingStatus::UnknownTradingStatus;
					if (fxcm_trading_status == "O")
						trade_status = FXCMTradingStatus::TradingOpen;
					else if (fxcm_trading_status == "C") 
						trade_status = FXCMTradingStatus::TradingClosed;

					struct FXCMSecurityInformation security_info{
						.symbol = symbol,
						.currency = currency,
						.product = product,
						.pip_size = pip_size,
						.point_size = point_size,
						.max_quanity = max_quanity,
						.min_quantity = min_quantity,
						.round_lots = round_lots,
						.factor = factor,
						.contract_multiplier = contract_multiplier,
						.prod_id = prod_id,
						.interest_buy = interest_buy,
						.interest_sell = interest_sell,
						.subscription_status = subscription_status,
						.sort_order = sort_order,
						.cond_dist_stop = cond_dist_stop,
						.cond_dist_limit = cond_dist_limit,
						.cond_dist_entry_stop = cond_dist_entry_stop,
						.cond_dist_entry_limit = cond_dist_entry_limit,
						.fxcm_trading_status = trade_status,
					};
					status.security_informations.emplace(symbol, std::move(security_info));
				}
				catch (FIX::FieldNotFound& error) {
					log::error<false>(
						"FixClient::onMessage[FIX44::TradingSessionStatus]: security info field not found {}",
						error.what()
					);
				}
				catch (...) {
					log::error<false>(
						"FixClient::onMessage[FIX44::TradingSessionStatus]: security info error"
					);
				}
			}

			status.trade_session_status = static_cast<common::fix::TradeSessionStatus>(
				FIX::IntConvertor::convert(message.getField(FIX::FIELD::TradSesStatus))
				);

			status.server_timezone = FIX::IntConvertor::convert(message.getField(FXCM_SERVER_TIMEZONE));
			status.server_timezone_name = message.getField(FXCM_SERVER_TIMEZONE);

			int params_count = FIX::IntConvertor::convert(message.getField(FXCM_NO_PARAMS));
			for (int i = 1; i <= params_count; i++) {
				FIX::FieldMap field_map = message.getGroupRef(i, FXCM_NO_PARAMS);
				status.session_parameters.emplace(
					field_map.getField(FXCM_PARAM_NAME),
					field_map.getField(FXCM_PARAM_VALUE)
				);
			}

			log::debug<dl4, false>(
				"FixClient::onMessage[FIX44::TradingSessionStatus]: publish trading session status {}",
				status.to_string()
			);

			trading_session_status_queue.push(status);
		} 
		catch(FIX::FieldNotFound &error) {
			log::error<false>(
				"FixClient::onMessage[FIX44::TradingSessionStatus]: field not found {}",
				error.what()
			);
		}
	}

	void FixClient::onMessage(const FIX44::CollateralInquiryAck& message, const FIX::SessionID& session_ID)
	{

	}

	// CollateralReport is a message containing important information for each account under the login. It is returned
	// as a response to CollateralInquiry. You will receive a CollateralReport for each account under your login.
	// Notable fields include Account(1) which is the AccountID and CashOutstanding(901) which is the account balance
	void FixClient::onMessage(const FIX44::CollateralReport& message, const FIX::SessionID& session_ID)
	{
		try {
			const auto& account = message.getField(FIX::FIELD::Account);
			account_ids.insert(account);

			log::debug<dl0, false>("FixClient::onMessage[FIX44::CollateralReport]: inserted account={}", account);

			// account balance, which is the cash balance in the account, not including any profit or losses on open trades
			double balance = FIX::DoubleConvertor::convert(message.getField(FIX::FIELD::CashOutstanding));
			double start_cash = FIX::DoubleConvertor::convert(message.getField(FIX::FIELD::StartCash));
			double end_cash = FIX::DoubleConvertor::convert(message.getField(FIX::FIELD::EndCash));
			double margin_ratio = FIX::DoubleConvertor::convert(message.getField(FIX::FIELD::MarginRatio));
			double margin = FIX::DoubleConvertor::convert(message.getField(FXCM_USED_MARGIN));
			double maintenance_margin = FIX::DoubleConvertor::convert(message.getField(FXCM_USED_MARGIN3));
			double cash_daily = FIX::DoubleConvertor::convert(message.getField(FXCM_CASH_DAILY));
			auto margin_call_status = parse_fxcm_margin_call_status(message.getField(FXCM_MARGIN_CALL));
			auto sending_time = parse_datetime(message.getHeader().getField(FIX::FIELD::SendingTime));

			// CollateralReport NoPartyIDs group can be inspected for additional account information such as AccountName or HedgingStatus
			FIX44::CollateralReport::NoPartyIDs group;
			message.getGroup(1, group); // CollateralReport will only have 1 NoPartyIDs group

			// Get the number of NoPartySubIDs repeating groups
			// For each group, get both the PartySubIDType and the PartySubID (the value)
			std::vector<std::pair<int, std::string>> party_sub_ids;
			int num_sub_id = FIX::IntConvertor::convert(group.getField(FIX::FIELD::NoPartySubIDs));
			for (int u = 1; u <= num_sub_id; u++) {
				FIX44::CollateralReport::NoPartyIDs::NoPartySubIDs sub_group;
				group.getGroup(u, sub_group);

				int sub_type = FIX::IntConvertor::convert(sub_group.getField(FIX::FIELD::PartySubIDType));
				const auto& sub_value = sub_group.getField(FIX::FIELD::PartySubID);
				party_sub_ids.emplace_back(std::make_pair(sub_type, sub_value));
			}

			FXCMCollateralReport collateral_report{
				.account = account,
				.sending_time = sending_time,
				.balance = balance,
				.start_cash = start_cash,
				.end_cash = end_cash,
				.margin_ratio = margin_ratio,
				.margin = margin,
				.maintenance_margin = maintenance_margin,
				.cash_daily = cash_daily,
				.margin_call_status = margin_call_status,
				.party_sub_ids = party_sub_ids
			};

			log::debug<dl4, false>(
				"FixClient::onMessage[FIX44::CollateralReport]: publish collateral report for account={} balance={}", 
				account, balance
			);

			collateral_report_queue.push(collateral_report);
		}
		catch (FIX::FieldNotFound& error) {
			log::error<false>(
				"FixClient::onMessage[FIX44::CollateralReport]: field not found {}",
				error.what()
			);
		}
	}

	void FixClient::onMessage(const FIX44::RequestForPositionsAck& message, const FIX::SessionID& session_ID)
	{
		FIX::PosReqStatus pos_req_status;
		FIX::PosReqResult pos_req_result;
		FIX::TotalNumPosReports total_num_reports;
		
		message.get(pos_req_status);
		message.get(pos_req_result);
		message.get(total_num_reports);

		if (pos_req_status == FIX::PosReqStatus_REJECTED) {
			log::error<false>(
				"FixClient::onMessage[FIX44::RequestForPositionsAck]: request for position rejected text={}",
				message.getField(FIX::FIELD::Text)
			);
		}

		// if a PositionReport is requested and no positions exist for that request, the Text field will
		// indicate that no positions matched the requested criteria 
		if (pos_req_result == FIX::PosReqResult_NO_POSITIONS_FOUND_THAT_MATCH_CRITERIA) {
			log::debug<dl1, false>(
				"FixClient::onMessage[FIX44::RequestForPositionsAck]: publish empty position report list text={}",
				message.getField(FIX::FIELD::Text)
			);
			position_snapshot_reports_queue.push(FXCMPositionReports());
		}
	}

	void FixClient::onMessage(const FIX44::PositionReport& message, const FIX::SessionID& session_ID)
	{
		try {
			FIX::Symbol symbol;
			FIX::Account account;
			FIX::Currency currency;
			FIX::PosReqType pos_req_type; 
			FIX::SettlPrice settle_price;
			FIX::SettlPriceType settle_price_type;
			FIX::UnsolicitedIndicator unsoliciated_ind;

			// valid for open and closed positions 
			std::string position_id;
			double interest = 0;
			double commission = 0;
			std::chrono::nanoseconds open_time;

			// valid only for open positions
			double used_margin = 0;

			// valid only for closed positions
			double close_pnl = 0;
			double close_settle_price = 0;
			std::chrono::nanoseconds close_time;
			std::string close_order_id;
			std::string close_cl_ord_id;

			message.get(symbol);
			message.get(account);
			message.get(currency);
			message.get(pos_req_type);
			message.get(settle_price);
			message.get(settle_price_type);

			position_id = message.getField(FXCM_POS_ID);
			interest = FIX::DoubleConvertor::convert(message.getField(FXCM_POS_INTEREST));
			commission = FIX::DoubleConvertor::convert(message.getField(FXCM_POS_COMMISSION));
			open_time = parse_datetime(message.getField(FXCM_POS_OPEN_TIME));

			int num_net_positions = FIX::IntConvertor::convert(message.getField(FIX::FIELD::NoPositions));
			if (num_net_positions > 1) {
				log::error<false>("FixClient::onMessage[FIX44::PositionReport] error: num_net_positions={} > 1 not expected", num_net_positions);
			}
			FIX44::PositionReport::NoPositions group;
			message.getGroup(1, group);

			int position_qty = 0;
			if (group.isSetField(FIX::FIELD::LongQty)) {
				position_qty = FIX::IntConvertor::convert(group.getField(FIX::FIELD::LongQty));
			} 
			else if (group.isSetField(FIX::FIELD::ShortQty)) {
				position_qty = -std::abs(FIX::IntConvertor::convert(group.getField(FIX::FIELD::ShortQty)));
			}
			else {
				log::error<false>("FixClient::onMessage[FIX44::PositionReport] error: neither LongQty nor ShortQty defined");
			}
			
			bool is_open = true;
			if (pos_req_type == 0) { // ‘0’ - Open Position (positions)
				is_open = true;
				used_margin = FIX::DoubleConvertor::convert(message.getField(FXCM_USED_MARGIN));

			} else if (pos_req_type == 1) { // ‘1’ - Closed Position (trades)
				is_open = false;
				close_pnl = FIX::DoubleConvertor::convert(message.getField(FXCM_CLOSE_PNL));
				close_settle_price = FIX::DoubleConvertor::convert(message.getField(FXCM_CLOSE_SETTLE_PRICE));
				close_time = parse_datetime(message.getField(FXCM_POS_CLOSE_TIME));
				close_order_id = message.getField(FXCM_CLOSE_ORDER_ID);
				close_cl_ord_id = message.getField(FXCM_CLOSE_CL_ORD_ID);
			}
			else {
				log::error<false>("FixClient::onMessage[FIX44::PositionReport] error: unexpected pos_req_type={}", pos_req_type.getValue());
			}

			bool is_closed = !is_open;
			FXCMPositionReport position_report{
				.account = account.getValue(),
				.symbol = symbol.getValue(),
				.currency = currency.getValue(),
				.position_id = position_id,
				.settle_price = settle_price.getValue(),
				.is_open = is_open,
				.interest = interest,
				.commission = commission,
				.open_time = open_time,
				.used_margin = is_open ? used_margin : std::optional<double>(),
				.close_pnl = is_closed ? close_pnl : std::optional<double>(),
				.close_settle_price = is_closed ? close_pnl : std::optional<double>(),
				.close_time = is_closed ? close_time : std::optional<std::chrono::nanoseconds>(),
				.close_order_id = is_closed ? close_order_id : std::optional<std::string>(),
				.close_cl_ord_id = is_closed ? close_cl_ord_id : std::optional<std::string>(),
			};

			message.get(unsoliciated_ind);

			if (unsoliciated_ind.getValue()) { // this message is result of a subscription
				log::debug<dl1, false>("FixClient::onMessage[FIX44::PositionReport]: publish position subscription report");

				position_report_queue.push(position_report);
			}
			else { // 325=N (snapshot request)
				FIX::TotalNumPosReports total_num_reports;
				message.get(total_num_reports);

				bool last = false;
				if (message.isSetField(FIX::FIELD::LastRptRequested)) {
					last = message.getField(FIX::FIELD::LastRptRequested) == "Y";
				}

				position_report_list.emplace_back(position_report);

				bool is_last = total_num_reports.getValue() == position_report_list.size();
				if (is_last) {
					log::debug<dl1, false>(
						"FixClient::onMessage[FIX44::PositionReport]: publish {} position snapshot reports, last={}", 
						position_report_list.size(), (last ? "true" : "false")
					);
					position_snapshot_reports_queue.push(
						FXCMPositionReports{.reports=std::move(position_report_list)}
					);

					position_report_list = std::vector<FXCMPositionReport>();
				}
			}
		}
		catch (FIX::FieldNotFound& error) {
			log::error<false>(
				"FixClient::onMessage[FIX44::PositionReport]: field not found {}",
				error.what()
			);
		}
	}

	void FixClient::onMessage(const FIX44::MarketDataSnapshotFullRefresh& message, const FIX::SessionID& session_ID)
	{
		auto& symbol = message.getField(FIX::FIELD::Symbol);

		auto& top_of_book = top_of_books.insert_or_assign(symbol, TopOfBook(symbol)).first->second;
		double session_high_price;
		double session_low_price;
		std::chrono::nanoseconds timestamp;

		FIX::MDEntryType entry_type;
		FIX::MDEntryDate date;
		FIX::MDEntryTime time;
		FIX::MDEntrySize size;
		FIX::MDEntryPx price;

		int entry_count = FIX::IntConvertor::convert(message.getField(FIX::FIELD::NoMDEntries));
		for (int i = 1; i <= entry_count; i++) {
			FIX44::MarketDataSnapshotFullRefresh::NoMDEntries group;
			message.getGroup(i, group);
			if (i == 1) {
				timestamp = parse_date_and_time(group);
				top_of_book.timestamp = timestamp;
			}
			
			group.get(entry_type);
			group.get(price);

			if (entry_type == FIX::MDEntryType_BID) {  
				top_of_book.bid_price = price;
				if (group.getIfSet(size)) {
					top_of_book.bid_volume = size;
				}
			}
			else if (entry_type == FIX::MDEntryType_OFFER) {  
				top_of_book.ask_price = price;
				if (group.getIfSet(size)) {
					top_of_book.ask_volume = size;
				}
			}
			else if (entry_type == FIX::MDEntryType_TRADING_SESSION_HIGH_PRICE) {
				session_high_price = price;
			}
			else if (entry_type == FIX::MDEntryType_TRADING_SESSION_LOW_PRICE) {
				session_low_price = price;
			}
			else {
				log::error<false>(
					"FixClient::onMessage[FIX44::MarketDataSnapshotFullRefresh]: unexpected md entry type={} price={}", 
					entry_type.getValue(), price.getValue()
				);
			}
		}

		log::debug<dl5, false>("FixClient::onMessage[FIX44::MarketDataSnapshotFullRefresh]: top={}", top_of_book.to_string());

		top_of_book_queue.push(top_of_book); // publish snapshot related top of book
	}

	void FixClient::onMessage(const FIX44::MarketDataIncrementalRefresh& message, const FIX::SessionID&)
	{
		double session_high_price;
		double session_low_price;
		std::chrono::nanoseconds timestamp = common::get_current_system_clock();

		FIX::MDEntryType entry_type;
		FIX::MDEntryDate date;
		FIX::MDEntryTime time;
		FIX::MDEntrySize size;
		FIX::MDEntryPx price;

		std::set<TopOfBook*> change_set;

		int entry_count = FIX::IntConvertor::convert(message.getField(FIX::FIELD::NoMDEntries));
		for (int i = 1; i <= entry_count; i++) {
			FIX44::MarketDataIncrementalRefresh::NoMDEntries group;
			message.getGroup(i, group);
			auto& symbol = group.getField(FIX::FIELD::Symbol);

			auto it = top_of_books.find(symbol);
			if (it == top_of_books.end()) {
				log::error<false>("FixClient::onMessage[FIX44::MarketDataIncrementalRefresh]: did not find symbol={} in top of books", symbol);
				continue;
			}

			if (i == 1) {
				timestamp = parse_date_and_time(group);
			}

			group.get(entry_type);
			group.get(price);

			if (entry_type == FIX::MDEntryType_BID) {  
				it->second.bid_price = price;
				if (group.getIfSet(size)) {
					it->second.bid_volume = size;
				}
				it->second.timestamp = timestamp;
				change_set.insert(&it->second);
			}
			else if (entry_type == FIX::MDEntryType_OFFER) { 
				it->second.ask_price = price;
				if (group.getIfSet(size)) {
					it->second.ask_volume = size;
				}
				it->second.timestamp = timestamp;
				change_set.insert(&it->second);
			}
			else if (entry_type == FIX::MDEntryType_TRADING_SESSION_HIGH_PRICE) {
				session_high_price = price;
			}
			else if (entry_type == FIX::MDEntryType_TRADING_SESSION_LOW_PRICE) {
				session_low_price = price;
			}
			else {
				log::error<false>(
					"FixClient::onMessage[FIX44::MarketDataIncrementalRefresh]: unexpected md entry type={} price={}", 
					entry_type.getValue(), price.getValue()
				);
			}
		}

		for (auto top_of_book : change_set) {
			log::debug<dl5, false>("FixClient::onMessage[FIX44::MarketDataIncrementalRefresh]: top={}", top_of_book->to_string());

			top_of_book_queue.push(*top_of_book); // publish snapshot related top of book
		}
	}

	void FixClient::onMessage(const FIX44::ExecutionReport& message, const FIX::SessionID&) 
	{
		try {
			FIX::Symbol symbol;
			FIX::ExecID exec_id;
			FIX::ExecType exec_type;
			FIX::OrderID ord_id;
			FIX::ClOrdID cl_ord_id;
			FIX::OrdStatus ord_status;
			FIX::OrdType ord_type;
			FIX::Side side;
			FIX::TimeInForce tif;
			FIX::Price price;
			FIX::OrderQty order_qty;
			FIX::LastQty last_qty;
			FIX::LastPx last_px;
			FIX::LeavesQty leaves_qty;
			FIX::CumQty cum_qty;
			FIX::AvgPx avg_px;
			FIX::Text text;
			FIX::LastRptRequested last_rpt_requested;
			FIX::MassStatusReqID mass_status_req_id;

			message.get(exec_type);

			if (exec_type != FIX::ExecType_ORDER_STATUS) {
				message.get(symbol);
				message.get(exec_id);
				message.get(ord_id);
				message.get(cl_ord_id);
				message.get(ord_status);
				message.getIfSet(ord_type);
				message.get(side);
				message.get(tif);
				message.get(price);
				message.get(avg_px);
				message.get(order_qty);
				message.get(last_qty);
				message.get(leaves_qty);
				message.get(last_px);
				message.get(cum_qty);
				message.getIfSet(text);

				std::string position_id;
				if (message.isSetField(FXCM_FIX_FIELDS::FXCM_POS_ID)) {
					position_id = message.getField(FXCM_FIX_FIELDS::FXCM_POS_ID);
				}
				else {
					log::error<false>("FixClient::on_message[ExecutionReport]: no position id set {}", fix_string(message));
				}

				ExecReport report(
					symbol.getString(),
					ord_id.getString(),
					cl_ord_id.getString(),
					exec_id.getString(),
					exec_type.getValue(),
					ord_type.getValue(),
					ord_status.getValue(),
					side.getValue(),
					tif.getValue(),
					price.getValue(),
					avg_px.getValue(),
					order_qty.getValue(),
					last_qty.getValue(),
					last_px.getValue(),
					cum_qty.getValue(),
					leaves_qty.getValue(),
					text.getString(),
					position_id
				);

				log::debug<dl0, false>("FixClient::on_message[ExecutionReport]: {}", report.to_string("position_id"));

				exec_report_queue.push(report);
			}
			else {
				message.get(ord_status);

				if (ord_status != FIX::OrdStatus_REJECTED) {
					message.get(symbol);
					message.get(exec_id);
					message.get(ord_id);
					message.getIfSet(cl_ord_id);
					message.getIfSet(ord_type);
					message.get(side);
					message.get(tif);
					message.get(price);
					message.get(avg_px);
					message.get(order_qty);
					message.get(last_qty);
					message.get(leaves_qty);
					message.get(last_px);
					message.get(cum_qty);

					message.getIfSet(text);
					message.getIfSet(mass_status_req_id);

					auto tot_num_reports = 0;
					if (message.isSetField(FIX::FIELD::TotNumReports)) {
						const auto& s = message.getField(FIX::FIELD::TotNumReports);
						tot_num_reports = std::atoi(s.c_str());
					}

					auto last_rpt_requested = true;
					if (message.isSetField(FIX::FIELD::LastRptRequested)) {
						const auto& s = message.getField(FIX::FIELD::LastRptRequested);
						last_rpt_requested = s == "Y";
					}

					std::string position_id;
					if (message.isSetField(FXCM_FIX_FIELDS::FXCM_POS_ID)) {
						position_id = message.getField(FXCM_FIX_FIELDS::FXCM_POS_ID);
					}
					else {
						log::error<false>("FixClient::on_message[ExecutionReport]: no position id set {}", fix_string(message));
					}

					StatusExecReport report(
						symbol.getString(),
						ord_id.getString(),
						cl_ord_id.getString(),
						exec_id.getString(),
						mass_status_req_id.getString(),
						exec_type.getValue(),
						ord_type.getValue(),
						ord_status.getValue(),
						side.getValue(),
						tif.getValue(),
						price.getValue(),
						avg_px.getValue(),
						order_qty.getValue(),
						last_qty.getValue(),
						last_px.getValue(),
						cum_qty.getValue(),
						leaves_qty.getValue(),
						text.getString(),
						tot_num_reports,
						last_rpt_requested,
						position_id
					);

					log::debug<dl0, false>(
						"FixClient::on_message[StatusExecutionReport(ord_status!=FIX::OrdStatus_REJECTED)]: {}", 
						report.to_string("position_id")
					);

					status_exec_report_queue.push(report);
				}
				else {
					// skip some fields because of Quickfix parsing issues
					message.get(exec_id);
					message.get(mass_status_req_id);
					message.getIfSet(text);

					StatusExecReport report(
						"N/A",
						"N/A",
						"N/A",
						exec_id.getString(),
						mass_status_req_id.getString(),
						exec_type.getValue(),
						'0',
						FIX::OrdStatus_REJECTED,
						FIX::Side_UNDISCLOSED,
						FIX::TimeInForce_GOOD_TILL_CANCEL,
						0,
						0,
						0,
						0,
						0,
						0,
						0,
						text.getString(),
						0,
						true
					);

					log::debug<dl0, false>("FixClient::on_message[StatusExecutionReport(ord_status==FIX::OrdStatus_REJECTED)]: {}", report.to_string());

					status_exec_report_queue.push(report);
				}
			}
		}
		catch (...) {
			log::debug<dl0, false>("FixClient::on_message[ExecutionReport]: error parsing execution report {}", fix_string(message));
		}
	}
	
	void FixClient::onMessage(const FIX44::OrderCancelReject& message, const FIX::SessionID&) 
	{
		FIX::Text text;
		message.getIfSet(text);
		auto raw = fix_string(message);
		auto service_msg = reject_service_message("OrderCancelReject", text.getString(), raw);
		service_message_queue.push(service_msg);

		log::error<false>("onMessage[FIX44::OrderCancelReject]: {}", raw);
	}

	void FixClient::onMessage(const FIX44::BusinessMessageReject& message, const FIX::SessionID&)
	{
		FIX::Text text;
		message.getIfSet(text);
		auto raw = fix_string(message);
		auto service_msg = reject_service_message("BusinessMessageReject", text.getString(), raw);
		service_message_queue.push(service_msg);

		log::error<false>("onMessage[FIX44::OrderCancelReject]: {}", raw);
	}

	void FixClient::onMessage(const FIX44::MarketDataRequestReject& message, const FIX::SessionID& session_ID)
	{
		FIX::Text text;
		message.getIfSet(text);
		auto raw = fix_string(message);
		auto service_msg = reject_service_message("BusinessMessageReject", text.getString(), raw);
		service_message_queue.push(service_msg);

		log::error<false>("onMessage[FIX44::MarketDataRequestReject]: {}", raw);
	}

	FIX::Message FixClient::market_data_snapshot(const FIX::Symbol& symbol) {
		FIX44::MarketDataRequest request;
		auto request_id = std::format("{}_{}", symbol.getString(), id_generator.genID());
		request.setField(FIX::MDReqID(request_id));
		request.setField(FIX::SubscriptionRequestType(FIX::SubscriptionRequestType_SNAPSHOT));
		request.setField(FIX::MDUpdateType(FIX::MDUpdateType_FULL_REFRESH));  
		request.setField(FIX::MarketDepth(0));
		request.setField(FIX::NoRelatedSym(1));
		FIX44::MarketDataRequest::NoRelatedSym symbols_group;
		symbols_group.setField(symbol);
		request.addGroup(symbols_group);
		FIX44::MarketDataRequest::NoMDEntryTypes entry_types;
		entry_types.setField(FIX::MDEntryType(FIX::MDEntryType_BID));
		entry_types.setField(FIX::MDEntryType(FIX::MDEntryType_OFFER));
		entry_types.setField(FIX::MDEntryType(FIX::MDEntryType_TRADING_SESSION_HIGH_PRICE));
		entry_types.setField(FIX::MDEntryType(FIX::MDEntryType_TRADING_SESSION_LOW_PRICE));
		request.addGroup(entry_types);

		log::debug<dl4, false>("FixClient::market_data_snapshot: {}", fix_string(request));

		FIX::Session::sendToTarget(request, market_data_session_id);

		return request;
	}

	std::optional<FIX::Message> FixClient::subscribe_market_data(
		const FIX::Symbol& symbol, 
		bool incremental
	) {
		auto it = market_data_subscriptions.find(symbol.getString());

		if (it != market_data_subscriptions.end()) {
			log::error<true>("subscribe_market_data: error - market data already subscribed for symbol {}", symbol.getString());
			return std::optional<FIX::Message>();
		}
		else {
			auto request_id = std::format("{}_{}", symbol.getString(), id_generator.genID());
			auto subscription_request_type = FIX::SubscriptionRequestType_SNAPSHOT_AND_UPDATES;
			auto md_update_type = incremental ? FIX::MDUpdateType_INCREMENTAL_REFRESH : FIX::MDUpdateType_FULL_REFRESH;
			FIX44::MarketDataRequest request;
			request.setField(FIX::MDReqID(request_id));
			request.setField(FIX::SubscriptionRequestType(subscription_request_type));
			request.setField(FIX::MDUpdateType(md_update_type));
			request.setField(FIX::MarketDepth(0));
			request.setField(FIX::NoRelatedSym(1));
			FIX44::MarketDataRequest::NoRelatedSym symbols_group;
			symbols_group.setField(symbol);
			request.addGroup(symbols_group);
			FIX44::MarketDataRequest::NoMDEntryTypes entry_types;
			entry_types.setField(FIX::MDEntryType(FIX::MDEntryType_BID));
			entry_types.setField(FIX::MDEntryType(FIX::MDEntryType_OFFER));
			entry_types.setField(FIX::MDEntryType(FIX::MDEntryType_TRADING_SESSION_HIGH_PRICE));
			entry_types.setField(FIX::MDEntryType(FIX::MDEntryType_TRADING_SESSION_LOW_PRICE));
			request.addGroup(entry_types);

			log::debug<dl4, false>("FixClient::subscribe_market_data: {}", fix_string(request));

			FIX::Session::sendToTarget(request, market_data_session_id);

			market_data_subscriptions.emplace(symbol.getString(), request_id);

			return std::optional<FIX::Message>(request);
		}
	}

	std::optional<FIX::Message> FixClient::unsubscribe_market_data(const FIX::Symbol& symbol)
	{
		auto it = market_data_subscriptions.find(symbol.getString());

		if (it != market_data_subscriptions.end()) {
			const auto& request_id = it->second;
			FIX44::MarketDataRequest request;
			request.setField(FIX::MDReqID(request_id));
			request.setField(FIX::SubscriptionRequestType(FIX::SubscriptionRequestType_DISABLE_PREVIOUS_SNAPSHOT));
			request.setField(FIX::MarketDepth(0));
			request.setField(FIX::NoRelatedSym(1));
			FIX44::MarketDataRequest::NoRelatedSym symbols_group;
			symbols_group.setField(symbol);
			request.addGroup(symbols_group);
			FIX44::MarketDataRequest::NoMDEntryTypes entry_types;
			entry_types.setField(FIX::MDEntryType(FIX::MDEntryType_BID));
			entry_types.setField(FIX::MDEntryType(FIX::MDEntryType_OFFER));
			entry_types.setField(FIX::MDEntryType(FIX::MDEntryType_TRADING_SESSION_HIGH_PRICE));
			entry_types.setField(FIX::MDEntryType(FIX::MDEntryType_TRADING_SESSION_LOW_PRICE));
			request.addGroup(entry_types);

			log::debug<dl4, false>("FixClient::unsubscribe_market_data: {}", fix_string(request));

			FIX::Session::sendToTarget(request, market_data_session_id);

			return std::optional<FIX::Message>(request);
		}
		else {
			log::error<true>("unsubscribe_market_data: error - market data not subscribed for symbol {}", symbol.getString());
			return std::optional<FIX::Message>();
		}
	}

	FIX::Message FixClient::trading_session_status_request()
	{
		FIX44::TradingSessionStatusRequest request;
		request.setField(FIX::TradSesReqID(id_generator.genID()));
		request.setField(FIX::TradingSessionID("FXCM"));
		request.setField(FIX::SubscriptionRequestType(FIX::SubscriptionRequestType_SNAPSHOT));
		
		log::debug<dl4, false>("FixClient::trading_session_status_request: {}", fix_string(request));

		FIX::Session::sendToTarget(request, trading_session_id);

		return request;
	}

	FIX::Message FixClient::collateral_inquiry(const char subscription_req_type)
	{
		// will trigger a CollateralReport for each account under the login
		FIX44::CollateralInquiry request;
		request.setField(FIX::CollInquiryID(id_generator.genID()));
		request.setField(FIX::TradingSessionID("FXCM"));
		request.setField(FIX::SubscriptionRequestType(subscription_req_type));

		log::debug<dl4, false>("FixClient::collateral_inquiry: {}", fix_string(request));

		FIX::Session::sendToTarget(request, trading_session_id);

		return request;
	}

	FIX::Message FixClient::request_for_positions(const std::string& account, int pos_req_type, const char subscription_req_type)
	{
		FIX44::RequestForPositions request;
		request.setField(FIX::PosReqID(id_generator.genID()));
		request.setField(FIX::PosReqType(pos_req_type));
		request.setField(FIX::Account(account));
		request.setField(FIX::SubscriptionRequestType(subscription_req_type));
		request.setField(FIX::AccountType(FIX::AccountType_CARRIED_NON_CUSTOMER_SIDE_CROSS_MARGINED));
		request.setField(FIX::TransactTime());
		request.setField(FIX::ClearingBusinessDate());
		request.setField(FIX::TradingSessionID("FXCM"));
		request.setField(FIX::NoPartyIDs(1));
		FIX44::RequestForPositions::NoPartyIDs parties_group;
		parties_group.setField(FIX::PartyID("FXCM ID"));
		parties_group.setField(FIX::PartyIDSource('D'));
		parties_group.setField(FIX::PartyRole(3));
		parties_group.setField(FIX::NoPartySubIDs(1));
		FIX44::RequestForPositions::NoPartyIDs::NoPartySubIDs sub_parties;
		sub_parties.setField(FIX::PartySubIDType(FIX::PartySubIDType_SECURITIES_ACCOUNT_NUMBER));
		sub_parties.setField(FIX::PartySubID(account));
		parties_group.addGroup(sub_parties);
		request.addGroup(parties_group);

		log::debug<dl1, false>("FixClient::request_for_positions: {}", fix_string(request));

		FIX::Session::sendToTarget(request, trading_session_id);

		return request;
	}

	FIX::Message FixClient::order_mass_status_request() {
		FIX44::OrderMassStatusRequest request;
		request.setField(FIX::MassStatusReqID(id_generator.genID()));
		request.setField(FIX::MassStatusReqType(FIX::MassStatusReqType_STATUS_FOR_ALL_ORDERS));

		log::debug<dl1, false>("FixClient::order_mass_status_request: {}", fix_string(request));

		FIX::Session::sendToTarget(request, trading_session_id);

		return request;
	}

	std::optional<FIX::Message> FixClient::new_order_single(
		const FIX::Symbol& symbol, 
		const FIX::ClOrdID& cl_ord_id, 
		const FIX::Side& side, 
		const FIX::OrdType& ord_type, 
		const FIX::TimeInForce& tif,
		const FIX::OrderQty& order_qty, 
		const FIX::Price& price, 
		const FIX::StopPx& stop_price, 
		const std::optional<std::string>& position_id,
		const std::optional<FIX::Account>& account
	) const {
		FIX44::NewOrderSingle order(
			cl_ord_id,
			side,
			FIX::TransactTime(),
			ord_type);

		order.set(FIX::HandlInst('1'));
		order.set(symbol);
		order.set(order_qty);
		order.set(tif);
		
		order.setField(FIX::TradingSessionID("FXCM"));  // TODO do it via group?

		if (account.has_value()) {
			order.set(account.value());
		}
		else if (!account_ids.empty()) {
			order.set(FIX::Account(*account_ids.begin()));
		}
		else {
			return std::optional<FIX::Message>();  
		}
		 
		if (ord_type == FIX::OrdType_LIMIT || ord_type == FIX::OrdType_STOP_LIMIT) {
			order.set(price);
		}

		if (ord_type == FIX::OrdType_STOP || ord_type == FIX::OrdType_STOP_LIMIT) {
			order.set(stop_price);
		}

		if (position_id.has_value()) {
			order.setField(FXCM_FIX_FIELDS::FXCM_POS_ID, position_id.value());
		}

		log::debug<dl4, false>("FixClient::newOrderSingle[{}]: {}" , trading_session_id.toString(), fix_string(order));

		FIX::Session::sendToTarget(order, trading_session_id);

		return std::optional<FIX::Message>(order);
	}	

	std::optional<FIX::Message> FixClient::order_cancel_request(
		const FIX::Symbol& symbol,
		const FIX::OrderID& ord_id,
		const FIX::OrigClOrdID& orig_cl_ord_iD,
		const FIX::ClOrdID& cl_ord_id,
		const FIX::Side& side,
		const FIX::OrderQty& order_qty,
		const std::optional<std::string>& position_id,
		const std::optional<FIX::Account>& account
	) const {
		FIX44::OrderCancelRequest request(
			orig_cl_ord_iD,
			cl_ord_id, 
			side, 
			FIX::TransactTime());

		request.set(symbol);
		request.set(ord_id);
		request.set(order_qty);

		if (account.has_value()) {
			request.set(account.value());
		}
		else if (!account_ids.empty()) {
			request.set(FIX::Account(*account_ids.begin()));
		}
		else {
			return std::optional<FIX::Message>();
		}

		if (position_id.has_value()) {
			request.setField(FXCM_FIX_FIELDS::FXCM_POS_ID, position_id.value());
		}

		log::debug<dl4, false>("FixClient::orderCancelRequest[{}]: {}", trading_session_id.toString(), fix_string(request));

		FIX::Session::sendToTarget(request, trading_session_id);

		return std::optional<FIX::Message>(request);
	}

	std::optional<FIX::Message> FixClient::order_cancel_replace_request(
		const FIX::Symbol& symbol,
		const FIX::OrderID& ord_id,
		const FIX::OrigClOrdID& orig_cl_ord_id,
		const FIX::ClOrdID& cl_ord_id,
		const FIX::Side& side,
		const FIX::OrdType& ord_type,
		const FIX::OrderQty& order_qty,
		const FIX::Price& price,
		const FIX::TimeInForce& tif,
		const std::optional<std::string>& position_id,
		const std::optional<FIX::Account>& account
	) const {
		FIX44::OrderCancelReplaceRequest request(
			orig_cl_ord_id, 
			cl_ord_id,
			side, 
			FIX::TransactTime(),
			ord_type);

		request.set(FIX::HandlInst('1'));
		request.set(symbol);
		request.set(ord_id);
		request.set(order_qty);
		request.set(price);
		request.set(tif);

		if (account.has_value()) {
			request.set(account.value());
		}
		else if (!account_ids.empty()) {
			request.set(FIX::Account(*account_ids.begin()));
		}
		else {
			return std::optional<FIX::Message>();
		}

		if (position_id.has_value()) {
			request.setField(FXCM_FIX_FIELDS::FXCM_POS_ID, position_id.value());
		}

		log::debug<dl0, false>("FixClient::orderCancelReplaceRequest[{}]: {}", trading_session_id.toString(), fix_string(request));

		FIX::Session::sendToTarget(request, trading_session_id);

		return std::optional<FIX::Message>(request);
	}

	void FixClient::logout() {
		FIX44::Logout logout;
		FIX::Session::sendToTarget(logout, trading_session_id);
		FIX::Session::sendToTarget(logout, market_data_session_id);
	}

	void FixClient::heartbeat(const std::string& test_req_id, bool trading_sess) {
		FIX44::Heartbeat hb;
		hb.setField(FIX::TestReqID(test_req_id));
		if (trading_sess)
			FIX::Session::sendToTarget(hb, trading_session_id);
		else
			FIX::Session::sendToTarget(hb, market_data_session_id);
	}

	bool FixClient::has_book(const std::string& symbol) {
		return top_of_books.contains(symbol);
	}

	TopOfBook FixClient::top_of_book(const std::string& symbol) {
		auto it = top_of_books.find(symbol);
		if (it != top_of_books.end()) {
			return it->second;
		}
		else {
			throw std::runtime_error(std::format("symbol {} not found in top_of_books", symbol));
		}
	}
}
