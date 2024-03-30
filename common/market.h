#ifndef MARKET_H
#define MARKET_H

#include <map>
#include <queue>
#include <string>
#include <functional>
#include <mutex>

#include "nlohmann/json.h"

#include "order.h"
#include "market_data.h"
#include "price_sampler.h"
#include "bar_builder.h"
#include "json.h"


namespace common {

	class Market
	{
	public:
		explicit Market(
			const std::shared_ptr<PriceSampler>& price_sampler,
			const std::chrono::nanoseconds& bar_period,
			const std::chrono::nanoseconds& history_age,
			const std::chrono::nanoseconds& histroy_sample_period,
			std::mutex& mutex
		);

		Market(const Market&) = delete;

		Market& operator= (const Market&) = delete;

		bool insert(const Order& order);

		void erase(const Order& order);

		Order& find(Order::Side side, std::string id);

		bool match(std::queue<Order>&);

		void display() const;

		void simulate_next();

		const TopOfBook& get_top_of_book() const;

		const TopOfBook& get_previous_top_of_book() const;

		const std::map<std::chrono::nanoseconds, Bar>& get_bars() const;

		nlohmann::json get_bars_as_json(const std::chrono::nanoseconds& from, const std::chrono::nanoseconds& to) const;

	private:
		typedef std::multimap<double, Order, std::greater<double>> BidOrders;
		typedef std::multimap<double, Order, std::less<double>> AskOrders;

		void match(Order& bid, Order& ask);

		std::string symbol;
		std::shared_ptr<PriceSampler> price_sampler;
		std::chrono::nanoseconds bar_period;
		std::chrono::nanoseconds history_age;
		BarBuilder bar_builder;
		std::mutex& mutex;

		std::map<std::chrono::nanoseconds, TopOfBook> top_of_books;
		std::map<std::chrono::nanoseconds, Bar> bars;

		std::queue<Order> order_updates;
		BidOrders bid_orders;
		AskOrders ask_orders;
	};

}

#endif
