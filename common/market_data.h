#ifndef MARKET_DATA_H
#define MARKET_DATA_H

#include "pch.h"

#include "time_utils.h"

namespace common {

	const uint32_t TO_PIPS = 10000;
	const uint32_t TO_POINTS = 10 * TO_PIPS;

	struct BookLevel {
		double bid_price{ NAN };
		double bid_volume{0};
		double ask_price{ NAN };
		double ask_volume{0};
	};

	class TopOfBook
	{
		friend std::ostream& operator<<(std::ostream&, const TopOfBook&);

	public:

		TopOfBook() {}

		TopOfBook(
			const std::string& symbol,
			const std::chrono::nanoseconds& timestamp,
			double bid_price,
			double bid_volume,
			double ask_price,
			double ask_volume
		) : symbol(symbol)
		  , timestamp(timestamp)
		  , bid_price(bid_price)
		  , bid_volume(bid_volume)
		  , ask_price(ask_price)
		  , ask_volume(ask_volume)
		{}

		double mid() const {
			return 0.5 * (bid_price + ask_price);
		}

		double spread() const {
			return ask_price - bid_price;
		}

		std::string symbol{};
		std::chrono::nanoseconds timestamp{ 0 };
		double bid_price{ 0 };
		double bid_volume{ 0 };
		double ask_price{ 0 };
		double ask_volume{ 0 };

		std::string to_string() const {
			return "symbol=" + symbol + ", " 
				"timestamp=" + common::to_string(timestamp) + ", " 
				"bid_price=" + std::to_string(bid_price) + ", "
				"bid_volume=" + std::to_string(bid_volume) + ", "
				"ask_price=" + std::to_string(ask_price) + ", "
				"ask_volume=" + std::to_string(ask_volume);
		}
	};

	inline std::ostream& operator<<(std::ostream& ostream, const TopOfBook& top)
	{
		return ostream << top.to_string();
	}
}

#endif  


