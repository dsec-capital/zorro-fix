#ifndef ORDERMATCHER_H
#define ORDERMATCHER_H

#include <map>
#include <iostream>
#include <random>
#include <thread>
#include <chrono>

#include "market.h"

namespace common {

	class Markets {
	public:
		typedef std::map<std::string, Market> market_map_t;

		Markets(market_map_t& markets);

		OrderInsertResult insert(const Order& order);

		std::optional<Order> find(const std::string& symbol, const std::string& ord_id, Order::Side side);

		std::optional<Order> erase(const std::string& symbol, const std::string& ord_id, Order::Side side);

		void display(std::string symbol) const;

		void display() const;

		market_map_t& markets;
	};

}

#endif
