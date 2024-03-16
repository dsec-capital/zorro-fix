#ifndef MARKET_H
#define MARKET_H

#include "common/order.h"
#include "common/market_data.h"
#include "common/price_sampler.h"

#include <map>
#include <queue>
#include <string>
#include <functional>
#include <optional>
#include <mutex>

#include "quickfix/Message.h"

using namespace common;

class Market
{
public:
	explicit Market(
		const std::string &symbol,
		const std::shared_ptr<PriceSampler>& priceSampler,
		std::mutex& mutex
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

	std::mutex& m_mutex;
	std::string m_symbol;
	std::shared_ptr<PriceSampler> m_priceSampler;
	TopOfBook m_topOfBook;
	TopOfBook m_topOfBookPrevious;

	std::queue<Order> m_orderUpdates;
	BidOrders m_bidOrders;
	AskOrders m_askOrders;

	double m_simulatedMidPrice{0};
};

#endif
