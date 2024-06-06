#include "Test_Common.h"

static var startTime;
static var AskQuote = 0;
static var BidQuote = 0;
static TRADE* BidTrade = 0;
static TRADE* AskTrade = 0;

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
		printf("\nInitRun: Asset=%s, PIP=%.5f", Asset, PIP);
		brokerCommand(BROKER_CMD_CREATE_SECURITY_INFO_FILE, "Log/security_infos.csv");
		brokerCommand(BROKER_CMD_GET_CLOSED_POSITIONS, "Log/positions_closed.csv");
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
			AskTrade = enterShort(diagnostics_tmf);
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
			BidTrade = enterLong(diagnostics_tmf);
			printf("\n======> enterLong: OrderLimit=%.5f prev quote=%.5f", OrderLimit, BidQuote);
			BidQuote = LimitBid;
			OrderLimit = 0;
		}
	}

	diagnostics_trades();
}