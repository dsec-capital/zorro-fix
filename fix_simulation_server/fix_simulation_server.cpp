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

        if (tbl.is_table()) {
            for (auto [k, v] : *tbl.as_table()) {
                auto symbol = std::string(k.str());
                std::replace(symbol.begin(), symbol.end(), '_', '/');
                std::cout << "symbol=" << symbol << ", values=" << v.as_table() << "\n";
                if (v.is_table()) {
                    auto sym_tbl = *v.as_table();
                    auto price = sym_tbl["price"].value<double>();
                    auto spread = sym_tbl["spread"].value<double>();
                    auto bid_volume = sym_tbl["bid_volume"].value<double>();
                    auto ask_volume = sym_tbl["ask_volume"].value<double>();
                    std::cout << "parsed symbol sucessfully \n";
                }
                if (tbl[k.str()]["market_simulator"].is_table()) {
                    auto sim_tbl = *tbl[k.str()]["market_simulator"].as_table();
                    auto model = sim_tbl["model"].value<std::string>();
                    auto alpha_plus = sim_tbl["alpha_plus"].value<double>();
                    auto alpha_neg = sim_tbl["alpha_neg"].value<double>();
                    auto tick_probs_arr = sim_tbl["tick_probs"].as_array();
                    std::vector<double> tick_probs;
                    tick_probs_arr->for_each([&tick_probs](toml::value<double>& elem)
                        {
                            tick_probs.push_back(elem.get());
                        });
                    std::cout << "parsed model \n";
                }
            }
        }
        else {
            throw std::runtime_error("market config file incorrect");
        }

        FIX::FileStoreFactory storeFactory(settings);
        FIX::ScreenLogFactory logFactory(settings); 
        screenLogger = logFactory.create();

        auto market_update_period = 2000ms;
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
 