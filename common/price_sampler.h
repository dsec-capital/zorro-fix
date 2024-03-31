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

       std::string symbol;
       std::mt19937& gen;

    public:

        typedef std::map<std::chrono::nanoseconds, TopOfBook> history_t;

        PriceSampler(const std::string& symbol, std::mt19937& gen) : symbol(symbol), gen(gen) {}

        const std::string& get_symbol() const {
           return symbol;
        }

        virtual TopOfBook sample(const TopOfBook& current, const std::chrono::nanoseconds& t1) = 0;

        virtual void push() = 0;

        virtual void pop() = 0;

        void sample_path(
           const std::chrono::nanoseconds& now,
           const TopOfBook& current,
           const std::chrono::nanoseconds& to,
           const std::chrono::nanoseconds& sample_period,
           std::map<std::chrono::nanoseconds, TopOfBook>& history
        ) {
           push();
           TopOfBook state = current;
           history.insert(std::make_pair(now, current));
           std::chrono::nanoseconds t = now;
           if (to < now) {
              while (t > to) {
                 t = t - sample_period;
                 state = sample(state, t);
                 history.insert(std::make_pair(t, state)); 
              }
           }
           else {
              while (t < to) {
                 t = t + sample_period;
                 state = sample(state, t);
                 history.insert(std::make_pair(t, state));
              }
           }
           pop();
        }
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
        ++it;
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