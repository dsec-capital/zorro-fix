#include "pch.h"

#include "book.h"
#include "logger.h"
#include "time_utils.h"

#include <cmath>
#include <format>

using namespace std::literals;
using namespace std::chrono_literals;

namespace zfix {

    Book::Book()
        : bids([](const double& x, const double& y) -> bool { return x > y; })
        , asks([](const double& x, const double& y) -> bool { return x < y; })
        , precision(10)
    {}

    void Book::set_precision(uint32_t prec) {
        precision = prec;
    }

    uint32_t Book::scale(double price) const {
        return uint32_t(price * precision);
    }

    double Book::unscale(uint32_t price) const {
        return price / double(precision);
    }

    void Book::update_book(double p_unscaled, double a, bool is_bid) {
        auto p = scale(p_unscaled);
        if (is_bid) {
            if (a == 0) {
                auto it = bids.find(p);
                if (it != bids.end()) {
                    bids.erase(it);
                }
            }
            else {
                bids[p] = a;
            }
        }
        else {
            if (a == 0) {
                auto it = asks.find(p);
                if (it != asks.end()) {
                    asks.erase(it);
                }
            }
            else {
                asks[p] = a;
            }
        }
    }

    void Book::clear_book() {
        bids.clear();
        asks.clear();
    }

    bool Book::is_crossing() const {
        if (!bids.empty() && !asks.empty()) {
            auto top_bid = bids.begin()->first;
            auto top_ask = asks.begin()->first;
            if (top_bid >= top_ask) {
                return true;
            }
        }
        return false;
    }

    std::pair<double, double> Book::best_bid() const {
        return std::make_pair(unscale(bids.begin()->first), bids.begin()->second);
    }

    std::pair<double, double> Book::best_ask() const {
        return std::make_pair(unscale(asks.begin()->first), asks.begin()->second);
    }

    TopOfBook Book::topOfBook(const std::string& symbol) const {
        if (bids.empty() || asks.empty())
            throw std::runtime_error(std::format("empty book for symbol {}", symbol));
        return TopOfBook(
            symbol,
            unscale(bids.begin()->first), bids.begin()->second,
            unscale(asks.begin()->first), asks.begin()->second
        );
    }

    double Book::spread() const {
        if (!bids.empty() && !asks.empty()) {
            auto top_bid = bids.begin()->first;
            auto top_ask = asks.begin()->first;
            return unscale(top_ask - top_bid);
        }
        return std::numeric_limits<double>::quiet_NaN();
    }

    double Book::vwap(double price_level, bool is_bid) const {
        double weighted_price = 0;
        double volume = 0;
        if (is_bid) {
            if (bids.empty()) return 0;
            for (auto it = bids.begin(); it->first >= price_level && it != bids.end(); ++it) {
                weighted_price += unscale(it->first) * it->second;
                volume += it->second;
            }
        }
        else {
            if (asks.empty()) return 0;
            for (auto it = asks.begin(); it->first <= price_level && it != asks.end(); ++it) {
                weighted_price += unscale(it->first) * it->second;
                volume += it->second;
            }
        }
        return weighted_price / volume;
    }

    double Book::vwap_mid(double bid_level, double ask_level) const {
        auto bid = vwap(bid_level, true);
        auto ask = vwap(ask_level, false);
        return 0.5 * (bid + ask);
    }

    void Book::log_book(int levels, const std::string& pre) const {
        using namespace std::chrono;
        auto itb = bids.begin();
        auto ita = asks.begin();
        auto duration = duration_cast<milliseconds>(timestamp);
        auto tp = time_point(duration);
        auto time_str = to_string(tp);
        LOG_INFO("------------------------------- %f -------------------------------", spread());
        for (auto l = 0; l < levels && itb != bids.end() && ita != asks.end(); ++l, ++itb, ++ita) {
            LOG_INFO("%s %s [%d] bid price %d|%f, ask price %d|%f",
                pre.c_str(), time_str.c_str(), l, itb->first, itb->second, ita->first, ita->second);
        }
    }
}
