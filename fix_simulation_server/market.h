#ifndef MARKET_H
#define MARKET_H

#include "order.h"
#include "price_sampler.h"

#include <map>
#include <queue>
#include <string>
#include <functional>
#include <optional>
#include <mutex>

#include "quickfix/Message.h"


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

class Market
{
public:
	explicit Market(
		const std::string &symbol,
		double initialBidVolume,
		double initialAskVolume,
		const std::shared_ptr<PriceSampler>& priceSampler
	);

	Market(const Market&) = delete;

	Market& operator= (const Market&) = delete;

	bool insert(const Order& order);
	
	void erase(const Order& order);

	Order& find(Order::Side side, std::string id);
	
	bool match(std::queue<Order>&);
	
	void display() const;

	void simulateNext();

	const TopOfBook& getTopOfBook() const;

	FIX::Message getSnapshotMessage(const std::string& senderCompID, const std::string& targetCompID, const std::string& mdReqID);

	std::optional<FIX::Message> getUpdateMessage(const std::string& senderCompID, const std::string& targetCompID, const std::string& mdReqID);

private:
	typedef std::multimap<double, Order, std::greater<double>> BidOrders;
	typedef std::multimap<double, Order, std::less<double>> AskOrders;

	void match(Order& bid, Order& ask);

	std::mutex m_mutex;
	std::shared_ptr<PriceSampler> m_priceSampler;
	std::string m_symbol;
	TopOfBook m_topOfBook;
	TopOfBook m_topOfBookPrevious;

	std::queue<Order> m_orderUpdates;
	BidOrders m_bidOrders;
	AskOrders m_askOrders;

	double m_simulatedMidPrice{0};

};

#endif
