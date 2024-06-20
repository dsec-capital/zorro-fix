#include "Offer.h"

Offer::Offer(std::string instrument, DATE lastUpdate, double bid, double ask, int minuteVolume, int digits) :
    mInstrument(instrument),
    mLastUpdate(lastUpdate),
    mBid(bid),
    mAsk(ask),
    mMinuteVolume(minuteVolume),
    mDigits(digits)
{
}

Offer::~Offer()
{
}

void Offer::setLastUpdate(DATE lastUpdate)
{
    mLastUpdate = lastUpdate;
}

void Offer::setBid(double bid)
{
    mBid = bid;
}

void Offer::setAsk(double ask)
{
    mAsk = ask;
}

void Offer::setMinuteVolume(int minuteVolume)
{
    mMinuteVolume = minuteVolume;
}

std::string Offer::getInstrument()
{
    return mInstrument;
}

DATE Offer::getLastUpdate()
{
    return mLastUpdate;
}

double Offer::getBid()
{
    return mBid;
}

double Offer::getAsk()
{
    return mAsk;
}

int Offer::getMinuteVolume()
{
    return mMinuteVolume;
}

int Offer::getDigits()
{
    return mDigits;
}

IOffer *Offer::clone()
{
    return new Offer(mInstrument, mLastUpdate, mBid, mAsk, mMinuteVolume, mDigits);
}
