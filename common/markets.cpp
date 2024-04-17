#include "pch.h"

#include "markets.h"

namespace common {

	Markets::Markets(market_map_t& markets) : markets(markets) {}

	typename Markets::market_map_t::iterator Markets::get_market(const std::string& symbol) {
		auto it = markets.find(symbol);
		if (it == markets.end()) {
			throw std::runtime_error(std::format("no market defined for symbol {}", symbol));
		}
		return it;
	}

	std::tuple<const Order*, bool, int> Markets::insert(const Order& order, std::queue<Order>& orders)
	{
		auto it = get_market(order.get_symbol());
		return it->second.insert(order, orders);
	}

	Order& Markets::find(std::string symbol, Order::Side side, std::string id)
	{
		auto it = markets.find(symbol);
		if (it == markets.end()) throw std::exception();
		return it->second.find(side, id);
	}

	void Markets::display(std::string symbol) const
	{
		auto it = markets.find(symbol);
		if (it == markets.end()) return;
		it->second.display();
	}

	void Markets::display() const
	{
		std::cout << "SYMBOLS:" << std::endl;
		std::cout << "--------" << std::endl;

		for (auto it = markets.begin(); it != markets.end(); ++it)
			std::cout << it->first << std::endl;
	}
}