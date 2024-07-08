#pragma once

#include "pch.h"
#include "fix_client.h"

#include "quickfix/config.h"
#include "quickfix/FileStore.h"
#include "quickfix/SocketInitiator.h"
#include "quickfix/Values.h"
#ifdef HAVE_SSL
#include "quickfix/ThreadedSSLSocketInitiator.h"
#include "quickfix/SSLSocketInitiator.h"
#endif
#include "quickfix/SessionSettings.h"
#include "quickfix/Log.h"
#include "quickfix/FileLog.h"

#include "spdlog/spdlog.h"

#include "common/market_data.h"
#include "common/exec_report.h"
#include "common/order_tracker.h"
#include "common/blocking_queue.h"
#include "common/time_utils.h"

namespace zorro {

	using namespace common;

	class FixService {
	public:
		FixService(
			const std::string& settings_cfg_file,
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

		~FixService();

#ifdef HAVE_SSL
		FixService(
			const std::string& settings_cfg_file,
			unsigned int num_required_session_logins,
			BlockingTimeoutQueue<ExecReport>& exec_report_queue,
			BlockingTimeoutQueue<StatusExecReport>& status_exec_report_queue,
			BlockingTimeoutQueue<TopOfBook>& top_of_book_queue,
			BlockingTimeoutQueue<ServiceMessage>& service_message_queue,
			BlockingTimeoutQueue<FXCMPositionReport>& position_reports_queue,
			BlockingTimeoutQueue<FXCMPositionReports>& position_snapshot_reports_queue,
			BlockingTimeoutQueue<FXCMCollateralReport>& collateral_report_queue,
			BlockingTimeoutQueue<FXCMTradingSessionStatus>& trading_session_status_queue
			const std::string& isSSL
		); 
#endif

		void start();

		void cancel();

		FixClient& client();

	private:

		void run();

		void create_factories();

		bool started;
		std::string settings_cfg_file;
		FIX::SessionSettings* settings;
		FIX::FileStoreFactory* store_factory;
		FIX::FileLogFactory* log_factory;
		FIX::Initiator* initiator;
		FixClient* fix_client;
		std::thread thread;
	};
}
