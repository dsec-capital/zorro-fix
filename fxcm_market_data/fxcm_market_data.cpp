#include "pch.h"

#include "fxcm_market_data.h"

#include "ResponseListener.h"
#include "SessionStatusListener.h"
#include "LoginParams.h"
#include "SampleParams.h"
#include "CommonSources.h"
#include "CommunicatorStatusListener.h"
#include "LocalFormat.h"

namespace fxcm {
    
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
        if (!communicator->isReady())
        {
            spdlog::error("fetch_historical_prices error: communicator not ready");
            return false;
        }

        // create timeframe entity
        O2G2Ptr<pricehistorymgr::ITimeframeFactory> timeframeFactory = communicator->getTimeframeFactory();
        pricehistorymgr::IError* error = NULL;
        O2G2Ptr<IO2GTimeframe> timeframeObj = timeframeFactory->create(timeframe, &error);
        O2G2Ptr<pricehistorymgr::IError> autoError(error);
        if (!timeframeObj)
        {
            spdlog::error("fetch_historical_prices error: timeframe {} invalid", timeframe);
            return false;
        }

        // create and send a history request
        O2G2Ptr<pricehistorymgr::IPriceHistoryCommunicatorRequest> request =
            communicator->createRequest(instrument, timeframeObj, from, to, quotes_count, &error);
        if (!request)
        {
            spdlog::error("fetch_historical_prices error: failded creating request {}", error->getMessage());
            return false;
        }

        responseListener->setRequest(request);
        if (!communicator->sendRequest(request, &error))
        {
            spdlog::error("fetch_historical_prices error: failded sending request {}", error->getMessage());
            return false;
        }

        // wait results
        responseListener->wait();

        // print results if any
        O2G2Ptr<pricehistorymgr::IPriceHistoryCommunicatorResponse> response = responseListener->getResponse();
        if (response)
        {
            // use IO2GMarketDataSnapshotResponseReader to extract price data from the response object 
            pricehistorymgr::IError* error = NULL;
            O2G2Ptr<IO2GMarketDataSnapshotResponseReader> reader = communicator->createResponseReader(response, &error);
            O2G2Ptr<pricehistorymgr::IError> autoError(error);

            if (reader)
            {
                if (!reader->isBar())
                {
                    spdlog::error("fetch_historical_prices error: failded sending requestexpected bars");
                    return false;
                }

                auto n = reader->size();

                LocalFormat format;

                if (n > 0) {
                    spdlog::debug(
                        "fetch_historical_prices: {} bars from {} to {} in request interval from {} to {}",
                        n, format.formatDate(reader->getDate(0)), format.formatDate(reader->getDate(n - 1)),
                        format.formatDate(from), format.formatDate(to)
                    );

                    for (int i = 0; i < n; ++i) {
                        DATE dt = reader->getDate(i); // beginning of the bar

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

                    return true;
                }
                else {
                    spdlog::error(
                        "fetch_historical_prices error: no bars in request interval from {} to {}",
                        n, format.formatDate(from), format.formatDate(to)
                    );
                    return false;
                }
            }
            else {
                spdlog::error("fetch_historical_prices error: failded create reader");
                return false;
            }
        }
        else {
            spdlog::error("fetch_historical_prices error: failded receiving request {}", error->getMessage());
            return false;
        }
    }

    int get_historical_prices(
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
        const char* pin
    ) {
        // create the ForexConnect trading session
        O2G2Ptr<IO2GSession> session = CO2GTransport::createSession();
        O2G2Ptr<SessionStatusListener> statusListener = new SessionStatusListener(session, true, session_id, pin);

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
            spdlog::error("get_historical_prices: error {}", error->getMessage());
            return -1;
        }

        // log in to ForexConnect
        session->login(login, password, url, connection);

        bool has_error = false;

        if (statusListener->waitEvents() && statusListener->isConnected())
        {
            O2G2Ptr<CommunicatorStatusListener> communicatorStatusListener(new CommunicatorStatusListener());
            communicator->addStatusListener(communicatorStatusListener);

            // wait until the communicator signals that it is ready
            if (communicator->isReady() ||
                communicatorStatusListener->waitEvents() && communicatorStatusListener->isReady())
            {
                // attach the instance of the class that implements the IPriceHistoryCommunicatorListener
                // interface to the communicator
                O2G2Ptr<ResponseListener> responseListener(new ResponseListener());
                communicator->addListener(responseListener);

                has_error = fetch_historical_prices(
                    bars,
                    communicator,
                    instrument,
                    timeframe,
                    date_from,
                    date_to,
                    0,
                    responseListener
                );

                std::cout << "Done!" << std::endl;

                communicator->removeListener(responseListener);
            }

            communicator->removeStatusListener(communicatorStatusListener);
            statusListener->reset();

            session->logout();
            statusListener->waitEvents();
        }
        else {
            has_error = true;
        }

        session->unsubscribeSessionStatus(statusListener);

        return has_error ? -1 : 0;
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
        const char* outputFile,
        bool utcMode)
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

                if (!utcMode)
                    dt = hptools::date::DateConvertTz(dt, hptools::date::UTC, hptools::date::EST);

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

    /** Print sample parameters data.

        @param sProcName
            The sample process name.
        @param loginParams
            The LoginParams instance pointer.
        @param sampleParams
            The LoginParams instance pointer.
     */
    void print_sample_params(std::string& sProcName, LoginParams* loginParams, SampleParams* sampleParams)
    {
        std::cout << "Running " << sProcName << " with arguments:" << std::endl;

        LocalFormat localFormat;

        // login (common) information
        if (loginParams)
        {
            std::cout << loginParams->getLogin() << " * "
                << loginParams->getURL() << " "
                << loginParams->getConnection() << " "
                << loginParams->getSessionID() << " "
                << loginParams->getPin() << std::endl;
        }

        // sample specific information
        if (sampleParams)
        {
            std::cout << "Instrument='" << sampleParams->getInstrument() << "', "
                << "Timeframe='" << sampleParams->getTimeframe() << "', ";

            const char* timezone = sampleParams->getTimezone();
            if (isNaN(sampleParams->getDateFrom()))
                std::cout << "DateFrom='', ";
            else
            {
                auto dateFrom = sampleParams->getDateFrom();

                if (strcmp(timezone, "EST") == 0)
                    dateFrom = hptools::date::DateConvertTz(dateFrom, hptools::date::UTC, hptools::date::EST);

                std::cout << "DateFrom='" << localFormat.formatDate(dateFrom) << "' (" << timezone << "), ";
            }

            if (isNaN(sampleParams->getDateTo()))
                std::cout << "DateTo='', ";
            else
            {
                auto dateTo = sampleParams->getDateTo();

                if (strcmp(timezone, "EST") == 0)
                    dateTo = hptools::date::DateConvertTz(dateTo, hptools::date::UTC, hptools::date::EST);

                std::cout << "DateTo='" << localFormat.formatDate(dateTo) << "' (" << timezone << "), ";
            }

            std::cout << "QuotesCount='" << sampleParams->getQuotesCount() << "'";

            std::cout << std::endl;
        }
    }

    /** Print expected sample-login parameters and their description.

        @param sProcName
            The sample process name.
     */
    void print_help(std::string& sProcName)
    {
        std::cout << sProcName << " sample parameters:" << std::endl << std::endl;

        std::cout << "/login | --login | /l | -l" << std::endl;
        std::cout << "Your user name." << std::endl << std::endl;

        std::cout << "/password | --password | /p | -p" << std::endl;
        std::cout << "Your password." << std::endl << std::endl;

        std::cout << "/url | --url | /u | -u" << std::endl;
        std::cout << "The server URL. For example, http://www.fxcorporate.com/Hosts.jsp." << std::endl << std::endl;

        std::cout << "/connection | --connection | /c | -c" << std::endl;
        std::cout << "The connection name. For example, \"Demo\" or \"Real\"." << std::endl << std::endl;

        std::cout << "/sessionid | --sessionid " << std::endl;
        std::cout << "The database name. Required only for users who have accounts in more than one database. "
            "Optional parameter." << std::endl << std::endl;

        std::cout << "/pin | --pin " << std::endl;
        std::cout << "Your pin code. Required only for users who have a pin. "
            "Optional parameter." << std::endl << std::endl;

        std::cout << "/instrument | --instrument | /i | -i" << std::endl;
        std::cout << "An instrument which you want to use in sample. "
            "For example, \"EUR/USD\"." << std::endl << std::endl;

        std::cout << "/timeframe | --timeframe " << std::endl;
        std::cout << "Time period which forms a single candle. "
            "For example, m1 - for 1 minute, H1 - for 1 hour." << std::endl << std::endl;

        std::cout << "/datefrom | --datefrom " << std::endl;
        std::cout << "Date/time from which you want to receive historical prices. "
            "If you leave this argument as it is, it will mean from last trading day. "
            "Format is \"m.d.Y H:M:S\". Optional parameter." << std::endl << std::endl;

        std::cout << "/dateto | --dateto " << std::endl;
        std::cout << "Date/time until which you want to receive historical prices. "
            "If you leave this argument as it is, it will mean to now. Format is \"m.d.Y H:M:S\". "
            "Optional parameter." << std::endl << std::endl;

        std::cout << "/tz | --tz " << std::endl;
        std::cout << "Timezone for /datefrom and /dateto parameters: EST or UTC "
            "Optional parameter. Default: EST" << std::endl << std::endl;

        std::cout << "/count | --count " << std::endl;
        std::cout << "Count of historical prices you want to receive. If you "
            << "leave this argument as it is, it will mean -1 (use some default "
            << "value or ignore if datefrom is specified)" << std::endl << std::endl;

        std::cout << "/output | --output " << std::endl;
        std::cout << "The output file name." << std::endl;
    }

    inline std::tm gmtime_xp(const std::time_t& t) {
        std::tm bt{};
#if defined(_MSC_VER)
        gmtime_s(&bt, &t);
#else
        bt = *std::gmtime(&t);
#endif 
        return bt;
    }

    /** Check parameters for correct values.

        @param loginParams
            The LoginParams instance pointer.
        @param sampleParams
            The SampleParams instance pointer.
        @return
            true if parameters are correct.
     */
    bool check_obligatory_params(LoginParams* loginParams, SampleParams* sampleParams)
    {
        // check login parameters
        if (strlen(loginParams->getLogin()) == 0)
        {
            std::cout << LoginParams::Strings::loginNotSpecified << std::endl;
            return false;
        }
        if (strlen(loginParams->getPassword()) == 0)
        {
            std::cout << LoginParams::Strings::passwordNotSpecified << std::endl;
            return false;
        }
        if (strlen(loginParams->getURL()) == 0)
        {
            std::cout << LoginParams::Strings::urlNotSpecified << std::endl;
            return false;
        }
        if (strlen(loginParams->getConnection()) == 0)
        {
            std::cout << LoginParams::Strings::connectionNotSpecified << std::endl;
            return false;
        }

        // check other parameters
        if (strlen(sampleParams->getInstrument()) == 0)
        {
            std::cout << SampleParams::Strings::instrumentNotSpecified << std::endl;
            return false;
        }
        if (strlen(sampleParams->getTimeframe()) == 0)
        {
            std::cout << SampleParams::Strings::timeframeNotSpecified << std::endl;
            return false;
        }
        if (strlen(sampleParams->getOutputFile()) == 0)
        {
            std::cout << SampleParams::Strings::outputFileNotSpecified << std::endl;
            return false;
        }

        const char* timezone = sampleParams->getTimezone();
        if (strcmp(timezone, "EST") != 0 && strcmp(timezone, "UTC") != 0)
        {
            std::cout << SampleParams::Strings::timezoneNotSupported << ": " << timezone << std::endl;
            return false;
        }

        bool bIsDateFromNotSpecified = false;
        bool bIsDateToNotSpecified = false;
        DATE dtFrom = sampleParams->getDateFrom();
        DATE dtTo = sampleParams->getDateTo();

        time_t tNow = time(NULL); // get time now

        //struct tm *tmNow = gmtime(&tNow);
        std::tm tmNow = gmtime_xp(tNow);

        DATE dtNow = 0;
        CO2GDateUtils::CTimeToOleTime(&tmNow, &dtNow);
        LocalFormat localFormat;

        if (isNaN(dtFrom))
        {
            bIsDateFromNotSpecified = true;
            dtFrom = 0;
            sampleParams->setDateFrom(dtFrom);
        }
        else
        {
            if (dtFrom - dtNow > 0.0001)
            {
                std::cout << "Sorry, 'DateFrom' value " << localFormat.formatDate(dtFrom) << " should be in the past" << std::endl;
                return false;
            }
        }

        if (isNaN(dtTo))
        {
            bIsDateToNotSpecified = true;
            dtTo = 0;
            sampleParams->setDateTo(dtTo);
        }
        else
        {
            if (!bIsDateFromNotSpecified && dtFrom - dtTo > 0.001)
            {
                std::cout << "Sorry, 'DateTo' value " << localFormat.formatDate(dtTo) << " should be later than 'DateFrom' value "
                    << localFormat.formatDate(dtFrom) << std::endl;
                return false;
            }
        }

        return true;
    }

}



