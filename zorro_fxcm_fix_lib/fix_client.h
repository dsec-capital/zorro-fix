#pragma once

#include "quickfix/Application.h"
#include "quickfix/MessageCracker.h"
#include "quickfix/Values.h"
#include "quickfix/Mutex.h"

#include "quickfix/fix44/ExecutionReport.h"
#include "quickfix/fix44/CollateralInquiry.h"
#include "quickfix/fix44/CollateralInquiryAck.h"
#include "quickfix/fix44/CollateralReport.h"
#include "quickfix/fix44/MarketDataRequest.h"
#include "quickfix/fix44/MarketDataRequestReject.h"
#include "quickfix/fix44/MarketDataSnapshotFullRefresh.h"
#include "quickfix/fix44/MarketDataIncrementalRefresh.h"
#include "quickfix/fix44/OrderCancelRequest.h"
#include "quickfix/fix44/OrderCancelReject.h"
#include "quickfix/fix44/OrderCancelReplaceRequest.h"
#include "quickfix/fix44/NewOrderList.h"
#include "quickfix/fix44/NewOrderSingle.h"
#include "quickfix/fix44/PositionReport.h"
#include "quickfix/fix44/RequestForPositions.h"
#include "quickfix/fix44/RequestForPositionsAck.h"
#include "quickfix/fix44/OrderMassStatusRequest.h"
#include "quickfix/fix44/OrderStatusRequest.h"
#include "quickfix/fix44/SecurityList.h"
#include "quickfix/fix44/TradingSessionStatus.h"
#include "quickfix/fix44/TradingSessionStatusRequest.h"
#include "quickfix/fix44/BusinessMessageReject.h"
#include "quickfix/fix44/Logon.h"
#include "quickfix/fix44/Logout.h"
#include "quickfix/fix44/Heartbeat.h"
#include "quickfix/fix44/TestRequest.h"

#include "spdlog/spdlog.h"

#include "common/id_generator.h"
#include "common/blocking_queue.h"
#include "common/exec_report.h"
#include "common/market_data.h"
#include "common/order_tracker.h"
#include "common/book.h"
#include "common/fix.h"

#include <variant>
#include <future>

namespace zorro
{
	using namespace common;

	std::string fix_string(const FIX::Message& msg);

	// Service message structure:
	// Required key: "type" std::string type of message
	typedef std::map<
		std::string, std::variant<bool, int, unsigned int, double, std::string>
	> ServiceMessage;

	template<class T>
	inline const T& sm_get_or_else(const ServiceMessage& map, const std::string_view& key, const T& other) {
		auto it = map.find(std::string(key));
		if (it != map.end()) {
			return std::get<T>(it->second);
		}
		else {
			return other;
		}
	}

	constexpr std::string_view SERVICE_MESSAGE_TYPE = "type";
	constexpr std::string_view SERVICE_MESSAGE_LOGON_STATUS = "logon_status";
	constexpr std::string_view SERVICE_MESSAGE_LOGON_STATUS_READY = "ready";
	constexpr std::string_view SERVICE_MESSAGE_LOGON_STATUS_SESSION_LOGINS = "session_logins";

	constexpr std::string_view SERVICE_MESSAGE_REJECT = "reject";
	constexpr std::string_view SERVICE_MESSAGE_REJECT_TYPE = "reject_type";
	constexpr std::string_view SERVICE_MESSAGE_REJECT_TEXT = "reject_text";
	constexpr std::string_view SERVICE_MESSAGE_REJECT_RAW_MESSAGE = "raw_message";

	enum FXCMProductId {
		UnknownProductId = 0,
		Forex = 1,
		Index = 2,
		Commodity = 3,
		Treasury = 4,
		Bullion = 5,
		Shares = 6,
		FxIndex = 7,
		Product8 = 8,
		Crypto = 9
	};

	// O - Open, C - Closed
	enum FXCMTradingStatus {
		UnknownTradingStatus = 0,
		TradingOpen = 2,
		TradingClosed = 3
	};
	
	struct FXCMSecurityInformation {
		std::string symbol;
		std::string currency;
		int product;
		int pip_size;
		double point_size;;
		double max_quanity;
		double min_quantity;
		int round_lots;
		int factor;
		double contract_multiplier;
		FXCMProductId prod_id;
		double interest_buy;
		double interest_sell;
		std::string subscription_status;
		int sort_order;
		double cond_dist_stop;
		double cond_dist_limit;
		double cond_dist_entry_stop;
		double cond_dist_entry_limit;
		FXCMTradingStatus fxcm_trading_status;

		std::string to_string() const;
	};

	struct FXCMTradingSessionStatus {
		common::fix::TradeSessionStatus trade_session_status{ common::fix::TradeSessionStatus::UNDEFINED };
		std::string server_timezone_name{ "UTC" };
		int server_timezone{ 0 };
		std::map<std::string, std::string> session_parameters;
		std::map<std::string, FXCMSecurityInformation> security_informations;

		std::string to_string() const;
	};

	enum FXCMMarginCallStatus {
		MarginCallStatus_Fine = 0,						// N: account has no problems
		MarginCallStatus_MaintenanceMarginAleart = 1,	// W: maintenance margin alert, 
		MarginCallStatus_LiquidationReached = 2, 		// Y : account reached liquidation margin call,
		MarginCallStatus_EquityAlert = 3, 				// A: equity alert
		MarginCallStatus_EquityStop = 4					// Q: equity stop, 
	};

	FXCMMarginCallStatus parse_fxcm_margin_call_status(const std::string& status);

	struct FXCMCollateralReport {
		std::string account;
		std::chrono::nanoseconds sending_time;
		double balance;
		double start_cash;
		double end_cash;
		double margin_ratio;
		double margin;
		double maintenance_margin;
		double cash_daily;
		FXCMMarginCallStatus margin_call_status;
		std::vector<std::pair<int, std::string>> party_sub_ids;

		std::string to_string() const;
	};

	struct FXCMPositionReport {
		std::string account;
		std::string symbol;
		std::string currency;
		std::string position_id;

		double settle_price;

		bool is_open;

		// valid for open and closed positions 
		double interest;
		double commission;
		std::chrono::nanoseconds open_time;

		// valid only for open positions
		std::optional<double> used_margin;

		// valid only for closed positions
		std::optional<double> close_pnl;
		std::optional<double> close_settle_price;
		std::optional<std::chrono::nanoseconds> close_time;
		std::optional<std::string> close_order_id;
		std::optional<std::string> close_cl_ord_id;

		std::string to_string() const;
	};

	struct FXCMPositionReports {
		std::vector<FXCMPositionReport> reports;

		std::string to_string() const;
	};

	enum class RequestsOnLogon
	{
		RequestsOnLogon_TradingSessionStatus	= 1 << 0,  
		RequestsOnLogon_CollateralReport		= 1 << 1,  
		RequestsOnLogon_PositionReport			= 1 << 2,  
		RequestsOnLogon_OrderStatusReport		= 1 << 3
	};

	/*
	 * FXCM FIX Client Application
	 * 
	 */
	class FixClient: public FIX::Application, public FIX::MessageCracker
	{
	public:
		FixClient(
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
		);

		std::future<bool> login_state();

		unsigned int get_session_logins();

		std::set<std::string> get_account_ids();

		// Sends TradingSessionStatusRequest message in order to receive a TradingSessionStatus message.
		// Note that TradingSessionStatus message also contains security informations.
		FIX::Message trading_session_status_request();

		// Sends the CollateralInquiry message in order to receive a CollateralReport message.
		FIX::Message collateral_inquiry(
			const char subscription_req_type = FIX::SubscriptionRequestType_SNAPSHOT_AND_UPDATES
		);

		// Sends RequestForPositions in order to receive a PositionReport messages if positions
		// matching the requested criteria exist; otherwise, a RequestForPositionsAck will be
		// sent with the acknowledgement that no positions exist.
		// pos_req_type = 0 for open position, 1 for closed position 
		// subscription_request_type = FIX::SubscriptionRequestType(FIX::SubscriptionRequestType_SNAPSHOT)
		// or FIX::SubscriptionRequestType(FIX::SubscriptionRequestType_SNAPSHOT_AND_UPDATES)
		FIX::Message request_for_positions(
			const std::string& account, 
			int pos_req_type, 
			const char subscription_req_type = FIX::SubscriptionRequestType_SNAPSHOT_AND_UPDATES
		);

		// Sends OrderMassStatusRequest to get status of all open orders
		FIX::Message order_mass_status_request();

		FIX::Message market_data_snapshot(const FIX::Symbol& symbol, bool incremental = true, bool subscribe_after_snapshot = false);

		std::optional<FIX::Message> subscribe_market_data(const FIX::Symbol& symbol, bool incremental = true, bool add_session_high_low = false);

		std::optional<FIX::Message> unsubscribe_market_data(const FIX::Symbol& symbol, bool add_session_high_low = false);

		std::optional<FIX::Message> new_order_single(
			const FIX::Symbol& symbol,
			const FIX::ClOrdID& cl_ord_id,
			const FIX::Side& side,
			const FIX::OrdType& ord_type,
			const FIX::TimeInForce& tif,
			const FIX::OrderQty& order_qty,
			const FIX::Price& price,
			const FIX::StopPx& stop_price,
			const std::optional<std::string>& position_id = std::optional<std::string>(),
			const std::optional<FIX::Account>& account = std::optional<FIX::Account>()
		) const;

		std::optional<FIX::Message> order_cancel_request(
			const FIX::Symbol& symbol,
			const FIX::OrderID& ord_id,
			const FIX::OrigClOrdID& orig_cl_ord_id,
			const FIX::ClOrdID& ,
			const FIX::Side& side,
			const FIX::OrderQty& order_qty,
			const std::optional<std::string>& position_id = std::optional<std::string>(),
			const std::optional<FIX::Account>& account = std::optional<FIX::Account>()
		) const;

		std::optional<FIX::Message> order_cancel_replace_request(
			const FIX::Symbol& symbol,
			const FIX::OrderID& ord_id,
			const FIX::OrigClOrdID& orig_cl_ord_id,
			const FIX::ClOrdID& cl_ord_id,
			const FIX::Side& side,
			const FIX::OrdType& ord_type,
			const FIX::OrderQty& order_qty,
			const FIX::Price& price,
			const FIX::TimeInForce& tif,
			const std::optional<std::string>& position_id = std::optional<std::string>(),
			const std::optional<FIX::Account>& account = std::optional<FIX::Account>()
		) const;

		void logout();

		void test_request(const std::string& test_req_id, bool trading_sess);

	private:

		ServiceMessage logon_service_message() const;

		ServiceMessage reject_service_message(const std::string& reject_type, const std::string& reject_text, const std::string& raw) const;

		bool is_trading_session(const FIX::SessionID& sess_id) const;

		bool is_market_data_session(const FIX::SessionID& sess_id) const;

		// custom FXCM FIX fields
		enum FXCM_FIX_FIELDS
		{
			FXCM_SYM_PRECISION = 9001,			// Precision of prices (pip size) e.g. 5 for EUR/USD and 3 for USD/JPY
			FXCM_SYM_POINT_SIZE = 9002,			// Size of price point e.g. 0.0001 for EUR/USD and 0.001 for USD/JPY
			FXCM_SYM_INTEREST_BUY = 9003,		// Interest Rate for sell side open positions
			FXCM_SYM_INTEREST_SELL = 9004,		// Interest Rate for buy side open position 
			FXCM_SYM_SORT_ORDER = 9005,			// Sorting index of instrument
			FXCM_START_DATE = 9012,				// UTCDate Used in Historical Snapshots RequestForPositions
			FXCM_START_TIME = 9013,				// UTCTimeOnly Used in Historical Snapshots RequestForPositions
			FXCM_END_DATE = 9014,				// UTCDate Used in Historical Snapshots RequestForPositions
			FXCM_END_TIME = 9015,				// UTCTimeOnly Used in Historical Snapshots RequestForPositions
			FXCM_NO_PARAMS = 9016,
			FXCM_PARAM_NAME = 9017,
			FXCM_PARAM_VALUE = 9018,
			FXCM_SERVER_TIMEZONE = 9019,		// Server time zone, number of hours out from UTC (-5 for EST)
			FXCM_CONTINUOUS_FLAG = 9020,		// Integer
			FXCM_REQUEST_REJECT_REASON = 9025,  // Integer Request Rejection Reason Code 
												// 0: Unknown, 1: Generic, 2: Data not found 3: Trading session not found, 4: Other
			FXCM_ERROR_DETAILS = 9029,			// String Error details of server side processing
			FXCM_SERVER_TIMEZONE_NAME = 9030,	// Server time zone name 
			FXCM_USED_MARGIN = 9038,			// Float Amount of used margin nominated for an account or position (liquidation level)
			FXCM_POS_INTEREST = 9040,			// Float Amount of interest applied to the position
			FXCM_POS_ID = 9041,					// String, also known as the ticker - the id of the position that was opened for a trade
			FXCM_POS_OPEN_TIME = 9042,			// UTCTimestamp Time when trading position was opened
			FXCM_CLOSE_SETTLE_PRICE = 9043,		// Float Closing price of trading position
			FXCM_POS_CLOSE_TIME = 9044,			// UTCTimestamp Time when trading position was closed
			FXCM_MARGIN_CALL = 9045,			// String Milestones status of account from risk management point of view 
												// N: account has no problems, Y: account reached liquidation margin call, 
												// W: maintenance margin alert, Q: equity stop, A: equity alert
			FXCM_USED_MARGIN3 = 9046,			// Float Amount of used margin nominated for an account or position (maintenance level)
			FXCM_CASH_DAILY = 9047,				// Float Non - trade related daily activity on the account (uses for daily equity based risk management)
			FXCM_CLOSE_CL_ORD_ID = 9048,		// String ClOrdID of the order that closes a position
			FXCM_CLOSE_SECONDARY_CL_ORD_ID = 9049, // String SecondaryClOrdID of the order that closes a position
			FXCM_ORD_TYPE = 9050,				// String See documentation 
			FXCM_ORD_STATUS = 9051,				// String See documentation 
			FXCM_CLOSE_PNL = 9052,				// String Profit Loss amount that calculated on the postion
			FXCM_POS_COMMISSION = 9053,			// Float Trading commission applied to the position
			FXCM_CLOSE_ORDER_ID = 9054,			// String Order ID of the order that closes a position
			FXCM_MAX_NO_RESULTS = 9060,			// Int Used in conjunction with Request For Positions message for Closed Positions Historical Snapshots
			FXCM_PEG_FLUCTUATE_PTS = 9061,		// Int Specifies the number trailing stop fluctuating points
			FXCM_SUBSCRIPTION_STATUS = 9076,	// String FXCM Subscription status(D / T / V / M)
			FXCM_POS_ID_REF = 9078,				// String Origin / Reference FXCM Trade ID
			FXCM_CONTINGENCY_ID = 9079,			// String FXCM Contingency ID
			FXCM_PRODUCT_ID = 9080,				// 1: Forex, 2: Index, 3: Commodity, 4: Treasury, 5: Bullion, 6: Shares, 7: FX Index
			FXCM_COND_DIST_STOP = 9090,			// Float Condition distance for Stop order
			FXCM_COND_DIST_LIMIT = 9091,		// Float Condition distance for Limit order
			FXCM_COND_DIST_ENTRY_STOP = 9092,	// Float Condition distance for Entry Stop order
			FXCM_COND_DIST_ENTRY_LIMIT = 9093,	// Float Condition distance for Entry Limit order
			FXCM_MAX_QUANTITY = 9094,			// Float Max quantity allowed
			FXCM_MIN_QUANTITY = 9095,			// Float Min quantity allowed
			FXCM_TRADING_STATUS = 9096,			// String Security Trading Status : O - Open, C - Closed
		};

		FIX::SessionSettings session_settings;
		unsigned int num_required_session_logins;
		BlockingTimeoutQueue<ExecReport>& exec_report_queue;
		BlockingTimeoutQueue<StatusExecReport>& status_exec_report_queue;
		BlockingTimeoutQueue<TopOfBook>& top_of_book_queue;
		BlockingTimeoutQueue<ServiceMessage>& service_message_queue;
		BlockingTimeoutQueue<FXCMPositionReport>& position_report_queue;
		BlockingTimeoutQueue<FXCMPositionReports>& position_snapshot_reports_queue;
		BlockingTimeoutQueue<FXCMCollateralReport>& collateral_report_queue;
		BlockingTimeoutQueue<FXCMTradingSessionStatus>& trading_session_status_queue;

		FIX::SessionID trading_session_id;
		FIX::SessionID market_data_session_id;
		std::set<std::string> account_ids;

		std::map<std::string, bool> session_login_status;
		std::promise<bool> login_promise;
		std::atomic<bool> done;
		IDGenerator id_generator;
		bool subscribe_after_snapshot{ false };

		std::map<std::string, std::string> market_data_subscriptions;
		std::map<std::string, TopOfBook> top_of_books;
		std::vector<FXCMPositionReport> position_report_list;

		std::mutex mutex;

		// should not be used anymore, use the top_of_book_queue
		bool has_book(const std::string& symbol);
		TopOfBook top_of_book(const std::string& symbol);

		// FIX Application interface

		void onCreate(const FIX::SessionID&);

		void onLogon(const FIX::SessionID& sessionID);

		void onLogout(const FIX::SessionID& sessionID);

		void toAdmin(FIX::Message&, const FIX::SessionID&);
		
		void toApp(
			FIX::Message&, const FIX::SessionID&
		) EXCEPT(FIX::DoNotSend);
		
		void fromAdmin(
			const FIX::Message&, const FIX::SessionID&
		) EXCEPT(FIX::FieldNotFound, FIX::IncorrectDataFormat, FIX::IncorrectTagValue, FIX::RejectLogon);
		
		void fromApp(
			const FIX::Message& message, const FIX::SessionID& sessionID
		) EXCEPT(FIX::FieldNotFound, FIX::IncorrectDataFormat, FIX::IncorrectTagValue, FIX::UnsupportedMessageType);

		void onMessage(const FIX44::Heartbeat&, const FIX::SessionID&);

		void onMessage(const FIX44::TradingSessionStatus&, const FIX::SessionID&);

		void onMessage(const FIX44::ExecutionReport&, const FIX::SessionID&);

		void onMessage(const FIX44::CollateralInquiryAck&, const FIX::SessionID&);
		void onMessage(const FIX44::CollateralReport&, const FIX::SessionID&);

		void onMessage(const FIX44::RequestForPositionsAck&, const FIX::SessionID&);
		void onMessage(const FIX44::PositionReport&, const FIX::SessionID&);

		void onMessage(const FIX44::MarketDataSnapshotFullRefresh&, const FIX::SessionID&);
		void onMessage(const FIX44::MarketDataIncrementalRefresh&, const FIX::SessionID&);

		void onMessage(const FIX44::OrderCancelReject&, const FIX::SessionID&);
		void onMessage(const FIX44::BusinessMessageReject&, const FIX::SessionID&);
		void onMessage(const FIX44::MarketDataRequestReject&, const FIX::SessionID&);
	};
}

