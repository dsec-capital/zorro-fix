#ifdef _MSC_VER
#pragma warning( disable : 4503 4355 4786 )
#endif

#include "quickfix/config.h"
#include "quickfix/Log.h"
#include "quickfix/FileStore.h"
#include "quickfix/SocketAcceptor.h"
#include "quickfix/SessionSettings.h"

#include "market.h"
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

        std::optional<double> price, spread, tick_size;
        std::optional<std::string> model;

        std::map<std::string, Market> markets;

        if (tbl.is_table()) {
            for (auto [k, v] : *tbl.as_table()) {
                auto symbol = std::string(k.str());
                std::replace(symbol.begin(), symbol.end(), '_', '/');
                std::cout << "symbol=" << symbol << ", values=" << v.as_table() << "\n";
                if (v.is_table()) {
                    auto sym_tbl = *v.as_table();
                    price = sym_tbl["price"].value<double>();
                    spread = sym_tbl["spread"].value<double>();
                    tick_size = sym_tbl["tick_size"].value<double>();
                }
                else {
                    throw std::runtime_error("market config file incorrect");
                }
                if (tbl[k.str()]["market_simulator"].is_table()) {
                    auto sim_tbl = *tbl[k.str()]["market_simulator"].as_table();
                    model = sim_tbl["model"].value<std::string>();
                    if (model == "fodra-pham") {
                        std::vector<double> tick_probs;
                        std::optional<double> bid_volume, ask_volume;
                        std::optional<double> alpha_plus, alpha_neg;
                        bid_volume = sim_tbl["bid_volume"].value<double>();
                        ask_volume = sim_tbl["ask_volume"].value<double>();
                        alpha_plus = sim_tbl["alpha_plus"].value<double>();
                        alpha_neg = sim_tbl["alpha_neg"].value<double>();
                        auto tick_probs_arr = sim_tbl["tick_probs"].as_array();
                        tick_probs_arr->for_each([&tick_probs](toml::value<double>& elem)
                            {
                                tick_probs.push_back(elem.get());
                            });
                        auto sampler = std::make_shared<FodraPhamSampler<std::mt19937>>(
                                generator,
                                alpha_plus.value(),
                                alpha_neg.value(),
                                tick_probs,
                                tick_size.value(),
                                price.value(),
                                spread.value(),
                                bid_volume.value(),
                                ask_volume.value(),
                                1
                            );
                        markets.try_emplace(symbol, symbol, sampler, mutex);
                    }
                    else {
                        throw std::runtime_error("unknown price sampler type");
                    }
                }
                else {
                    throw std::runtime_error(std::format("no market simulator defined for {}", symbol));
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
        Application application(markets, market_update_period, screenLogger, mutex);
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
 