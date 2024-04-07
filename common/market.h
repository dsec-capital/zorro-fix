#ifndef MARKET_H
#define MARKET_H

#include <map>
#include <queue>
#include <string>
#include <functional>
#include <mutex>
#include <chrono>

#include "nlohmann/json.h"

#include "order.h"
#include "order_matcher.h"
#include "market_data.h"
#include "price_sampler.h"
#include "bar_builder.h"
#include "json.h"

namespace common {

	constexpr auto OWNER_MARKET_SIMULATOR = "mkt_sim";

	class Market : public OrderMatcher
	{
	public:
		explicit Market(
			const std::shared_ptr<PriceSampler>& price_sampler,
			const TopOfBook& current,
			const std::chrono::nanoseconds& bar_period,
			const std::chrono::nanoseconds& history_age,
			const std::chrono::nanoseconds& histroy_sample_period,
			bool prune_bars,
			std::mutex& mutex
		);

		Market(const Market&) = delete;

		Market& operator= (const Market&) = delete;

		void simulate_next();

		void update_quotes(const TopOfBook& current, const TopOfBook& previous, std::queue<Order>& orders);

		std::pair<TopOfBook, TopOfBook> get_top_of_book() const;

		void extend_bar_history(const std::chrono::nanoseconds& until_past);

		std::tuple<std::chrono::nanoseconds, std::chrono::nanoseconds, size_t> get_bar_range() const;

		std::pair<nlohmann::json, int> get_bars_as_json(const std::chrono::nanoseconds& from, const std::chrono::nanoseconds& to);

	private:

		std::string quoting_cl_ord_id();

		std::string symbol;
		std::shared_ptr<PriceSampler> price_sampler;
		std::chrono::nanoseconds bar_period;
		std::chrono::nanoseconds history_age;
		std::chrono::nanoseconds history_sample_period;
		bool prune_bars;
		std::mutex& mutex;

		BarBuilder bar_builder;
		ReverseBarBuilder history_bar_builder;

		TopOfBook current;
		TopOfBook previous;
		TopOfBook oldest;
		std::map<std::chrono::nanoseconds, TopOfBook> top_of_books;
		std::map<std::chrono::nanoseconds, Bar> bars;

		bool quoting;
		int cl_ord_id;
		Order bid_order, ask_order;
	};

}

#endif
