#ifndef TIME_UTILS_H
#define TIME_UTILS_H

#include "pch.h"

namespace common {

    using namespace std::chrono;
    using namespace std::chrono_literals;

    typedef time_point<system_clock> time_point_t;
    typedef time_point<steady_clock> steady_time_point_t;

    // parsing datetime string for example "2017-09-15 13:11:34.356648"
    inline nanoseconds parse_datetime(const std::string& d) {
       std::istringstream in(d);
       time_point<system_clock>  tp;
       in >> parse("%F %T", tp);
       return duration_cast<nanoseconds>(tp.time_since_epoch());
    }

    inline nanoseconds get_current_system_clock() {
        return duration_cast<nanoseconds>(system_clock::now().time_since_epoch());
    }
    
    inline std::tm localtime_xp(const std::time_t& t)
    {
        std::tm bt{};
#if defined(__unix__)
        localtime_r(&t, &bt);
#elif defined(_MSC_VER)
        localtime_s(&bt, &t);
#else
        static std::mutex mtx;
        std::lock_guard<std::mutex> lock(mtx);
        bt = *std::localtime(&t);
#endif
        return bt;
    }

    inline std::tm gmtime_xp(const std::time_t &t) {
       std::tm bt{};
#if defined(_MSC_VER)
       gmtime_s(&bt, &t);
#else
       bt = *std::gmtime(&t);
#endif 
       return bt;
    }

    inline std::string time32_to_string(const __time32_t& ts, long ms = 0)
    {
       const auto bt = gmtime_xp(ts);
       std::ostringstream oss;
       oss << std::put_time(&bt, "%Y-%m-%d %H:%M:%S");
       if (ms > 0)
          oss << '.' << std::setfill('0') << std::setw(3) << ms;
       oss << "Z";
       return oss.str();
    }

    inline std::string to_string(const time_point_t& ts)
    {
       const auto t = system_clock::to_time_t(ts);
       const auto ms = duration_cast<milliseconds>(ts.time_since_epoch()) % 1000;
       const auto gt = gmtime_xp(t);
       std::ostringstream oss;
       oss << std::put_time(&gt, "%Y-%m-%d %H:%M:%S");
       oss << '.' << std::setfill('0') << std::setw(3) << ms.count() << "Z";
       return oss.str();
    }

    inline std::string to_string(const nanoseconds& ns) {
       using namespace std::chrono;
       auto us = duration_cast<microseconds>(ns);
       auto tp = time_point<system_clock>(us);
       return to_string(tp);
    }

    inline std::string now_str() {
        using namespace std::chrono;
        return to_string(system_clock::now());
    }

    inline time_t steady_clock_to_time_t(const std::chrono::steady_clock::time_point& t)
    {
        return system_clock::to_time_t(system_clock::now() +
            std::chrono::duration_cast<system_clock::duration>(t - std::chrono::steady_clock::now()));
    }

    inline std::string to_string(const steady_time_point_t& ts)
    {
        const auto t = steady_clock_to_time_t(ts);
        const auto ms = duration_cast<milliseconds>(ts.time_since_epoch()) % 1000;
        const auto gt = gmtime_xp(t);
        std::ostringstream os;
        os << std::put_time(&gt, "%Y-%m-%d %H:%M:%S");
        os << '.' << std::setfill('0') << std::setw(3) << ms.count() << "Z";
        return os.str();
    }

    inline std::string timestamp_postfix() {
        const auto timer = system_clock::to_time_t(system_clock::now());
        const std::tm bt = localtime_xp(timer);
        std::ostringstream oss;
        oss << std::put_time(&bt, "%F_%H%M%S");  
        return oss.str();
    }
}

#endif  
