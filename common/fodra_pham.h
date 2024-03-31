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
    class FodraPham : public PriceSampler {
        double alpha_plus;
        double alpha_neg;
        std::vector<double> tick_probs;
        double tick_size;
        int direction_state;
        int direction_state_;
        std::discrete_distribution<> jump_distribution;
        std::bernoulli_distribution from_up_jump;
        std::bernoulli_distribution from_down_jump;
        
    public:

        FodraPham(
            const std::string& symbol, 
            std::mt19937& gen,
            double alpha_plus,
            double alpha_neg,
            const std::vector<double>& tick_probs,
            double tick_size,
            int initial_dir
        ) : PriceSampler(symbol, gen)
          , alpha_plus(alpha_plus)
          , alpha_neg(alpha_neg)
          , tick_probs(tick_probs)
          , tick_size(tick_size)
          , direction_state(initial_dir)
          , jump_distribution(tick_probs.begin(), tick_probs.end())
          , from_up_jump((1. + alpha_plus) / 2)
          , from_down_jump((1. + alpha_neg) / 2)
        {
            assert(direction_state == 1 || direction_state == -1);
        }

        virtual TopOfBook sample(const TopOfBook& current, const std::chrono::nanoseconds& t1) {
            auto mid = next(current.mid());
            return TopOfBook(
               symbol,
               t1,
               mid - current.spread() / 2,
               current.bid_volume,
               mid + current.spread() / 2,
               current.ask_volume
            );            
        }

        virtual void push() {
           direction_state_ = direction_state;
        }

        virtual void pop() {
           direction_state = direction_state_;
        }

    private:

        int sample_next_direction() {
            if (direction_state == 1) {
                auto stay = from_up_jump(gen);
                return stay ? 1 : -1;
            }
            else {
                auto stay = from_down_jump(gen);
                return stay ? 1 : -1;
            }
        }

        double next(double mid) {
            auto tick_jump = jump_distribution(gen) * tick_size;
            direction_state = sample_next_direction();
            return mid + direction_state * tick_jump;
        }
    };
}

#endif
