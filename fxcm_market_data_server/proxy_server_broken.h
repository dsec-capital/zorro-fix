#pragma once

#include "httplib/httplib.h"
#include "nlohmann/json.h"
#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"

#include "common/bar.h"
#include "common/time_utils.h"

namespace fxcm {

    using namespace httplib;
    using namespace nlohmann;
    using namespace std::literals::chrono_literals;

    class ProxyServer {

        int server_port;
        std::string server_host;

        Server server;
        std::atomic_bool done{ false };
        std::thread thread;
        std::shared_ptr<spdlog::logger> spd_logger;
        std::shared_ptr<fxcm::ForexConnect> service;
        bool ready{ false };
        std::chrono::high_resolution_clock::time_point started;

        typedef double DATE;

    public:

        ProxyServer(
            const std::string& login,
            const std::string& password,
            const std::string& connection,
            const std::string& url,
            const std::string& server_host,
            int server_port,
            std::chrono::milliseconds login_timeout = 15000ms
        ) : server_host(server_host)
          , server_port(server_port)
        {
            auto cwd = std::filesystem::current_path().string();

            auto logger_name = "fxcm_proxy_server";
            auto log_level = spdlog::level::debug;
            auto flush_interval = std::chrono::seconds(2);

            std::vector<spdlog::sink_ptr> sinks{
                std::make_shared<spdlog::sinks::stdout_color_sink_mt>(),
                std::make_shared<spdlog::sinks::basic_file_sink_mt>("file", "fxcm_proxy_server"),
            };
            spd_logger = std::make_shared<spdlog::logger>("name", begin(sinks), end(sinks));
            spdlog::register_logger(spd_logger);
            spd_logger->set_level(log_level);

            spdlog::set_level(log_level);
            spdlog::flush_every(flush_interval);
            spdlog::debug("Logging started, logger_name={}, level={}, cwd={}", logger_name, (int)spd_logger->level(), cwd);

            service = std::make_shared<fxcm::ForexConnect>(login, password, connection, url);

            started = std::chrono::high_resolution_clock::now();

            bool res = service->login();
            spdlog::info("waiting for login res={}", res);

            ready = service->wait_for_login(login_timeout);

            auto dt = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - started);

            auto msg = std::format("logged in with logged_in={} in dt={}", ready, dt);
            spdlog::info(msg);
            std::cout << msg << std::endl;

            // http://localhost:8080/status
            server.Get("/status", [this](const Request& req, Response& res) {
                res.set_content(std::format("started={} ready={}", common::to_string(started), ready), "text/plain");
            });

            // for example http://localhost:8080/bars?symbol=EUR/USD&from=2024-06-18 00:00:00&timeframe=m1
            server.Get("/bars", [this](const Request& req, Response& res) {                
                std::stringstream msg;
                msg << "<==== /bars";

                std::string symbol = "nan";
                std::string timeframe = "m1";
                std::chrono::nanoseconds from{ 0 };
                auto to = common::get_current_system_clock();

                if (req.has_param("symbol")) {
                    symbol = req.get_param_value("symbol");
                    msg << std::format(" symbol={}", symbol);
                }
                if (req.has_param("from")) {
                    auto from_param = req.get_param_value("from");
                    from = common::parse_datetime(from_param);
                    msg << std::format(" from={}", from_param);
                }
                if (req.has_param("to")) {
                    auto to_param = req.get_param_value("to");
                    to = common::parse_datetime(to_param);
                    msg << std::format(" to={}", to_param);
                }
                if (req.has_param("timeframe")) {
                    auto timeframe = req.get_param_value("timeframe");
                    msg << std::format(" timeframe={}", timeframe);
                }

                spdlog::info(msg.str());

                try {
                    std::vector<common::BidAskBar<DATE>> bars;
                    auto date_from = common::nanos_to_date(from);
                    auto date_to = common::nanos_to_date(to);

                    spdlog::debug("fetch symbol={} timeframe={} date_from={} date_to={}", symbol, timeframe, date_from, date_to);

                    service->fetch(bars, symbol, timeframe, date_from, date_to);

                    spdlog::debug("fetched num_bars={}", bars.size());
                }
                catch (...) {
                    std::string what = "unknown exception";
                    std::exception_ptr ex = std::current_exception();
                    try
                    {
                        std::rethrow_exception(ex);
                    }
                    catch (std::exception const& e)
                    {
                        what = e.what();
                    }
                    auto error = std::format("error: no bar data for symbol={} error={}", symbol, what);
                    spdlog::error(error);

                    json j;
                    j["error"] = error;
                    auto body = j.dump();
                    res.set_content(body, "application/json");
                }
            });
        }

        void run() {
            thread = std::thread([&]() {
                server.listen(server_host, server_port);
                done = true;
                if (ready) {
                    service->logout();
                    service->wait_for_logout(10000ms);
                }
            });

            //while (!server.is_running() && !done)
            //    std::this_thread::sleep_for(std::chrono::milliseconds{ 10 });
            //server.stop();
            //thread.join();

            spdlog::info("====> rest server stated on {}:{}", server_host, server_port);
        }
    };
}

