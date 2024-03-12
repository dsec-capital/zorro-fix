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
#include <toml++/toml.hpp>

using namespace std::chrono_literals;


int main(int argc, char** argv)
{
    if (argc != 3)
    {
        std::cout << "usage: " << argv[0]
            << " settings_file market_config_file." << std::endl;
        return 0;
    }
    std::string settings_file = argv[1];
    std::string market_config_file = argv[2];

    FIX::Log* screenLogger;
    try
    {
        FIX::SessionSettings settings(settings_file);

        toml::table tbl;
        tbl = toml::parse_file(market_config_file);
        std::cout << tbl << "\n";

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
    catch (const toml::parse_error& err)
    {
        std::cout << "Market config file parsing failed:\n" << err << "\n";
        return 1;
    }
    catch (std::exception& e)
    {
        std::cout << e.what() << std::endl;
        return 1;
    }
}
