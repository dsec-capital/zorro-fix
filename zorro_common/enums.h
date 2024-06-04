#pragma once

namespace zorro {

	enum ExchangeStatus {
		Unavailable = 0,
		Closed = 1,
		Open = 2
	};

	enum BrokerLoginStatus {
		LoggedOut = 0,
		LoggedIn = 1
	};

	enum BrokerError {
		OrderRejectedOrTimeout = 0,
		TradeOrderIdUUID = -1,
		BrokerAPITimeout = -2,
		OrderAcceptedWithoutOrderId = -3
	};

}
