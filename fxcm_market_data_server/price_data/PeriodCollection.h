#pragma once

#include <queue>
#include <memory>

#include "PriceDataInterfaces.h"
#include "../PriceUpdateController.h"
#include "../ThreadSafeAddRefImpl.h"

#include "../log.h"

class Period;

/** Implementation of the period collection. */
class PeriodCollection : public IPriceUpdateListener, public TThreadSafeAddRefImpl<IPeriodCollection>
{
 public:
    PeriodCollection(const char *instrument, const char *timeframe, bool alive, IPriceUpdateController *controller);
    
    virtual ~PeriodCollection();

    /** Adds a new historical period into the collection.
    
        @param time
            The date/time when the period starts
        @param bidOpen
            The open bid price of the period
        @param bidHigh
            The high bid price of the period
        @param bidLow
            The low bid price of the period
        @param bidClose
            The close bid price of the period
        @param askOpen
            The open ask price of the period
        @param askHigh
            The high ask price of the period
        @param askLow
            The low ask price of the period
        @param askClose
            The close ask price of the period        
        @param volume
            The minute volume of the period
     */
    void add(DATE time, double bidOpen, double bidHigh, double bidLow, double bidClose,
             double askOpen, double askHigh, double askLow, double askClose, int volume);

    /** Indicates that loading of the historical data finished and sets 
        the accumulated volume of the last minute.

        @param lastMinute 
            The last minute when the server accumulated the tick volume. 
        @param lastMinuteVolume
            The last minute accumulated tick volume
     */
    void finish(DATE lastMinute, int lastMinuteVolume);

 public:
    /** @name IPeriodCollection interface implementation */
    //@{
    virtual int size();
    virtual IPeriod *getPeriod(int index);
    virtual const char *getInstrument();
    virtual const char *getTimeframe();
    virtual bool isAlive();
    virtual void addListener(ICollectionUpdateListener* callback);
    virtual void removeListener(ICollectionUpdateListener* callback);
    //@}

 protected:
    /** @name IPriceUpdateListener interface implementation */
    //@{
    virtual void onCollectionUpdate(IOffer *offer);
    //@}

 private:
    void handleOffer(IOffer *offer);
    void notifyLastPeriodUpdated();
    INT64 dateToMinute(DATE time);

    /** Storage of the periods. */
    typedef std::vector<O2G2Ptr<Period> > Periods;
    Periods mPeriods;

    /** The name of the instrument. */
    std::string mInstrument;
    /** The name of the time frame. */
    std::string mTimeframe;
    /** Timeframe parsed.  */
    quotesmgr::CandlePeriod::Unit mTimeframeUnit;
    int mTimeframeLength;
    /** The offset when the trading day starts (in hours). */
    int mTradingDayOffset;
    /** Last minute when tick volume was accumulated. */
    INT64 mLastMinute;
    /** The accumulated tick volume. */
    int mLastMinuteVolume;
    /** The flag indicating whether collection is alive (e.g. receives updates). */
    bool mAlive;
    /** The flag indicating whether collection is completely filled with initial data. */
    bool mFilled;
    /** Mutex. */
    Mutex mMutex;
    /** The price update controller. */
    IPriceUpdateController *mController;
    /** The queue of the ticks received when collection is already created, but
        isn't filled yet by the data, received from the server.
     */
    std::queue<O2G2Ptr<IOffer> > mWaitingUpdates;
    /** The listeners. */
    typedef std::list<ICollectionUpdateListener *> Listeners;
    Listeners mListeners;

    std::shared_ptr<spdlog::logger> data_logger;
};
