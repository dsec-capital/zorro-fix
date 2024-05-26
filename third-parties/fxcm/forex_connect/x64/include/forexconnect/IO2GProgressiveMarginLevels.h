#pragma once


/** Progressive Margin Levels*/
class IO2GProgressiveMarginLevels : public IAddRef
{
 public:
    virtual ~IO2GProgressiveMarginLevels() = default;
 
    /** Get From amount thresholds to specific level */
    virtual int getFrom(int index) = 0;

    /** Get Till (including) amount thresholds to specific level
    Last level defined Till as -1 meaning infinitive.*/
    virtual int getTill(int index) = 0;

    /** Get Margin Per Contract*/
    virtual double getMarginPerContract(int index) = 0;

    /** Levels count.*/
    virtual int getCount() = 0;
};

