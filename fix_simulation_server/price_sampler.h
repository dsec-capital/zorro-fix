#pragma once

#include <random>

class PriceSampler {
protected:
    std::mt19937& gen;
public:
    PriceSampler(std::mt19937& gen) : gen(gen) {}

    virtual double actualMidPrice() = 0;
    virtual double actualSpread() = 0;
    virtual double actualBidVolume() = 0;
    virtual double actualAskVolume() = 0;

    virtual double nextMidPrice() = 0;
    virtual double nextSpread() = 0;
    virtual double nextBidVolume() = 0;
    virtual double nextAskVolume() = 0;
};