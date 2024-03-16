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
			double bidPrice,
			double bidVolume,
			double askPrice,
			double askVolume
		) : symbol(symbol),
			bidPrice(bidPrice),
			bidVolume(bidVolume),
			askPrice(askPrice),
			askVolume(askVolume)
		{}

		double mid() const {
			return 0.5 * (bidPrice + askPrice);
		}

		double spread() const {
			return askPrice - bidPrice;
		}

		std::string symbol{};
		double bidPrice{ 0 };
		double bidVolume{ 0 };
		double askPrice{ 0 };
		double askVolume{ 0 };

		std::string toString() const {
			return "symbol=" + symbol + ", "
				"bidPrice=" + std::to_string(bidPrice) + ", "
				"bidVolume=" + std::to_string(bidVolume) + ", "
				"askPrice=" + std::to_string(askPrice) + ", "
				"askVolume=" + std::to_string(askVolume);
		}
	};

	inline std::ostream& operator<<(std::ostream& ostream, const TopOfBook& top)
	{
		return ostream << top.toString();
	}
}

#endif  


