#ifdef _MSC_VER
#pragma warning( disable : 4786 )
#endif

#include "Market.h"

#include <iostream>
#include <random>
#include <thread>

#include "quickfix/fix44/MarketDataSnapshotFullRefresh.h"
#include "quickfix/fix44/MarketDataIncrementalRefresh.h"

Market::Market(
	const std::string& symbol,
	double initialBidVolume,
	double initialAskVolume,
	const std::shared_ptr<PriceSampler>& priceSampler
)
	: m_priceSampler(priceSampler)
	, m_simulatedMidPrice(priceSampler->initial_mid_price())
	, m_symbol(symbol)
	, m_topOfBook(
		symbol,
		m_simulatedMidPrice - priceSampler->initial_spread() / 2, 
		initialBidVolume,
	    m_simulatedMidPrice + priceSampler->initial_spread() / 2,
		initialAskVolume) 
	, m_topOfBookPrevious(m_topOfBook)
{}

void Market::simulateNext() {
	auto spread = m_priceSampler->next_spread();
	auto mid = m_priceSampler->next_mid_price();
	std::unique_lock<std::mutex> ul(m_mutex);
	m_topOfBookPrevious = m_topOfBook;
	m_topOfBook.bidPrice = mid - spread / 2;
	m_topOfBook.askPrice = mid + spread / 2;
}

const TopOfBook& Market::getTopOfBook() const {
	return m_topOfBook;
}

FIX::Message Market::getSnapshotMessage(const std::string& senderCompID, const std::string& targetCompID, const std::string& mdReqID) {
	std::unique_lock<std::mutex> ul(m_mutex);
	TopOfBook top = m_topOfBook;
	ul.unlock();

	FIX::DateTime now = FIX::DateTime::nowUtc();

	FIX44::MarketDataSnapshotFullRefresh message;
	message.set(FIX::Symbol(m_symbol));
	message.set(FIX::MDReqID(mdReqID));

	FIX44::MarketDataSnapshotFullRefresh::NoMDEntries group;

	group.set(FIX::MDEntryType(FIX::MDEntryType_BID));
	group.set(FIX::MDEntryPx(top.bidPrice));
	group.set(FIX::MDEntrySize(top.bidVolume));
	group.set(FIX::MDEntryDate(now));
	group.set(FIX::MDEntryTime(now));
	message.addGroup(group);

	group.set(FIX::MDEntryType(FIX::MDEntryType_OFFER));
	group.set(FIX::MDEntryPx(top.askPrice));
	group.set(FIX::MDEntrySize(top.askVolume));
	group.set(FIX::MDEntryDate(now));
	group.set(FIX::MDEntryTime(now));
	message.addGroup(group);

	auto& header = message.getHeader();
	header.setField(FIX::SenderCompID(senderCompID));
	header.setField(FIX::TargetCompID(targetCompID));

	return message;
}

std::optional<FIX::Message> Market::getUpdateMessage(const std::string& senderCompID, const std::string& targetCompID, const std::string& mdReqID) {
	std::unique_lock<std::mutex> ul(m_mutex);

	if (m_topOfBook.bidPrice == m_topOfBookPrevious.bidPrice &&
		m_topOfBook.bidVolume == m_topOfBookPrevious.bidVolume &&
		m_topOfBook.askPrice == m_topOfBookPrevious.askPrice &&
		m_topOfBook.askVolume == m_topOfBookPrevious.askVolume) {
		return std::optional<FIX::Message>();
	}
	
	FIX::DateTime now = FIX::DateTime::nowUtc();

	FIX44::MarketDataIncrementalRefresh message;
	message.set(FIX::MDReqID(mdReqID));

	FIX44::MarketDataIncrementalRefresh::NoMDEntries group;
	if (m_topOfBook.bidPrice != m_topOfBookPrevious.bidPrice || m_topOfBook.bidVolume != m_topOfBookPrevious.bidVolume) {
		group.set(FIX::Symbol(m_symbol));
		group.set(FIX::MDEntryType(FIX::MDEntryType_BID));
		group.set(FIX::MDEntryPx(m_topOfBook.bidPrice));
		group.set(FIX::MDEntrySize(m_topOfBook.bidVolume));
		group.set(FIX::MDEntryDate(now));
		group.set(FIX::MDEntryTime(now));
		message.addGroup(group);
	}

	if (m_topOfBook.askPrice != m_topOfBookPrevious.askPrice || m_topOfBook.askVolume != m_topOfBookPrevious.askVolume) {
		group.set(FIX::MDEntryType(FIX::MDEntryType_OFFER));
		group.set(FIX::Symbol(m_symbol));
		group.set(FIX::MDEntryPx(m_topOfBook.askPrice));
		group.set(FIX::MDEntrySize(m_topOfBook.askVolume));
		group.set(FIX::MDEntryDate(now));
		group.set(FIX::MDEntryTime(now));
		message.addGroup(group);
	}

	ul.unlock();

	auto& header = message.getHeader();
	header.setField(FIX::SenderCompID(senderCompID));
	header.setField(FIX::TargetCompID(targetCompID));

	return std::optional<FIX::Message>(message);
}

bool Market::insert(const Order& order)
{
	if (order.getSide() == Order::buy)
		m_bidOrders.insert(BidOrders::value_type(order.getPrice(), order));
	else
		m_askOrders.insert(AskOrders::value_type(order.getPrice(), order));
	return true;
}

void Market::erase(const Order& order)
{
	std::string id = order.getClientID();
	if (order.getSide() == Order::buy)
	{
		BidOrders::iterator i;
		for (i = m_bidOrders.begin(); i != m_bidOrders.end(); ++i)
			if (i->second.getClientID() == id)
			{
				m_bidOrders.erase(i);
				return;
			}
	}
	else if (order.getSide() == Order::sell)
	{
		AskOrders::iterator i;
		for (i = m_askOrders.begin(); i != m_askOrders.end(); ++i)
			if (i->second.getClientID() == id)
			{
				m_askOrders.erase(i);
				return;
			}
	}
}

bool Market::match(std::queue<Order>& orders)
{
	while (true)
	{
		if (!m_bidOrders.size() || !m_askOrders.size())
			return orders.size() != 0;

		BidOrders::iterator iBid = m_bidOrders.begin();
		AskOrders::iterator iAsk = m_askOrders.begin();

		if (iBid->second.getPrice() >= iAsk->second.getPrice())
		{
			Order& bid = iBid->second;
			Order& ask = iAsk->second;

			match(bid, ask);
			orders.push(bid);
			orders.push(ask);

			if (bid.isClosed()) m_bidOrders.erase(iBid);
			if (ask.isClosed()) m_askOrders.erase(iAsk);
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
		for (i = m_bidOrders.begin(); i != m_bidOrders.end(); ++i)
			if (i->second.getClientID() == id) return i->second;
	}
	else if (side == Order::sell)
	{
		AskOrders::iterator i;
		for (i = m_askOrders.begin(); i != m_askOrders.end(); ++i)
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
	for (iBid = m_bidOrders.begin(); iBid != m_bidOrders.end(); ++iBid)
		std::cout << iBid->second << std::endl;

	std::cout << std::endl << std::endl;

	std::cout << "ASKS:" << std::endl;
	std::cout << "-----" << std::endl << std::endl;
	for (iAsk = m_askOrders.begin(); iAsk != m_askOrders.end(); ++iAsk)
		std::cout << iAsk->second << std::endl;
}
