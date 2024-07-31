#ifndef FIX_H
#define FIX_H

#include "quickfix/FixFields.h"
#include "quickfix/FixValues.h"
#include "quickfix/Message.h"

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

	inline bool is_market_data_message(const FIX::Message& message) {
		const auto& msg_type = message.getHeader().getField(FIX::FIELD::MsgType);
		return msg_type == FIX::MsgType_MarketDataSnapshotFullRefresh || msg_type == FIX::MsgType_MarketDataIncrementalRefresh;
	}

	inline bool is_exec_report_message(const FIX::Message& message) {
		const auto& msg_type = message.getHeader().getField(FIX::FIELD::MsgType);
		return msg_type == FIX::MsgType_ExecutionReport;
	}

}

#endif 