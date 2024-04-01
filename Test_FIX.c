#include <profile.c>
#include <stdio.h>

static var startTime;
static bool Quoting = false;
TRADE* BidTrade;
TRADE* AskTrade;

function run() {

	set(TICKS + LOGFILE + PLOTNOW + PRELOAD);
	resf(BarMode, BR_WEEKEND);
	setf(BarMode, BR_FLAT);
	Verbose = 3;
	Hedge = 2;

	asset("EUR/USD");

	BarPeriod = 1;
	LookBack = 1440;

	Capital = 10000;

	setf(TradeMode, TR_GTC);

	if (is(INITRUN)) {
		startTime = timer();
	}

	var Close = priceClose();
	var HalfSpreadPIPs = 1;
	var LimitBid = Close - HalfSpreadPIPs * PIP;
	var LimitAsk = Close + HalfSpreadPIPs * PIP;

	if (!is(LOOKBACK)) {
		printf("\n======> New bar at %s, Close %f for %s", strdate(HMS, 0), Close, Asset);
		var Position = brokerCommand(GET_POSITION, Asset);
		printf("\n  Position %.2f, LotsPool %.2f", Position, (var)LotsPool);
	}

	MaxLong = 5;
	MaxShort = 5;
	if (!is(LOOKBACK) && !Quoting) {
		//brokerCommand(SET_ORDERTYPE, 2);
		Lots = 1;
		OrderLimit = LimitAsk;
		enterShort();

		OrderLimit = LimitBid;
		enterLong();

		Quoting = true;
	}

	for (open_trades)
	{
		printf("\n***** %d %s %d TradePriceOpen %.6f, TradeProfit %.6f, TradeEntryLimit %.6f",
			TradeID, ifelse(TradeIsOpen, "open", "pending"), ifelse(TradeIsLong, 1, -1), (var)TradePriceOpen, (var)TradeProfit, (var)TradeEntryLimit);
	}

	for (closed_trades)
	{
		printf("\n***** %d %s %d TradePriceOpen %.6f, TradeProfit %.6f, TradeEntryLimit %.6f, TradePriceOpen %.6f, TradePriceClose %.6f",
			TradeID, ifelse(TradeIsClosed, "closed", "unknown"), ifelse(TradeIsLong, 1, -1), (var)TradePriceOpen, (var)TradeProfit, (var)TradeEntryLimit, (var)TradePriceClose);
	}
}