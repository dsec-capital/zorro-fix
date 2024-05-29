#ifndef APPLICATION_H
#define APPLICATION_H

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
#include "quickfix/fix44/SecurityList.h"
#include "quickfix/fix44/TradingSessionStatus.h"
#include "quickfix/fix44/TradingSessionStatusRequest.h"

#include "spdlog/spdlog.h"

#include "common/id_generator.h"
#include "common/blocking_queue.h"
#include "common/exec_report.h"
#include "common/market_data.h"
#include "common/order_tracker.h"
#include "common/book.h"

namespace zorro
{
	using namespace common;

	std::string fix_string(const FIX::Message& msg);

	class Application: public FIX::Application, public FIX::MessageCracker
	{
	public:
		Application(
			const FIX::SessionSettings& session_settings,
			BlockingTimeoutQueue<ExecReport>& exec_report_queue,
			BlockingTimeoutQueue<TopOfBook>& top_of_book_queue
		);

		bool is_logged_in() const;

		int log_in_count() const;

		const std::set<std::string>& get_account_ids() const;

		// Sends TradingSessionStatusRequest message in order to receive a TradingSessionStatus message
		FIX::Message trading_session_status_request();

		// Sends the CollateralInquiry message in order to receive a CollateralReport message.
		FIX::Message collateral_inquiry();

		// Sends RequestForPositions in order to receive a PositionReport messages if positions
		// matching the requested criteria exist; otherwise, a RequestForPositionsAck will be
		// sent with the acknowledgement that no positions exist. 
		FIX::Message request_for_positions(const std::string& account_id);

		FIX::Message market_data_snapshot(const FIX::Symbol& symbol);

		std::optional<FIX::Message> subscribe_market_data(const FIX::Symbol& symbol, bool incremental);

		std::optional<FIX::Message> unsubscribe_market_data(
			const FIX::Symbol& symbol
		);

		FIX::Message new_order_single(
			const FIX::Symbol& symbol, 
			const FIX::ClOrdID& clOrdId, 
			const FIX::Side& side,
			const FIX::OrdType& ordType, 
			const FIX::TimeInForce& tif,
			const FIX::OrderQty& quantity, 
			const FIX::Price& price, 
			const FIX::StopPx& stopPrice
		) const;

		FIX::Message order_cancel_request(
			const FIX::Symbol& symbol,
			const FIX::OrderID& ordID,
			const FIX::OrigClOrdID& origClOrdID,
			const FIX::ClOrdID& clOrdId,
			const FIX::Side& side,
			const FIX::OrderQty& orderQty
		) const;

		FIX::Message order_cancel_replace_request(
			const FIX::Symbol& symbol,
			const FIX::OrderID& ordID,
			const FIX::OrigClOrdID& origClOrdID,
			const FIX::ClOrdID& clOrdId,
			const FIX::Side& side,
			const FIX::OrdType& ordType,
			const FIX::OrderQty& orderQty,
			const FIX::Price& price
		) const;



	private:

		bool is_trading_session(const FIX::SessionID& sess_id) const;

		bool is_market_data_session(const FIX::SessionID& sess_id) const;

		// custom FXCM FIX fields
		enum FXCM_FIX_FIELDS
		{
			FXCM_FIELD_PRODUCT_ID = 9080,
			FXCM_POS_ID = 9041,
			FXCM_POS_OPEN_TIME = 9042,
			FXCM_ERROR_DETAILS = 9029,
			FXCM_REQUEST_REJECT_REASON = 9025,
			FXCM_USED_MARGIN = 9038,
			FXCM_POS_CLOSE_TIME = 9044,
			FXCM_MARGIN_CALL = 9045,
			FXCM_ORD_TYPE = 9050,
			FXCM_ORD_STATUS = 9051,
			FXCM_CLOSE_PNL = 9052,
			FXCM_SYM_POINT_SIZE = 9002,
			FXCM_SYM_PRECISION = 9001,
			FXCM_TRADING_STATUS = 9096,
			FXCM_PEG_FLUCTUATE_PTS = 9061,
			FXCM_NO_PARAMS = 9016,
			FXCM_PARAM_NAME = 9017,
			FXCM_PARAM_VALUE = 9018
		};

		FIX::SessionSettings session_settings;
		BlockingTimeoutQueue<ExecReport>& exec_report_queue;
		BlockingTimeoutQueue<TopOfBook>& top_of_book_queue;

		FIX::SessionID trading_session_id;
		FIX::SessionID market_data_session_id;
		std::set<std::string> account_ids;

		std::atomic<bool> done;
		std::atomic<int> logged_in;
		IDGenerator id_generator;

		std::unordered_map<std::string, std::string> market_data_subscriptions;
		std::unordered_map<std::string, TopOfBook> top_of_books;
		OrderTracker order_tracker;

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

#endif
