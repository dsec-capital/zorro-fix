#ifndef MARKET_H
#define MARKET_H

#include "order.h"
#include "market_data.h"
#include "price_sampler.h"

#include <map>
#include <queue>
#include <string>
#include <functional>
#include <optional>
#include <mutex>

namespace common {

	class Market
	{
	public:
		explicit Market(
			const std::shared_ptr<PriceSampler>& price_sampler
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

	private:
		typedef std::multimap<double, Order, std::greater<double>> BidOrders;
		typedef std::multimap<double, Order, std::less<double>> AskOrders;

		void match(Order& bid, Order& ask);

		std::string symbol;
		std::shared_ptr<PriceSampler> price_sampler;

		TopOfBook top_of_book;
		TopOfBook top_of_book_previous;

		std::queue<Order> order_updates;
		BidOrders bid_orders;
		AskOrders ask_orders;

		double m_simulatedMidPrice{ 0 };
	};

}

#endif
