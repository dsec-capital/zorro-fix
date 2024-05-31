#ifndef FIX_H
#define FIX_H

#include "quickfix/FixFields.h"
#include "quickfix/FixValues.h"

namespace common::fix {

	// https://www.onixs.biz/fix-dictionary/4.4/tagNum_340.html
	enum TradeSessionStatus {
		UNDEFINED = 0,
		HALTED = 1,
		OPEN = 2,
		CLOSED = 3,
		PRE_OPEN = 4,
		PRE_CLOSE = 5,
		REQUEST_REJECTED = 6
	};

}

#endif 