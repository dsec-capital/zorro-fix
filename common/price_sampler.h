#ifndef PRICE_SAMPLER_H
#define PRICE_SAMPLER_H

#include <chrono>
#include <random>
#include <memory>
#include <toml++/toml.hpp>

#include "market_data.h"

namespace common {

    class PriceSampler {
    protected:

        std::mt19937& gen;

    public:

        typedef std::map<std::chrono::nanoseconds, TopOfBook> history_t;

        PriceSampler(std::mt19937& gen) : gen(gen) {}

        virtual std::chrono::nanoseconds actual_time() const = 0;

        virtual double actual_bid_price() const = 0;
        virtual double actual_ask_price() const = 0;
        virtual double actual_bid_volume() const = 0;
        virtual double actual_ask_volume() const = 0;

        virtual double actual_spread() const = 0;

        virtual void simulate_next(const std::chrono::nanoseconds& now) = 0;

        virtual void initialize_history(
            const std::chrono::nanoseconds& from, 
            const std::chrono::nanoseconds& now,
            const std::chrono::nanoseconds& sample_period
        ) = 0;

        history_t::const_reverse_iterator history_rbegin() const {
            return history.crbegin();
        }

        history_t::const_reverse_iterator history_rend() const {
            return history.crend();
        }

        size_t history_size() const {
            return history.size();
        }

    protected:

        std::chrono::nanoseconds history_age{ 0 };
        history_t history;
    };

    std::shared_ptr<PriceSampler> price_sampler_factory(
        std::mt19937& generator,
        toml::table& tbl,
        const std::string& symbol,
        double price,
        double spread,
        double tick_size,
        int initial_dir = 1
    );

}

#endif 