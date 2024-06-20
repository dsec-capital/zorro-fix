#pragma once

#include <string>
#include <iostream>
#include <memory>

#include "ForexConnect.h"
#include "ResponseListener.h"
#include "PriceUpdateController.h"
#include "PeriodCollectionUpdateObserver.h"
#include "price_data/PeriodCollection.h"

class LiveBarStreamer {
    pricehistorymgr::IPriceHistoryCommunicator* communicator;

    std::string symbol;
    DATE date_from;
    DATE date_to;
    IO2GTimeframe* timeframe;
    int quotes_count{ 0 };

    PriceUpdateController priceUpdateController;
    PeriodCollection periods;
    PeriodCollectionUpdateObserver livePriceViewer;

    void process_history(pricehistorymgr::IPriceHistoryCommunicatorResponse* response);

public:
    LiveBarStreamer(
        IO2GSession* session,
        pricehistorymgr::IPriceHistoryCommunicator* communicator,
        const std::string& symbol,
        DATE from,
        DATE to,
        IO2GTimeframe* timeframe,
        int quotes_count = 0
    );

    ~LiveBarStreamer();

    bool subscribe();
};
