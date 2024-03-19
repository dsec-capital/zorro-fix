#ifndef BAR_BUILDER_H
#define BAR_BUILDER_H

#include <chrono>

#include "utils.h"

namespace common {

  struct Bar {
    std::chrono::nanoseconds start;
    std::chrono::nanoseconds end;
    double open, close, high, low;
  };

  class BarBuilder {
    std::chrono::nanoseconds bar_period;
    std::function<void (const std::chrono::nanoseconds&, const std::chrono::nanoseconds&, double, double, double, double)> on_bar;
    std::chrono::nanoseconds start{};
    std::chrono::nanoseconds end{};
    std::chrono::nanoseconds last_time{};
    double open{0}, close{0}, high{0}, low{0};

  public:
    BarBuilder(
      const std::chrono::nanoseconds& bar_period,
      std::function<void (const std::chrono::nanoseconds&, const std::chrono::nanoseconds&, double, double, double, double)> on_bar
    ) : bar_period(bar_period)
      , on_bar(on_bar)
    {}

    // emit a bar based on time event only, e.g. based on a timer event
    void close(const std::chrono::nanoseconds& time) {
      if (time >= end && open != 0) {
        on_bar(start, end, open, high, low, close);
        open = 0;
      }
    }

    // add new timeseries value and emit a bar if new timeseries value is newer than current end
    void add(const std::chrono::nanoseconds& time, double value) {
      if (open == 0) {
        open = value;
        high = open;
        low = open;
        close = open;
        start = round_down(time, bar_period);
        end = start + bar_period;
        last_time = time;
      } else {
        if (time <= end) {
          high = std::max(high, value);
          low = std::min(low, value);
          close = value;
          last_time = time;
        } else {
          on_bar(start, end, open, high, low, close);
          auto new_start = round_down(time, bar_period)
          open = last_time == new_start ? close : value;
          high = std::max(open, value);
          low = std::min(open, value);
          close = value;
          start = new_start;
          end = new_start + bar_period;
          last_time = time;
        }
        assert(open > 0);
      }
    }
  };

#endif //BAR_BUILDER_H
