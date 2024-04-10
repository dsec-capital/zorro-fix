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

#include "spdlog/spdlog.h"

#include "common/market_data.h"
#include "common/exec_report.h"
#include "common/order_tracker.h"
#include "common/blocking_queue.h"
#include "common/time_utils.h"

#include "broker_commands.h"

namespace zfix {

	using namespace common;

	class FixThread {
		bool started;
		std::string settings_cfg_file;
		FIX::SessionSettings settings;
		FIX::FileStoreFactory store_factory;
		FIX::ScreenLogFactory log_factory;
		std::unique_ptr<FIX::Initiator> initiator;
		std::unique_ptr<zfix::Application> application;
		std::thread thread;

		void run() {
			initiator->start();
			spdlog::debug("FixThread: FIX initiator started");
		}

	public:
		FixThread(
			const std::string& settings_cfg_file,
			BlockingTimeoutQueue<ExecReport>& exec_report_queue,
			BlockingTimeoutQueue<TopOfBook>& top_of_book_queue
		) :
			started(false),
			settings_cfg_file(settings_cfg_file),
			settings(settings_cfg_file),
			store_factory(settings),
			log_factory(settings)
		{
			application = std::unique_ptr<zfix::Application>(new Application(
				settings, 
				exec_report_queue,
				top_of_book_queue
			));
			initiator = std::unique_ptr<FIX::Initiator>(
				new FIX::SocketInitiator(*application, store_factory, settings, log_factory)
			);
			spdlog::debug("FixThread: FIX application and FIX initiator created");
		}

#ifdef HAVE_SSL
		FixThread(
			const std::string& settingsCfgFile,
			std::function<void(const char*)> brokerError,
			const std::string& isSSL
		) :
			started(false),
			settingsCfgFile(settingsCfgFile),
			settings(settingsCfgFile),
			storeFactory(settings),
			logFactory(settings),
			brokerError(brokerError)
		{
			application = std::unique_ptr<zfix::Application>(new Application(settings));

			if (isSSL.compare("SSL") == 0)
				initiator = std::unique_ptr<FIX::Initiator>(
					new FIX::ThreadedSSLSocketInitiator(application, storeFactory, settings, logFactory));
			else if (isSSL.compare("SSL-ST") == 0)
				initiator = std::unique_ptr<FIX::Initiator>(
					new FIX::SSLSocketInitiator(application, storeFactory, settings, logFactory));
			else
				initiator = std::unique_ptr<FIX::Initiator>(
					new FIX::SocketInitiator(*application, storeFactory, settings, logFactory)
				);
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

		zfix::Application& fix_app() {
			return *application;
		}
	};
}
#endif 