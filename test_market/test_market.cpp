#include <iostream>
#include <format>

#include "common/market.h"
#include "common/order_matcher.h"
#include "common/market_data.h"

using namespace common;

#define SYMBOL "BTCUSDT"

static int cl_ord_id = 0; 

Order create_order(
	Order::Side side, double price, long quantity=100, Order::Type type=Order::Type::limit,
	const std::string& owner="owner", const std::string& target="target") {
	++cl_ord_id;
	return Order(std::format("cl_ord_id_{}", cl_ord_id), SYMBOL, owner, target, side, type, price, quantity);
}

int main()
{
    std::mutex mutex;

    auto matcher = OrderMatcher(mutex);

	auto o1 = create_order(Order::Side::buy, 99, 9);
	auto o2 = create_order(Order::Side::buy, 100, 10);
	auto o3 = create_order(Order::Side::sell, 101, 11);
	auto o4 = create_order(Order::Side::sell, 102, 12);

	matcher.insert(o1);
	matcher.insert(o2);
	matcher.insert(o3);
	matcher.insert(o4);

	OrderMatcher::level_vector_t levels;
	matcher.book_levels([](const Order& o) {return o.get_quantity(); }, levels);
	std::cout << to_string(levels);

	matcher.display();

    std::cout << "done" << std::endl;
}

