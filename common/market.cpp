#include "pch.h"

#include "market.h"
#include "time_utils.h"
#include "json.h"

namespace common {
    
   Market::Market(
       const std::shared_ptr<PriceSampler>& price_sampler,
       const TopOfBook& current,
       const std::chrono::nanoseconds& bar_period,
       const std::chrono::nanoseconds& history_age,
       const std::chrono::nanoseconds& history_sample_period,
       bool prune_bars,
       std::mutex& mutex
   ) : OrderMatcher(mutex)
     , symbol(price_sampler->get_symbol())
     , price_sampler(price_sampler)
     , bar_period(bar_period)
     , history_age(history_age)
     , history_sample_period(history_sample_period)
     , prune_bars(prune_bars)
     , mutex(mutex)
     , bar_builder(bar_period, [this](const std::chrono::nanoseconds& end, double o, double h, double l, double c) {
           std::cout << std::format("[{}] new bar end={} open={:.5f} high={:.5f} low={:.5f} close={:.5f}\n", symbol, common::to_string(end), o, h, l, c);
           this->bars.try_emplace(end, end, o, h, l, c);
        })
     , history_bar_builder(current.timestamp, bar_period, [this](const std::chrono::nanoseconds& end, double o, double h, double l, double c) {
           // std::cout << std::format("[{}] hist bar end={} open={:.5f} high={:.5f} low={:.5f} close={:.5f}\n", symbol, common::to_string(end), o, h, l, c);
           this->bars.try_emplace(end, end, o, h, l, c);
        })
     , current(current)
     , previous(current)
     , oldest(current)
   {
      auto now = current.timestamp;
      top_of_books.try_emplace(now, current);
      extend_bar_history(current.timestamp - history_age);
   }

   void Market::simulate_next() {
      auto now = get_current_system_clock();

      std::lock_guard<std::mutex> ul(mutex);

      previous = current;
      current = price_sampler->sample(current, now);
      top_of_books.insert(std::make_pair(now, current));

      auto ageCutoff = now - history_age;
      while (top_of_books.begin() != top_of_books.end() && top_of_books.begin()->first < ageCutoff) {
         top_of_books.erase(top_of_books.begin());
      }

      auto mid = current.mid();
      bar_builder.add(now, mid);
      if (prune_bars) {
         while (bars.begin() != bars.end() && bars.begin()->first < ageCutoff) {
            bars.erase(bars.begin());
         }
      }
   }

   std::pair<TopOfBook, TopOfBook> Market::get_top_of_book() const {
      std::lock_guard<std::mutex> ul(mutex);
      return std::make_pair(current, previous);
   }

   std::tuple<std::chrono::nanoseconds, std::chrono::nanoseconds, size_t> Market::get_bar_range() const {
      std::lock_guard<std::mutex> ul(mutex);
      std::chrono::nanoseconds from, to;
      if (!bars.empty()) {
         from = bars.begin()->second.end;
         to = bars.rbegin()->second.end;
      }
      return std::make_tuple(from, to, bars.size());
   }

   void Market::extend_bar_history(const std::chrono::nanoseconds& past) {
      auto until = round_down(past, bar_period);
      std::cout << std::format("====> extend_bar_history from {} until {}", to_string(oldest.timestamp), to_string(until)) << std::endl;
      std::lock_guard<std::mutex> ul(mutex);
      auto t = oldest.timestamp;
      while (t > until) {
         t = t - history_sample_period;
         oldest = price_sampler->sample(oldest, t);
         history_bar_builder.add(oldest.timestamp, oldest.mid());
      }
   }

   std::pair<nlohmann::json, int> Market::get_bars_as_json(const std::chrono::nanoseconds& from, const std::chrono::nanoseconds& to) {
      auto [bars_from, bars_to, num_bars] = get_bar_range();
      if (bars_from > from) {
         extend_bar_history(from);
      }
      std::lock_guard<std::mutex> ul(mutex);
      return to_json(from, to, bars);
   }
}


