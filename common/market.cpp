#include "pch.h"

#include "market.h"
#include "time_utils.h"
#include "json.h"

namespace common {

	Market::Market(
		const std::shared_ptr<PriceSampler>& price_sampler,
		const std::chrono::nanoseconds& bar_period,
		const std::chrono::nanoseconds& history_age,
		const std::chrono::nanoseconds& sample_period,
		std::mutex& mutex
	) : price_sampler(price_sampler)
	  , bar_period(bar_period)
	  , history_age(history_age)
      , bar_builder(bar_period, [this](const std::chrono::nanoseconds& end, double o, double h, double l, double c) {
			this->bars.try_emplace(end, end, o, h, l, c);
		  })
      , mutex(mutex)
	{
		auto now = get_current_system_clock();
		top_of_books.try_emplace(now, price_sampler->actual_top_of_book());
		price_sampler->initialize_history(now - history_age, now, sample_period, top_of_books);
		build_bars(bar_builder, top_of_books, bars);
	}

	void Market::simulate_next() {
		auto now = get_current_system_clock();

		std::unique_lock<std::mutex> ul(mutex);
		top_of_books.try_emplace(now, price_sampler->simulate_next(now));

		auto ageCutoff = now - history_age;
		while (top_of_books.begin() != top_of_books.end() && top_of_books.begin()->first < ageCutoff) {
			top_of_books.erase(top_of_books.begin());
		}

		bar_builder.add(now, get_top_of_book().mid());
		while (bars.begin() != bars.end() && bars.begin()->first < ageCutoff) {
			bars.erase(bars.begin());
		}
		ul.release();

		std::cout << symbol << ": " << top_of_books.rbegin()->second << std::endl;
	}

	const TopOfBook& Market::get_top_of_book() const {
		std::unique_lock<std::mutex> ul(mutex);
		return top_of_books.rbegin()->second;
	}

	const TopOfBook& Market::get_previous_top_of_book() const {
		std::unique_lock<std::mutex> ul(mutex);
		auto it = top_of_books.rbegin();
		return (it++)->second;
	}

	const std::map<std::chrono::nanoseconds, Bar>& Market::get_bars() const {
		return bars;
	}

	nlohmann::json Market::get_bars_as_json(const std::chrono::nanoseconds& from, const std::chrono::nanoseconds& to) const {
		return to_json(from, to, bars);
	}

	bool Market::insert(const Order& order)
	{
		if (order.get_side() == Order::buy)
			bid_orders.insert(BidOrders::value_type(order.get_price(), order));
		else
			ask_orders.insert(AskOrders::value_type(order.get_price(), order));
		return true;
	}

	void Market::erase(const Order& order)
	{
		std::string id = order.get_client_id();
		if (order.get_side() == Order::buy)
		{
			BidOrders::iterator i;
			for (i = bid_orders.begin(); i != bid_orders.end(); ++i)
				if (i->second.get_client_id() == id)
				{
					bid_orders.erase(i);
					return;
				}
		}
		else if (order.get_side() == Order::sell)
		{
			AskOrders::iterator i;
			for (i = ask_orders.begin(); i != ask_orders.end(); ++i)
				if (i->second.get_client_id() == id)
				{
					ask_orders.erase(i);
					return;
				}
		}
	}

	bool Market::match(std::queue<Order>& orders)
	{
		while (true)
		{
			if (!bid_orders.size() || !ask_orders.size())
				return orders.size() != 0;

			BidOrders::iterator iBid = bid_orders.begin();
			AskOrders::iterator iAsk = ask_orders.begin();

			if (iBid->second.get_price() >= iAsk->second.get_price())
			{
				Order& bid = iBid->second;
				Order& ask = iAsk->second;

				match(bid, ask);
				orders.push(bid);
				orders.push(ask);

				if (bid.isClosed()) bid_orders.erase(iBid);
				if (ask.isClosed()) ask_orders.erase(iAsk);
			}
			else
				return orders.size() != 0;
		}
	}

	Order& Market::find(Order::Side side, std::string id)
	{
		if (side == Order::buy)
		{
			BidOrders::iterator i;
			for (i = bid_orders.begin(); i != bid_orders.end(); ++i)
				if (i->second.get_client_id() == id) return i->second;
		}
		else if (side == Order::sell)
		{
			AskOrders::iterator i;
			for (i = ask_orders.begin(); i != ask_orders.end(); ++i)
				if (i->second.get_client_id() == id) return i->second;
		}
		throw std::exception();
	}

	void Market::match(Order& bid, Order& ask)
	{
		double price = ask.get_price();
		long quantity = 0;

		if (bid.get_open_quantity() > ask.get_open_quantity())
			quantity = ask.get_open_quantity();
		else
			quantity = bid.get_open_quantity();

		bid.execute(price, quantity);
		ask.execute(price, quantity);
	}

	void Market::display() const
	{
		BidOrders::const_iterator iBid;
		AskOrders::const_iterator iAsk;

		std::cout << "BIDS:" << std::endl;
		std::cout << "-----" << std::endl << std::endl;
		for (iBid = bid_orders.begin(); iBid != bid_orders.end(); ++iBid)
			std::cout << iBid->second << std::endl;

		std::cout << std::endl << std::endl;

		std::cout << "ASKS:" << std::endl;
		std::cout << "-----" << std::endl << std::endl;
		for (iAsk = ask_orders.begin(); iAsk != ask_orders.end(); ++iAsk)
			std::cout << iAsk->second << std::endl;
	}

}


