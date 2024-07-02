#pragma once

#include "pch.h"
#include "fix_service.h"

namespace zorro {

	using namespace common;

	void FixService::run() {
		initiator->start();
		spdlog::debug("FixService: FIX initiator started");
	}

	FixService::FixService(
		const std::string& settings_cfg_file,
		unsigned int requests_on_logon,
		unsigned int num_required_session_logins,
		BlockingTimeoutQueue<ExecReport>& exec_report_queue,
		BlockingTimeoutQueue<StatusExecReport>& status_exec_report_queue,
		BlockingTimeoutQueue<TopOfBook>& top_of_book_queue,
		BlockingTimeoutQueue<ServiceMessage>& service_message_queue,
		BlockingTimeoutQueue<FXCMPositionReports>& position_reports_queue,
		BlockingTimeoutQueue<FXCMCollateralReport>& collateral_report_queue,
		BlockingTimeoutQueue<FXCMTradingSessionStatus>& trading_session_status_queue
	) :
		started(false),
		settings_cfg_file(settings_cfg_file)
	{
		create_factories();
		fix_client = new FixClient(
			*settings,
			requests_on_logon,
			num_required_session_logins,
			exec_report_queue,
			status_exec_report_queue,
			top_of_book_queue,
			service_message_queue,
			position_reports_queue,
			collateral_report_queue,
			trading_session_status_queue
		);
		initiator = new FIX::SocketInitiator(*fix_client, *store_factory, *settings, *log_factory);
		spdlog::debug("FixService: FIX fix_client and FIX initiator created");
	}

	FixService::~FixService() {
		if (!initiator->isStopped()) {
			initiator->stop();
		}
		delete initiator;
		delete settings;
		delete store_factory;
		delete log_factory;
		delete fix_client;
	}

#ifdef HAVE_SSL
	FixService::FixService(
		const std::string& settings_cfg_file,
		unsigned int requests_on_logon,
		unsigned int num_required_session_logins,
		BlockingTimeoutQueue<ExecReport>& exec_report_queue,
		BlockingTimeoutQueue<StatusExecReport>& status_exec_report_queue,
		BlockingTimeoutQueue<TopOfBook>& top_of_book_queue,
		BlockingTimeoutQueue<ServiceMessage>& service_message_queue,
		BlockingTimeoutQueue<FXCMPositionReports>& position_reports_queue,
		BlockingTimeoutQueue<FXCMCollateralReport>& collateral_report_queue,
		BlockingTimeoutQueue<FXCMTradingSessionStatus>& trading_session_status_queue
		const std::string& isSSL
	) :
		started(false),
		settings_cfg_file(settings_cfg_file)
	{
		create_factories();
		application = new Application(
			*settings,
			requests_on_logon,
			num_required_session_logins,
			exec_report_queue,
			status_exec_report_queue,
			top_of_book_queue,
			service_message_queue,
			position_reports_queue,
			collateral_report_queue,
			trading_session_status_queue
		);
		if (isSSL.compare("SSL") == 0)
			initiator = new FIX::ThreadedSSLSocketInitiator(*application, *store_factory, *settings, *log_factory);
		else if (isSSL.compare("SSL-ST") == 0)
			initiator = new FIX::SSLSocketInitiator(*application, *store_factory, *settings, *log_factory);
		else
			initiator = new FIX::SocketInitiator(*application, *store_factory, *settings, *log_factory);

		spdlog::debug("FixService: FIX fix_client and FIX initiator created");
	}
#endif

	void FixService::start() {
		started = true;
		thread = std::thread(&FixService::run, this);
		spdlog::debug("FixService: FIX fix_client thread started");
	}

	void FixService::cancel() {
		if (!started)
			return;
		started = false;
		initiator->stop(true);
		spdlog::debug("FixService: FIX initiator and fix client stopped - going to join");
		if (thread.joinable())
			thread.join();
		spdlog::debug("FixService: FIX initiator stopped - joined");
	}

	FixClient& FixService::client() {
		return *fix_client;
	}

	void FixService::create_factories() {
		try {
			settings = new FIX::SessionSettings(settings_cfg_file);
			store_factory = new FIX::FileStoreFactory(*settings);
			log_factory = new FIX::FileLogFactory(*settings);
		}
		catch (FIX::ConfigError error) {
			spdlog::error("FixService: config error {}", error.what());
		}
	}
}
