#include <profile.c>
#include <stdio.h>

#define ORDERTYPE_GTC 2

static var startTime;
static var AskQuote = 0;
static var BidQuote = 0;
static TRADE* BidTrade = 0;
static TRADE* AskTrade = 0;


var round_up(var in, var multiple) {
	var m = fmod(in, multiple);
	if (m == 0.0) {
		return in;
	}
	else {
		var down = in - m;
		return down + ifelse(in < 0.0, -multiple, multiple);
	}
}

var round_down(var in, var multiple) {
	return in - fmod(in, multiple);
}

void tick() {
	if (is(LOOKBACK)) return;
	var close = priceClose();
	printf("\ntic [%s] %s ask=%.5f bid=%.5f spread=%.6f",
		strdate("%H:%M:%S.", NOW),
		Asset,
		close,
		close - Spread,
		Spread
	);
}

void tmf() {
	var Close = priceClose();
	var TopAsk = Close;
	var TopBid = Close - Spread;
	printf("\ntmf [%s] id=%d %s missed=%s pending=%s open=%s unfilled=%s TradePriceOpen=%.5f TradePriceClose=%.5f TradeFill=%.5f TradeProfit=%.5f",
		strdate("%H:%M:%S.", NOW),
		TradeID,
		ifelse(TradeIsLong, "buy", "sell"),
		ifelse(TradeIsMissed, "yes", "no"),
		ifelse(TradeIsPending, "yes", "no"),
		ifelse(TradeIsOpen, "yes", "no"),
		ifelse(TradeIsUnfilled, "yes", "no"),
		TradePriceOpen,
		TradePriceClose,
		TradeFill,
		TradeProfit
	);

	return 0;

}

function run() {

	set(TICKS + LOGFILE + PLOTNOW + PRELOAD);
	resf(BarMode, BR_WEEKEND);
	setf(BarMode, BR_FLAT);
	Verbose = 7 + DIAG + ALERT;
	Hedge = 2;

	asset("EUR/USD");

	BarPeriod = 1;
	LookBack = 60;

	Capital = 10000;

	setf(TradeMode, TR_GTC);

	if (is(INITRUN)) {
		startTime = timer();
		printf("\nInitRun: Asset=%s, PIP=%.5f", Asset, PIP);
	}

	var Close = priceClose();
	var DepthPIPs = 1;
	var TolPIPs = 1;
	var TopAsk = Close;
	var TopBid = Close - Spread;
	var LimitAsk = round_up(TopAsk + DepthPIPs * PIP, PIP);
	var LimitBid = round_down(TopBid - DepthPIPs * PIP, PIP);

	if (!is(LOOKBACK)) {
		var Position = brokerCommand(GET_POSITION, Asset);
		printf("\n======> %s bar at %s, limit ask=%.5f top ask=%.5f top bid=%.5f limit bid=%.5f\n  Position %.2f, BidQuote %.5f, AskQuote %.5f",
			Asset,
			strdate(HMS, 0),
			LimitAsk,
			TopAsk,
			TopBid,
			LimitBid,
			Position,
			BidQuote,
			AskQuote
		);
	}

	MaxLong = 10;
	MaxShort = 10;
	if (!is(LOOKBACK)) {
		brokerCommand(SET_ORDERTYPE, ORDERTYPE_GTC);
		Lots = 5;

		var BidTol = abs(LimitBid - BidQuote);
		var AskTol = abs(LimitAsk - AskQuote);
		printf("\n======> quoting tol bid=%.5f ask=%.5f", BidTol, AskTol);

		if (AskTol > TolPIPs * PIP) {
			if (AskTrade) {
				printf("\n======> cancelling ask quote %d", AskTrade->nID);
				brokerCommand(DO_CANCEL, AskTrade->nID);
			}
			OrderLimit = LimitAsk;
			AskTrade = enterShort(tmf);
			printf("\n======> enterShort: OrderLimit=%.5f prev quote=%.5f", OrderLimit, AskQuote);
			AskQuote = LimitAsk;
			OrderLimit = 0;
		}

		if (BidTol > TolPIPs * PIP) {
			if (BidTrade) {
				printf("\n======> cancelling ask quote %d", BidTrade->nID);
				brokerCommand(DO_CANCEL, BidTrade->nID);
			}
			OrderLimit = LimitBid;
			BidTrade = enterLong(tmf);
			printf("\n======> enterLong: OrderLimit=%.5f prev quote=%.5f", OrderLimit, BidQuote);
			BidQuote = LimitBid;
			OrderLimit = 0;
		}
	}

	for (open_trades)
	{
		printf("\n+++++ %d %s %s TradeLots(open)=%.6f TradeLotsTarget=%.6f TradePriceOpen=%.6f, TradeProfit=%.6f, TradeEntryLimit=%.6f",
			TradeID,
			ifelse(TradeIsOpen, "open", "pending"),
			ifelse(TradeIsLong, "buy", "sell"),
			(var)TradeLots,
			(var)TradeLotsTarget,
			(var)TradePriceOpen,
			(var)TradeProfit,
			(var)TradeEntryLimit);
	}

	for (closed_trades)
	{
		printf("\n----- %d %s %d TradePriceOpen=%.6f TradeProfit=%.6f TradeEntryLimit=%.6f TradePriceOpen=%.6f TradePriceClose=%.6f",
			TradeID,
			ifelse(TradeIsClosed, "closed", "unknown"),
			ifelse(TradeIsLong, 1, -1),
			(var)TradePriceOpen,
			(var)TradeProfit,
			(var)TradeEntryLimit,
			(var)TradePriceClose);
	}
}