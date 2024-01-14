

#include "pch.h"
#include "framework.h"

#pragma warning(disable : 4996 4244 4312)

#define _CRT_SECURE_NO_DEPRECATE

#include <time.h>
#include <string>
#include <string>
#include <sstream>
#include <vector>
#include <memory>
#include <fstream>
#include <type_traits>
#include <unordered_map>

#include "zorro_fix_plugin.h"
#include "logger.h"

#define PLUGIN_VERSION	2
#define INITIAL_PRICE	1.0
#define INITIAL_ID		1000

namespace {


}

namespace zfix {

    DATE convertTime(__time32_t t32)
    {
        return (DATE)t32 / (24. * 60. * 60.) + 25569.; // 25569. = DATE(1.1.1970 00:00)
    }

    __time32_t convertTime(DATE date)
    {
        return (__time32_t)((date - 25569.) * 24. * 60. * 60.);
    }

    void showMsg(const char* text, const char* detail) {
        if (!BrokerError) return;
        if (!detail) detail = "";
        static char msg[1024];
        sprintf_s(msg, "%s %s", text, detail);
        BrokerError(msg);
    }

    DLLFUNC int BrokerOpen(char* Name, FARPROC fpError, FARPROC fpProgress) {
        if (Name) strcpy_s(Name, 32, "Zorro-Fix-Bridge");
        (FARPROC&)BrokerError = fpError;
        (FARPROC&)BrokerProgress = fpProgress;
        showMsg("BrokerOpen: Zorro-Fix-Bridge plugin opened");
        return PLUGIN_VERSION;
    }

    DLLFUNC_C void BrokerHTTP(FARPROC fpSend, FARPROC fpStatus, FARPROC fpResult, FARPROC fpFree) {
        (FARPROC&)http_send = fpSend;
        (FARPROC&)http_status = fpStatus;
        (FARPROC&)http_result = fpResult;
        (FARPROC&)http_free = fpFree;
    }

    DLLFUNC int BrokerLogin(char* User, char* Pwd, char* Type, char* Account) {
        if (User) {
            showMsg("BrokerLogin: Zorro-Fix-Bridge connected");

            return 1;
        }
        else {
            showMsg("BrokerLogin: no user");
        }
        return 0;
    }

    DLLFUNC int BrokerTime(DATE* pTimeGMT) {
        return 2;	// broker is online
    }

    DLLFUNC int BrokerAccount(char* Account, double* pdBalance, double* pdTradeVal, double* pdMarginVal) {
        double Balance = 10000;
        double TradeVal = 0;
        double MarginVal = 0;

        if (pdBalance && Balance > 0.) *pdBalance = Balance;
        if (pdTradeVal && TradeVal > 0.) *pdTradeVal = TradeVal;
        if (pdMarginVal && MarginVal > 0.) *pdMarginVal = MarginVal;
        return 1;
    }

    DLLFUNC int BrokerAsset(char* Asset, double* pPrice, double* pSpread,
        double* pVolume, double* pPip, double* pPipCost, double* pMinAmount,
        double* pMarginCost, double* pRollLong, double* pRollShort) {
         
        if (pPrice) *pPrice = 0;    
        if (pSpread) *pSpread = 0; 
        return 1;
    }

    DLLFUNC int BrokerHistory2(char* Asset, DATE tStart, DATE tEnd, int nTickMinutes, int nTicks, T6* ticks)
    {
        for (int i = 0; i < nTicks; i++) {
            ticks->fOpen = 0;
            ticks->fClose = 0;
            ticks->fHigh = 0;
            ticks->fLow = 0;
            ticks->time = 0;
            ticks++;
        }
        return nTicks;
    }


    DLLFUNC_C int BrokerBuy2(char* Asset, int nAmount, double dStopDist, double dLimit, double* pPrice, int* pFill)
    {
        auto start = std::time(nullptr);

        double qty = std::abs(nAmount);
        if (dLimit) {
            // limit
        }
        else {
            dLimit = NAN;
             
        }

        if (dStopDist) {

        }

        LOG_DEBUG("BrokerBuy2 %s orderText=%s nAmount=%d qty=%f dStopDist=%f limit=%f\n", Asset, "text", nAmount, qty, dStopDist, dLimit);

        auto internalOrdId = 0;
        
        bool fill = true;

        if (fill) {
            if (pPrice) {
                *pPrice = 0;
            }
            if (pFill) {
                *pFill = 0;
            }

            // reset s_amount, next asset might have lotAmount = 1, in that case SET_AMOUNT will not be called in advance

            return internalOrdId;
        }

        return internalOrdId;
    }

    DLLFUNC int BrokerTrade(int nTradeID, double* pOpen, double* pClose, double* pCost, double* pProfit) {
        LOG_DEBUG("BrokerTrade: %d\n", nTradeID);

        
        return 0;
    }

    DLLFUNC_C int BrokerSell2(int nTradeID, int nAmount, double Limit, double* pClose, double* pCost, double* pProfit, int* pFill) {
        LOG_DEBUG("BrokerSell2 nTradeID=%d nAmount=%d limit=%f\n", nTradeID, nAmount, Limit);

        return 0;
    }

    DLLFUNC double BrokerCommand(int command, DWORD dwParameter) {
        switch (command)
        {
        case GET_COMPLIANCE:
            return 2;

        case GET_BROKERZONE:
            return 0;   // historical data in UTC time

        case GET_MAXTICKS:
            return 1000;

        case GET_MAXREQUESTS:
            return 30;

        case GET_LOCK:
            return 1;

        case GET_POSITION: {
            
            return 0;
            break;
        }

        case SET_ORDERTEXT: {
            return dwParameter;
        }

        case SET_SYMBOL: {
            auto s_asset = (char*)dwParameter;
            return 1;
        }

        case SET_MULTIPLIER: {
            auto s_multiplier = (int)dwParameter;
            return 1;
        }

        case SET_ORDERTYPE: {
            switch ((int)dwParameter) {
            case 0:
                return 0;
            default:
                break;
            }

            if ((int)dwParameter >= 8) {
                return (int)dwParameter;
            }

            LOG_DEBUG("SET_ORDERTYPE: %d s_tif=%s\n", (int)dwParameter, "tif");
            return 0; // tifToZorroOrderType(s_tif);
        }

        case GET_PRICETYPE:
            return 0;

        case SET_PRICETYPE: {
            auto s_priceType = (int)dwParameter;
            LOG_DEBUG("SET_PRICETYPE: %d\n", s_priceType);
            return dwParameter;
        }

        case GET_VOLTYPE:
            return 0;

        case SET_AMOUNT: {
            auto s_amount = *(double*)dwParameter;
            LOG_DEBUG("SET_AMOUNT: %.8f\n", s_amount);
            break;
        }

        case SET_DIAGNOSTICS: {
            if ((int)dwParameter == 1 || (int)dwParameter == 0) {
                Logger::instance().setLevel((int)dwParameter ? LogLevel::L_DEBUG : LogLevel::L_OFF);
                return dwParameter;
            }
            break;
        }

        case SET_HWND:
        case GET_CALLBACK:
        case SET_CCY:
            break;

        case GET_HEARTBEAT:
            return 500;

        case SET_LEVERAGE: {
            LOG_DEBUG("BrokerCommand: SET_LEVERAGE param=%d\n", (int)dwParameter);
            LOG_DEBUG("BrokerCommand: SET_LEVERAGE param=%f\n", *(double*)dwParameter);
            break;
        }

        case SET_LIMIT: {
            auto limit = *(double*)dwParameter;
            LOG_DEBUG("BrokerCommand: SET_LIMIT param=%f\n", limit);
            break;
        }

        case SET_FUNCTIONS:
            LOG_DEBUG("BrokerCommand: SET_FUNCTIONS param=%d\n", (int)dwParameter);
            break;

        default:
            LOG_DEBUG("BrokerCommand: unhandled command %d param=%lu\n", command, dwParameter);
            break;
        }

        return 0.;
    }
}


