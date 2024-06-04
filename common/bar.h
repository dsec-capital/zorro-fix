#ifndef BAR_H
#define BAR_H

#include <chrono>

#include "time_utils.h"

namespace common {

    class Bar {
    public:
        Bar(
            const std::chrono::nanoseconds& end,
            double open,
            double high,
            double low,
            double close
        ) : end(end)
          , open(open)
          , high(high)
          , low(low)
          , close(close)
        {}

        std::chrono::nanoseconds end;
        double open;
        double high;
        double low;
        double close;

        std::string to_string() const {
            return
                "end=" + common::to_string(end) + ", " +
                "open=" + std::to_string(open) + ", " +
                "high=" + std::to_string(high) + ", " +
                "low=" + std::to_string(low) + ", " +
                "close=" + std::to_string(close);
        }
    };

    template<typename T>
    class BidAskBar {
    public:
        BidAskBar(
            const T& timestamp,
            double bid_open,
            double bid_high,
            double bid_low,
            double bid_close,
            double ask_open,
            double ask_high,
            double ask_low,
            double ask_close,
            double volume
        ) : timestamp(timestamp)
          , bid_open(bid_open)
          , bid_high(bid_high)
          , bid_low(bid_low)
          , bid_close(bid_close)
          , ask_open(ask_open)
          , ask_high(ask_high)
          , ask_low(ask_low)
          , ask_close(ask_close)
          , volume(volume)
        {}

        T timestamp;
        double bid_open;
        double bid_high;
        double bid_low;
        double bid_close;
        double ask_open;
        double ask_high;
        double ask_low;
        double ask_close;
        double volume;

        std::string to_string() const {
            return
                "timestamp=" + std::to_string(timestamp) + ", " +
                "bid_open=" + std::to_string(bid_open) + ", " +
                "bid_high=" + std::to_string(bid_high) + ", " +
                "bid_low=" + std::to_string(bid_low) + ", " +
                "bid_close=" + std::to_string(bid_close) + ", " +
                "ask_open=" + std::to_string(ask_open) + ", " +
                "ask_high=" + std::to_string(ask_high) + ", " +
                "ask_low=" + std::to_string(ask_low) + ", " +
                "ask_close=" + std::to_string(ask_close) + ", " +
                "volume=" + std::to_string(volume);
        }
    };
}
#endif  
