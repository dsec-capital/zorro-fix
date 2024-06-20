#include "LiveBarStreamer.h"

#include "common/time_utils.h"

#include "spdlog/spdlog.h"

LiveBarStreamer::LiveBarStreamer(
    IO2GSession* session,
    pricehistorymgr::IPriceHistoryCommunicator* communicator,
    const std::string& symbol,
    DATE from,
    DATE to,
    IO2GTimeframe* timeframe,
    int quotes_count
) : communicator(communicator) 
  , symbol(symbol)
  , date_from(from)
  , date_to(to)
  , timeframe(timeframe)
  , quotes_count(quotes_count)
  , priceUpdateController(session, symbol.c_str())
  , periods(symbol.c_str(), timeframe->getID(), true, &priceUpdateController) 
  , livePriceViewer(&periods)
{
    auto ready = true;
    if (!priceUpdateController.wait())
        ready = false;
}

LiveBarStreamer::~LiveBarStreamer() {
    spdlog::debug("LiveBarStreamer: cancel live update for {}", symbol);
}

bool LiveBarStreamer::subscribe()
{
    if (!communicator->isReady())
    {
        spdlog::error("LiveBarStreamer: communicator is not ready for");
        return false;
    }

    // create and send a history request
    pricehistorymgr::IError* error = NULL;
    O2G2Ptr<pricehistorymgr::IPriceHistoryCommunicatorRequest> request(
        communicator->createRequest(symbol.c_str(), timeframe, date_from, date_to, quotes_count, &error));
    O2G2Ptr<pricehistorymgr::IError> autoError(error);
    if (!request)
    {
        spdlog::error("LiveBarStreamer: could not create request {}", error ? error->getMessage() : "unknow error");
        return false;
    }

    O2G2Ptr<ResponseListener> responseListener(new ResponseListener());
    communicator->addListener(responseListener);

    responseListener->setRequest(request);
    if (!communicator->sendRequest(request, &error))
    {
        communicator->removeListener(responseListener);
        spdlog::error("LiveBarStreamer: could not send request {}", error ? error->getMessage() : "unknow error");
        return false;
    }

    // wait results
    responseListener->wait();

    O2G2Ptr<pricehistorymgr::IPriceHistoryCommunicatorResponse> response(responseListener->getResponse());

    communicator->removeListener(responseListener);

    O2G2Ptr<IO2GMarketDataSnapshotResponseReader> reader = communicator->createResponseReader(response, &error);
    if (error)
    {
        spdlog::error("LiveBarStreamer: could not create response reader {}", error ? error->getMessage() : "unknow error");
        return false;
    }

    process_history(response);

    // finally notify the collection that all bars are added, so it can
    // add all ticks collected while the request was being executed
    // and start update the data by forthcoming ticks
    periods.finish(reader->getLastBarTime(), reader->getLastBarVolume());

    return true;
}

void LiveBarStreamer::process_history(pricehistorymgr::IPriceHistoryCommunicatorResponse* response)
{
    // use IO2GMarketDataSnapshotResponseReader to extract price data from the response object 
    pricehistorymgr::IError* error = NULL;
    O2G2Ptr<IO2GMarketDataSnapshotResponseReader> reader = communicator->createResponseReader(response, &error);
    O2G2Ptr<pricehistorymgr::IError> autoError(error);
    if (reader)
    {
        for (int i = 0; i < reader->size(); ++i)
        {
            DATE dt = reader->getDate(i);
            auto ns = common::date_to_nanos(dt);
            std::string time = common::to_string(ns);

            periods.add(reader->getDate(i), reader->getBidOpen(i), reader->getBidHigh(i), reader->getBidLow(i), reader->getBidClose(i),
                reader->getAskOpen(i), reader->getAskHigh(i), reader->getAskLow(i), reader->getAskClose(i), reader->getVolume(i));

            std::stringstream ss;
            ss << "Symbol=" << symbol
               << ", DateTime=" << time << std::fixed << std::setprecision(6);

            if (reader->isBar())
            {
                ss << ", BidOpen=" << reader->getBidOpen(i) << ", BidHigh=" << reader->getBidHigh(i)
                   << ", BidLow=" << reader->getBidLow(i) << ", BidClose=" << reader->getBidClose(i)
                   << ", AskOpen=" << reader->getAskOpen(i) << ", AskHigh=" << reader->getAskHigh(i)
                   << ", AskLow=" << reader->getAskLow(i) << ", AskClose=" << reader->getAskClose(i)
                   << ", Volume=" << reader->getVolume(i);
            }
            else
            {
                ss << ", Bid=" << reader->getBid(i) << ", Ask=" << reader->getAsk(i);
            }
            spdlog::debug(ss.str());
        }
    }
}

