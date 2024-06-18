#pragma once

#include <memory>

#include "httplib/httplib.h"
#include "nlohmann/json.h"
#include "spdlog/spdlog.h"

#include "common/bar.h"
#include "common/json.h"
#include "common/time_utils.h"

#include "log.h"

#include "LocalFormat.h"
#include "ResponseListener.h"
#include "SessionStatusListener.h"
#include "CommunicatorStatusListener.h"

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
        bool ready{ false };
        std::chrono::high_resolution_clock::time_point started;

        typedef double DATE;

        O2G2Ptr<IO2GSession> session;
        O2G2Ptr<SessionStatusListener> statusListener;
        O2G2Ptr<pricehistorymgr::IPriceHistoryCommunicator> communicator;

    public:

        bool is_ready() const {
            return ready;
        }

        ~ProxyServer() {
            session->logout();
            statusListener->waitEvents();
        }

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
            spd_logger = create_logger();

            char* pin = nullptr;
            char* session_id = nullptr;
            session = CO2GTransport::createSession();
            statusListener = new SessionStatusListener(session, true, session_id, pin);

            session->subscribeSessionStatus(statusListener);
            statusListener->reset();

            pricehistorymgr::IError* error = NULL;
            communicator = O2G2Ptr<pricehistorymgr::IPriceHistoryCommunicator>(
                pricehistorymgr::PriceHistoryCommunicatorFactory::createCommunicator(session, "History", &error)
            );
            O2G2Ptr<pricehistorymgr::IError> autoError(error);

            if (!communicator)
            {
                spdlog::error("failed to initialize communcator {}", error ? error->getMessage() : "unknown error");
            }

            session->login(login.c_str(), password.c_str(), url.c_str(), connection.c_str());

            if (statusListener->waitEvents() && statusListener->isConnected())
            {
                ready = true;
                spdlog::info("connected - server ready to accept service requests");
            }
            else {
                spdlog::error("connectiion timeout - incomming service requests will be rejected");
            }

            // http://localhost:8080/status
            server.Get("/status", [this](const Request& req, Response& res) {
                json j;
                j["started"] = common::to_string(started);
                j["ready"] = ready;
                auto body = j.dump();
                res.set_content(body, "application/json");
            });

            // for example http://localhost:8080/bars?symbol=EUR/USD&from=2024-06-18 00:00:00&timeframe=m1
            server.Get("/bars", [this](const Request& req, Response& res) {                
                try {
                    if (!ready) {
                        throw std::runtime_error("server not ready!");
                    }

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
                    std::vector<common::BidAskBar<DATE>> bars;
                    auto date_from = common::nanos_to_date(from);
                    auto date_to = common::nanos_to_date(to);
                    auto quotes_count = 0;

                    spdlog::debug("fetch symbol={} timeframe={} date_from={} date_to={}", symbol, timeframe, date_from, date_to);

                    O2G2Ptr<CommunicatorStatusListener> communicatorStatusListener(new CommunicatorStatusListener());
                    communicator->addStatusListener(communicatorStatusListener);

                    bool has_error = false;
                    std::string error_message;

                    if (communicator->isReady() || communicatorStatusListener->waitEvents() && communicatorStatusListener->isReady())
                    {
                        O2G2Ptr<ResponseListener> responseListener(new ResponseListener());
                        communicator->addListener(responseListener);

                        O2G2Ptr<pricehistorymgr::ITimeframeFactory> timeframeFactory = communicator->getTimeframeFactory();
                        pricehistorymgr::IError* error = NULL;
                        O2G2Ptr<IO2GTimeframe> timeframeObj = timeframeFactory->create(timeframe.c_str(), &error);
                        O2G2Ptr<pricehistorymgr::IError> autoError(error);
                        if (!timeframeObj)
                        {
                            error_message = std::format("timeframe {} incorrect", timeframe);
                            has_error = true;
                        }

                        O2G2Ptr<pricehistorymgr::IPriceHistoryCommunicatorRequest> request = communicator->createRequest(
                            symbol.c_str(), timeframeObj, date_from, date_to, quotes_count, &error
                        );
                        if (!request)
                        {
                            error_message = std::format("failed to create request {}", error ? error->getMessage() : "unknown error");
                            has_error = true;
                        }

                        responseListener->setRequest(request);
                        if (!communicator->sendRequest(request, &error))
                        {
                            error_message = std::format("failed to send request {}", error ? error->getMessage() : "unknown error");
                            has_error = true;
                        }

                        if (!has_error) 
                        {
                            responseListener->wait();

                            // print results if any
                            O2G2Ptr<pricehistorymgr::IPriceHistoryCommunicatorResponse> response = responseListener->getResponse();
                            if (response) {
                                spdlog::debug("response obtainded!");

                                pricehistorymgr::IError* error = NULL;
                                O2G2Ptr<IO2GMarketDataSnapshotResponseReader> reader = communicator->createResponseReader(response, &error);
                                O2G2Ptr<pricehistorymgr::IError> autoError(error);
                                if (reader) {
                                    if (!reader->isBar())
                                    {
                                        error_message = std::format("failded sending requestexpected bars");
                                        has_error = true;
                                    }
                                    else {
                                        auto n = reader->size();

                                        if (n > 0) {
                                            LocalFormat format;

                                            spdlog::debug(
                                                "{} bars from {} to {} in request interval from {} to {}",
                                                n, format.formatDate(reader->getDate(0)), format.formatDate(reader->getDate(n - 1)),
                                                format.formatDate(date_from), format.formatDate(date_to)
                                            );

                                            for (int i = 0; i < n; ++i) {
                                                DATE dt = reader->getDate(i); // beginning of the bar

                                                if (dt < date_from) {
                                                    continue;
                                                }

                                                common::BidAskBar<DATE> bar(
                                                    dt,
                                                    reader->getBidOpen(i),
                                                    reader->getBidHigh(i),
                                                    reader->getBidLow(i),
                                                    reader->getBidClose(i),
                                                    reader->getAskOpen(i),
                                                    reader->getAskHigh(i),
                                                    reader->getAskLow(i),
                                                    reader->getAskClose(i),
                                                    reader->getVolume(i)
                                                );

                                                bars.emplace_back(bar);
                                            }
                                        }
                                    }
                                }
                                else {
                                    error_message = std::format("failed to create reader {}", error ? error->getMessage() : "unknown error");
                                    has_error = true;
                                }
                            }
                        }

                        communicator->removeListener(responseListener);
                    }
                    else {
                        error_message = std::format("communicator not ready or status listener timeout");
                        has_error = true;
                    }

                    communicator->removeStatusListener(communicatorStatusListener);
                    statusListener->reset();

                    if (!has_error) {
                        auto content = common::to_json(bars);
                        res.set_content(content.dump(), "application/json");
                    }
                    else {
                        throw std::runtime_error(error_message);
                    }

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
                    auto error = std::format("error: no bar data for error={}", what);
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

                }
            });

            //while (!server.is_running() && !done)
            //    std::this_thread::sleep_for(std::chrono::milliseconds{ 10 });
            //server.stop();
            //thread.join();

            spdlog::info("server stated on {}:{}", server_host, server_port);
        }
    };
}

