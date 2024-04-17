#include <iostream>
#include <format>

#include "common/market.h"
#include "common/order_matcher.h"
#include "common/market_data.h"
#include "common/price_sampler.h"
#include "common/white_noise.h"
#include "common/time_utils.h"

using namespace common;

constexpr auto SYMBOL = "APPL";

static int ord_id = 0; 

std::mutex mutex;

auto by_quantity = [](const Order& o) {return o.get_quantity(); };
auto by_open_quantity = [](const Order& o) {return o.get_open_quantity(); };
auto by_last_exec_quantity = [](const Order& o) {return o.get_last_executed_quantity(); };

std::string generate_id(const std::string& label) {
	return std::format("{}_{}", label, ++ord_id);
}

Order create_order(
	Order::Side side, 
	double price, 
	long quantity=100, 
	const std::string& owner = "market", 
	Order::Type type=Order::Type::limit,
	const std::string& target="target") {
	return Order(generate_id("ord_id"), generate_id("cl_ord_id"), SYMBOL, owner, target, side, type, price, quantity);
}

void test_quoting() {
	std::random_device random_device;
	std::mt19937 generator(random_device());
	auto tick_scale = 100000.0;
	auto tick_size = 0.00001;
	auto sampler = std::make_shared<WhiteNoise>(
		SYMBOL,
		generator,
		0.4,
		tick_scale
	);

	std::queue<Order> order_q;
	auto matcher = OrderMatcher(mutex);

	auto n = 100;
	auto dt = std::chrono::hours(12);
	auto t0 = get_current_system_clock() - n*dt;

	auto bid = 100.0;
	auto ask = 101.0;
	auto current = TopOfBook(SYMBOL, t0, bid, 10, ask, 11);

	// two trader orders, two from the market 
	auto o1 = create_order(Order::Side::buy, bid - 10*tick_size, 2, "trader");
	auto o2 = create_order(Order::Side::sell, ask + 10 * tick_size, 2, "trader");
	auto o_bid = create_order(Order::Side::buy, current.bid_price, 100, "market");
	auto o_ask = create_order(Order::Side::sell, current.ask_price, 100, "market");

	matcher.insert(o1, order_q);
	matcher.insert(o2, order_q);
	matcher.insert(o_bid, order_q);
	matcher.insert(o_ask, order_q);

	assert(order_q.empty());

	for (auto i = 0; i < 5000; ++i) {
		t0 += dt;
		auto next = sampler->sample(current, t0);

		//std::cout << std::format("{:>8.1f} | {:<8.1f}", next.bid_price, next.ask_price) << std::endl;

		if (next.bid_price != current.bid_price) {
			matcher.erase(o_bid.get_ord_id(), o_bid.get_side());
			o_bid = create_order(Order::Side::buy, next.bid_price, 100, "market");
			matcher.insert(o_bid, order_q);
		}
		if (next.ask_price != current.ask_price) {
			matcher.erase(o_ask.get_ord_id(), o_ask.get_side());
			o_ask = create_order(Order::Side::sell, next.ask_price, 100, "market");
			matcher.insert(o_ask, order_q);
		}

		if (!order_q.empty()) {
			std::cout << "match at " << t0 << std::endl;
			std::cout << std::format("  current {:>8.5f} | {:<8.5f}", current.bid_price, current.ask_price) << std::endl;
			std::cout << std::format("  next    {:>8.5f} | {:<8.5f}", next.bid_price, next.ask_price) << std::endl;
		}

		std::vector<Order> new_orders;
		while (!order_q.empty()) {
			auto& o = order_q.front();

			std::cout << std::format(
				"  {} price={:>8.5f}, exec price={:>8.5f} owner={}", 
				to_string(o.get_side()), o.get_price(), o.get_last_executed_price(), o.get_owner()
			) << std::endl;

			if (o.get_side() == Order::buy && o.get_owner() == "trader") {
				auto price = next.bid_price - 10 * tick_size;
				auto o = create_order(Order::Side::buy, price, 2, "trader");
				new_orders.emplace_back(o);
				std::cout << std::format("  new buy order at {:>8.5f}", price) << std::endl;
			}

			if (o.get_side() == Order::sell && o.get_owner() == "trader") {
				auto price = next.ask_price + 10 * tick_size;
				auto o = create_order(Order::Side::sell, price, 2, "trader");
				new_orders.emplace_back(o);
				std::cout << std::format("  new sell order at {:>8.5f}", price) << std::endl;
			}

			order_q.pop();
		}

		auto m = 0;
		for (auto& o : new_orders) {
			auto [op, error, dm] = matcher.insert(o, order_q);
			m += dm;
		}

		// should not have any matches
		assert(m == 0 && order_q.empty());

		current = next;
	}
}

void test_aggressive_sell() {
	auto matcher = OrderMatcher(mutex);

	std::queue<Order> order_q;

	auto o1 = create_order(Order::Side::buy, 100, 1, "trader");
	auto o2 = create_order(Order::Side::sell, 110, 1, "trader");
	matcher.insert(o1, order_q);
	matcher.insert(o2, order_q);
	std::cout << "by_quantity" << std::endl;
	print_levels(matcher, by_quantity);

	auto o3 = create_order(Order::Side::sell, 90, 10, "market");
	matcher.insert(o3, order_q);
	std::cout << "by_quantity" << std::endl;
	print_levels(matcher, by_quantity);

	std::cout << "by_last_exec_quantity" << std::endl;
	print_levels(matcher, by_last_exec_quantity);
	std::cout << "by_open_quantity" << std::endl;
	print_levels(matcher, by_open_quantity);

	while (!order_q.empty()) {
		std::cout << order_q.front().to_string() << std::endl;
		order_q.pop();
	}
}

void test_aggressive_buy() {
	auto matcher = OrderMatcher(mutex);

	std::queue<Order> order_q;

	auto o1 = create_order(Order::Side::buy, 100, 1, "trader");
	auto o2 = create_order(Order::Side::sell, 110, 1, "trader");
	matcher.insert(o1, order_q);
	matcher.insert(o2, order_q);
	std::cout << "by_quantity" << std::endl;
	print_levels(matcher, by_quantity);

	auto o3 = create_order(Order::Side::buy, 120, 10, "market");
	matcher.insert(o3, order_q);
	std::cout << "by_quantity" << std::endl;
	print_levels(matcher, by_quantity);

	std::cout << "by_last_exec_quantity" << std::endl;
	print_levels(matcher, by_last_exec_quantity);
	std::cout << "by_open_quantity" << std::endl;
	print_levels(matcher, by_open_quantity);

	while (!order_q.empty()) {
		std::cout << order_q.front().to_string() << std::endl;
		order_q.pop();
	}
}

int main()
{
	test_quoting();
	//test_aggressive_sell();
	//test_aggressive_buy();
}

