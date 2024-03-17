#ifndef MARKET_DATA_H
#define MARKET_DATA_H

#include <string>
#include <iomanip>
#include <ostream>

namespace common {

	class TopOfBook
	{
		friend std::ostream& operator<<(std::ostream&, const TopOfBook&);

	public:

		TopOfBook() {}

		TopOfBook(
			const std::string& symbol,
			double bid_price,
			double bid_volume,
			double ask_price,
			double ask_volume
		) : symbol(symbol),
			bid_price(bid_price),
			bid_volume(bid_volume),
			ask_price(ask_price),
			ask_volume(ask_volume)
		{}

		double mid() const {
			return 0.5 * (bid_price + ask_price);
		}

		double spread() const {
			return ask_price - bid_price;
		}

		std::string symbol{};
		double bid_price{ 0 };
		double bid_volume{ 0 };
		double ask_price{ 0 };
		double ask_volume{ 0 };

		std::string toString() const {
			return "symbol=" + symbol + ", "
				"bid_price=" + std::to_string(bid_price) + ", "
				"bid_volume=" + std::to_string(bid_volume) + ", "
				"ask_price=" + std::to_string(ask_price) + ", "
				"ask_volume=" + std::to_string(ask_volume);
		}
	};

	inline std::ostream& operator<<(std::ostream& ostream, const TopOfBook& top)
	{
		return ostream << top.toString();
	}
}

#endif  


