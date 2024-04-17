#include "pch.h"

#include "order_matcher.h"

#include "spdlog/spdlog.h"

namespace common {

    OrderMatcher::OrderMatcher(std::mutex& mutex) : mutex(mutex) {}

    std::tuple<const Order*, bool, int> OrderMatcher::insert(const Order& order_in, std::queue<Order>& orders)
    {
        std::lock_guard<std::mutex> ul(mutex);
        auto num = 0;
        auto error = false;
        auto order = order_in;
        const Order* order_ptr = nullptr;
        const auto& price = order.get_price();
        if (order.get_side() == Order::buy) {
            auto it = ask_orders.begin();
            if (it != ask_orders.end() && price >= it->second.get_price()) {
                num += match(order, orders);
            }
            if (!order.is_closed()) {
                auto it = bid_orders.insert(std::make_pair(price, order));
                order_ptr = &it->second;
            }
        }
        else {
            auto it = bid_orders.begin();
            if (it != bid_orders.end() && price <= it->second.get_price()) {
                num += match(order, orders);
            }
            if (!order.is_closed()) {
                ask_orders.insert(std::make_pair(price, order));
                order_ptr = &it->second;
            }
        }
        return std::make_tuple(order_ptr, error, num);
    }

    std::optional<Order> OrderMatcher::erase(const std::string& ord_id, const Order::Side& side)
    {
        std::lock_guard<std::mutex> ul(mutex);
        if (side == Order::buy)
        {
            for (auto it = bid_orders.begin(); it != bid_orders.end(); ++it) {
                if (it->second.get_ord_id() == ord_id) {
                    auto order = std::optional<Order>(it->second);
                    bid_orders.erase(it);
                    return order;
                }
            }
        }
        else if (side == Order::sell)
        {
            for (auto it = ask_orders.begin(); it != ask_orders.end(); ++it) {
                if (it->second.get_ord_id() == ord_id) {
                    auto order = std::optional<Order>(it->second);
                    ask_orders.erase(it);
                    return order;
                }
            }
        }

        return std::optional<Order>();
    }

    int OrderMatcher::match(Order& order, std::queue<Order>& orders)
    {
        std::lock_guard<std::mutex> ul(mutex);
        auto num = 0;
        const auto& price = order.get_price();
        if (order.get_side() == Order::Side::buy) {
            auto it = ask_orders.begin();
            while (it != ask_orders.end() && it->second.get_price() <= price) {
                auto& ask = it->second;
                const auto& exec_price = ask.get_price();
                long quantity = std::min(ask.get_open_quantity(), order.get_open_quantity());
                ask.execute(exec_price, quantity);
                order.execute(exec_price, quantity);
                orders.push(ask);
                orders.push(order);

                spdlog::debug("OrderMatcher::match: ask side match: ask={} order={}", ask.to_string(), order.to_string());

                ++num;
                if (ask.is_closed())
                    it = ask_orders.erase(it);
                else
                    ++it;
            }
        }
        else {
            auto it = bid_orders.begin();
            while (it != bid_orders.end() && it->second.get_price() >= price) {
                auto& bid = it->second;
                const auto& exec_price = bid.get_price();
                long quantity = std::min(bid.get_open_quantity(), order.get_open_quantity());
                bid.execute(exec_price, quantity);
                order.execute(exec_price, quantity);
                orders.push(bid);
                orders.push(order);

                spdlog::debug("OrderMatcher::match: bid side match: ask={} order={}", bid.to_string(), order.to_string());

                ++num;
                if (bid.is_closed())
                    it = bid_orders.erase(it);
                else
                    ++it;
            }
        }
        return num;
    }

    Order& OrderMatcher::find(Order::Side side, std::string ord_id)
    {
        std::lock_guard<std::mutex> ul(mutex);
        if (side == Order::buy)
        {
            bid_order_map_t::iterator i;
            for (i = bid_orders.begin(); i != bid_orders.end(); ++i)
                if (i->second.get_ord_id() == ord_id) return i->second;
        }
        else if (side == Order::sell)
        {
            ask_order_map_t::iterator i;
            for (i = ask_orders.begin(); i != ask_orders.end(); ++i)
                if (i->second.get_ord_id() == ord_id) return i->second;
        }
        throw std::runtime_error(std::format(
            "OrderMatcher::find: could not find order with ord_id={}, side={}",
            ord_id, side == Order::buy ? "buy" : "sell"
        ));
    }

    typename OrderMatcher::bid_map_t OrderMatcher::bid_map(const std::function<double(const Order&)>& f) const 
    {
        std::lock_guard<std::mutex> ul(mutex);
        bid_map_t bids;
        for (auto it = bid_orders.begin(); it != bid_orders.end(); ++it) {
            bids[it->first] += f(it->second);
        }
        return bids;
    }

    typename OrderMatcher::ask_map_t OrderMatcher::ask_map(const std::function<double(const Order&)>& f) const 
    {
        std::lock_guard<std::mutex> ul(mutex);
        ask_map_t asks;
        for (auto it = ask_orders.begin(); it != ask_orders.end(); ++it) {
            asks[it->first] += f(it->second);
        }
        return asks;
    }

    void OrderMatcher::book_levels(const std::function<double(const Order&)>& f, typename OrderMatcher::level_vector_t& levels) const {
        bid_map_t bids = bid_map(f);
        ask_map_t asks = ask_map(f);
        levels.clear();

        if (bids.empty() && asks.empty()) {
            return;
        }

        auto bit = bids.begin();
        auto ait = asks.begin();
        while (true) {
            BookLevel level;
            if (bit != bids.end()) {
                level.bid_price = bit->first;
                level.bid_volume = bit->second;
                ++bit;
            }
            if (ait != asks.end()) {
                level.ask_price = ait->first;
                level.ask_volume = ait->second;
                ++ait;
            }
            levels.emplace_back(level);
            if (bit == bids.end() && ait == asks.end()) {
                break;
            }
        }
    }

    void OrderMatcher::display() const
    {
        std::cout << to_string() << std::endl;
    }

    std::string OrderMatcher::to_string() const {
        std::unique_lock<std::mutex> ul(mutex);
        bid_order_map_t bids = bid_orders;
        ask_order_map_t asks = ask_orders;
        ul.release();

        std::string rows;
        rows += "OrderMatcher[\n";
        for (auto ait = asks.begin(); ait != asks.end(); ++ait) {
            const auto& o = ait->second;
            auto side = o.get_side() == Order::Side::buy ? "bid" : "ask";
            rows += std::format(
                "[{}] : symbol={}, owner={}, ord_id={}, cl_ord_id={}, price={:8.5f}, side={}, quantity={}, open_quantity={}, executed_quantity={}, avg_executed_price=={:8.5f}, last_executed_price=={:8.5f}, last_executed_quantity={}\n",
                ait->first, o.get_symbol(), o.get_owner(), o.get_ord_id(), o.get_cl_ord_id(), o.get_price(), side, o.get_quantity(), o.get_open_quantity(), o.get_executed_quantity(), o.get_avg_executed_price(), o.get_last_executed_price(), o.get_last_executed_quantity()
            );
        }
        rows += "------------------------------------\n";
        for (auto bit = bids.begin(); bit != bids.end(); ++bit) {
            const auto& o = bit->second;
            auto side = o.get_side() == Order::Side::buy ? "bid" : "ask";
            rows += std::format(
                "[{}] : symbol={}, owner={}, ord_id={}, cl_ord_id={}, price={:8.5f}, side={}, quantity={}, open_quantity={}, executed_quantity={}, avg_executed_price=={:8.5f}, last_executed_price=={:8.5f}, last_executed_quantity={}\n",
                bit->first, o.get_symbol(), o.get_owner(), o.get_ord_id(), o.get_cl_ord_id(), o.get_price(), side, o.get_quantity(), o.get_open_quantity(), o.get_executed_quantity(), o.get_avg_executed_price(), o.get_last_executed_price(), o.get_last_executed_quantity()
            );
        }
        rows += "]";
        return rows;
    }

    int OrderMatcher::by_quantity(const Order& o) {
        return o.get_quantity();
    };

    int OrderMatcher::by_open_quantity(const Order& o) {
        return o.get_open_quantity();
    };

    int OrderMatcher::by_last_exec_quantity(const Order& o) {
        return o.get_last_executed_quantity();
    };

    std::string to_string(const typename OrderMatcher::level_vector_t& levels) {
        std::string str;
        for (auto it = levels.begin(); it != levels.end(); ++it) {
            str += std::format(
                "[{:10.2f}] {:>8.5f} | {:<8.5f} [{:10.2f}]",
                it->bid_volume, it->bid_price, it->ask_price, it->ask_volume
            ) + "\n";
        }
        return str;
    }
}


