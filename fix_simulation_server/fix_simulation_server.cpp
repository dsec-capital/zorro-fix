#ifdef _MSC_VER
#pragma warning( disable : 4503 4355 4786 )
#endif

#include "quickfix/config.h"
#include "quickfix/Log.h"
#include "quickfix/FileStore.h"
#include "quickfix/SocketAcceptor.h"
#include "quickfix/SessionSettings.h"

#include "common/price_sampler.h"
#include "common/market.h"
#include "common/utils.h"

#include "application.h"
#include "rest_server.h"

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
        std::cout << "usage: " << argv[0]  << " settings_file market_config_file." << std::endl;
        return 0;
    }

    try
    {
        std::mutex mutex;
        std::string settings_file = argv[1];
        std::string market_config_file = argv[2];
        std::random_device random_device;
        std::mt19937 generator(random_device());
        FIX::Log* screenLogger;

        FIX::SessionSettings settings(settings_file);

        toml::table tbl;
        tbl = toml::parse_file(market_config_file);

        std::map<std::string, Market> markets;

        auto cfg = tbl["config"];
        auto server_host = cfg["http_server_host"].value<std::string>().value();
        auto server_port = cfg["http_server_port"].value<int>().value();
        auto market_update_period = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::hours(cfg["market_update_period_millis"].value<int>().value())
        );

        for (auto& [k, v] : *tbl["market"].as_table()) {
            auto& sym_tbl = *v.as_table();
            auto symbol = sym_tbl["symbol"].value<std::string>().value();
            auto price = sym_tbl["price"].value<double>().value();
            auto spread = sym_tbl["spread"].value<double>().value();
            auto tick_size = sym_tbl["tick_size"].value<double>().value();
            auto bar_period = std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::seconds(sym_tbl["bar_period_seconds"].value<int>().value())
            );
            auto history_age = std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::hours(sym_tbl["history_age_hours"].value<int>().value())
            );
            auto history_sample_period = std::chrono::milliseconds(
                sym_tbl["history_sample_period_millis"].value<int>().value()
            );

            auto sampler = price_sampler_factory(
                generator,
                *sym_tbl["market_simulator"].as_table(),
                symbol,
                price,
                spread,
                tick_size
            );
            if (sampler != nullptr) {
                markets.try_emplace(
                    symbol,
                    sampler,
                    bar_period,
                    history_age,
                    history_sample_period,
                    mutex
                );
            }
            else {
                throw std::runtime_error("unknown price sampler type");
            }
        }

        RestServer restServer(server_host, server_port, markets, mutex);

        FIX::FileStoreFactory storeFactory(settings);
        FIX::ScreenLogFactory logFactory(settings); 
        screenLogger = logFactory.create();

        Application application(markets, market_update_period, screenLogger, mutex);
        FIX::SocketAcceptor acceptor(application, storeFactory, settings, logFactory);

        acceptor.start();
        application.startMarketDataUpdates();
        restServer.run();

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
 