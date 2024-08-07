#pragma once

#include "zorro_common/zorro.h"

#include "zorro_fxcm_fix_include.h"

namespace zorro
{
	DLLFUNC_C int BrokerOpen(char* Name, FARPROC fpError, FARPROC fpProgress);
	DLLFUNC_C void BrokerHTTP(FARPROC fpSend, FARPROC fpStatus, FARPROC fpResult, FARPROC fpFree);
	DLLFUNC_C int BrokerLogin(char* User, char* Pwd, char* Type, char* Account);
	DLLFUNC_C int BrokerTime(DATE* pTimeGMT);
	DLLFUNC_C int BrokerAsset(char* Asset, double* pPrice, double* pSpread, double* pVolume, double* pPip, double* pPipCost, double* pLotAmount, double* pMarginCost, double* pRollLong, double* pRollShort);
	DLLFUNC_C int BrokerHistory2(char* Asset, DATE tStart, DATE tEnd, int nTickMinutes, int nTicks, T6* ticks);
	DLLFUNC_C int BrokerBuy2(char* Asset, int nAmount, double dStopDist, double dLimit, double* pPrice, int* pFill);
	DLLFUNC_C int BrokerTrade(int nTradeID, double* pOpen, double* pClose, double* pCost, double* pProfit);
	DLLFUNC_C int BrokerSell2(int nTradeID, int nAmount, double Limit, double* pClose, double* pCost, double* pProfit, int* pFill);
	DLLFUNC_C double BrokerCommand(int Command, intptr_t dwParameter);

	int(__cdecl* BrokerError)(const char* txt);
	int(__cdecl* BrokerProgress)(const int percent);
	int(__cdecl* http_send)(char* url, char* data, char* header);
	long(__cdecl* http_status)(int id);
	long(__cdecl* http_result)(int id, char* content, long size);
	void(__cdecl* http_free)(int id);
}
