#pragma once

#include <string>
#include <vector>

#include "common/bar.h"

#include "ForexConnect.h"

#include "LocalFormat.h"
#include "SessionStatusListener.h"

namespace fxcm {

    constexpr inline const char* default_url = "http://www.fxcorporate.com/Hosts.jsp";
    constexpr inline const char* real_connection = "Real";
    constexpr inline const char* demo_connection = "Demo";

    typedef double DATE;

    class ForexConnectData {
    public:
        ForexConnectData(
            const std::string& login_user,
            const std::string& password,
            const std::string& connection,
            const std::string& url,
            const std::string& session_id = "",
            const std::string& pin = "",
            int timeout = 10000
        );

        ~ForexConnectData();

        bool login();

        void logout();

        /*
            Get historical bars

            The name of the timeframe is the name of the timeframe measurement unit followed 
            by the length of the time period. The names of the units are:

            Example(s)
                t   Ticks       t1 - ticks ==> use fetch_quotes
                m   Minutes     m1 - 1 minute, m5 - 5 minutes, m30 - 30 minutes.
                H   Hours       H1 - 1 hour, H6 - 6 hours, H12 - 12 hours.
                D   Days        D1 - 1 day.
                W   Weeks       W1 - 1 week.
                M   Months      M1 - 1 month.

            The timestamp of a bar is the beginning of the bar period.
        */
        bool fetch(
            std::vector<common::BidAskBar<DATE>>& bars,
            const std::string& instrument,
            const std::string& timeframe,
            DATE date_from,
            DATE date_to,
            int quotes_count = 0 
        );

        bool fetch(
            std::vector<common::Quote<DATE>>& bars,
            const std::string& instrument,
            DATE date_from,
            DATE date_to
        );

        O2G2Ptr<quotesmgr::IInstrument> get_instrument(const std::string& instrument);

    private:

        std::string login_user;
        std::string password;
        std::string connection;
        std::string url;
        std::string session_id;
        std::string pin;
        bool logged_in;

        LocalFormat format;

        O2G2Ptr<IO2GSession> session;
        O2G2Ptr<SessionStatusListener> statusListener;
        O2G2Ptr<pricehistorymgr::IPriceHistoryCommunicator> communicator;
    };

    /*
        Get historical prices

        Returns true on success.
    */
    bool get_historical_prices(
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
        const char* pin = nullptr,
        int timeout = 10000
    );

}
