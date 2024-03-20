#include "pch.h"

#include "market.h"
#include "time_utils.h"

namespace common {

	Market::Market(
		const std::shared_ptr<PriceSampler>& price_sampler,
		const std::chrono::nanoseconds& bar_period,
		const std::chrono::nanoseconds& history_age,
		const std::chrono::nanoseconds& sample_period
	) : price_sampler(price_sampler)
	  , bar_period(bar_period)
	  , history_age(history_age)
      , bar_builder(bar_period, [this](const std::chrono::nanoseconds& start, const std::chrono::nanoseconds& end, double o, double h, double l, double c) {
			  this->bars.try_emplace(end, Bar{ start, end, o, h, l, c });
		  })
	{
		auto now = get_current_system_clock();
		top_of_books.try_emplace(now, price_sampler->actual_top_of_book());
		price_sampler->initialize_history(now - history_age, now, sample_period, top_of_books);
		build_bars(bar_builder, top_of_books, bars);
	}

	void Market::simulate_next() {
		auto now = get_current_system_clock();
		top_of_books.try_emplace(now, price_sampler->simulate_next(now));
		std::cout << symbol << ": " << top_of_books.rbegin()->second << std::endl;

		auto ageCutoff = now - history_age;
		while (top_of_books.begin() != top_of_books.end() && top_of_books.begin()->first < ageCutoff) {
			top_of_books.erase(top_of_books.begin());
		}

		bar_builder.add(now, get_top_of_book().mid());
		while (bars.begin() != bars.end() && bars.begin()->first < ageCutoff) {
			bars.erase(bars.begin());
		}
	}

	const TopOfBook& Market::get_top_of_book() const {
		return top_of_books.rbegin()->second;
	}

	const TopOfBook& Market::get_previous_top_of_book() const {
		auto it = top_of_books.rbegin();
		return (it++)->second;
	}

	bool Market::insert(const Order& order)
	{
		if (order.getSide() == Order::buy)
			bid_orders.insert(BidOrders::value_type(order.getPrice(), order));
		else
			ask_orders.insert(AskOrders::value_type(order.getPrice(), order));
		return true;
	}

	void Market::erase(const Order& order)
	{
		std::string id = order.getClientID();
		if (order.getSide() == Order::buy)
		{
			BidOrders::iterator i;
			for (i = bid_orders.begin(); i != bid_orders.end(); ++i)
				if (i->second.getClientID() == id)
				{
					bid_orders.erase(i);
					return;
				}
		}
		else if (order.getSide() == Order::sell)
		{
			AskOrders::iterator i;
			for (i = ask_orders.begin(); i != ask_orders.end(); ++i)
				if (i->second.getClientID() == id)
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

			if (iBid->second.getPrice() >= iAsk->second.getPrice())
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
				if (i->second.getClientID() == id) return i->second;
		}
		else if (side == Order::sell)
		{
			AskOrders::iterator i;
			for (i = ask_orders.begin(); i != ask_orders.end(); ++i)
				if (i->second.getClientID() == id) return i->second;
		}
		throw std::exception();
	}

	void Market::match(Order& bid, Order& ask)
	{
		double price = ask.getPrice();
		long quantity = 0;

		if (bid.getOpenQuantity() > ask.getOpenQuantity())
			quantity = ask.getOpenQuantity();
		else
			quantity = bid.getOpenQuantity();

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


