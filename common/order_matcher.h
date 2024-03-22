#ifndef ORDERMATCHER_H
#define ORDERMATCHER_H

#include <map>
#include <iostream>
#include <random>
#include <thread>
#include <chrono>

#include "market.h"

namespace common {

	class OrderMatcher {
	public:
		typedef std::map<std::string, Market> Markets;

		OrderMatcher(
			Markets& markets
		)
			: markets(markets)
		{}

		Markets::iterator get_market(const std::string& symbol) {
			auto it = markets.find(symbol);
			if (it == markets.end()) {
				throw std::runtime_error(std::format("no market defined for symbol {}", symbol));
			}
			return it;
		}

		bool insert(const Order& order)
		{
			auto it = get_market(order.get_symbol());
			return it->second.insert(order);
		}

		void erase(const Order& order)
		{
			Markets::iterator i = markets.find(order.get_symbol());
			if (i == markets.end()) return;
			i->second.erase(order);
		}

		Order& find(std::string symbol, Order::Side side, std::string id)
		{
			Markets::iterator i = markets.find(symbol);
			if (i == markets.end()) throw std::exception();
			return i->second.find(side, id);
		}

		bool match(std::string symbol, std::queue<Order>& orders)
		{
			Markets::iterator i = markets.find(symbol);
			if (i == markets.end()) return false;
			return i->second.match(orders);
		}

		bool match(std::queue<Order>& orders)
		{
			Markets::iterator i;
			for (i = markets.begin(); i != markets.end(); ++i)
				i->second.match(orders);
			return orders.size() != 0;
		}

		void display(std::string symbol) const
		{
			Markets::const_iterator i = markets.find(symbol);
			if (i == markets.end()) return;
			i->second.display();
		}

		void display() const
		{
			std::cout << "SYMBOLS:" << std::endl;
			std::cout << "--------" << std::endl;

			Markets::const_iterator i;
			for (i = markets.begin(); i != markets.end(); ++i)
				std::cout << i->first << std::endl;
		}

		Markets& markets;
	};

}

#endif
