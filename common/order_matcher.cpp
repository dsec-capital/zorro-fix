#include "pch.h"

#include "order_matcher.h"

namespace common {

   OrderMatcher::OrderMatcher(std::mutex& mutex) : mutex(mutex) {}

   std::tuple<const Order*, bool, int> OrderMatcher::insert(const Order& order_in, std::queue<Order>& orders)
   {
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

   void OrderMatcher::erase(const Order& order)
   {
      std::string id = order.get_client_id();
      if (order.get_side() == Order::buy)
      {
          for (auto it = bid_orders.begin(); it != bid_orders.end(); ++it) {
              if (it->second.get_client_id() == id) {
                  bid_orders.erase(it);
                  return;
              }
          }
      }
      else if (order.get_side() == Order::sell)
      {
          for (auto it = ask_orders.begin(); it != ask_orders.end(); ++it) {
              if (it->second.get_client_id() == id) {
                  ask_orders.erase(it);
                  return;
              }
          }
      }
   }

   int OrderMatcher::match(Order& order, std::queue<Order>& orders)
   {
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
               ++num;
               if (bid.is_closed())
                   it = bid_orders.erase(it);
               else
                   ++it;
           }
       }
       return num;
   }

   Order& OrderMatcher::find(Order::Side side, std::string id)
   {
      if (side == Order::buy)
      {
         bid_order_map_t::iterator i;
         for (i = bid_orders.begin(); i != bid_orders.end(); ++i)
            if (i->second.get_client_id() == id) return i->second;
      }
      else if (side == Order::sell)
      {
         ask_order_map_t::iterator i;
         for (i = ask_orders.begin(); i != ask_orders.end(); ++i)
            if (i->second.get_client_id() == id) return i->second;
      }
      throw std::exception();
   }

   typename OrderMatcher::bid_map_t OrderMatcher::bid_map(const std::function<double(const Order&)>& f) const {
       bid_map_t bids;
       for (auto it = bid_orders.begin(); it != bid_orders.end(); ++it) {
           bids[it->first] += f(it->second);
       }
       return bids;
   }

   typename OrderMatcher::ask_map_t OrderMatcher::ask_map(const std::function<double(const Order&)>& f) const {
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
      std::cout << "BIDS:" << std::endl;
      std::cout << "-----" << std::endl << std::endl;
      for (auto bit = bid_orders.begin(); bit != bid_orders.end(); ++bit) {
          std::cout << bit->second << std::endl;
      }

      std::cout << std::endl << std::endl;

      std::cout << "ASKS:" << std::endl;
      std::cout << "-----" << std::endl << std::endl;
      for (auto ait = ask_orders.begin(); ait != ask_orders.end(); ++ait) {
          std::cout << ait->second << std::endl;
      }
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
               "[{:10.1f}] {:>8.1f} | {:<8.1f} [{:10.1f}]",
               it->bid_volume, it->bid_price, it->ask_price, it->ask_volume
           ) + "\n";
       }
       return str;
   }
}


