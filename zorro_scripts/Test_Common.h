#include <stdio.h>
#include <profile.c>

#define ORDERTYPE_GTC 2

#define BROKER_CMD_CREATE_ASSET_LIST_FILE 2000
#define BROKER_CMD_CREATE_SECURITY_INFO_FILE 2001
#define BROKER_CMD_GET_OPEN_POSITIONS 2002
#define BROKER_CMD_GET_CLOSED_POSITIONS 2002

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

void diagnostics_tmf() {
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

void diagnostics_trades() {
	for (open_trades)
	{
		printf("\n+++++ %d %s %s TradeLots(open)=%.6f TradeLotsTarget=%.6f TradePriceOpen=%.6f TradeProfit=%.6f",
			TradeID,
			ifelse(TradeIsOpen, "open", "pending"),
			ifelse(TradeIsLong, "buy", "sell"),
			(var)TradeLots,
			(var)TradeLotsTarget,
			(var)TradePriceOpen,
			(var)TradeProfit);
	}

	for (closed_trades)
	{
		printf("\n+++++ %d %s %s TradeLots(open)=%.6f TradeLotsTarget=%.6f TradePriceOpen=%.6f TradePriceClose=%.6f TradeProfit=%.6f",
			TradeID,
			ifelse(TradeIsOpen, "closed", "pending"),
			ifelse(TradeIsLong, "buy", "sell"),
			(var)TradeLots,
			(var)TradeLotsTarget,
			(var)TradePriceOpen,
			(var)TradePriceClose,
			(var)TradeProfit);
	}
}
