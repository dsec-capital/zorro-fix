#ifndef PRICE_SAMPLER_H
#define PRICE_SAMPLER_H

#include <random>
#include <memory>
#include <toml++/toml.hpp>

namespace common {

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

    std::shared_ptr<PriceSampler> price_sampler_factory(
        std::mt19937& generator,
        toml::table& tbl,
        double price,
        double spread,
        double tick_size,
        int initial_dir = 1
    );

}

#endif 