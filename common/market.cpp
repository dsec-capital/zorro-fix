#include "pch.h"

#include "spdlog/spdlog.h"

#include "market.h"
#include "json.h"
#include "time_utils.h"

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
                spdlog::info("[{}] new bar end={} open={:.5f} high={:.5f} low={:.5f} close={:.5f}\n", symbol, common::to_string(end), o, h, l, c);
                this->bars.try_emplace(end, end, o, h, l, c);
            })
        , history_bar_builder(current.timestamp, bar_period, [this](const std::chrono::nanoseconds& end, double o, double h, double l, double c) {
                spdlog::debug("[{}] hist bar end={} open={:.5f} high={:.5f} low={:.5f} close={:.5f}\n", symbol, common::to_string(end), o, h, l, c);
                this->bars.try_emplace(end, end, o, h, l, c);
            })
        , current(current)
        , previous(current)
        , oldest(current)
        , quoting(false)
        , bid_order(
            std::format("quote_ord_id_0"),
            std::format("quote_cl_ord_id_0"),
            symbol,
            OWNER_MARKET_SIMULATOR,
            "",
            Order::Side::buy,
            Order::Type::limit,
            current.bid_price,
            (long)current.bid_volume
        )
        , ask_order(
            std::format("quote_ord_id_1"),
            std::format("quote_cl_ord_id_1"),
            symbol,
            OWNER_MARKET_SIMULATOR,
            "",
            Order::Side::sell,
            Order::Type::limit,
            current.ask_price,
            (long)current.ask_volume
        )
    {
        auto now = current.timestamp;
        top_of_books.try_emplace(now, current);
        extend_bar_history(current.timestamp - history_age);
    }

    void Market::simulate_next() {
        std::lock_guard<std::mutex> ul(mutex);
        auto now = get_current_system_clock();
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

    const Order& Market::get_bid_order() const {
        return bid_order;
    }

    const Order& Market::get_ask_order() const {
        return ask_order;
    }

    OrderInsertResult Market::quote(const Order& order_ins) {
        auto result = insert(order_ins);
        if (!result.error) {
            if (Order::Side::buy) {
                bid_order = order_ins;
            }
            else {
                ask_order = order_ins;
            }
        }
        return result;
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
        std::lock_guard<std::mutex> ul(mutex);
        auto until = round_down(past, bar_period);
        spdlog::info("====> extend_bar_history from {} until {}", common::to_string(oldest.timestamp), common::to_string(until));
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


