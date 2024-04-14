#ifndef ORDER_MATCHER_H
#define ORDER_MATCHER_H

#include "pch.h"

#include "order.h"
#include "market_data.h"

namespace common {

	class OrderMatcher
	{
	public:
		typedef std::map<double, double, std::greater<double>> bid_map_t;
		typedef std::map<double, double, std::less<double>> ask_map_t;
		typedef std::vector<BookLevel> level_vector_t;

		explicit OrderMatcher(std::mutex& mutex);

		OrderMatcher(const OrderMatcher&) = delete;

		OrderMatcher& operator= (const OrderMatcher&) = delete;

		std::tuple<const Order*, bool, int> insert(const Order& order, std::queue<Order>& orders);

		int match(Order& order, std::queue<Order>&);

		void erase(const Order& order);

		Order& find(Order::Side side, std::string id);

		bid_map_t bid_map(const std::function<double(const Order&)> &f) const;

		ask_map_t ask_map(const std::function<double(const Order&)> &f) const;

		void book_levels(const std::function<double(const Order&)> &f, level_vector_t& levels) const;

		static int by_quantity(const Order& o);
		
		static int by_open_quantity(const Order& o);

		static int by_last_exec_quantity(const Order& o);

		void display() const;

		std::string to_string() const;

	private:
		// note: insertion order is only maintained for elements with  
		// identical keys, which properly implements price time priority 
		typedef std::multimap<double, Order, std::greater<double>> bid_order_map_t;
		typedef std::multimap<double, Order, std::less<double>> ask_order_map_t;

		std::mutex& mutex;

		std::queue<Order> order_updates;
		bid_order_map_t bid_orders;
		ask_order_map_t ask_orders;
	};

	std::string to_string(const typename OrderMatcher::level_vector_t& levels);

	template<typename Op>
	inline std::string to_string(const OrderMatcher& matcher, Op op) {
		OrderMatcher::level_vector_t levels;
		matcher.book_levels(op, levels);
		return to_string(levels);
	}

	template<typename Op>
	inline void print_levels(const OrderMatcher& matcher, Op op) {
		OrderMatcher::level_vector_t levels;
		matcher.book_levels(op, levels);
		std::cout << to_string(levels) << std::endl;
	}
}

#endif
