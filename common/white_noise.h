#ifndef WHITE_NOISE_H
#define WHITE_NOISE_H

#include "pch.h"

#include "price_sampler.h"

namespace common {

   /*
       Mid Price Model

       Simple white noise model
   */
   class WhiteNoise : public PriceSampler {
   protected:
      std::normal_distribution<> noise{1.0, 0.0};
      double millis_per_day{ 86400000.0 };
      double sigma;

   public:

      WhiteNoise(
          const std::string& symbol, 
          std::mt19937& gen,
          double sigma
      ) : PriceSampler(symbol, gen)
        , sigma(sigma)
      {}

      virtual TopOfBook sample(const TopOfBook& current, const std::chrono::nanoseconds& t1) {
         auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - current.timestamp).count();
         double dt = (double)std::abs(dur) / millis_per_day;
         auto mid = current.mid() + std::sqrt(dt)*sigma*noise(gen);
         return TopOfBook(
            symbol,
            t1,
            mid - current.spread() / 2,
            current.bid_volume,
            mid + current.spread() / 2,
            current.ask_volume
         );
      }

      virtual void push() {}

      virtual void pop() {}
   };
}

#endif

