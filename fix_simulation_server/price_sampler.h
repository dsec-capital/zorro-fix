#pragma once

#include <random>

class PriceSampler {
protected:
    std::mt19937& gen;
public:
    PriceSampler(std::mt19937& gen) : gen(gen) {}

    virtual double initial_mid_price() = 0;
    virtual double initial_spread() = 0;
    virtual double next_mid_price() = 0;
    virtual double next_spread() = 0;
};