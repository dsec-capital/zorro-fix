#ifndef FIX_THREAD_H
#define FIX_THREAD_H

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
#include "application.h"

#include <string>
#include <vector>
#include <memory>
#include <sstream>
#include <thread>
#include <iostream>
#include <chrono>

#include "logger.h"
#include "market_data.h"
#include "exec_report.h"
#include "order_tracker.h"
#include "broker_commands.h"
#include "blocking_queue.h"
#include "time_utils.h"

namespace zfix {

	class FixThread {
		bool started;
		std::string settingsCfgFile;
		FIX::SessionSettings settings;
		FIX::FileStoreFactory storeFactory;
		FIX::ScreenLogFactory logFactory;
		std::unique_ptr<FIX::Initiator> initiator;
		std::unique_ptr<zfix::Application> application;
		std::thread thread;

		void run() {
			initiator->start();
		}

	public:
		FixThread(
			const std::string& settingsCfgFile,
			BlockingTimeoutQueue<ExecReport>& execReportQueue
		) :
			started(false),
			settingsCfgFile(settingsCfgFile),
			settings(settingsCfgFile),
			storeFactory(settings),
			logFactory(settings)
		{
			application = std::unique_ptr<zfix::Application>(new Application(
				settings, 
				execReportQueue
			)
			);
			initiator = std::unique_ptr<FIX::Initiator>(
				new FIX::SocketInitiator(*application, storeFactory, settings, logFactory)
			);
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
		}

		void cancel() {
			if (!started)
				return;
			started = false;
			initiator->stop(true);
			LOG_INFO("FixThread: FIX initiator and application stopped - going to join\n")
				if (thread.joinable())
					thread.join();
			LOG_INFO("FixThread: FIX initiator stopped - joined\n")
		}

		zfix::Application& fixApp() {
			return *application;
		}
	};
}
#endif 