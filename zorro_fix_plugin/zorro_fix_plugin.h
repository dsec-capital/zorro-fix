#pragma once

#include <windows.h>   

typedef double DATE;
#include "zorro/trading.h"	       

#define DLLFUNC extern __declspec(dllexport)
#define DLLFUNC_C extern "C" __declspec(dllexport)

namespace zfix
{
	int(__cdecl* BrokerError)(const char* txt);
	int(__cdecl* BrokerProgress)(const int percent);
	int(__cdecl* http_send)(char* url, char* data, char* header);
	long(__cdecl* http_status)(int id);
	long(__cdecl* http_result)(int id, char* content, long size);
	void(__cdecl* http_free)(int id);

	// zorro functions
	DLLFUNC_C int BrokerOpen(char* Name, FARPROC fpError, FARPROC fpProgress);
	DLLFUNC_C void BrokerHTTP(FARPROC fpSend, FARPROC fpStatus, FARPROC fpResult, FARPROC fpFree);
	DLLFUNC_C int BrokerLogin(char* User, char* Pwd, char* Type, char* Account);
	DLLFUNC_C int BrokerTime(DATE* pTimeGMT);
	DLLFUNC_C int BrokerAsset(char* Asset, double* pPrice, double* pSpread, double* pVolume, double* pPip, double* pPipCost, double* pLotAmount, double* pMarginCost, double* pRollLong, double* pRollShort);
	DLLFUNC_C int BrokerHistory2(char* Asset, DATE tStart, DATE tEnd, int nTickMinutes, int nTicks, T6* ticks);
	DLLFUNC_C int BrokerBuy2(char* Asset, int nAmount, double dStopDist, double dLimit, double* pPrice, int* pFill);
	DLLFUNC_C int BrokerTrade(int nTradeID, double* pOpen, double* pClose, double* pCost, double* pProfit);
	DLLFUNC_C int BrokerSell2(int nTradeID, int nAmount, double Limit, double* pClose, double* pCost, double* pProfit, int* pFill);
	DLLFUNC_C double BrokerCommand(int Command, DWORD dwParameter);

	inline std::string broker_command_string(int command) {
		switch (command)
		{
			case GET_COMPLIANCE:
				return "GET_COMPLIANCE";
			case GET_BROKERZONE:
				return "GET_BROKERZONE";    
			case GET_MAXTICKS:
				return "GET_MAXTICKS";
			case GET_MAXREQUESTS:
				return "GET_MAXREQUESTS";
			case GET_LOCK:
				return "GET_LOCK";
			case GET_POSITION: 
				return "GET_POSITION";
			case GET_NTRADES:
				return "GET_NTRADES";
			case GET_AVGENTRY:
				return "GET_AVGENTRY";
			case DO_CANCEL:
				return "DO_CANCEL";
			case SET_ORDERTEXT:
				return "SET_ORDERTEXT";
			case SET_SYMBOL: 
				return "SET_SYMBOL";
			case SET_MULTIPLIER: 
				return "SET_MULTIPLIER";					
			case SET_ORDERTYPE: 
				return "SET_ORDERTYPE";
			case GET_PRICETYPE:
				return "GET_PRICETYPE";
			case SET_PRICETYPE: 
				return "SET_PRICETYPE";
			case GET_VOLTYPE:
				return "GET_VOLTYPE";
			case SET_VOLTYPE:
				return "SET_VOLTYPE";
			case SET_AMOUNT:
				return "SET_AMOUNT";
			case SET_DIAGNOSTICS:
				return "SET_DIAGNOSTICS";
			case SET_HWND:
				return "SET_HWND";
			case GET_CALLBACK:
				return "GET_CALLBACK";
			case SET_CCY:
				return "SET_CCY";
			case GET_HEARTBEAT:
				return "GET_HEARTBEAT";
			case SET_LEVERAGE: 
				return "SET_LEVERAGE";
			case SET_LIMIT: 
				return "SET_LIMIT";
			case SET_FUNCTIONS:
				return "SET_FUNCTIONS";
			case GET_EXCHANGES:
				return "GET_EXCHANGES";
			case SET_EXCHANGES:
				return "SET_EXCHANGES";
			default:
				return "Unknow broker command";
		}
	}
}
