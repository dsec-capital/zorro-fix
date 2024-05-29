#ifdef _MSC_VER
#pragma warning(disable : 4503 4355 4786)
#endif

#include "pch.h"

#include "application.h"

#include "quickfix/config.h"
#include "quickfix/Session.h"

#include "spdlog/spdlog.h"

#include "common/time_utils.h"

namespace zorro {

	using namespace common;

	std::string fix_string(const FIX::Message& msg) {
		auto s = msg.toString();
		std::replace(s.begin(), s.end(), '\x1', '|');  
		return s;
	}

	template<typename G>
	inline std::chrono::nanoseconds parse_date_and_time(const G& g, int date_field = FIX::FIELD::MDEntryDate, int time_field = FIX::FIELD::MDEntryTime) {
		std::stringstream in;
		in << g.getField(date_field) << "-" << g.getField(time_field);
		std::chrono::time_point<std::chrono::system_clock>  tp;
		in >> std::chrono::parse("%Y%m%d-%T", tp);
		return std::chrono::duration_cast<std::chrono::nanoseconds>(tp.time_since_epoch());
	}

	template<typename G>
	inline std::chrono::nanoseconds parse_datetime(const G& g, int date_time_field = FIX::FIELD::SendingTime) {
		std::stringstream in;
		in << g.getField(date_time_field);
		std::chrono::time_point<std::chrono::system_clock>  tp;
		in >> std::chrono::parse("%Y%m%d-%T", tp);
		return std::chrono::duration_cast<std::chrono::nanoseconds>(tp.time_since_epoch());
	}

	Application::Application(
		const FIX::SessionSettings& session_settings,
		BlockingTimeoutQueue<ExecReport>& exec_report_queue,
		BlockingTimeoutQueue<TopOfBook>& top_of_book_queue
	) : session_settings(session_settings)
	  , exec_report_queue(exec_report_queue)
	  , top_of_book_queue(top_of_book_queue)
	  , done(false)
	  , logged_in(0)
     , order_tracker("account")
	{}

	bool Application::is_logged_in() const {
		return logged_in.load() >= 2;
	}

	int Application::log_in_count() const {
		return logged_in.load();
	}

	const std::set<std::string>& Application::get_account_ids() const {
		return account_ids;
	}

	bool Application::is_trading_session(const FIX::SessionID& sess_id) const {
		return !sess_id.getSenderCompID().getString().starts_with("MD_");
	}

	bool Application::is_market_data_session(const FIX::SessionID& sess_id) const {
		return sess_id.getSenderCompID().getString().starts_with("MD_");
	}

	void Application::onCreate(const FIX::SessionID& sess_id) {
		// FIX Session created. We must now logon. QuickFIX will automatically send the Logon(A) message
		auto is_trading = true;
		if (is_market_data_session(sess_id)) {
			market_data_session_id = sess_id;
			is_trading = false;
		}
		else {
			trading_session_id = sess_id;
		}
		spdlog::debug(
			"Application::onCreate is trading={}, sessionID={}", is_trading, sess_id.toString()
		);
	}

	void Application::onLogon(const FIX::SessionID& sess_id)
	{
		logged_in++;
		spdlog::debug("Application::onLogon sessionID={} login count={}", sess_id.toString(), logged_in.load());

		if (is_trading_session(sess_id)) {

		}
	}

	void Application::onLogout(const FIX::SessionID& sessionID)
	{
		logged_in--;
		spdlog::debug("Application::onLogout sessionID={}", sessionID.toString());
	}

	void Application::fromAdmin(
		const FIX::Message& message, const FIX::SessionID& sessionID
	) EXCEPT(FIX::FieldNotFound, FIX::IncorrectDataFormat, FIX::IncorrectTagValue, FIX::RejectLogon)
	{
		spdlog::debug("Application::fromAdmin IN <{}> {}", sessionID.toString(), fix_string(message));
	}

	void Application::toAdmin(FIX::Message& message, const FIX::SessionID& sessionID) 
	{
		// Logon (A) requires to set the Username and Password fields
		const auto& msg_type = message.getHeader().getField(FIX::FIELD::MsgType);
		if (msg_type == "A") {
			// get both username and password from settings file
			auto user = session_settings.get().getString("Username");
			auto pass = session_settings.get().getString("Password");
			message.setField(FIX::Username(user));
			message.setField(FIX::Password(pass));
		}

		// all messages sent to FXCM must contain the TargetSubID field (both Administrative and Application messages) 
		auto sub_ID = session_settings.get().getString("TargetSubID");
		message.getHeader().setField(FIX::TargetSubID(sub_ID));

		spdlog::debug("Application::toAdmin OUT <{}> {}", sessionID.toString(), fix_string(message));
	}

	void Application::fromApp(const FIX::Message& message, const FIX::SessionID& sessionID)
		EXCEPT(FIX::FieldNotFound, FIX::IncorrectDataFormat, FIX::IncorrectTagValue, FIX::UnsupportedMessageType)
	{
		spdlog::debug("Application::fromApp IN <{}> {}", sessionID.toString(), fix_string(message));
		crack(message, sessionID);
	}

	void Application::toApp(FIX::Message& message, const FIX::SessionID& sessionID) EXCEPT(FIX::DoNotSend)
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

		spdlog::debug("Application::toApp OUT <{}> {}", sessionID.toString(), fix_string(message));
	}

	// The TradingSessionStatus message is used to provide an update on the status of the market. Furthermore, 
	// this message contains useful system parameters as well as information about each trading security (embedded SecurityList).
	// TradingSessionStatus should be requested upon successful Logon and subscribed to. The contents of the
	// TradingSessionStatus message, specifically the SecurityList and system parameters, should dictate how fields
	// are set when sending messages to FXCM.
	void Application::onMessage(const FIX44::TradingSessionStatus& message, const FIX::SessionID& session_ID)
	{
		// Check TradSesStatus field to see if the trading desk is open or closed
		//	2 = Open
		//	3 = Closed
		auto trad_status = message.getField(FIX::FIELD::TradSesStatus);

		// Within the TradingSessionStatus message is an embeded SecurityList. From SecurityList we can see
		// the list of available trading securities and information relevant to each; e.g., point sizes,
		// minimum and maximum order quantities by security, etc. 
		
		//cout << "  SecurityList via TradingSessionStatus -> " << endl;

		int symbols_count = FIX::IntConvertor::convert(message.getField(FIX::FIELD::NoRelatedSym));
		for (int i = 1; i <= symbols_count; i++) {
			// Get the NoRelatedSym group and for each, print out the Symbol value
			FIX44::SecurityList::NoRelatedSym symbols_group;
			message.getGroup(i, symbols_group);
			auto symbol = symbols_group.getField(FIX::FIELD::Symbol);
			//cout << "    Symbol -> " << symbol << endl;
		}
		
		// Also within TradingSessionStatus are FXCM system parameters. This includes important information
		// such as account base currency, server time zone, the time at which the trading day ends, and more.
		
		//cout << "  System Parameters via TradingSessionStatus -> " << endl;
		
		// Read field FXCMNoParam (9016) which shows us how many system parameters are 
		// in the message
		int params_count = FIX::IntConvertor::convert(message.getField(FXCM_NO_PARAMS)); // FXCMNoParam (9016)
		for (int i = 1; i <= params_count; i++) {
			// For each paramater, print out both the name of the paramater and the value of the 
			// paramater. FXCMParamName (9017) is the name of the paramater and FXCMParamValue(9018)
			// is of course the paramater value
			FIX::FieldMap field_map = message.getGroupRef(i, FXCM_NO_PARAMS);

			std::cout << "    Param Name -> " << field_map.getField(FXCM_PARAM_NAME)
				<< " - Param Value -> " << field_map.getField(FXCM_PARAM_VALUE) << std::endl;
		}

		// Request accounts under our login
		//GetAccounts();

		// ** Note on Text(58) ** 
		// You will notice that Text(58) field is always set to "Market is closed. Any trading
		// functionality is not available." This field is always set to this value; therefore, do not 
		// use this field value to determine if the trading desk is open. As stated above, use TradSesStatus for this purpose
	}

	void Application::onMessage(const FIX44::CollateralInquiryAck& message, const FIX::SessionID& session_ID)
	{

	}

	// CollateralReport is a message containing important information for each account under the login. It is returned
	// as a response to CollateralInquiry. You will receive a CollateralReport for each account under your login.
	// Notable fields include Account(1) which is the AccountID and CashOutstanding(901) which is the account balance
	void Application::onMessage(const FIX44::CollateralReport& message, const FIX::SessionID& session_ID)
	{
		const auto& account_id = message.getField(FIX::FIELD::Account);
		account_ids.insert(account_id);

		spdlog::debug("Application::onMessage[CollateralReport]: inserted account id={}", account_id);

		// account balance, which is the cash balance in the account, not including any profit or losses on open trades
		const auto& balance = message.getField(FIX::FIELD::CashOutstanding);

		//cout << "  AccountID -> " << accountID << endl;
		//cout << "  Balance -> " << balance << endl;
		 
		// CollateralReport NoPartyIDs group can be inspected for additional account information such as AccountName or HedgingStatus
		FIX44::CollateralReport::NoPartyIDs group;
		message.getGroup(1, group); // CollateralReport will only have 1 NoPartyIDs group
		
		// Get the number of NoPartySubIDs repeating groups
		int number_subID = FIX::IntConvertor::convert(group.getField(FIX::FIELD::NoPartySubIDs));
		// For each group, print out both the PartySubIDType and the PartySubID (the value)
		for (int u = 1; u <= number_subID; u++) {
			FIX44::CollateralReport::NoPartyIDs::NoPartySubIDs sub_group;
			group.getGroup(u, sub_group);

			const auto& sub_type = sub_group.getField(FIX::FIELD::PartySubIDType);
			const auto& sub_value = sub_group.getField(FIX::FIELD::PartySubID);
			std::cout << "    " << sub_type << " -> " << sub_value << std::endl;
		}
	}

	void Application::onMessage(const FIX44::RequestForPositionsAck& message, const FIX::SessionID& session_ID)
	{
		std::string pos_reqID = message.getField(FIX::FIELD::PosReqID);

		// if a PositionReport is requested and no positions exist for that request, the Text field will
		// indicate that no positions matched the requested criteria 
		if (message.isSetField(FIX::FIELD::Text))
			spdlog::debug("Application::onMessage[FIX44::RequestForPositionsAck]: text={}", message.getField(FIX::FIELD::Text));
	}

	void Application::onMessage(const FIX44::PositionReport& message, const FIX::SessionID& session_ID)
	{
		// Print out important position related information such as accountID and symbol 
		std::string accountID = message.getField(FIX::FIELD::Account);

		//string symbol = pr.getField(FIELD::Symbol);
		//string positionID = pr.getField(FXCM_POS_ID);
		//string pos_open_time = pr.getField(FXCM_POS_OPEN_TIME);
		//cout << "PositionReport -> " << endl;
		//cout << "   Account -> " << accountID << endl;
		//cout << "   Symbol -> " << symbol << endl;
		//cout << "   PositionID -> " << positionID << endl;
		//cout << "   Open Time -> " << pos_open_time << endl;
	}

	void Application::onMessage(const FIX44::MarketDataSnapshotFullRefresh& message, const FIX::SessionID& session_ID)
	{
		auto& symbol = message.getField(FIX::FIELD::Symbol);

		auto& top_of_book = top_of_books.insert_or_assign(symbol, TopOfBook()).first->second;
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
				spdlog::debug("Application::onMessage[FIX44::MarketDataSnapshotFullRefresh]: unexpected md entry type={} price={}", entry_type, price);
			}
		}

		spdlog::debug("Application::onMessage[FIX44::MarketDataSnapshotFullRefresh]: top={}", top_of_book.to_string());

		top_of_book_queue.push(top_of_book); // publish snapshot related top of book
	}

	void Application::onMessage(const FIX44::MarketDataIncrementalRefresh& message, const FIX::SessionID&)
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
				spdlog::debug("Application::onMessage[FIX44::MarketDataIncrementalRefresh]: did not find symbol={} in top of books", symbol);
				continue;
			}

			if (i == 1) {
				timestamp = parse_date_and_time(group);
			}

			group.get(size);
			group.get(price);
			group.get(entry_type);
			auto& entry_type = group.getField(FIX::FIELD::MDEntryType);
			if (entry_type[0] == FIX::MDEntryType_BID) { // Bid
				it->second.bid_price = price;
				it->second.bid_volume = size;
				it->second.timestamp = timestamp;
				change_set.insert(&it->second);
			}
			else if (entry_type[0] == FIX::MDEntryType_OFFER) { // Ask (Offer)
				it->second.ask_price = price;
				it->second.ask_volume = size;
				it->second.timestamp = timestamp;
				change_set.insert(&it->second);
			}
			else if (entry_type[0] == FIX::MDEntryType_TRADING_SESSION_HIGH_PRICE) {
				session_high_price = price;
			}
			else if (entry_type[0] == FIX::MDEntryType_TRADING_SESSION_LOW_PRICE) {
				session_low_price = price;
			}
			else {
				spdlog::debug("Application::onMessage[FIX44::MarketDataIncrementalRefresh]: unexpected md entry type={} price={}", entry_type, price);
			}
		}

		for (auto top_of_book : change_set) {
			top_of_book_queue.push(*top_of_book); // publish snapshot related top of book
		}
	}

	void Application::onMessage(const FIX44::ExecutionReport& message, const FIX::SessionID&) 
	{
		FIX::Symbol symbol;
		FIX::ExecID exec_id;
		FIX::ExecType exec_type;
		FIX::OrderID ord_id;
		FIX::ClOrdID cl_ord_id;
		FIX::OrdStatus ord_status;
		FIX::OrdType ord_type;
		FIX::Side side;
		FIX::Price price;
		FIX::OrderQty order_qty;
		FIX::LastQty last_qty;
		FIX::LastPx last_px;
		FIX::LeavesQty leaves_qty;
		FIX::CumQty cum_qty;
		FIX::AvgPx avg_px;
		FIX::Text text;

		message.get(symbol);
		message.get(exec_id);
		message.get(exec_type);
		message.get(ord_id);
		message.get(cl_ord_id);
		message.get(ord_status);
		message.get(ord_type);
		message.get(side);
		message.get(price);
		message.get(avg_px);
		message.get(order_qty);
		message.get(last_qty);
		message.get(leaves_qty);
		message.get(last_px);
		message.get(cum_qty);
		message.get(text);

		ExecReport report(
			symbol.getString(),
			ord_id.getString(),
			cl_ord_id.getString(),
			exec_id.getString(),
			exec_type.getValue(),
			ord_type.getValue(),
			ord_status.getValue(),
			side.getValue(),
			price.getValue(),
			avg_px.getValue(),
			order_qty.getValue(),
			last_qty.getValue(),
			last_px.getValue(),
			cum_qty.getValue(),
			leaves_qty.getValue(),
			text.getString()
		);

		order_tracker.process(report); // we update a local tracker too, which is actually not needed

		exec_report_queue.push(report);
	}
	
	void Application::onMessage(const FIX44::OrderCancelReject&, const FIX::SessionID&) 
	{
	}

	void Application::onMessage(const FIX44::BusinessMessageReject&, const FIX::SessionID&)
	{
	}

	void Application::onMessage(const FIX44::MarketDataRequestReject& message, const FIX::SessionID& session_ID)
	{
		spdlog::error("onMessage[FIX44::MarketDataRequestReject]: {}", fix_string(message));
	}

	FIX::Message Application::market_data_snapshot(const FIX::Symbol& symbol) {
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

		spdlog::debug("Application::market_data_snapshot: {}", fix_string(request));

		FIX::Session::sendToTarget(request, market_data_session_id);

		return request;
	}

	std::optional<FIX::Message> Application::subscribe_market_data(
		const FIX::Symbol& symbol, 
		bool incremental
	) {
		auto it = market_data_subscriptions.find(symbol.getString());

		if (it != market_data_subscriptions.end()) {
			spdlog::error("subscribe_market_data: error - market data already subscribed for symbol {}", symbol.getString());
			return std::optional<FIX::Message>();
		}
		else {
			auto request_id = std::format("{}_{}", symbol.getString(), id_generator.genID());
			auto subscription_request_type = FIX::SubscriptionRequestType_SNAPSHOT_AND_UPDATES;
			auto md_update_type = incremental ? FIX::MDUpdateType_INCREMENTAL_REFRESH : FIX::MDUpdateType_FULL_REFRESH;
			FIX44::MarketDataRequest request;
			request.setField(FIX::MDReqID(request_id));
			request.setField(FIX::SubscriptionRequestType(FIX::SubscriptionRequestType_SNAPSHOT));
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

			spdlog::debug("Application::subscribe_market_data: {}", fix_string(request));

			FIX::Session::sendToTarget(request, market_data_session_id);

			market_data_subscriptions.emplace(symbol.getString(), request_id);

			return std::optional<FIX::Message>(request);
		}
	}

	std::optional<FIX::Message> Application::unsubscribe_market_data(const FIX::Symbol& symbol)
	{
		// Unsubscribe from EUR/USD. Note that our request_ID is the exact same
		// that was sent for our request to subscribe. This is necessary to 
		// unsubscribe. This request below is identical to our request to subscribe
		// with the exception that SubscriptionRequestType is set to
		// "SubscriptionRequestType_DISABLE_PREVIOUS_SNAPSHOT_PLUS_UPDATE_REQUEST"
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

			spdlog::debug("Application::unsubscribe_market_data: {}", fix_string(request));

			FIX::Session::sendToTarget(request, market_data_session_id);

			return std::optional<FIX::Message>(request);
		}
		else {
			spdlog::error("unsubscribe_market_data: error - market data not subscribed for symbol {}", symbol.getString());
			return std::optional<FIX::Message>();
		}
	}

	FIX::Message Application::trading_session_status_request()
	{
		// Request TradingSessionStatus message 
		FIX44::TradingSessionStatusRequest request;
		request.setField(FIX::TradSesReqID(id_generator.genID()));
		request.setField(FIX::TradingSessionID("FXCM"));
		request.setField(FIX::SubscriptionRequestType(FIX::SubscriptionRequestType_SNAPSHOT));
		
		spdlog::debug("Application::trading_session_status_request: {}", fix_string(request));

		FIX::Session::sendToTarget(request, trading_session_id);

		return request;
	}

	FIX::Message Application::collateral_inquiry()
	{
		// request CollateralReport message. We will receive a CollateralReport for each account under our login
		FIX44::CollateralInquiry request;
		request.setField(FIX::CollInquiryID(id_generator.genID()));
		request.setField(FIX::TradingSessionID("FXCM"));
		request.setField(FIX::SubscriptionRequestType(FIX::SubscriptionRequestType_SNAPSHOT));
		FIX::Session::sendToTarget(request, trading_session_id);

		return request;
	}

	FIX::Message Application::request_for_positions(const std::string& account_id)
	{
		FIX44::RequestForPositions request;
		request.setField(FIX::PosReqID(id_generator.genID()));
		request.setField(FIX::PosReqType(FIX::PosReqType_POSITIONS));
		// AccountID for the request. This must be set for routing purposes. We must
		// also set the Parties AccountID field in the NoPartySubIDs group
		request.setField(FIX::Account(account_id));
		request.setField(FIX::SubscriptionRequestType(FIX::SubscriptionRequestType_SNAPSHOT));
		request.setField(FIX::AccountType(FIX::AccountType_CARRIED_NON_CUSTOMER_SIDE_CROSS_MARGINED));
		request.setField(FIX::TransactTime());
		request.setField(FIX::ClearingBusinessDate());
		request.setField(FIX::TradingSessionID("FXCM"));
		// Set NoPartyIDs group. These values are always as seen below
		request.setField(FIX::NoPartyIDs(1));
		FIX44::RequestForPositions::NoPartyIDs parties_group;
		parties_group.setField(FIX::PartyID("FXCM ID"));
		parties_group.setField(FIX::PartyIDSource('D'));
		parties_group.setField(FIX::PartyRole(3));
		parties_group.setField(FIX::NoPartySubIDs(1));
		// Set NoPartySubIDs group
		FIX44::RequestForPositions::NoPartyIDs::NoPartySubIDs sub_parties;
		sub_parties.setField(FIX::PartySubIDType(FIX::PartySubIDType_SECURITIES_ACCOUNT_NUMBER));
		// Set Parties AccountID
		sub_parties.setField(FIX::PartySubID(account_id));
		// Add NoPartySubIds group
		parties_group.addGroup(sub_parties);
		// Add NoPartyIDs group
		request.addGroup(parties_group);

		spdlog::debug("Application::request_for_positions: {}", fix_string(request));

		FIX::Session::sendToTarget(request, trading_session_id);

		return request;
	}

	FIX::Message Application::new_order_single(
		const FIX::Symbol& symbol, 
		const FIX::ClOrdID& clOrdId, 
		const FIX::Side& side, 
		const FIX::OrdType& ordType, 
		const FIX::TimeInForce& tif,
		const FIX::OrderQty& orderQty, 
		const FIX::Price& price, 
		const FIX::StopPx& stopPrice
	) const {
		FIX44::NewOrderSingle order(
			clOrdId,
			side,
			FIX::TransactTime(),
			ordType);

		order.set(FIX::HandlInst('1'));
		order.set(symbol);
		order.set(orderQty);
		order.set(tif);

		if (ordType == FIX::OrdType_LIMIT || ordType == FIX::OrdType_STOP_LIMIT)
			order.set(price);

		if (ordType == FIX::OrdType_STOP || ordType == FIX::OrdType_STOP_LIMIT)
			order.set(stopPrice);

		spdlog::debug("Application::newOrderSingle: {}" , fix_string(order));

		FIX::Session::sendToTarget(order, trading_session_id);

		return order;
	}	

	FIX::Message Application::order_cancel_request(
		const FIX::Symbol& symbol,
		const FIX::OrderID& ordID,
		const FIX::OrigClOrdID& origClOrdID,
		const FIX::ClOrdID& clOrdID,
		const FIX::Side& side,
		const FIX::OrderQty& orderQty		
	) const {
		FIX44::OrderCancelRequest request(
			origClOrdID,
			clOrdID, 
			side, 
			FIX::TransactTime());

		request.set(symbol);
		request.set(ordID);
		request.set(orderQty);

		spdlog::debug("Application::orderCancelRequest: {}", fix_string(request));

		FIX::Session::sendToTarget(request, trading_session_id);

		return request;
	}

	FIX::Message Application::order_cancel_replace_request(
		const FIX::Symbol& symbol,
		const FIX::OrderID& ordID,
		const FIX::OrigClOrdID& origClOrdID,
		const FIX::ClOrdID& clOrdID,
		const FIX::Side& side,
		const FIX::OrdType& ordType,
		const FIX::OrderQty& orderQty,
		const FIX::Price& price
	) const {
		FIX44::OrderCancelReplaceRequest request(
			origClOrdID, 
			clOrdID,
			side, 
			FIX::TransactTime(),
			ordType);

		request.set(FIX::HandlInst('1'));
		request.set(symbol);
		request.set(ordID);
		request.set(price);
		request.set(orderQty);

		spdlog::debug("Application::orderCancelReplaceRequest: {}", fix_string(request));

		FIX::Session::sendToTarget(request, trading_session_id);

		return request;
	}

	bool Application::has_book(const std::string& symbol) {
		return top_of_books.contains(symbol);
	}

	TopOfBook Application::top_of_book(const std::string& symbol) {
		auto it = top_of_books.find(symbol);
		if (it != top_of_books.end()) {
			return it->second;
		}
		else {
			throw std::runtime_error(std::format("symbol {} not found in top_of_books", symbol));
		}
	}
}
