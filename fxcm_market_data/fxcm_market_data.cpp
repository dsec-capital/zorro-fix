#include "pch.h"

#include "fxcm_market_data.h"

#include "ResponseListener.h"
#include "SessionStatusListener.h"
#include "CommonSources.h"
#include "CommunicatorStatusListener.h"
#include "LocalFormat.h"

namespace fxcm {

    ForexConnectData::ForexConnectData(
        const std::string& login_user,
        const std::string& password,
        const std::string& connection,
        const std::string& url,
        const std::string& session_id,
        const std::string& pin,
        int timeout
    ) : login_user(login_user)
      , password(password)
      , connection(connection)
      , url(url)
      , session_id(session_id)
      , pin(pin)
      , logged_in(false)
    {
        session = CO2GTransport::createSession();
        statusListener = new SessionStatusListener(session, true, session_id.c_str(), pin.c_str(), timeout);

        // subscribe IO2GSessionStatus interface implementation for the status events
        session->subscribeSessionStatus(statusListener);
        statusListener->reset();

        // create an instance of IPriceHistoryCommunicator
        pricehistorymgr::IError* error_ptr = NULL;
        O2G2Ptr<pricehistorymgr::IPriceHistoryCommunicator> communicator(
            pricehistorymgr::PriceHistoryCommunicatorFactory::createCommunicator(session, "History", &error_ptr)
        );
        O2G2Ptr<pricehistorymgr::IError> error(error_ptr);

        if (!communicator)
        {
            auto msg = std::format(
                "ForexConnectData::ForexConnectData: error {}", error->getMessage()
            );
            spdlog::error(msg);
            throw std::runtime_error(msg);
        }
    }

    ForexConnectData::~ForexConnectData() {
        session->unsubscribeSessionStatus(statusListener);
    }

    bool ForexConnectData::login() {
        logged_in = session->login(login_user.c_str(), password.c_str(), url.c_str(), connection.c_str());
        statusListener->waitEvents();
        logged_in &= statusListener->isConnected();
        return logged_in;
    }

    void ForexConnectData::logout() {
        if (logged_in) {
            session->logout();
            statusListener->waitEvents();
        }
    }

    bool ForexConnectData::fetch(
        std::vector<common::BidAskBar<DATE>>& bars,
        const std::string& instrument,
        const std::string& timeframe,
        DATE date_from,
        DATE date_to, 
        int quotes_count
    ) {
        bars.clear();

        if (!logged_in) {
            return false;
        }

        O2G2Ptr<CommunicatorStatusListener> communicatorStatusListener(new CommunicatorStatusListener());
        communicator->addStatusListener(communicatorStatusListener);

        O2G2Ptr<ResponseListener> responseListener(new ResponseListener());
        communicator->addListener(responseListener);

        bool success = true;

        if (communicator->isReady() ||
            communicatorStatusListener->waitEvents() && communicatorStatusListener->isReady())
        {
            try 
            {
                O2G2Ptr<ResponseListener> responseListener(new ResponseListener());
                communicator->addListener(responseListener);

                O2G2Ptr<pricehistorymgr::ITimeframeFactory> timeframeFactory = communicator->getTimeframeFactory();
                pricehistorymgr::IError* error = NULL;
                O2G2Ptr<IO2GTimeframe> timeframeObj = timeframeFactory->create(timeframe.c_str(), &error);
                O2G2Ptr<pricehistorymgr::IError> autoError(error);
                
                if (!timeframeObj) {
                    throw std::runtime_error(std::format("timeframe {} invalid error={}", timeframe, error->getMessage()));
                }

                O2G2Ptr<pricehistorymgr::IPriceHistoryCommunicatorRequest> request = communicator->createRequest(
                    instrument.c_str(), timeframeObj, date_from, date_to, quotes_count, &error
                );

                if (!request) {
                    throw std::runtime_error(std::format("failded creating request error={}", error->getMessage()));
                }

                responseListener->setRequest(request);

                if (!communicator->sendRequest(request, &error)) {
                    throw std::runtime_error(std::format("failded sending request error={}", error->getMessage()));
                }

                responseListener->wait();
                O2G2Ptr<pricehistorymgr::IPriceHistoryCommunicatorResponse> response = responseListener->getResponse();

                if (!response) {
                    throw std::runtime_error(std::format("failded receiving request error={}", error->getMessage()));
                }

                O2G2Ptr<IO2GMarketDataSnapshotResponseReader> reader = communicator->createResponseReader(response, &error);

                if (!reader) {
                    throw std::runtime_error(std::format("failded to create reader error={}", error->getMessage()));
                }

                if (!reader->isBar()) {
                    throw std::runtime_error("failded sending requestexpected bars");
                }

                auto n = reader->size();

                for (int i = 0; i < n; ++i) {
                    DATE dt = reader->getDate(i); // timestamp of the beginning of the bar period

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

                spdlog::debug(
                    "ForexConnectData::fetch_bars [{} - {}]: loaded {} bars", 
                    format.formatDate(date_from), format.formatDate(date_to), bars.size()
                );
                if (bars.size() > 0) {
                    spdlog::debug(
                        "ForexConnectData::fetch_bars [{} - {}]: \n  first bar timestamp={}, bar={}\n  last bar timestamp={}, bar={}",
                        format.formatDate(date_from), format.formatDate(date_to),
                        format.formatDate(bars.begin()->timestamp), bars.begin()->to_string(),
                        format.formatDate(bars.rbegin()->timestamp), bars.rbegin()->to_string()
                    );
                }
            }
            catch (std::runtime_error& e) {
                spdlog::error("ForexConnectData::fetch_bars error: {}", e.what());
                success = false;
            }
        }

        communicator->removeListener(responseListener);
        communicator->removeStatusListener(communicatorStatusListener);
        statusListener->reset();

        return success;
    }

    bool ForexConnectData::fetch(
        std::vector<common::Quote<DATE>>& quotes,
        const std::string& instrument,
        DATE date_from,
        DATE date_to
    ) {
        quotes.clear();

        if (!logged_in) {
            return false;
        }

        O2G2Ptr<CommunicatorStatusListener> communicatorStatusListener(new CommunicatorStatusListener());
        communicator->addStatusListener(communicatorStatusListener);

        // wait until the communicator signals that it is ready
        if (communicator->isReady() ||
            communicatorStatusListener->waitEvents() && communicatorStatusListener->isReady())
        {
            O2G2Ptr<ResponseListener> responseListener(new ResponseListener());
            communicator->addListener(responseListener);

            communicator->removeListener(responseListener);
        }

        communicator->removeStatusListener(communicatorStatusListener);
        statusListener->reset();

        return true;
    }
    
    bool fetch_historical_prices(
        std::vector<common::BidAskBar<DATE>>& bars,
        pricehistorymgr::IPriceHistoryCommunicator* communicator,
        const char* instrument,
        const char* timeframe,
        DATE from,
        DATE to,
        int quotes_count,
        ResponseListener* responseListener
    ) {
        LocalFormat format;

        spdlog::debug(
            "fetch_historical_prices: instrument={} timeframe={}, from={}, to={}, quotes_count={}",
            instrument, timeframe, format.formatDate(from), format.formatDate(to), quotes_count
        );

        if (!communicator->isReady())
        {
            spdlog::error("fetch_historical_prices phm_error: communicator not ready");
            return false;
        }

        // create timeframe entity
        O2G2Ptr<pricehistorymgr::ITimeframeFactory> timeframeFactory = communicator->getTimeframeFactory();
        pricehistorymgr::IError* error = NULL;
        O2G2Ptr<IO2GTimeframe> timeframeObj = timeframeFactory->create(timeframe, &error);
        O2G2Ptr<pricehistorymgr::IError> auto_error(error);
        if (!timeframeObj)
        {
            spdlog::error("fetch_historical_prices phm_error: timeframe {} invalid", timeframe);
            return false;
        }

        // create and send a history request
        O2G2Ptr<pricehistorymgr::IPriceHistoryCommunicatorRequest> request = communicator->createRequest(
            instrument, timeframeObj, from, to, quotes_count, &error
        );
        if (!request)
        {
            spdlog::error(
                "fetch_historical_prices phm_error: failded creating request {}", 
                error != 0 ? error->getMessage() : "error handler not initialized - cannot get error message"
            );
            return false;
        }

        responseListener->setRequest(request);
        if (!communicator->sendRequest(request, &error))
        {
            spdlog::error(
                "fetch_historical_prices phm_error: failded sending request {}", 
                error != 0 ? error->getMessage() : "error handler not initialized - cannot get error message"
            );
            return false;
        }

        // wait results
        responseListener->wait();

        // print results if any
        O2G2Ptr<pricehistorymgr::IPriceHistoryCommunicatorResponse> response = responseListener->getResponse();
        if (response)
        {
            // use IO2GMarketDataSnapshotResponseReader to extract price data from the response object 
            pricehistorymgr::IError* phm_error = NULL;
            O2G2Ptr<IO2GMarketDataSnapshotResponseReader> reader = communicator->createResponseReader(response, &phm_error);
            O2G2Ptr<pricehistorymgr::IError> phm_auto_error(phm_error);

            if (reader)
            {
                if (!reader->isBar())
                {
                    spdlog::error("fetch_historical_prices phm_error: failded sending requestexpected bars");
                    return false;
                }

                auto n = reader->size();

                if (n > 0) {
                    spdlog::debug(
                        "fetch_historical_prices: {} bars from {} to {} in request interval from {} to {}",
                        n, format.formatDate(reader->getDate(0)), format.formatDate(reader->getDate(n - 1)),
                        format.formatDate(from), format.formatDate(to)
                    );

                    for (int i = 0; i < n; ++i) {
                        DATE dt = reader->getDate(i); // beginning of the bar
                        
                        if (dt < from) {
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

                    spdlog::debug("fetch_historical_prices: successfully loaded {} bars", bars.size());
                    if (bars.size() > 0) {
                        spdlog::debug(
                            "fetch_historical_prices [{} - {}]: first bar timestamp={}, bar={}",
                             format.formatDate(from), format.formatDate(to), format.formatDate(bars.begin()->timestamp), bars.begin()->to_string()
                        );
                        spdlog::debug(
                            "fetch_historical_prices [{} - {}]: last bar timestamp={}, bar={}",
                            format.formatDate(from), format.formatDate(to), format.formatDate(bars.rbegin()->timestamp), bars.rbegin()->to_string()
                        );
                    }

                    return true;
                }
                else {
                    spdlog::error(
                        "fetch_historical_prices phm_error: no bars in request interval from {} to {}",
                        n, format.formatDate(from), format.formatDate(to)
                    );
                    return false;
                }
            }
            else {
                spdlog::error("fetch_historical_prices phm_error: failded create reader");
                return false;
            }
        }
        else {
            spdlog::error(
                "fetch_historical_prices phm_error: failded receiving request {}", 
                error != 0 ? error->getMessage() : "error handler not initialized - cannot get error message"
                );
            return false;
        }
    }

    bool get_historical_prices(
        std::vector<common::BidAskBar<DATE>>& bars,
        const char* login,
        const char* password,
        const char* connection,
        const char* url,
        const char* instrument,
        const char* timeframe,
        DATE date_from,
        DATE date_to,
        const std::string& timezone,
        const char* session_id,
        const char* pin,
        int timeout
    ) {
        LocalFormat format;

        spdlog::debug(
            "get_historical_prices: instrument={} timeframe={}, from={}, to={}",
            instrument, timeframe, format.formatDate(date_from), format.formatDate(date_to)
        );

        try {
            // create the ForexConnect trading session
            O2G2Ptr<IO2GSession> session = CO2GTransport::createSession();
            O2G2Ptr<SessionStatusListener> statusListener = new SessionStatusListener(session, true, session_id, pin, timeout);

            // subscribe IO2GSessionStatus interface implementation for the status events
            session->subscribeSessionStatus(statusListener);
            statusListener->reset();

            // create an instance of IPriceHistoryCommunicator
            pricehistorymgr::IError* error = NULL;
            O2G2Ptr<pricehistorymgr::IPriceHistoryCommunicator> communicator(
                pricehistorymgr::PriceHistoryCommunicatorFactory::createCommunicator(session, "History", &error)
            );
            O2G2Ptr<pricehistorymgr::IError> autoError(error);

            if (!communicator)
            {
                spdlog::error(
                    "get_historical_prices: phm_error {}", 
                    error != 0 ? error->getMessage() : "error handler not initialized - cannot get error message"
                );
                return false;
            }

            // log in to ForexConnect
            bool login_success = session->login(login, password, url, connection);

            spdlog::debug("get_historical_prices: login success={}", login_success);

            bool success = false;

            if (statusListener->waitEvents() && statusListener->isConnected())
            {
                O2G2Ptr<CommunicatorStatusListener> communicatorStatusListener(new CommunicatorStatusListener());
                communicator->addStatusListener(communicatorStatusListener);

                // wait until the communicator signals that it is ready
                if (communicator->isReady() ||
                    communicatorStatusListener->waitEvents() && communicatorStatusListener->isReady())
                {
                    // attach the instance of the class that implements the IPriceHistoryCommunicatorListener interface to the communicator
                    O2G2Ptr<ResponseListener> responseListener(new ResponseListener());
                    communicator->addListener(responseListener);

                    success = fetch_historical_prices(
                        bars,
                        communicator,
                        instrument,
                        timeframe,
                        date_from,
                        date_to,
                        0,
                        responseListener
                    );

                    if (success) {
                        LocalFormat local_format;

                        if (!bars.empty()) {
                            spdlog::debug(
                                "get_historical_prices: done, read {} bars covering {} - {}",
                                bars.size(), local_format.formatDate(bars.begin()->timestamp), local_format.formatDate(bars.rbegin()->timestamp)
                            );
                        }
                        else {
                            spdlog::debug(
                                "get_historical_prices: done, could not load any bars in {} - {}",
                                local_format.formatDate(date_from), local_format.formatDate(date_to)
                            );
                        }
                    }
                    else {
                        spdlog::error("get_historical_prices: phm_error in fetch_historical_prices");
                    }

                    communicator->removeListener(responseListener);
                }

                communicator->removeStatusListener(communicatorStatusListener);
                statusListener->reset();

                session->logout();
                statusListener->waitEvents();
            }

            session->unsubscribeSessionStatus(statusListener);

            return success;
        }
        catch (...)
        {
            spdlog::error("get_historical_prices: got other exception");
            return false;
        }
    }

    /** Gets precision of a specified instrument.

        @param communicator
            The price history communicator.
        @param instrument
            The instrument.
        @return
            The precision.
     */
    int get_instrument_precision(pricehistorymgr::IPriceHistoryCommunicator* communicator, const char* instrument)
    {
        int precision = 6;

        O2G2Ptr<quotesmgr::IQuotesManager> quotesManager = communicator->getQuotesManager();
        quotesmgr::IError* error = NULL;
        O2G2Ptr<quotesmgr::IInstruments> instruments = quotesManager->getInstruments(&error);
        O2G2Ptr<quotesmgr::IError> autoError(error);
        if (instruments)
        {
            O2G2Ptr<quotesmgr::IInstrument> instr = instruments->find(instrument);
            if (instr)
                precision = instr->getPrecision();
        }

        return precision;
    }

    /** Writes history data from response.

        @param communicator
            The price history communicator.
        @param response
            The response. Cannot be null.
        @param instrument
            The instrument.
        @param outputFile
            The output file name.
     */
    void write_prices(
        pricehistorymgr::IPriceHistoryCommunicator* communicator,
        pricehistorymgr::IPriceHistoryCommunicatorResponse* response,
        const char* instrument,
        const char* outputFile)
    {
        std::fstream fs;
        fs.open(outputFile, std::fstream::out | std::fstream::trunc);
        if (!fs.is_open())
        {
            std::cout << "Could not open the output file." << std::endl;
            return;
        }

        LocalFormat localFormat;
        const char* separator = localFormat.getListSeparator();
        int precision = get_instrument_precision(communicator, instrument);

        // use IO2GMarketDataSnapshotResponseReader to extract price data from the response object 
        pricehistorymgr::IError* error = NULL;
        O2G2Ptr<IO2GMarketDataSnapshotResponseReader> reader = communicator->createResponseReader(response, &error);
        O2G2Ptr<pricehistorymgr::IError> autoError(error);
        if (reader)
        {
            if (reader->isBar())
            {
                fs << "DateTime" << separator
                    << "BidOpen" << separator << "BidHigh" << separator
                    << "BidLow" << separator << "BidClose" << separator
                    << "AskOpen" << separator << "AskHigh" << separator
                    << "AskLow" << separator << "AskClose" << separator
                    << "Volume" << separator << std::endl;
            }
            else
            {
                fs << "DateTime" << separator
                    << "Bid" << separator << "Ask" << separator << std::endl;
            }

            for (int i = 0; i < reader->size(); ++i)
            {
                DATE dt = reader->getDate(i);

                std::string time = localFormat.formatDate(dt);
                if (reader->isBar())
                {
                    fs << time << separator
                        << localFormat.formatDouble(reader->getBidOpen(i), precision) << separator
                        << localFormat.formatDouble(reader->getBidHigh(i), precision) << separator
                        << localFormat.formatDouble(reader->getBidLow(i), precision) << separator
                        << localFormat.formatDouble(reader->getBidClose(i), precision) << separator
                        << localFormat.formatDouble(reader->getAskOpen(i), precision) << separator
                        << localFormat.formatDouble(reader->getAskHigh(i), precision) << separator
                        << localFormat.formatDouble(reader->getAskLow(i), precision) << separator
                        << localFormat.formatDouble(reader->getAskClose(i), precision) << separator
                        << localFormat.formatDouble(reader->getVolume(i), 0) << separator
                        << std::endl;
                }
                else
                {
                    fs << time << separator
                        << localFormat.formatDouble(reader->getBid(i), precision) << separator
                        << localFormat.formatDouble(reader->getAsk(i), precision) << separator
                        << std::endl;
                }
            }
        }

        fs.close();
    }

}



