#pragma once

#include <string>
#include <vector>

#include "common/bar.h"

#include "spdlog/spdlog.h"

namespace fxcm {

    constexpr inline const char* default_url = "http://www.fxcorporate.com/Hosts.jsp";
    constexpr inline const char* real_connection = "Real";
    constexpr inline const char* demo_connection = "Demo";

    typedef double DATE;

    int get_historical_prices(
        std::vector<common::BidAskBar<DATE>>& bars,
        const char* login,
        const char* password,
        const char* connection,
        const char* url,
        const char* instrument,
        const char* timeframe,
        DATE date_from,
        DATE date_to,
        const std::string& timezone = "UTC",
        const char* session_id = nullptr,
        const char* pin = nullptr
    );

}
