// Test Trade GUI 

#define MAXLOTS	100	
#define POINT 0.1*PIP

#define BROKER_CMD_CREATE_ASSET_LIST_FILE		2000
#define BROKER_CMD_CREATE_SECURITY_INFO_FILE	2001
#define BROKER_CMD_GET_OPEN_POSITIONS			2002
#define BROKER_CMD_GET_CLOSED_POSITIONS			2003
#define BROKER_CMD_PRINT_ORDER_TRACKER			2010
#define BROKER_CMD_GET_ORDER_TRACKER_SIZE		2011
#define BROKER_CMD_GET_ORDER_ORDER_MASS_STATUS  2012

#define AssetPanelOffset 2
#define TradePanelOffset 6

#define CANCEL_WITH_BROKER_COMMAND

bool PrintOrderStatus = true;
int LimitDepth;
int OrderMode; 					// 0 Market, 1 Limit 
int NextTradeIdx = 0;
var RoundingStep = PIP;
bool AbsLimitLevel = false;
int AbsLimitLevelRow;
int AbsLimitLevelCol;
int PositionRow;
int PositionCol;

void setupPannel() {
	int n = 0;
	panel(20, 16, GREY, 80);
	panelSet(0, n++, "Buy", YELLOW, 1, 4);
	panelSet(0, n++, "Sell", YELLOW, 1, 4);
	panelSet(0, n++, ifelse(OrderMode == 0, "Market Order", "Limit Order"), YELLOW, 1, 4);
	panelSet(0, n++, "Cancel All", YELLOW, 1, 4);
	panelSet(0, n++, ifelse(RoundingStep == PIP, "Round[PIP]", "Round[PNT]"), YELLOW, 1, 4);
	panelSet(0, n++, "Get Pos", YELLOW, 1, 4);
	panelSet(0, n++, "Print OTrk", YELLOW, 1, 4);
	panelSet(0, n++, "Print OPos", YELLOW, 1, 4);
	panelSet(0, n++, "Print CPos", YELLOW, 1, 4);
	panelSet(0, n++, "Print OStat", YELLOW, 1, 4);
	panelSet(0, n, ifelse(AbsLimitLevel, "Lmt[ABS]", "Lmt[REL]"), YELLOW, 1, 4);
	panelSet(1, n, "0", 0, 1, 2);
	AbsLimitLevelRow = 1;
	AbsLimitLevelCol = n;
	n++;

	int c = 0;
	panelSet(TradePanelOffset - 1, c++, "Trade", ColorPanel[3], 1, 1);
	panelSet(TradePanelOffset - 1, c++, "ID", ColorPanel[3], 1, 1);
	panelSet(TradePanelOffset - 1, c++, "Action", ColorPanel[3], 1, 1);
	panelSet(TradePanelOffset - 1, c++, "Direction", ColorPanel[3], 1, 1);
	panelSet(TradePanelOffset - 1, c++, "LimitPrice", ColorPanel[3], 1, 1);
	panelSet(TradePanelOffset - 1, c++, "DistFT[PIP]", ColorPanel[3], 1, 1);
	panelSet(TradePanelOffset - 1, c++, "Lots", ColorPanel[3], 1, 1);
	panelSet(TradePanelOffset - 1, c++, "LotsTarget", ColorPanel[3], 1, 1);
	panelSet(TradePanelOffset - 1, c++, "Profit", ColorPanel[3], 1, 1);
	panelSet(TradePanelOffset - 1, c++, "Commission", ColorPanel[3], 1, 1);
	panelSet(TradePanelOffset - 1, c++, "PriceOpen", ColorPanel[3], 1, 1);
	panelSet(TradePanelOffset - 1, c++, "PriceClose", ColorPanel[3], 1, 1);
	panelSet(TradePanelOffset - 1, c++, "IsOpen", ColorPanel[3], 1, 1);
	panelSet(TradePanelOffset - 1, c++, "IsPending", ColorPanel[3], 1, 1);
	panelSet(TradePanelOffset - 1, c++, "IsUnfilled", ColorPanel[3], 1, 1);
	panelSet(TradePanelOffset - 1, c++, "IsClosed", ColorPanel[3], 1, 1);

	int row = AssetPanelOffset;
	int c = 0;
	panelSet(row, c++, "Asset", ColorPanel[3], 1, 1);
	panelSet(row, c++, "TopBid", ColorPanel[3], 1, 1);
	panelSet(row, c++, "TopAsk", ColorPanel[3], 1, 1);
	panelSet(row, c++, "Spread[PIP]", ColorPanel[3], 1, 1);
	panelSet(row, c, "Position", ColorPanel[3], 1, 1);
	PositionRow = row + 1;
	PositionCol = c;
	c++;
}

int tmf(var TradeIdx, var LimitPrice) {
	int row = TradePanelOffset + (int)TradeIdx;

	var TopAsk = priceClose();
	var TopBid = TopAsk - Spread;

	printf("\ntmf %s bid=%.5f ask=%.5f spread=%.2f", strtr(ThisTrade), TopBid, TopAsk, Spread / PIP);

	var DistToFarTouch = 0;
	if (LimitPrice > 0) {
		if (TradeIsLong && TradeIsUnfilled)
			DistToFarTouch = (TopAsk - LimitPrice) / PIP;
		else
			DistToFarTouch = (LimitPrice - TopBid) / PIP;
	}

	bool Cancelable = TradeLots < TradeLotsTarget && TradeIsUnfilled;
	bool CancelledUnfilled = TradeLots <= TradeLotsTarget && TradeIsUnfilled;
	bool Filled = TradeLots == TradeLotsTarget && !TradeIsUnfilled;

	string Dir = ifelse(LimitPrice == 0, ifelse(TradeDir == 1, "Buy", "Sell"), ifelse(TradeDir == 1, "Bid", "Ask"));

	int c = 0;
	panelSet(row, c++, strtr(ThisTrade), ColorPanel[2], 1, 4);
	panelSet(row, c++, sftoa((var)TradeID, 0), ColorPanel[2], 1, 1);
	if (LimitPrice == 0)
		panelSet(row, c, "Filled", ORANGE, 2, 4);
	else if (Filled)
		panelSet(row, c, "Filled", ORANGE, 2, 4);
	else if (Cancelable)
		panelSet(row, c, "Cancel", YELLOW, 2, 4);
	else
		panelSet(row, c, "Cancelled", OLIVE, 2, 4);
	++c;
	panelSet(row, c++, Dir, ColorPanel[2], 1, 1);
	panelSet(row, c++, sftoa(LimitPrice, 5), ColorPanel[2], 1, 1);
	panelSet(row, c++, sftoa(DistToFarTouch, 2), ColorPanel[2], 1, 1);
	panelSet(row, c++, sftoa(TradeLots, 0), ColorPanel[2], 1, 1);
	panelSet(row, c++, sftoa(TradeLotsTarget, 0), ColorPanel[2], 1, 1);
	panelSet(row, c++, sftoa(TradeProfit, 5), ColorPanel[2], 1, 1);
	panelSet(row, c++, sftoa(TradeCommission, 4), ColorPanel[2], 1, 1);
	panelSet(row, c++, sftoa(TradePriceOpen, 5), ColorPanel[2], 1, 1);
	panelSet(row, c++, sftoa(TradePriceClose, 5), ColorPanel[2], 1, 1);
	panelSet(row, c++, ifelse(TradeIsOpen, "true", "false"), ColorPanel[2], 1, 1);
	panelSet(row, c++, ifelse(TradeIsPending, "true", "false"), ColorPanel[2], 1, 1);
	panelSet(row, c++, ifelse(TradeIsUnfilled, "true", "false"), ColorPanel[2], 1, 1);
	panelSet(row, c++, ifelse(TradeIsClosed, "true", "false"), ColorPanel[2], 1, 1);

	return 0;
}

void tick()
{
	if (is(LOOKBACK)) return;
	var TopAsk = priceClose();
	var TopBid = TopAsk - Spread;

	int row = AssetPanelOffset + 1;
	int c = 0;
	panelSet(row, c++, Asset, ColorPanel[2], 1, 1);
	panelSet(row, c++, sftoa(TopBid, 5), ColorPanel[2], 1, 1);
	panelSet(row, c++, sftoa(TopAsk, 5), ColorPanel[2], 1, 1);
	panelSet(row, c++, sftoa(Spread / PIP, 2), ColorPanel[2], 1, 1);
}

void doTrade(int What)
{
	Lots = slider(1);
	LimitDepth = slider(2);

	if (What == 1) { // Buy
		var Close = priceClose();
		var TopBid = Close - Spread;

		if (OrderMode == 1) {
			var limit = ifelse(AbsLimitLevel,
				atof(panelGet(AbsLimitLevelRow, AbsLimitLevelCol)),
				TopBid - (var)LimitDepth * POINT
			);
			//OrderLimit = round_down(limit, RoundingStep);
			OrderLimit = roundto(limit - 0.5 * RoundingStep, RoundingStep);
			printf("\n==> Limit Buy at OrderLimit=%.5f [%s] from top %.5f limit depth %i", OrderLimit, ifelse(AbsLimitLevel, "ABS", "REL"), TopBid, LimitDepth);
			enterLong(tmf, (var)NextTradeIdx++, OrderLimit);
			OrderLimit = 0;
		}
		else {
			printf("\n==> Market Buy at top %.5f", TopBid);
			enterLong(tmf, (var)NextTradeIdx++, 0);
		}
	}
	else if (What == 2) { // Sell 		
		var Close = priceClose();
		var TopAsk = Close;

		if (OrderMode == 1) {
			var limit = ifelse(AbsLimitLevel,
				atof(panelGet(AbsLimitLevelRow, AbsLimitLevelCol)),
				TopAsk + (var)LimitDepth * POINT
			);
			//OrderLimit = round_up(limit, RoundingStep);
			OrderLimit = roundto(limit + 0.5 * RoundingStep, RoundingStep);
			printf("\n==> Limit Sell at OrderLimit=%.5f from top %.5f limit depth %i", OrderLimit, TopAsk, LimitDepth);
			enterShort(tmf, (var)NextTradeIdx++, OrderLimit);
			OrderLimit = 0;
		}
		else {
			printf("\n==> Market Sell at top %.5f", TopAsk);
			enterShort(tmf, (var)NextTradeIdx++, 0);
		}
	}
}

void click(int row, int col)
{
	if (!is(RUNNING)) return; // only clickable when session is active
	sound("click.wav");

	if (row == -3) { // Asset Box
		panelSet(4, 0, AssetBox);
		asset(AssetBox);
		return;
	}

	string Text = panelGet(row, col);
	printf("\nclicked cell (%d,%d)=%s", row, col, Text);

	if (Text == "Cancel") {
		int id = atoi(panelGet(row, 1));
		for (open_trades) {
			if (TradeID == id) {
				printf("\n%s found trade - going to cancel", strtr(ThisTrade));

				if (TradeIsUnfilled) {
#ifdef CANCEL_WITH_BROKER_COMMAND
					brokerCommand(DO_CANCEL, ThisTrade->nID);
					cancelTrade(ThisTrade->nID);
#else				
					exitTrade(ThisTrade);
#endif				
				}

				panelSet(row, col, "Cancelled", OLIVE, 1, 1);
			}
		}
	}
	else if (Text == "Cancel All") {
		exitLong("*");
		exitShort("*");
	}
	else if (Text == "Market Order") {
		panelSet(row, col, "Limit Order", 0, 0, 0);
		OrderMode = 1;
		printf("\nusing limit orders");
	}
	else if (Text == "Limit Order") {
		panelSet(row, col, "Market Order", 0, 0, 0);
		OrderMode = 0;
		printf("\nusing market orders");
	}
	else if (Text == "Round[PIP]") {
		panelSet(row, col, "Round[PNT]", 0, 0, 0);
		RoundingStep = POINT;
		printf("\nround to points");
	}
	else if (Text == "Round[PNT]") {
		panelSet(row, col, "Round[PIP]", 0, 0, 0);
		RoundingStep = PIP;
		printf("\nround to pips");
	}
	else if (Text == "Lmt[REL]") {
		panelSet(row, col, "Lmt[ABS]", 0, 0, 0);
		AbsLimitLevel = true;
		printf("\nfix limit order level");
	}
	else if (Text == "Lmt[ABS]") {
		panelSet(row, col, "Lmt[REL]", 0, 0, 0);
		AbsLimitLevel = false;
		printf("\nfix limit order level");
	}
	else if (Text == "Get Pos") {
		var pos = brokerCommand(GET_POSITION, Asset);
		panelSet(PositionRow, PositionCol, sftoa(pos, 2), ColorPanel[2], 1, 1);
		printf("\nobtained position %.2f", pos);
	}
	else if (Text == "Print OTrk") {
		brokerCommand(BROKER_CMD_PRINT_ORDER_TRACKER, 0);
	}
	else if (Text == "Print OPos") {
		brokerCommand(BROKER_CMD_GET_OPEN_POSITIONS, 0);
	}
	else if (Text == "Print CPos") {
		brokerCommand(BROKER_CMD_GET_CLOSED_POSITIONS, 0);
	}
	else if (Text == "Print OStat") {
		brokerCommand(BROKER_CMD_GET_ORDER_ORDER_MASS_STATUS, 0);
	}
	else {
		if (Text == "Buy")
			call(1, doTrade, 1, 0);
		if (Text == "Sell")
			call(1, doTrade, 2, 0);
	}
}

function run()
{
	if (is(TESTMODE)) {
		quit("Click [Trade]!");
		return;
	}

	if (Init) {
		OrderMode = 1;
		LimitDepth = 10;
		RoundingStep = PIP;
		AbsLimitLevel = false;
		set(NFA | OFF);
	}

	set(TICKS + LOGFILE + PLOTNOW + PRELOAD);
	resf(BarMode, BR_WEEKEND);
	setf(BarMode, BR_FLAT);
	setf(TradeMode, TR_GTC);
	setf(TradeMode, TR_FRC);
	setf(TradeMode, TR_FILLED);
	//resf(TradeMode, TR_FILLED);

	Verbose = 7 + DIAG + ALERT;
	Hedge = 2;
	LookBack = 0;

	BarPeriod = 1;
	PlotPeriod = 5;
	NumYears = 1;
	//TradesPerBar = 1;	

	asset(Asset);

	if (Init) {
		//brokerCommand(SET_ORDERTYPE, 8); // activate stop orders
		//StopFactor = 1; // let broker observe the stops
	}

	if (Init) {
		SaveMode = 0;
		int N = brokerTrades(0);
		printf("\n%i account positions read", N);
	}

	Lots = slider(1, 1, 1, MAXLOTS, "Lots", 0);
	LimitDepth = slider(2, 10, -50, 200, "Limit", "Limit depth in Points from top");

	if (is(INITRUN)) {
		setupPannel();
	}

	if (!is(LOOKBACK)) {
		printf("\nis NFA %i, Hedge %i, Balance %s, Equite %s, Margin %s, ClosePx %s",
			ifelse(is(NFA), "true", "false"), Hedge,
			sftoa(Balance, 2), sftoa(Equity, 2), sftoa(MarginVal, 2),
			sftoa(priceClose(0), 5));

		var Pos = brokerCommand(GET_POSITION, SymbolTrade);
		if (Pos != 0)
			printf(" T %.2f", Pos);

		for (open_trades) {
			if (TradeIsPending)
				printf("\n***** %s still pending", strtr(ThisTrade));
			else
				printf("\n***** %s Lots: %i Target: %i", strtr(ThisTrade), TradeLots, TradeLotsTarget);
		}
	}

	if (once(!is(LOOKBACK))) {
		printf("\n%s: PIP %s, PIPCost %s, Mult %.2f", Asset, sftoa(PIP, 2), sftoa(PIPCost, 2), LotAmount);
		printf("\n%s: Levg %.0f, MCost %s", Asset, Leverage, sftoa(MarginCost, 2));
		printf("\n%s: Roll+ %f, Roll- %f", Asset, RollLong, RollShort);
	}
}