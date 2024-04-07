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

		Markets(
			market_map_t& markets
		)
			: markets(markets)
		{}

		market_map_t::iterator get_market(const std::string& symbol) {
			auto it = markets.find(symbol);
			if (it == markets.end()) {
				throw std::runtime_error(std::format("no market defined for symbol {}", symbol));
			}
			return it;
		}

		bool insert(const Order& order)
		{
			// TODO 
			std::queue<Order> order_q;
			auto it = get_market(order.get_symbol());
			return it->second.insert(order, order_q);
		}

		void erase(const Order& order)
		{
			market_map_t::iterator i = markets.find(order.get_symbol());
			if (i == markets.end()) return;
			i->second.erase(order);
		}

		Order& find(std::string symbol, Order::Side side, std::string id)
		{
			market_map_t::iterator i = markets.find(symbol);
			if (i == markets.end()) throw std::exception();
			return i->second.find(side, id);
		}

		bool match(std::string symbol, std::queue<Order>& orders)
		{
			market_map_t::iterator i = markets.find(symbol);
			if (i == markets.end()) return false;
			//return i->second.match(orders);
			return false; // TODO
		}

		bool match(std::queue<Order>& orders)
		{
			market_map_t::iterator i;
			//for (i = markets.begin(); i != markets.end(); ++i)
			//	i->second.match(orders);
			return orders.size() != 0;
		}

		void display(std::string symbol) const
		{
			market_map_t::const_iterator i = markets.find(symbol);
			if (i == markets.end()) return;
			i->second.display();
		}

		void display() const
		{
			std::cout << "SYMBOLS:" << std::endl;
			std::cout << "--------" << std::endl;

			market_map_t::const_iterator i;
			for (i = markets.begin(); i != markets.end(); ++i)
				std::cout << i->first << std::endl;
		}

		market_map_t& markets;
	};

}

#endif
