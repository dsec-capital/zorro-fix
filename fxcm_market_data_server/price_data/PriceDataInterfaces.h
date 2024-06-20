#pragma once

#include <string>

#include "ForexConnect.h"

/** Interface to the offer. */
class IOffer : public IAddRef
{
 public:
    /** Gets the instrument name. */
    virtual std::string getInstrument() = 0;

    /** Gets the date/time (in UTC time zone) when the offer was updated the last time. */
    virtual DATE getLastUpdate() = 0;

    /** Gets the latest offer bid price. */
    virtual double getBid() = 0;

    /** Gets the latest offer ask price.*/
    virtual double getAsk() = 0;

    /** Gets the offer accumulated last minute volume. */
    virtual int getMinuteVolume() = 0;

    /** Gets the number of significant digits after decimal point. */
    virtual int getDigits() = 0;

    /** Makes a copy of the offer. */
    virtual IOffer *clone() = 0;

 protected:
    virtual ~IOffer() {}
};

///////////////////////////////////////////////////////////////////////////////

/** A price bar (candle) interface. */
class IBar : public IAddRef
{
 public:
    /** Open (the first price) of the time period. */
    virtual double getOpen() = 0;

    /** High (the greatest price) of the time period. */
    virtual double getHigh() = 0;

    /** Low (the smallest price) of the time period. */
    virtual double getLow() = 0;

    /** Close (the latest price) of the time period. */
    virtual double getClose() = 0;

 protected:
    virtual ~IBar() {}
};

///////////////////////////////////////////////////////////////////////////////

/** The interface for the price information in the time period. */
class IPeriod : public IAddRef
{
 public:
    /** Gets date/time when the period starts (In UTC time zone). */
    virtual DATE getTime() = 0;

    /** Gets ask bar. */
    virtual IBar *getAsk() = 0;

    /** Gets bid bar. */
    virtual IBar *getBid() = 0;

    /** Gets tick volume. */
    virtual int getVolume() = 0;

 protected:
    virtual ~IPeriod() {}
};

///////////////////////////////////////////////////////////////////////////////

class IPeriodCollection;

/** The listener for period collection updates. 
    Used by IPeriodCollection.
 */
class ICollectionUpdateListener
{
 public:
    virtual void onCollectionUpdate(IPeriodCollection *collection, int index) = 0;

 protected:
    virtual ~ICollectionUpdateListener() {}
};

///////////////////////////////////////////////////////////////////////////////

/** Interface to the collection of the price periods. */
class IPeriodCollection : public IAddRef
{
 public:
    /** The count of periods in the collection. */
    virtual int size() = 0;

    /** Get the period by its index. */
    virtual IPeriod *getPeriod(int index) = 0;

    /** Gets the instrument name of the collection. */
    virtual const char *getInstrument() = 0;

    /** Gets the timeframe name of the collection. */
    virtual const char *getTimeframe() = 0;

    /** Gets flag indicating that the collection is alive (i.e. is updated when a new price coming). */
    virtual bool isAlive() = 0;

    /** Adds an update listener. */
    virtual void addListener(ICollectionUpdateListener *callback) = 0;

    /** Removes an update listener. */
    virtual void removeListener(ICollectionUpdateListener *callback) = 0;

 protected:
    virtual ~IPeriodCollection() {}
};
