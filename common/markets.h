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

		typename market_map_t::iterator get_market(const std::string& symbol);

		std::tuple<const Order*, bool, int> insert(const Order& order, std::queue<Order>& orders);

		void erase(const Order& order);

		Order& find(std::string symbol, Order::Side side, std::string id);

		void display(std::string symbol) const;

		void display() const;

		market_map_t& markets;
	};

}

#endif
