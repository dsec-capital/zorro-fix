#ifdef _MSC_VER
#pragma warning( disable : 4503 4355 4786 )
#endif

#include "quickfix/config.h"
#include "quickfix/Log.h"
#include "quickfix/FileStore.h"
#include "quickfix/SocketAcceptor.h"
#include "quickfix/SessionSettings.h"

#include "application.h"

#include <string>
#include <iostream>
#include <fstream>
#include <chrono>

using namespace std::chrono_literals;


int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cout << "usage: " << argv[0]
            << " FILE." << std::endl;
        return 0;
    }
    std::string file = argv[1];

    FIX::Log* screenLogger;
    try
    {
        FIX::SessionSettings settings(file);


        FIX::FileStoreFactory storeFactory(settings);
        FIX::ScreenLogFactory logFactory(settings);
        screenLogger = logFactory.create();

        auto market_update_period = 100ms;
        Application application(screenLogger, market_update_period);
        FIX::SocketAcceptor acceptor(application, storeFactory, settings, logFactory);

        acceptor.start();
        application.startMarketDataUpdates();

        while (true)
        {
            std::string value;
            std::cin >> value;

            if (value == "#symbols")
                application.orderMatcher().display();
            else if (value == "#quit")
                break;
            else
                application.orderMatcher().display(value);

            std::cout << std::endl;
        }

        application.stopMarketDataUpdates();
        acceptor.stop();

        return 0;
    }
    catch (std::exception& e)
    {
        std::cout << e.what() << std::endl;
        return 1;
    }
}
