#ifndef FIX_THREAD_H
#define FIX_THREAD_H

#include "pch.h"
#include "application.h"

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

	class FixThread {
		bool started;
		std::string settings_cfg_file;
		FIX::SessionSettings *settings;
		FIX::FileStoreFactory *store_factory;
		FIX::FileLogFactory *log_factory;
		FIX::Initiator* initiator;
		zorro::Application* application;
		std::thread thread;

		void run() {
			initiator->start();
			spdlog::debug("FixThread: FIX initiator started");
		}

	public:
		FixThread(
			const std::string& settings_cfg_file,
			BlockingTimeoutQueue<ExecReport>& exec_report_queue,
			BlockingTimeoutQueue<TopOfBook>& top_of_book_queue,
			BlockingTimeoutQueue<FXCMPositionReports>& position_reports_queue,
			BlockingTimeoutQueue<FXCMCollateralReport>& collateral_report_queue
		) :
			started(false),
			settings_cfg_file(settings_cfg_file)
		{
			create_factories();
			application = new Application(
				*settings, 
				exec_report_queue, 
				top_of_book_queue, 
				position_reports_queue, 
				collateral_report_queue
			);
			initiator = new FIX::SocketInitiator(*application, *store_factory, *settings, *log_factory);
			spdlog::debug("FixThread: FIX application and FIX initiator created");
		}

		~FixThread() {
			if (!initiator->isStopped()) {
				initiator->stop();
			}
			delete initiator;
			delete settings;
			delete store_factory;
			delete log_factory;
		}

#ifdef HAVE_SSL
		FixThread(
			const std::string& settings_cfg_file,
			BlockingTimeoutQueue<ExecReport>& exec_report_queue,
			BlockingTimeoutQueue<TopOfBook>& top_of_book_queue
			const std::string& isSSL
		) :
			started(false),
			settings_cfg_file(settings_cfg_file)
		{
			create_factories();
			application = new Application(*settings, exec_report_queue, top_of_book_queue);
			if (isSSL.compare("SSL") == 0)
				initiator = new FIX::ThreadedSSLSocketInitiator(*application, *store_factory, *settings, *log_factory);
			else if (isSSL.compare("SSL-ST") == 0)
				initiator = new FIX::SSLSocketInitiator(*application, *store_factory, *settings, *log_factory);
			else
				initiator = new FIX::SocketInitiator(*application, *store_factory, *settings, *log_factory);

			spdlog::debug("FixThread: FIX application and FIX initiator created");
		}
#endif

		void start() {
			started = true;
			thread = std::thread(&FixThread::run, this);
			spdlog::debug("FixThread: FIX application thread started");
		}

		void cancel() {
			if (!started)
				return;
			started = false;
			initiator->stop(true);
			spdlog::debug("FixThread: FIX initiator and application stopped - going to join");
			if (thread.joinable())
				thread.join();
			spdlog::debug("FixThread: FIX initiator stopped - joined");
		}

		zorro::Application& fix_app() {
			return *application;
		}

	private:

		void create_factories() {
			try {
				settings = new FIX::SessionSettings(settings_cfg_file);
				store_factory = new FIX::FileStoreFactory(*settings);
				log_factory = new FIX::FileLogFactory(*settings);
			}
			catch (FIX::ConfigError error) {
				spdlog::error("FixThread: config error {}", error.what());
			}
		}
	};
}
#endif 