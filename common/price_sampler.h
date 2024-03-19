#ifndef PRICE_SAMPLER_H
#define PRICE_SAMPLER_H

#include <chrono>
#include <random>
#include <memory>
#include <toml++/toml.hpp>

#include "utils.h"
#include "market_data.h"
#include "bar_builder.h"

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

        virtual TopOfBook simulate_next(const std::chrono::nanoseconds& now) = 0;

        virtual void initialize_history(
            const std::chrono::nanoseconds& from, 
            const std::chrono::nanoseconds& now,
            const std::chrono::nanoseconds& sample_period,
            std::map<std::chrono::nanoseconds, TopOfBook>& history
        ) = 0;

    protected:
        
    };

    inline void build_bars(
      BarBuilder& builder,
      const std::map<std::chrono::nanoseconds, TopOfBook> &history,
      std::map<std::chrono::nanoseconds, Bar>& bars
    ) {
      if (history.empty()) {
        return;
      }

      auto it = history.begin();
      auto from = round_up(it->first, builder.get_bar_period());
      while (it->first < from && it != history.end()) {
        ++it;
      }

      if (it == history.end()) {
        return;
      }

      while (it != history.end()) {
        builder.add(it->first, it->second.mid());
      }
    }

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