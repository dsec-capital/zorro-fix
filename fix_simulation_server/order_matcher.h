#ifndef ORDERMATCHER_H
#define ORDERMATCHER_H

#include "market.h"
#include <map>
#include <iostream>

class OrderMatcher
{
	typedef std::map < std::string, Market > Markets;

public:

	bool insert(const Order& order)
	{
		Markets::iterator i = m_markets.find(order.getSymbol());
		if (i == m_markets.end())
			i = m_markets.insert(std::make_pair(order.getSymbol(), Market())).first;
		return i->second.insert(order);
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

	bool match(std::string symbol, std::queue < Order >& orders)
	{
		Markets::iterator i = m_markets.find(symbol);
		if (i == m_markets.end()) return false;
		return i->second.match(orders);
	}

	bool match(std::queue < Order >& orders)
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

private:
	Markets m_markets;
};

#endif
