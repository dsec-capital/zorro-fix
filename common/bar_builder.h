#ifndef BAR_BUILDER_H
#define BAR_BUILDER_H

#include <chrono>
#include <functional>
#include <algorithm>

#include "nlohmann/json.h"

#include "utils.h"

namespace common {

    class Bar {
    public:
        Bar(
            const std::chrono::nanoseconds& start,
            const std::chrono::nanoseconds& end,
            double open,
            double high,
            double low,
            double close
          ) : start(start)
            , end(end)
            , open(open)
            , high(high)
            , low(low)
            , close(close)
        {}

        std::chrono::nanoseconds start;
        std::chrono::nanoseconds end;
        double open;
        double high; 
        double low;
        double close;

        std::string to_string() const {
            return
                "open=" + std::to_string(open) + ", " +
                "high=" + std::to_string(high) + ", " +
                "low=" + std::to_string(low) + ", " +
                "close=" + std::to_string(close);
        }
    };

    class BarBuilder {
        std::chrono::nanoseconds bar_period;
        std::function<void(const std::chrono::nanoseconds&, const std::chrono::nanoseconds&, double, double, double, double)> on_bar;
        std::chrono::nanoseconds start{};
        std::chrono::nanoseconds end{};
        std::chrono::nanoseconds last_time{};
        double open{ 0 }, high{ 0 }, low{ 0 }, close{ 0 };

    public:
        BarBuilder(
            const std::chrono::nanoseconds& bar_period,
            std::function<void(const std::chrono::nanoseconds&, const std::chrono::nanoseconds&, double, double, double, double)> on_bar
        ) : bar_period(bar_period)
            , on_bar(on_bar)
        {}

        std::chrono::nanoseconds get_bar_period() const {
            return bar_period;
        }

        // emit a bar based on time event only, e.g. based on a timer event
        void check_close(const std::chrono::nanoseconds& time) {
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
            }
            else {
                if (time <= end) {
                    if (value > high) high = value;
                    if (value < low) low = value;
                    close = value;
                    last_time = time;
                }
                else {
                    on_bar(start, end, open, high, low, close);
                    auto new_start = round_down(time, bar_period);
                    open = last_time == new_start ? close : value;
                    high = value > open ? value : open;
                    low = value < open ? value : open;
                    close = value;
                    start = new_start;
                    end = new_start + bar_period;
                    last_time = time;
                }
                assert(open > 0);
            }
        }
    };

}
#endif  
