#include "Test_Common.h"

static var startTime;
static bool Quoting = false;
static bool Inventory = false;
TRADE* BidTrade;
TRADE* AskTrade;

void tick() 
{
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

function run() 
{
	set(TICKS + LOGFILE + PLOTNOW + PRELOAD);
	resf(BarMode, BR_WEEKEND);
	setf(BarMode, BR_FLAT);
	Verbose = 7 + DIAG + ALERT;
	Hedge = 2;

	BarPeriod = 1;
	LookBack = 300;

	asset("EUR/USD");

	Capital = 10000;

	setf(TradeMode, TR_GTC);

	if (is(INITRUN)) {
		startTime = timer();
		brokerCommand(BROKER_CMD_CREATE_SECURITY_INFO_FILE, "Log/security_infos.csv");
		brokerCommand(BROKER_CMD_GET_CLOSED_POSITIONS, "Log/positions_closed.csv");
	}

	var Close = priceClose();
	var DepthPIPs = 2;
	var TopAsk = Close;
	var TopBid = Close - Spread;
	var LimitAsk = round_up(TopAsk + DepthPIPs * PIP, PIP);
	var LimitBid = round_down(TopBid - DepthPIPs * PIP, PIP);

	if (!is(LOOKBACK)) {
		var Position = brokerCommand(GET_POSITION, Asset);
		printf("\n======> %s bar at %s, limit ask=%.5f top ask=%.5f top bid=%.5f limit bid=%.5f\n  Position %.2f, LotsPool %.2f",
			Asset,
			strdate(HMS, 0),
			LimitAsk,
			TopAsk,
			TopBid,
			LimitBid,
			Position,
			(var)LotsPool
		);
	}

	MaxLong = 10;
	MaxShort = 10;
	if (!is(LOOKBACK) && !Quoting) {
		brokerCommand(SET_ORDERTYPE, ORDERTYPE_GTC);
		Lots = 5;
		OrderLimit = LimitAsk;
		enterShort(diagnostics_tmf);
		printf("\nenterShort: OrderLimit=%.5f", OrderLimit);

		OrderLimit = LimitBid;
		enterLong(diagnostics_tmf);
		printf("\nenterLong: OrderLimit=%.5f", OrderLimit);

		Quoting = true;
		OrderLimit = 0;
	}

	if (!is(LOOKBACK) && !Inventory) {
		Lots = 2;
		enterLong(diagnostics_tmf);

		int np = brokerCommand(BROKER_CMD_GET_OPEN_POSITIONS, "Log/positions_open.csv");

		Inventory = true;
	}

	diagnostics_trades();
}