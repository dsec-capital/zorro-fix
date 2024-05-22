#include "pch.h"

#include "markets.h"

namespace common {

	Markets::Markets(market_map_t& markets) : markets(markets) {}

	OrderInsertResult Markets::insert(const Order& order)
	{
		auto it = markets.find(order.get_symbol());
		if (it != markets.end()) {
			return it->second.insert(order);
		}
		else {
			return OrderInsertResult();
		}
	}

	std::optional<Order> Markets::find(const std::string& symbol, const std::string& ord_id, Order::Side side)
	{
		auto it = markets.find(symbol);
		if (it != markets.end()) {
			return it->second.find(ord_id, side);
		}
		else {
			return std::optional<Order>();
		}
	}

	std::optional<Order> Markets::erase(const std::string& symbol, const std::string& ord_id, Order::Side side) {
		auto it = markets.find(symbol);
		if (it != markets.end()) {
			return it->second.erase(ord_id, side);
		}
		else {
			return std::optional<Order>();
		}
	}

	std::optional<TopOfBook> Markets::get_current_top_of_book(const std::string& symbol) const
	{
		auto it = markets.find(symbol);
		if (it != markets.end()) {
			return it->second.get_current_top_of_book();
		}
		else {
			return std::optional<TopOfBook>();
		}
	}

	std::string Markets::to_string(std::string symbol) const
	{
		auto it = markets.find(symbol);
		if (it == markets.end()) return "";
		return it->second.to_string();
	}

	void Markets::display() const
	{
		std::cout << "SYMBOLS:" << std::endl;
		std::cout << "--------" << std::endl;

		for (auto it = markets.begin(); it != markets.end(); ++it)
			std::cout << it->first << std::endl;
	}
}