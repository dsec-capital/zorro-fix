#include <iostream>
#include <format>

#include "common/market.h"
#include "common/order_matcher.h"
#include "common/market_data.h"

using namespace common;

#define SYMBOL "BTCUSDT"

static int cl_ord_id = 0; 

template<typename Op>
void print(const OrderMatcher& matcher, Op op) {
	OrderMatcher::level_vector_t levels;
	matcher.book_levels(op, levels);
	std::cout << to_string(levels) << std::endl;
}

Order create_order(
	Order::Side side, double price, long quantity=100, Order::Type type=Order::Type::limit,
	const std::string& owner="owner", const std::string& target="target") {
	++cl_ord_id;
	return Order(std::format("cl_ord_id_{}", cl_ord_id), SYMBOL, owner, target, side, type, price, quantity);
}

int main()
{
    std::mutex mutex;

	auto by_quantity = [](const Order& o) {return o.get_quantity(); };
	auto by_open_quantity = [](const Order& o) {return o.get_open_quantity(); };
	auto by_last_exec_quantity = [](const Order& o) {return o.get_last_executed_quantity(); };

    auto matcher = OrderMatcher(mutex);

	auto o1 = create_order(Order::Side::buy, 99, 9);
	auto o2 = create_order(Order::Side::buy, 100, 10);
	auto o3 = create_order(Order::Side::sell, 101, 11);
	auto o4 = create_order(Order::Side::sell, 102, 12);

	matcher.insert(o1);
	matcher.insert(o2);
	matcher.insert(o3);
	matcher.insert(o4);
	print(matcher, by_quantity);

	auto ob = create_order(Order::Side::buy, 101, 20);
	matcher.insert(ob);
	print(matcher, by_quantity);

	std::queue<Order> q;
	matcher.match(q);
	print(matcher, by_last_exec_quantity);
	print(matcher, by_open_quantity);

	//matcher.display();

    std::cout << "done" << std::endl;
}

