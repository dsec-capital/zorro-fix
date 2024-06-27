#pragma once

#include <memory>

#include "httplib/httplib.h"
#include "nlohmann/json.h"
#include "spdlog/spdlog.h"

#include "common/bar.h"
#include "common/json.h"
#include "common/time_utils.h"

#include "LocalFormat.h"
#include "ResponseListener.h"
#include "SessionStatusListener.h"
#include "CommunicatorStatusListener.h"
#include "LiveBarStreamer.h"

namespace fxcm {

    using namespace httplib;
    using namespace nlohmann;
    using namespace std::literals::chrono_literals;

    O2G2Ptr<IO2GTimeframe> create_timeframe_object(pricehistorymgr::IPriceHistoryCommunicator* communicator, const std::string& timeframe) {
        O2G2Ptr<pricehistorymgr::ITimeframeFactory> timeframeFactory = communicator->getTimeframeFactory();
        pricehistorymgr::IError* error = NULL;
        O2G2Ptr<IO2GTimeframe> timeframeObj = timeframeFactory->create(timeframe.c_str(), &error);
        O2G2Ptr<pricehistorymgr::IError> autoError(error);
        if (!timeframeObj)
        {
            spdlog::error("timeframe {} incorrect", timeframe);
        }
        return timeframeObj;
    }

    class ProxyServer {

        int server_port;
        std::string server_host;

        Server server;
        std::atomic_bool done{ false };
        std::thread thread;
        bool ready{ false };
        std::chrono::high_resolution_clock::time_point started;

        typedef double DATE;

        O2G2Ptr<IO2GSession> session;
        O2G2Ptr<SessionStatusListener> statusListener;
        O2G2Ptr<pricehistorymgr::IPriceHistoryCommunicator> communicator;

        std::map<std::string, std::shared_ptr<LiveBarStreamer>> streamers;

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

            server.Get("/stop", [&](const auto& /*req*/, auto& /*res*/) {
                server.stop();
            });

            // http://localhost:8083/status
            server.Get("/status", [this](const Request& req, Response& res) {
                spdlog::info("<==== /status ready={} startd={}", ready, common::to_string(started));
                json j;
                j["started"] = common::to_string(started);
                j["ready"] = ready;
                auto body = j.dump();
                res.set_content(body, "application/json");
            });

            // http://localhost:8083/instr?symbol=EUR/USD
            server.Get("/instr", [this](const Request& req, Response& res) {
                try {
                    std::string symbol = "nan";
                    if (req.has_param("symbol")) {
                        symbol = req.get_param_value("symbol");
                    }

                    spdlog::info("<==== /instr symbol={}", symbol);

                    O2G2Ptr<quotesmgr::IQuotesManager> quotesManager = communicator->getQuotesManager();
                    quotesmgr::IError* error = NULL;
                    O2G2Ptr<quotesmgr::IInstruments> instruments = quotesManager->getInstruments(&error);
                    O2G2Ptr<quotesmgr::IError> autoError(error);
                    if (instruments)
                    {
                        O2G2Ptr<quotesmgr::IInstrument> instr = instruments->find(symbol.c_str());
                        if (instr) {
                            auto precision = instr->getPrecision();

                            json j;
                            j["symbol"] = symbol;
                            j["name"] = instr->getName();
                            j["contract_currency"] = instr->getContractCurrency();
                            j["precision"] = instr->getPrecision();
                            j["point_size"] = instr->getPointSize();
                            j["instrument_type"] = instr->getInstrumentType();
                            j["base_unit_size"] = instr->getBaseUnitSize();
                            j["contract_multiplier"] = instr->getContractMultiplier();
                            j["latest_quote_date"] = instr->getLatestQuoteDate("m1");

                            auto body = j.dump();
                            res.set_content(body, "application/json");
                        }
                    }
                }
                catch (...) {
                    std::string what = "unknown exception";
                    std::exception_ptr ex = std::current_exception();
                    try {
                        std::rethrow_exception(ex);
                    }
                    catch (std::exception const& e) {
                        what = e.what();
                    }
                    catch (...) {}
                    auto error = std::format("error: no bar data for error={}", what);
                    spdlog::error(error);

                    json j;
                    j["error"] = error;
                    auto body = j.dump();
                    res.set_content(body, "application/json");
                }
            });

            // for example http://localhost:8083/subscribe?symbol=EUR/USD
            server.Get("/subscribe", [this](const Request& req, Response& res) {
                try {
                    std::string symbol = "";
                    std::string timeframe = "m1";
                    std::chrono::nanoseconds from = 0ns;
                    std::chrono::nanoseconds to = 0ns;
                    int quotes_count = 0;

                    if (req.has_param("symbol")) {
                        symbol = req.get_param_value("symbol");
                    }
                    else {
                        throw std::runtime_error("no symbol");
                    }

                    auto it = streamers.find(symbol);
                    if (it != streamers.end()) {
                        throw std::runtime_error(std::format("symbol {} already subscribed", symbol));
                    }
                   
                    if (req.has_param("from")) {
                        auto from_param = req.get_param_value("from");
                        from = common::parse_datetime(from_param);
                    }
                    if (req.has_param("to")) {
                        auto to_param = req.get_param_value("to");
                        to = common::parse_datetime(to_param);
                    }
                    if (req.has_param("timeframe")) {
                        auto timeframe = req.get_param_value("timeframe");
                    }
                    if (req.has_param("quotes_count")) {
                        auto quotes_count = req.get_param_value("quotes_count");
                    }

                    auto timeframe_obj = create_timeframe_object(communicator, timeframe);

                    if (to == 0ns) {
                        to = common::get_current_system_clock();
                    }

                    if (from == 0ns) {
                        from = to - std::chrono::nanoseconds(static_cast<long long>(common::NANOS_PER_DAY / 24));
                    }

                    auto streamer = std::make_shared<LiveBarStreamer>(
                        session,
                        communicator,
                        symbol,
                        common::nanos_to_date(from),
                        common::nanos_to_date(to),
                        timeframe_obj,
                        quotes_count
                    );
                    streamers.emplace(symbol, streamer);
                    auto ready = streamer->subscribe();

                    spdlog::debug("subscribed to {} ready={}", symbol, ready);
                }
                catch (...) {
                    std::string what = "unknown exception";
                    std::exception_ptr ex = std::current_exception();
                    try {
                        std::rethrow_exception(ex);
                    }
                    catch (std::exception const& e) {
                        what = e.what();
                    }
                    catch (...) {}
                    auto error = std::format("error: no bar data for error={}", what);
                    spdlog::error(error);

                    json j;
                    j["error"] = error;
                    auto body = j.dump();
                    res.set_content(body, "application/json");
                }
            });

            // for example http://localhost:8083/bars?symbol=EUR/USD&from=2024-06-18 00:00:00&timeframe=m1
            server.Get("/bars", [this](const Request& req, Response& res) {
                std::string symbol = "nan";

                try {
                    if (!ready) {
                        throw std::runtime_error("server not ready!");
                    }

                    std::stringstream msg;
                    msg << "<==== /bars";

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

                    O2G2Ptr<CommunicatorStatusListener> communicatorStatusListener(new CommunicatorStatusListener());
                    communicator->addStatusListener(communicatorStatusListener);

                    bool has_error = false;
                    std::string error_message;

                    if (communicator->isReady() || communicatorStatusListener->waitEvents() && communicatorStatusListener->isReady())
                    {
                        O2G2Ptr<ResponseListener> responseListener(new ResponseListener());
                        communicator->addListener(responseListener);

                        O2G2Ptr<IO2GTimeframe> timeframeObj = create_timeframe_object(communicator, timeframe);
                        if (!timeframeObj)
                        {
                            error_message = std::format("timeframe {} incorrect", timeframe);
                            has_error = true;
                        }

                        pricehistorymgr::IError* error = NULL;
                        O2G2Ptr<pricehistorymgr::IPriceHistoryCommunicatorRequest> request = communicator->createRequest(
                            symbol.c_str(), timeframeObj, date_from, date_to, quotes_count, &error
                        );
                        O2G2Ptr<pricehistorymgr::IError> autoError(error);
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

                            O2G2Ptr<pricehistorymgr::IPriceHistoryCommunicatorResponse> response = responseListener->getResponse();
                            if (response) {
                                pricehistorymgr::IError* error = NULL;
                                O2G2Ptr<IO2GMarketDataSnapshotResponseReader> reader = communicator->createResponseReader(response, &error);
                                O2G2Ptr<pricehistorymgr::IError> autoError(error);
                                if (reader) {
                                    if (!reader->isBar())
                                    {
                                        error_message = std::format("failded sending request - expected bars");
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

                    spdlog::debug("fetched {} number of bars", bars.size());
                }
                catch (...) {
                    std::string what = "unknown exception";
                    std::exception_ptr ex = std::current_exception();
                    try {
                        std::rethrow_exception(ex);
                    }
                    catch (std::exception const& e) {
                        what = e.what();
                    }
                    catch (...) {}
                    auto error = std::format("error: no bar data for {} error={}", symbol, what);
                    spdlog::error(error);

                    json j;
                    j["error"] = error;
                    auto body = j.dump();
                    res.set_content(body, "application/json");
                }
            });
            
            // for example http://localhost:8083/ticks?symbol=EUR/USD&from=2024-06-27 00:00:00 
            server.Get("/ticks", [this](const Request& req, Response& res) {                
                std::string symbol = "nan";

                try {
                    if (!ready) {
                        throw std::runtime_error("server not ready!");
                    }

                    std::stringstream msg;
                    msg << "<==== /ticks";

                    std::string timeframe = "t1";
                    std::chrono::nanoseconds from{ 0 };
                    auto to = common::get_current_system_clock();
                    int count{ 0 };

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
                    if (req.has_param("count")) {
                        auto to_count = req.get_param_value("count");
                        count = std::stoi(to_count);
                        msg << std::format(" count={}", to_count);
                    }

                    spdlog::info(msg.str());
                    std::vector<common::Quote<DATE>> quotes;
                    auto date_from = common::nanos_to_date(from);
                    auto date_to = common::nanos_to_date(to);
                    auto quotes_count = count;

                    O2G2Ptr<CommunicatorStatusListener> communicatorStatusListener(new CommunicatorStatusListener());
                    communicator->addStatusListener(communicatorStatusListener);

                    bool has_error = false;
                    std::string error_message;

                    if (communicator->isReady() || communicatorStatusListener->waitEvents() && communicatorStatusListener->isReady())
                    {
                        O2G2Ptr<ResponseListener> responseListener(new ResponseListener());
                        communicator->addListener(responseListener);

                        O2G2Ptr<IO2GTimeframe> timeframeObj = create_timeframe_object(communicator, timeframe);
                        if (!timeframeObj)
                        {
                            error_message = std::format("timeframe {} incorrect", timeframe);
                            has_error = true;
                        }

                        pricehistorymgr::IError* error = NULL;
                        O2G2Ptr<pricehistorymgr::IPriceHistoryCommunicatorRequest> request = communicator->createRequest(
                            symbol.c_str(), timeframeObj, date_from, date_to, quotes_count, &error
                        );
                        O2G2Ptr<pricehistorymgr::IError> autoError(error);
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

                            O2G2Ptr<pricehistorymgr::IPriceHistoryCommunicatorResponse> response = responseListener->getResponse();
                            if (response) {
                                pricehistorymgr::IError* error = NULL;
                                O2G2Ptr<IO2GMarketDataSnapshotResponseReader> reader = communicator->createResponseReader(response, &error);
                                O2G2Ptr<pricehistorymgr::IError> autoError(error);
                                if (reader) {
                                    if (reader->isBar())
                                    {
                                        error_message = std::format("failded sending request - expected ticks");
                                        has_error = true;
                                    }
                                    else {
                                        auto n = reader->size();

                                        if (n > 0) {
                                            LocalFormat format;

                                            spdlog::debug(
                                                "{} ticks from {} to {} in request interval from {} to {}",
                                                n, format.formatDate(reader->getDate(0)), format.formatDate(reader->getDate(n - 1)),
                                                format.formatDate(date_from), format.formatDate(date_to)
                                            );

                                            for (int i = 0; i < n; ++i) {
                                                DATE dt = reader->getDate(i); // tick timestamp

                                                if (dt < date_from) {
                                                    continue;
                                                }

                                                common::Quote<DATE> quote(
                                                    dt,
                                                    reader->getBid(i),
                                                    reader->getAsk(i)
                                                );
                                                
                                                quotes.emplace_back(quote);
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
                        auto content = common::to_json(quotes);
                        res.set_content(content.dump(), "application/json");
                    }
                    else {
                        throw std::runtime_error(error_message);
                    }

                    spdlog::debug("fetched {} number of ticks", quotes.size());
                }
                catch (...) {
                    std::string what = "unknown exception";
                    std::exception_ptr ex = std::current_exception();
                    try {
                        std::rethrow_exception(ex);
                    }
                    catch (std::exception const& e) {
                        what = e.what();
                    }
                    catch (...) {}
                    auto error = std::format("error: no tick data for {} error={}", symbol, what);
                    spdlog::error(error);

                    json j;
                    j["error"] = error;
                    auto body = j.dump();
                    res.set_content(body, "application/json");
                }
            });
        }

        void run() {
            spdlog::info("server stated on {}:{}", server_host, server_port);
            auto res = server.listen(server_host, server_port);
            if (!res) {
                spdlog::error("server failed to listen on {}:{}", server_host, server_port);
            }
            else {
                spdlog::info("server listen return with {}", res);
            } 
        }
    };
}

