#ifndef ORDER_MATCHER_H
#define ORDER_MATCHER_H

#include "pch.h"

#include "order.h"
#include "market_data.h"

namespace common {

	class OrderInsertResult {
	public:
		OrderInsertResult();

		OrderInsertResult(const std::optional<Order>& order, std::vector<Order>&& matched, bool error=false);

		std::optional<Order> resting_order;
		std::vector<Order> matched;
		bool error;
	};

	class OrderMatcher
	{
	public:
		// note: insertion order is only maintained for elements with  
		// identical keys, which properly implements price time priority 
		typedef std::multimap<double, Order, std::greater<double>> bid_order_map_t;
		typedef std::multimap<double, Order, std::less<double>> ask_order_map_t;

		typedef std::map<double, double, std::greater<double>> bid_map_t;
		typedef std::map<double, double, std::less<double>> ask_map_t;
		typedef std::vector<BookLevel> level_vector_t;

		explicit OrderMatcher(std::mutex& mutex);

		OrderMatcher(const OrderMatcher&) = delete;

		OrderMatcher& operator= (const OrderMatcher&) = delete;

		OrderInsertResult insert(const Order& order);

		std::optional<Order> find(const std::string& ord_id, Order::Side side);

		std::optional<Order> erase(const std::string& ord_id, const Order::Side& side);

		double vwap_price(const Order::Side& side, long quantity, const std::string& owner);

		std::pair<bid_order_map_t, ask_order_map_t> get_orders() const;

		bid_map_t bid_map(const std::function<double(const Order&)> &f) const;

		ask_map_t ask_map(const std::function<double(const Order&)> &f) const;

		void book_levels(const std::function<double(const Order&)> &f, level_vector_t& levels) const;

		static int by_quantity(const Order& o);
		
		static int by_open_quantity(const Order& o);

		static int by_last_exec_quantity(const Order& o);

		std::string to_string() const;

	protected:
		std::mutex& mutex;

	private:
		void match(Order& order, std::vector<Order>&);

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
