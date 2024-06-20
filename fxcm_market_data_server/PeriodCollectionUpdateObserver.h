#pragma once

#include <string>
#include <iomanip>
#include <iostream>

#include "ForexConnect.h"

#include "price_data/PriceDataInterfaces.h"

/** The observer for live prices. It listens for periods collection updates and prints them. */
class PeriodCollectionUpdateObserver : public ICollectionUpdateListener
{
 public:
    PeriodCollectionUpdateObserver(IPeriodCollection *collection);
    ~PeriodCollectionUpdateObserver();

    void unsubscribe();

 protected:
    /** @name ICollectionUpdateListener interface implementation */
    //@{
    void onCollectionUpdate(IPeriodCollection *collection, int index);
    //@}

 private:
    O2G2Ptr<IPeriodCollection> mCollection;
};

