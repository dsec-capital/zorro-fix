#ifndef ORDERMATCHER_H
#define ORDERMATCHER_H

#include <map>
#include <iostream>
#include <random>
#include <thread>
#include <chrono>

#include "quickfix/Log.h"

#include "common/market.h"

using namespace common;


class Markets {
public:
	typedef std::map<std::string, Market> market_map_t;

	Markets(market_map_t& markets, FIX::Log* logger)
		: m_markets(markets)
		, m_logger(logger)
	{}

	market_map_t::iterator getMarket(const std::string& symbol) {
		auto it = m_markets.find(symbol);
		if (it == m_markets.end()) {
			throw std::runtime_error(std::format("no market defined for symbol {}", symbol));
		}
		return it;
	}

	bool insert(const Order& order)
	{
		auto it = getMarket(order.getSymbol());
		return it->second.insert(order);
	}

	void erase(const Order& order)
	{
		market_map_t::iterator i = m_markets.find(order.getSymbol());
		if (i == m_markets.end()) return;
		i->second.erase(order);
	}

	Order& find(std::string symbol, Order::Side side, std::string id)
	{
		market_map_t::iterator i = m_markets.find(symbol);
		if (i == m_markets.end()) throw std::exception();
		return i->second.find(side, id);
	}

	bool match(std::string symbol, std::queue<Order>& orders)
	{
		market_map_t::iterator i = m_markets.find(symbol);
		if (i == m_markets.end()) return false;
		return i->second.match(orders);
	}

	bool match(std::queue<Order>& orders)
	{
		market_map_t::iterator i;
		for (i = m_markets.begin(); i != m_markets.end(); ++i)
			i->second.match(orders);
		return orders.size() != 0;
	}

	void display(std::string symbol) const
	{
		market_map_t::const_iterator i = m_markets.find(symbol);
		if (i == m_markets.end()) return;
		i->second.display();
	}

	void display() const
	{
		std::cout << "SYMBOLS:" << std::endl;
		std::cout << "--------" << std::endl;

		market_map_t::const_iterator i;
		for (i = m_markets.begin(); i != m_markets.end(); ++i)
			std::cout << i->first << std::endl;
	}

	market_map_t& m_markets;
	FIX::Log* m_logger;

};

#endif
