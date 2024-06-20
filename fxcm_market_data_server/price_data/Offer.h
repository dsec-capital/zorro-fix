#pragma once

#include "PriceDataInterfaces.h"
#include "../ThreadSafeAddRefImpl.h"

/** Implementation of the offer interface. */
class Offer : public TThreadSafeAddRefImpl<IOffer>
{
 public:
    Offer(std::string instrument, DATE lastUpdate, double bid, double ask, int minuteVolume, int digits);

    void setLastUpdate(DATE lastUpdate);
    void setBid(double bid);
    void setAsk(double ask);
    void setMinuteVolume(int minuteVolume);

 public:
    /** @name IOffer interface implementation */
    //@{
    virtual std::string getInstrument();
    virtual DATE getLastUpdate();
    virtual double getBid();
    virtual double getAsk();
    virtual int getMinuteVolume();
    virtual int getDigits();
    virtual IOffer *clone();
    //@}

 protected:
    virtual ~Offer();

 private:
    std::string mInstrument;
    DATE mLastUpdate;
    double mBid;
    double mAsk;
    int mMinuteVolume;
    int mDigits;
};
