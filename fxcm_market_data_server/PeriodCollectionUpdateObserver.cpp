#include "PeriodCollectionUpdateObserver.h"

#include "common/time_utils.h"

PeriodCollectionUpdateObserver::PeriodCollectionUpdateObserver(IPeriodCollection *collection)
    : mCollection(collection)
{
    if (NULL == collection)
        return;

    collection->addRef();
    mCollection->addListener(this);
}

PeriodCollectionUpdateObserver::~PeriodCollectionUpdateObserver()
{
    unsubscribe();
}

void PeriodCollectionUpdateObserver::onCollectionUpdate(IPeriodCollection *collection, int index)
{
    O2G2Ptr<IPeriod> period(collection->getPeriod(index));
    O2G2Ptr<IBar> bid(period->getBid());
    O2G2Ptr<IBar> ask(period->getAsk());

    auto ns = common::date_to_nanos(period->getTime());
    std::string sDate = common::to_string(ns);

    std::cout << "Price updated:";
    std::cout << std::setprecision(6) 
        << " DateTime=" << sDate 
        << ", BidOpen=" << bid->getOpen()
        << ", BidHigh=" << bid->getHigh()
        << ", BidLow=" << bid->getLow()
        << ", BidClose=" << bid->getClose()
        << ", AskOpen=" << ask->getOpen()
        << ", AskHigh=" << ask->getHigh()
        << ", AskLow=" << ask->getLow()
        << ", AskClose=" << ask->getClose()
        << ", Volume=" << period->getVolume()
        << std::endl;
}

void PeriodCollectionUpdateObserver::unsubscribe()
{
    mCollection->removeListener(this);
}
