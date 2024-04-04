#include "pch.h"

#include "order_matcher.h"

namespace common {

   OrderMatcher::OrderMatcher(std::mutex& mutex) : mutex(mutex) {}

   bool OrderMatcher::insert(const Order& order)
   {
      if (order.get_side() == Order::buy)
         bid_orders.insert(bid_order_map_t::value_type(order.get_price(), order));
      else
         ask_orders.insert(ask_order_map_t::value_type(order.get_price(), order));
      return true;
   }

   void OrderMatcher::erase(const Order& order)
   {
      std::string id = order.get_client_id();
      if (order.get_side() == Order::buy)
      {
         bid_order_map_t::iterator i;
         for (i = bid_orders.begin(); i != bid_orders.end(); ++i)
            if (i->second.get_client_id() == id)
            {
               bid_orders.erase(i);
               return;
            }
      }
      else if (order.get_side() == Order::sell)
      {
         ask_order_map_t::iterator i;
         for (i = ask_orders.begin(); i != ask_orders.end(); ++i)
            if (i->second.get_client_id() == id)
            {
               ask_orders.erase(i);
               return;
            }
      }
   }

   bool OrderMatcher::match(std::queue<Order>& orders)
   {
      while (true)
      {
         if (!bid_orders.size() || !ask_orders.size())
            return orders.size() != 0;

         bid_order_map_t::iterator iBid = bid_orders.begin();
         ask_order_map_t::iterator iAsk = ask_orders.begin();

         if (iBid->second.get_price() >= iAsk->second.get_price())
         {
            Order& bid = iBid->second;
            Order& ask = iAsk->second;

            match(bid, ask);
            orders.push(bid);
            orders.push(ask);

            if (bid.is_closed()) bid_orders.erase(iBid);
            if (ask.is_closed()) ask_orders.erase(iAsk);
         }
         else
            return orders.size() != 0;
      }
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

   void OrderMatcher::match(Order& bid, Order& ask)
   {
      double price = ask.get_price();
      long quantity = 0;

      if (bid.get_open_quantity() > ask.get_open_quantity())
         quantity = ask.get_open_quantity();
      else
         quantity = bid.get_open_quantity();

      bid.execute(price, quantity);
      ask.execute(price, quantity);
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


