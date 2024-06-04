
/*
    !!! Caution:
    Do not change anything in this source code because it was automatically generated
    from XML class description
*/
#pragma once
#include "../IO2GRow.h"

class Order2Go2 IO2GSummaryRow : public IO2GRow
{
 protected:
    IO2GSummaryRow();
  
 public:

    
    //

};


class IO2GSummaryTableRow : public IO2GSummaryRow
{
 protected:
    IO2GSummaryTableRow();

 public:
    
    virtual const char* getOfferID() = 0;
    virtual int getDefaultSortOrder() = 0;
    virtual const char* getInstrument() = 0;
    virtual double getSellNetPL() = 0;
    virtual double getSellNetPLPip() = 0;
    virtual double getSellAmount() = 0;
    virtual double getSellAvgOpen() = 0;
    virtual double getBuyClose() = 0;
    virtual double getSellClose() = 0;
    virtual double getBuyAvgOpen() = 0;
    virtual double getBuyAmount() = 0;
    virtual double getBuyNetPL() = 0;
    virtual double getBuyNetPLPip() = 0;
    virtual double getAmount() = 0;
    virtual double getGrossPL() = 0;
    virtual double getNetPL() = 0;
    virtual double getRolloverInterestSum() = 0;
    virtual double getUsedMargin() = 0;
    virtual double getUsedMarginBuy() = 0;
    virtual double getUsedMarginSell() = 0;
    virtual double getCommission() = 0;
    virtual double getCloseCommission() = 0;
    virtual double getDividends() = 0;
    //

};

