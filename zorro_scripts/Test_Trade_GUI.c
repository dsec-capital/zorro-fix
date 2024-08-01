// Test Trade GUI 
#include <zorro.h>

#include "zorro_fxcm_fix_include.h"

#define MAXLOTS	100	
#define POINT 0.1*PIP

#define AssetPanelOffset 2
#define TradePanelOffset 6

bool LogTopOfBook = true;
bool CancelWithBrokerCmd = true;
bool CancelAll = true;
int LimitDepth;
int OrderMode; 					// 0 Market, 1 Limit 
int NextTradeIdx = 0;
var RoundingStep = PIP;
bool AbsLimitLevel = false;
int AbsLimitLevelRow;
int AbsLimitLevelCol;
int PartCancelAmountRow;
int PartCancelAmountCol;
int PositionRow;
int PositionCol;
int OrderActionCol;
int PositionActionCol;

GetOrderPositionIdArg order_pos_arg;
GetOrderMassStatusArg order_mass_status_arg;
GetPositionReportArg position_report_arg;
CancelReplaceArg cancel_replace_arg;

void setupPanel() {
	int n = 0;
	panel(20, 17, GREY, 82);
	panelSet(0, n++, "Buy", YELLOW, 1, 4);
	panelSet(0, n++, "Sell", YELLOW, 1, 4);
	panelSet(0, n++, ifelse(OrderMode == 0, "Market Order", "Limit Order"), YELLOW, 1, 4);
	panelSet(0, n++, "Cancel All", YELLOW, 1, 4);
	panelSet(0, n++, ifelse(RoundingStep == PIP, "Round[PIP]", "Round[PNT]"), YELLOW, 1, 4);
	panelSet(0, n++, "Get Pos", YELLOW, 1, 4);
	panelSet(0, n++, "OrdTracker[P]", YELLOW, 1, 4);
	panelSet(0, n++, "OpenPos[P]", YELLOW, 1, 4);
	panelSet(0, n++, "ClosedPos[P]", YELLOW, 1, 4);
	panelSet(0, n++, "Collat[P]", YELLOW, 1, 4);
	panelSet(0, n++, "OrdStatus[P]", YELLOW, 1, 4);
	panelSet(0, n, ifelse(AbsLimitLevel, "Lmt[ABS]", "Lmt[REL]"), YELLOW, 1, 4);
	panelSet(1, n, "0", 0, 1, 2);
	AbsLimitLevelRow = 1;
	AbsLimitLevelCol = n;
	n++;
	panelSet(0, n, ifelse(CancelAll, "CancelAll", "CancelRep"), YELLOW, 1, 4);
	panelSet(1, n, "0", 0, 1, 2);
	PartCancelAmountRow = 1;
	PartCancelAmountCol = n;
	n++;
	panelSet(0, n, ifelse(CancelWithBrokerCmd, "Cancel[BRK]", "Cancel[EXIT]"), YELLOW, 1, 4);
	n++;
	panelSet(0, n, ifelse(LogTopOfBook, "LogTOB[ON]", "LogTOB[OFF]"), YELLOW, 1, 4);
	n++;

	int c = 0;
	panelSet(TradePanelOffset - 1, c++, "Trade", ColorPanel[3], 1, 1);
	panelSet(TradePanelOffset - 1, c++, "ID", ColorPanel[3], 1, 1);
	panelSet(TradePanelOffset - 1, c, "O Action", ColorPanel[3], 1, 1);
	OrderActionCol = c;
	c++;
	panelSet(TradePanelOffset - 1, c, "P Action", ColorPanel[3], 1, 1);
	PositionActionCol = c;
	c++;
	panelSet(TradePanelOffset - 1, c++, "PositionId", ColorPanel[3], 1, 1);
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
	panelSet(TradePanelOffset - 1, c++, "IsClosed", ColorPanel[3], 1, 1);
	panelSet(TradePanelOffset - 1, c++, "IsPending", ColorPanel[3], 1, 1);

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

	if (LogTopOfBook)
		printf("\ntmf %s bid=%.5f ask=%.5f spread=%.2f", strtr(ThisTrade), TopBid, TopAsk, Spread / PIP);

	var DistToFarTouch = 0;
	if (LimitPrice > 0) {
		if (TradeIsLong)
			DistToFarTouch = (TopAsk - LimitPrice) / PIP;
		else
			DistToFarTouch = (LimitPrice - TopBid) / PIP;
	}

	bool Cancelable = TradeLots < TradeLotsTarget && (TradeIsOpen || TradeIsPending);
	bool CancelledUnfilled = TradeLots < TradeLotsTarget && !TradeIsOpen;
	bool Filled = TradeLots == TradeLotsTarget;

	string Dir = ifelse(LimitPrice == 0, ifelse(TradeDir == 1, "Buy", "Sell"), ifelse(TradeDir == 1, "Bid", "Ask"));

	string CurrentStatus = panelGet(row, OrderActionCol);
	if (CurrentStatus == "Failed")
		return 0;

	order_pos_arg.trade_id = TradeID;
	brokerCommand(BROKER_CMD_GET_ORDER_POSITION_ID, (void*)&order_pos_arg);

	bool Closable = TradeLots > 0 && order_pos_arg.has_open_position;

	int c = 0;
	panelSet(row, c++, strtr(ThisTrade), ColorPanel[2], 1, 4);
	panelSet(row, c++, sftoa((var)TradeID, 0), ColorPanel[2], 1, 1);
	if (TradeID == 0)  // some strange 
		panelSet(row, c, "Faild", ORANGE, 1, 1);	// failed or rejected
	else if (LimitPrice == 0)
		panelSet(row, c, "Filled", ORANGE, 1, 1);	// market order
	else if (Filled)
		panelSet(row, c, "Filled", ORANGE, 1, 1);	// filled limit order
	else if (Cancelable)
		panelSet(row, c, "Cancel", YELLOW, 2, 4);	// limit order that is new or partially filled
	else if (CancelledUnfilled)
		panelSet(row, c, "Cancelled", OLIVE, 1, 1);	// this is not clear how to properly detect
	else
		panelSet(row, c, "Unknown", OLIVE, 1, 1);	// more cases ?
	++c;
	if (Closable)
		panelSet(row, c, "Close", YELLOW, 2, 4);
	else
		panelSet(row, c, "No Position", OLIVE, 1, 1);
	++c;
	panelSet(row, c++, ifelse(order_pos_arg.has_open_position, order_pos_arg.position_id, "no position"), ColorPanel[2], 1, 1);
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
	panelSet(row, c++, ifelse(TradeIsClosed, "true", "false"), ColorPanel[2], 1, 1);
	panelSet(row, c++, ifelse(TradeIsPending, "true", "false"), ColorPanel[2], 1, 1);

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
			TRADE* trade = enterLong(tmf, (var)NextTradeIdx, OrderLimit);
			if (!trade) {
				panelSet(TradePanelOffset + NextTradeIdx, OrderActionCol, "Failed", ORANGE, 1, 1);
			}
			NextTradeIdx++;
			OrderLimit = 0;
		}
		else {
			printf("\n==> Market Buy at top %.5f", TopBid);
			TRADE* trade = enterLong(tmf, (var)NextTradeIdx, 0);
			if (!trade) {
				panelSet(TradePanelOffset + NextTradeIdx, OrderActionCol, "Failed", ORANGE, 1, 1);
			}
			NextTradeIdx++;
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
			TRADE* trade = enterShort(tmf, (var)NextTradeIdx, OrderLimit);
			if (!trade) {
				panelSet(TradePanelOffset + NextTradeIdx, OrderActionCol, "Failed", ORANGE, 1, 1);
			}
			NextTradeIdx++;
			OrderLimit = 0;
		}
		else {
			printf("\n==> Market Sell at top %.5f", TopAsk);
			TRADE* trade = enterShort(tmf, (var)NextTradeIdx, 0);
			if (!trade) {
				panelSet(TradePanelOffset + NextTradeIdx, OrderActionCol, "Failed", ORANGE, 1, 1);
			}
			NextTradeIdx++;
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
				bool Cancelable = TradeLots < TradeLotsTarget && (TradeIsOpen || TradeIsPending);
				int new_lots_gui = atoi(panelGet(PartCancelAmountRow, PartCancelAmountCol));
				int new_lots = ifelse(CancelAll, 0, new_lots_gui);
				printf("\n***** going to cancel %s | %i to new_lots=%i", strtr(ThisTrade), ThisTrade->nID, new_lots);

				if (Cancelable) {
					if (CancelWithBrokerCmd) {
						cancel_replace_arg.trade_id = ThisTrade->nID;
						cancel_replace_arg.amount = new_lots;
						printf("\ncancel with brokerCommand");
						int res = brokerCommand(DO_CANCEL, (void*)&cancel_replace_arg);
						if (res) {
							if (new_lots > 0) {
								ThisTrade->nLotsTarget = new_lots;
								printf("\npartial cancel - update nLotsTarget to new lot %i", new_lots);
							}
							else {
								cancelTrade(ThisTrade->nID);
							}
							panelSet(row, col, "Cancelled", OLIVE, 1, 1);
						}
						else {
							printf("\nDO_CANCEL failed");
							panelSet(row, col, "Failed", OLIVE, 1, 1);
						}
					}
					else {
						printf("\ncancel with exit trade - Zorro limitation: only full cancel supported - cancel full");
						// int res = exitTrade(ThisTrade, 0, new_lots); // Zorro bug, new_lots is not passed to BrokerSell2
						int res = exitTrade(ThisTrade);
						if (res) {
							panelSet(row, col, "Cancelled", OLIVE, 1, 1);
						}
						else {
							panelSet(row, col, "Failed", OLIVE, 1, 1);
						}
					}
				}
				else {
					printf("\ntrade not cancelable");
				}
			}
		}
	}
	else if (Text == "Close") {
		int id = atoi(panelGet(row, 1));
		for (open_trades) {
			if (TradeID == id) {
				printf("\n***** %s found trade - going to close", strtr(ThisTrade));

				bool Closable = TradeLots > 0 && TradeIsOpen;
				if (Closable) {
					exitTrade(ThisTrade);
					panelSet(row, col, "Closed", OLIVE, 1, 1);
				}
				else {
					printf("\n  trade not closable");
				}
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
	else if (Text == "CancelRep") {
		panelSet(row, col, "CancelAll", 0, 0, 0);
		CancelAll = true;
		printf("\nfcancel call");
	}
	else if (Text == "CancelAll") {
		panelSet(row, col, "CancelRep", 0, 0, 0);
		CancelAll = false;
		printf("\ncancel/replace");
	}
	else if (Text == "Cancel[BRK]") {
		panelSet(row, col, "Cancel[EXIT]", 0, 0, 0);
		CancelWithBrokerCmd = false;
		printf("\ncancel with exit trade");
	}
	else if (Text == "Cancel[EXIT]") {
		panelSet(row, col, "Cancel[BRK]", 0, 0, 0);
		CancelWithBrokerCmd = true;
		printf("\ncancel with broker cmd");
	}
	else if (Text == "Get Pos") {
		var pos = brokerCommand(GET_POSITION, Asset);
		panelSet(PositionRow, PositionCol, sftoa(pos, 2), ColorPanel[2], 1, 1);
		printf("\nobtained position %.2f", pos);
	}
	else if (Text == "OrdTracker[P]") {
		brokerCommand(BROKER_CMD_PRINT_ORDER_TRACKER, 0);
	}
	else if (Text == "OpenPos[P]") {
		brokerCommand(BROKER_CMD_GET_OPEN_POSITION_REPORTS, (void*)&position_report_arg);
		brokerCommand(BROKER_CMD_PRINT_POSIITION_REPORTS, 1);
	}
	else if (Text == "ClosedPos[P]") {
		brokerCommand(BROKER_CMD_GET_CLOSED_POSITION_REPORTS, (void*)&position_report_arg);
		brokerCommand(BROKER_CMD_PRINT_POSIITION_REPORTS, 2);
	}
	else if (Text == "Collat[P]") {
		brokerCommand(BROKER_CMD_PRINT_COLLATERAL_REPORTS, 0);
	}
	else if (Text == "OrdStatus[P]") {
		brokerCommand(BROKER_CMD_GET_ORDER_MASS_STATUS, (void*)&order_mass_status_arg);
	}
	else if (Text == "LogTOB[ON]") {
		panelSet(row, col, "LogTOB[OFF]", 0, 0, 0);
		LogTopOfBook = false;
		printf("\nlog top of book off");
	}
	else if (Text == "LogTOB[OFF]") {
		panelSet(row, col, "LogTOB[ON]", 0, 0, 0);
		LogTopOfBook = true;
		printf("\nlog top of book on");
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
		LogTopOfBook = true;
		CancelWithBrokerCmd = true;
		CancelAll = true;
		OrderMode = 1;
		LimitDepth = 10;
		RoundingStep = PIP;
		AbsLimitLevel = false;
		set(NFA | OFF);

		order_mass_status_arg.print = 1;
	}

	set(TICKS + LOGFILE + PLOTNOW + PRELOAD);
	resf(BarMode, BR_WEEKEND);
	setf(BarMode, BR_FLAT);
	setf(TradeMode, TR_GTC);
	setf(TradeMode, TR_FRC);
	resf(TradeMode, TR_FILLED);  // do not want rejected orders to be treated filled? not sure what the effect is

	Verbose = 7 + DIAG + ALERT;
	Hedge = 2;
	LookBack = 0;

	BarPeriod = 1;
	PlotPeriod = 5;
	NumYears = 1;

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
		setupPanel();
	}

	if (!is(LOOKBACK)) {
		printf("\nis NFA %s, Hedge %i, Balance %s, Equity %s, Margin %s, ClosePx %s",
			ifelse(is(NFA), "true", "false"), Hedge,
			sftoa(Balance, 2), sftoa(Equity, 2), sftoa(MarginVal, 2),
			sftoa(priceClose(0), 5));

		var pos = brokerCommand(GET_POSITION, SymbolTrade);
		if (pos != 0) {
			printf(" Pos %.2f", pos);
			panelSet(PositionRow, PositionCol, sftoa(pos, 2), ColorPanel[2], 1, 1);
		}
		for (open_trades) {
			if (TradeIsPending)
				printf("\n***** %s still pending", strtr(ThisTrade));
			else
				printf("\n***** %s Lots: %i Target: %i", strtr(ThisTrade), TradeLots, TradeLotsTarget);
		}
	}

	if (once(!is(LOOKBACK))) {
		printf("\n%s: PIP %s, PIPCost %s, Mult %.2f", Asset, sftoa(PIP, 2), sftoa(PIPCost, 2), LotAmount);
		printf("\n%s: Leverage %.0f, MarginCost %s", Asset, Leverage, sftoa(MarginCost, 2));
		printf("\n%s: Roll+ %f, Roll- %f", Asset, RollLong, RollShort);
	}
}