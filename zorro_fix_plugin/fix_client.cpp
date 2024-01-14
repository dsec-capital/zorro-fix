#include "pch.h"

#ifdef _MSC_VER
#pragma warning( disable : 4503 4355 4786 )
#endif

#include "quickfix/config.h"

#include "quickfix/FileStore.h"
#include "quickfix/SocketInitiator.h"
#ifdef HAVE_SSL
#include "quickfix/ThreadedSSLSocketInitiator.h"
#include "quickfix/SSLSocketInitiator.h"
#endif
#include "quickfix/SessionSettings.h"
#include "quickfix/Log.h"
#include "application.h"

#include <string>
#include <iostream>
#include <fstream>
#include <memory>

/* 
#include "getopt-repl.h"

int main(int argc, char** argv)
{
	if (argc < 2)
	{
		std::cout << "usage: " << argv[0]
			<< " FILE." << std::endl;
		return 0;
	}
	std::string file = argv[1];

#ifdef HAVE_SSL
	std::string isSSL;
	if (argc > 2)
	{
		isSSL.assign(argv[2]);
	}
#endif

	try
	{
		FIX::SessionSettings settings(file);

		Application application;
		FIX::FileStoreFactory storeFactory(settings);
		FIX::ScreenLogFactory logFactory(settings);

		std::unique_ptr<FIX::Initiator> initiator;
#ifdef HAVE_SSL
		if (isSSL.compare("SSL") == 0)
			initiator = std::unique_ptr<FIX::Initiator>(
				new FIX::ThreadedSSLSocketInitiator(application, storeFactory, settings, logFactory));
		else if (isSSL.compare("SSL-ST") == 0)
			initiator = std::unique_ptr<FIX::Initiator>(
				new FIX::SSLSocketInitiator(application, storeFactory, settings, logFactory));
		else
#endif
			initiator = std::unique_ptr<FIX::Initiator>(
				new FIX::SocketInitiator(application, storeFactory, settings, logFactory)
			);

		initiator->start();
		application.run();
		initiator->stop();

		return 0;
	}
	catch (std::exception& e)
	{
		std::cout << e.what();
		return 1;
	}
}

*/