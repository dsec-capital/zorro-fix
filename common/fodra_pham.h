#ifndef FODRA_PHAM_H
#define FODRA_PHAM_H

#include "pch.h"

#include "price_sampler.h"

namespace common {

    /*
        Mid Price Model

        Following Fodra and Pham

            - https://arxiv.org/pdf/1305.0105.pdf
            - https://park.itc.u-tokyo.ac.jp/takahashi-lab/WPs/Pham130927.pdf

        Currently the interarrival times are kept constant and not modelled with
        a Markov renewal process.
    */
    template<class Generator>
    class FodraPham {
        double alpha_plus;
        double alpha_neg;
        std::vector<double> tick_probs;
        double tick_size;
        double price_state;
        double spread_state;
        int direction_state;
        std::discrete_distribution<> jump_distribution;
        std::bernoulli_distribution from_up_jump;
        std::bernoulli_distribution from_down_jump;
    
    protected:
    
        std::chrono::nanoseconds actual_time{};

    public:

        FodraPham(
            const std::chrono::nanoseconds& actual_time, 
            double alpha_plus,
            double alpha_neg,
            const std::vector<double>& tick_probs,
            double tick_size,
            double initial_price,
            double initial_spread,
            int initial_dir
        ) : actual_time(actual_time)
          , alpha_plus(alpha_plus)
          , alpha_neg(alpha_neg)
          , tick_probs(tick_probs)
          , tick_size(tick_size)
          , price_state(initial_price)
          , spread_state(initial_spread)
          , direction_state(initial_dir)
          , jump_distribution(tick_probs.begin(), tick_probs.end())
          , from_up_jump((1. + alpha_plus) / 2)
          , from_down_jump((1. + alpha_neg) / 2)
        {
            assert(direction_state == 1 || direction_state == -1);
        }

        std::chrono::nanoseconds get_actual_time() const {
            return actual_time;
        }

        double sample_next(Generator& gen, const std::chrono::nanoseconds& now) {
            actual_time = now;
            next(gen, now);
            return price_state;
        }

        void initialize_history(
            Generator& gen,
            const std::chrono::nanoseconds& from,
            const std::chrono::nanoseconds& now,
            const std::chrono::nanoseconds& sample_period,
            std::map<std::chrono::nanoseconds, std::pair<double, double>>& history
        ) {
            auto orig_price_state = price_state;
            auto orig_direction_state = direction_state;
            auto orig_spread_state = spread_state;

            std::chrono::nanoseconds t = now;
            while (t > from) {
                t = t - sample_period;
                history.emplace(t, std::make_pair(next(gen, t), spread_state));
            }

            price_state = orig_price_state;
            direction_state = orig_direction_state;
            spread_state = orig_spread_state;
        }

    private:

        int sample_next_direction(Generator& gen) {
            if (direction_state == 1) {
                auto stay = from_up_jump(gen);
                return stay ? 1 : -1;
            }
            else {
                auto stay = from_down_jump(gen);
                return stay ? 1 : -1;
            }
        }

        double next(Generator& gen, const std::chrono::nanoseconds& now) {
            auto tick_jump = jump_distribution(gen) * tick_size;
            direction_state = sample_next_direction(gen);
            price_state = price_state + direction_state * tick_jump;
            return price_state;
        }
    };

    template<class Generator>
    class FodraPhamSampler : public PriceSampler, public FodraPham<Generator> {
        std::string symbol;
        double price_state;
        double spread_state;
        double bid_volume_state;
        double ask_volume_state;

    public:

        FodraPhamSampler(
            std::mt19937& gen,
            const std::string& symbol,
            const std::chrono::nanoseconds& actual_time,
            double alpha_plus,
            double alpha_neg,
            const std::vector<double>& tick_probs,
            double tick_size,
            double initial_price,
            double initial_spread,
            double initial_bid_volume,
            double initial_ask_volume,
            int initial_dir
        ) : PriceSampler(gen)
          , symbol(symbol)
          , FodraPham<Generator>(
              actual_time,
              alpha_plus,
              alpha_neg,
              tick_probs,
              tick_size,
              initial_price,
              initial_spread,
              initial_dir
          )
          , price_state(initial_price)
          , spread_state(initial_spread)
          , bid_volume_state(initial_bid_volume)
          , ask_volume_state(initial_ask_volume)
        {}

        virtual std::chrono::nanoseconds actual_time() const {
            return FodraPham<Generator>::actual_time;
        }

        virtual double actual_bid_price() const {
            return price_state - spread_state/2;
        }

        virtual double actual_ask_price() const {
            return price_state + spread_state / 2;
        }

        virtual double actual_bid_volume() const {
            return bid_volume_state;
        }

        virtual double actual_ask_volume() const {
            return ask_volume_state;
        }

        virtual double actual_spread() const {
            return spread_state;
        }

        virtual TopOfBook actual_top_of_book() const {
            return TopOfBook(
                symbol,
                price_state - spread_state / 2,
                bid_volume_state,
                price_state + spread_state / 2,
                ask_volume_state
            );
        }

        virtual TopOfBook simulate_next(const std::chrono::nanoseconds& now) {
            price_state = FodraPham<Generator>::sample_next(gen, now);
            return TopOfBook(symbol,
                price_state - spread_state / 2,
                bid_volume_state,
                price_state + spread_state / 2,
                ask_volume_state
            );
        }

        virtual void initialize_history(
            const std::chrono::nanoseconds& from,
            const std::chrono::nanoseconds& now,
            const std::chrono::nanoseconds& sample_period,
            std::map<std::chrono::nanoseconds, TopOfBook>& history
        ) {
            std::map<std::chrono::nanoseconds, std::pair<double, double>> mid_history;
            FodraPham<Generator>::initialize_history(
                gen, from, now, sample_period, mid_history
            );

            history.clear();
            for (const auto& [t, mid_spread] : mid_history) {
                const auto& [mid, spread] = mid_spread;
                history.try_emplace(t, 
                    symbol, 
                    mid - spread / 2,
                    bid_volume_state,
                    mid + spread / 2,
                    ask_volume_state
                );
            }
        }

    };

}

#endif
