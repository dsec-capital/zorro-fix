#include "pch.h"
#include "zorro.h"

namespace zorro {

	std::string broker_command_string(int command) {
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