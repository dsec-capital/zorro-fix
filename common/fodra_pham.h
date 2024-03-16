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

        Python sample implementation

            class SemiMarkovMicroStructure:

                def __init__(self, alpha_plus, alpha_neg, tick_probs, tick_size):
                    self.alpha_plus = alpha_plus
                    self.alpha_neg = alpha_neg
                    self.transition_matrix = np.array([
                        [(1. + alpha_plus)/2, (1. - alpha_plus)/2],
                        [(1. - alpha_neg)/2, (1. + alpha_neg)/2]
                    ])
                    self.direction_states = [-1, 1]
                    self.direction_chain = MarkovChain(self.transition_matrix, self.direction_states)
                    tick_jumps = np.cumsum([tick_size]*len(tick_probs))
                    self.tick_distribution = stats.rv_discrete(
                        name='tick_distribution',
                        values=(tick_jumps, tick_probs)
                    )

                def sample(self, n, initial_price=100, initial_dir=1):
                    directions = self.direction_chain.generate_states(initial_dir, n)
                    tick_jumps = self.tick_distribution.rvs(size=n)
                    increments = np.multiply(tick_jumps, directions)
                    relative_prices = np.cumsum(increments)
                    return initial_price + relative_prices
    */
    template<class Generator>
    class FodraPham {
    protected:
        double alpha_plus;
        double alpha_neg;
        std::vector<double> tick_probs; // slight difference to Python implementation: 0 jump allowed
        double tick_size;
        double price_state;
        int direction_state;
        std::discrete_distribution<> jump_distribution;
        std::bernoulli_distribution from_up_jump;
        std::bernoulli_distribution from_down_jump;

    public:
        FodraPham(
            double alpha_plus,
            double alpha_neg,
            const std::vector<double>& tick_probs,
            double tick_size,
            double initial_price,
            int initial_dir
        ) : alpha_plus(alpha_plus)
            , alpha_neg(alpha_neg)
            , tick_probs(tick_probs)
            , tick_size(tick_size)
            , price_state(initial_price)
            , direction_state(initial_dir)
            , jump_distribution(tick_probs.begin(), tick_probs.end())
            , from_up_jump((1. + alpha_plus) / 2)
            , from_down_jump((1. + alpha_neg) / 2)
        {
            assert(direction_state == 1 || direction_state == -1);
        }

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

        double sample_next(Generator& gen) {
            auto tick_jump = jump_distribution(gen) * tick_size;
            direction_state = sample_next_direction(gen);
            price_state = price_state + direction_state * tick_jump;
            return price_state;
        }

        std::vector<double> path(int n) {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::vector<double> samples;
            samples.reserve(n);
            for (int i = 0; i < n; ++i) {
                samples.emplace_back(sample_next(gen));
            }
            return samples;
        }
    };

    template<class Generator>
    class FodraPhamSampler : public PriceSampler, public FodraPham<Generator> {
        double price_state, spread_state;
        double bid_volume_state, ask_volume_state;

    public:
        FodraPhamSampler(
            std::mt19937& gen,
            double alpha_plus,
            double alpha_neg,
            const std::vector<double>& tick_probs,
            double tick_size,
            double initial_price,
            double initial_spread,
            double initial_bid_volume,
            double initial_ask_volume,
            int initial_dir
        )
            : PriceSampler(gen)
            , FodraPham<Generator>(
                alpha_plus,
                alpha_neg,
                tick_probs,
                tick_size,
                initial_price,
                initial_dir
            )
            , price_state(initial_price)
            , spread_state(initial_spread)
            , bid_volume_state(initial_bid_volume)
            , ask_volume_state(initial_ask_volume)
        {}

        virtual double actualMidPrice() {
            return price_state;
        }

        virtual double actualSpread() {
            return spread_state;
        }

        virtual double actualBidVolume() {
            return bid_volume_state;
        }

        virtual double actualAskVolume() {
            return ask_volume_state;
        }

        virtual double nextMidPrice() {
            price_state = FodraPham<Generator>::sample_next(gen);
            return price_state;
        }

        virtual double nextSpread() {
            return spread_state;
        }

        virtual double nextBidVolume() {
            return bid_volume_state;
        }

        virtual double nextAskVolume() {
            return ask_volume_state;
        }
    };

}

#endif
