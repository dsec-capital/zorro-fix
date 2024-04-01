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
   ) : symbol(price_sampler->get_symbol())
      , price_sampler(price_sampler)
      , bar_period(bar_period)
      , history_age(history_age)
      , history_sample_period(history_sample_period)
      , prune_bars(prune_bars)
      , mutex(mutex)
      , bar_builder(bar_period, [this](const std::chrono::nanoseconds& end, double o, double h, double l, double c) {
            std::cout << std::format("[{}] new bar end={} open={:.4f} high={:.4f} low={:.4f} close={:.4f}\n", symbol, common::to_string(end), o, h, l, c);
            this->bars.try_emplace(end, end, o, h, l, c);
         })
      , history_bar_builder(current.timestamp, bar_period, [this](const std::chrono::nanoseconds& end, double o, double h, double l, double c) {
            //std::cout << std::format("[{}] hist bar end={} open={:.4f} high={:.4f} low={:.4f} close={:.4f}\n", symbol, common::to_string(end), o, h, l, c);
            this->bars.try_emplace(end, end, o, h, l, c);
         })
      , current(current)
      , previous(current)
      , oldest(current)
   {
      auto now = current.timestamp;
      top_of_books.try_emplace(now, current);
      extend_bars(current.timestamp - history_age);
   }

   void Market::simulate_next() {
      auto now = get_current_system_clock();

      std::unique_lock<std::mutex> ul(mutex);

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

   void Market::extend_bars(const std::chrono::nanoseconds& past) {
      auto until = round_down(past, bar_period);
      std::cout << std::format("extend_bars from {} until {}", to_string(oldest.timestamp), to_string(until)) << std::endl;
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
         extend_bars(from);
      }
      std::lock_guard<std::mutex> ul(mutex);
      return to_json(from, to, bars);
   }

   bool Market::insert(const Order& order)
   {
      if (order.get_side() == Order::buy)
         bid_orders.insert(BidOrders::value_type(order.get_price(), order));
      else
         ask_orders.insert(AskOrders::value_type(order.get_price(), order));
      return true;
   }

   void Market::erase(const Order& order)
   {
      std::string id = order.get_client_id();
      if (order.get_side() == Order::buy)
      {
         BidOrders::iterator i;
         for (i = bid_orders.begin(); i != bid_orders.end(); ++i)
            if (i->second.get_client_id() == id)
            {
               bid_orders.erase(i);
               return;
            }
      }
      else if (order.get_side() == Order::sell)
      {
         AskOrders::iterator i;
         for (i = ask_orders.begin(); i != ask_orders.end(); ++i)
            if (i->second.get_client_id() == id)
            {
               ask_orders.erase(i);
               return;
            }
      }
   }

   bool Market::match(std::queue<Order>& orders)
   {
      while (true)
      {
         if (!bid_orders.size() || !ask_orders.size())
            return orders.size() != 0;

         BidOrders::iterator iBid = bid_orders.begin();
         AskOrders::iterator iAsk = ask_orders.begin();

         if (iBid->second.get_price() >= iAsk->second.get_price())
         {
            Order& bid = iBid->second;
            Order& ask = iAsk->second;

            match(bid, ask);
            orders.push(bid);
            orders.push(ask);

            if (bid.isClosed()) bid_orders.erase(iBid);
            if (ask.isClosed()) ask_orders.erase(iAsk);
         }
         else
            return orders.size() != 0;
      }
   }

   Order& Market::find(Order::Side side, std::string id)
   {
      if (side == Order::buy)
      {
         BidOrders::iterator i;
         for (i = bid_orders.begin(); i != bid_orders.end(); ++i)
            if (i->second.get_client_id() == id) return i->second;
      }
      else if (side == Order::sell)
      {
         AskOrders::iterator i;
         for (i = ask_orders.begin(); i != ask_orders.end(); ++i)
            if (i->second.get_client_id() == id) return i->second;
      }
      throw std::exception();
   }

   void Market::match(Order& bid, Order& ask)
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

   void Market::display() const
   {
      BidOrders::const_iterator iBid;
      AskOrders::const_iterator iAsk;

      std::cout << "BIDS:" << std::endl;
      std::cout << "-----" << std::endl << std::endl;
      for (iBid = bid_orders.begin(); iBid != bid_orders.end(); ++iBid)
         std::cout << iBid->second << std::endl;

      std::cout << std::endl << std::endl;

      std::cout << "ASKS:" << std::endl;
      std::cout << "-----" << std::endl << std::endl;
      for (iAsk = ask_orders.begin(); iAsk != ask_orders.end(); ++iAsk)
         std::cout << iAsk->second << std::endl;
   }

}


