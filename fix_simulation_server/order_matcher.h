#ifndef ORDERMATCHER_H
#define ORDERMATCHER_H

#include "market.h"
#include "fodra_pham.h"

#include <map>
#include <iostream>
#include <random>
#include <thread>
#include <chrono>

#include "quickfix/Log.h"

class OrderMatcher
{
	typedef std::map<std::string, Market> Markets;

	std::random_device random_device;
	std::mt19937 generator;
	FIX::Log* logger;

public:

	OrderMatcher(FIX::Log* logger)
		: random_device()
		, generator(random_device()) 
		, logger(logger)
	{}

	std::shared_ptr<PriceSampler> createSampler(const std::string& symbol) {
		// TODO read from a config file
		auto alpha_plus = 0.2;
		auto alpha_neg = -0.2;
		std::vector<double> tick_probs{ 0.1, 0.3, 0.2, 0.1, 0.1, 0.1, 0.1 };
		auto tick_size = 0.5;
		double initial_price = 100;
		double initial_spread = 2 * tick_size;
		int initial_dir = 1;
		return std::make_shared<FodraPhamSampler<std::mt19937>>(
			generator,
			alpha_plus,
			alpha_neg,
			tick_probs,
			tick_size,
			initial_price,
			initial_price,
			initial_dir
		);
	}

	Markets::iterator getMarket(const std::string& symbol) {
		auto it = m_markets.find(symbol);
		if (it == m_markets.end()) {
			it = m_markets.try_emplace(
				symbol, 
				symbol, 
				100, 
				100,
				createSampler(symbol)
			).first;
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
		Markets::iterator i = m_markets.find(order.getSymbol());
		if (i == m_markets.end()) return;
		i->second.erase(order);
	}

	Order& find(std::string symbol, Order::Side side, std::string id)
	{
		Markets::iterator i = m_markets.find(symbol);
		if (i == m_markets.end()) throw std::exception();
		return i->second.find(side, id);
	}

	bool match(std::string symbol, std::queue<Order>& orders)
	{
		Markets::iterator i = m_markets.find(symbol);
		if (i == m_markets.end()) return false;
		return i->second.match(orders);
	}

	bool match(std::queue<Order>& orders)
	{
		Markets::iterator i;
		for (i = m_markets.begin(); i != m_markets.end(); ++i)
			i->second.match(orders);
		return orders.size() != 0;
	}

	void display(std::string symbol) const
	{
		Markets::const_iterator i = m_markets.find(symbol);
		if (i == m_markets.end()) return;
		i->second.display();
	}

	void display() const
	{
		std::cout << "SYMBOLS:" << std::endl;
		std::cout << "--------" << std::endl;

		Markets::const_iterator i;
		for (i = m_markets.begin(); i != m_markets.end(); ++i)
			std::cout << i->first << std::endl;
	}

	Markets m_markets;
};

#endif
