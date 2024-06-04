#pragma once

class IO2GProgressiveMarginLevels;

/** Trading settings provider.*/
class IO2GTradingSettingsProvider : public IAddRef
{
 protected:
    IO2GTradingSettingsProvider(){};
 public:

    /** Gets condition distance for the stop. */
    virtual int getCondDistStopForTrade(const char* instrument) = 0;

    /** Gets condition distance for the limit. */
    virtual int getCondDistLimitForTrade(const char* instrument) = 0;

    /** Gets condition distance for the entry stop. */
    virtual int getCondDistEntryStop(const char* instrument) = 0;

    /** Gets condition distance for the entry limit. */
    virtual int getCondDistEntryLimit(const char* instrument) = 0;

    /** Gets minimum quantity (in lots) for an order for specified instrument and account. */
    virtual int getMinQuantity(const char* instrument, IO2GAccountRow* account) = 0;

    /** Gets maximum quantity (in lots) for an order for specified instrument and account. */
    virtual int getMaxQuantity(const char* instrument, IO2GAccountRow* account) = 0;

    /** Gets base unit size by the account identifier and instrument identifier. */
    virtual int getBaseUnitSize(const char* instrument, IO2GAccountRow* account) = 0;

    /** Gets marker status. */
    virtual O2GMarketStatus getMarketStatus(const char* instrument) = 0;

    /** Gets minimal trailing step. */
    virtual int getMinTrailingStep() = 0;

    /** Gets maximal trailing step. */
    virtual int getMaxTrailingStep() = 0;

    /** Gets a value of SHOW_MR system property as a enum value. */
    virtual O2GShowMR getShowMR() const = 0;

    /** Gets the MMR for the specified instrument by account. */
    virtual double getMMR(const char* instrument, IO2GAccountRow* account) = 0;

    /** Gets the MMR/LMR/EMR for the specified instrument by account.

        @param  instrument      Instrument.
        @param  account         Account row.
        @param  mmr             [out] MMR.
        @param  emr             [out] EMR.
        @param  lmr             [out] LMR.

        @return Whether three level margin is used.
    */
    virtual bool getMargins(const char* instrument, IO2GAccountRow* account, double& mmr, double& emr, double& lmr) = 0;

    /** Gets the Progressive margins levels for the specified instrument.

        @param  instrument      Instrument.
        @param  account         Account row.

        @return Progressive margins levels. If progressive margins is not used then return nullptr 
    */
    virtual IO2GProgressiveMarginLevels* getProgressiveMarginLevels(const char* instrument, IO2GAccountRow* account) = 0;

    /** Detect whether three level margin is used  for specified account.

        @param  account         Account row.

        @return Whether three level margin is used.
    */
    virtual bool isTier3(IO2GAccountRow* account) = 0;

    /** Detect whether progressive margin is used for specified instrument.

        @param  instrument      Instrument.
        @param  account         Account row.cd 

        @return Whether progressive margin is used.
    */
    virtual bool isProgressiveMargin(const char* instrument, IO2GAccountRow* account) = 0;
};

