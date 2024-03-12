#ifndef TIME_UTILS_H
#define TIME_UTILS_H

#include <string>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace zfix {
    using namespace std::chrono_literals;

    typedef std::chrono::time_point<std::chrono::system_clock> time_point;
    typedef std::chrono::time_point<std::chrono::steady_clock> time_point_steady;

    inline time_point to_timepoint(std::chrono::nanoseconds nanos) {
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(nanos);
        return time_point(duration);
    }

    inline std::tm localtime_xp(std::time_t timer)
    {
        std::tm bt{};
#if defined(__unix__)
        localtime_r(&timer, &bt);
#elif defined(_MSC_VER)
        localtime_s(&bt, &timer);
#else
        static std::mutex mtx;
        std::lock_guard<std::mutex> lock(mtx);
        bt = *std::localtime(&timer);
#endif
        return bt;
    }

    inline std::string to_string(const time_point& ts)
    {
        using namespace std::chrono;
        const auto ms = duration_cast<milliseconds>(ts.time_since_epoch()) % 1000;
        const auto timer = system_clock::to_time_t(ts);
        const std::tm bt = localtime_xp(timer);
        std::ostringstream oss;
        oss << std::put_time(&bt, "%H:%M:%S"); // HH:MM:SS
        oss << '.' << std::setfill('0') << std::setw(3) << ms.count();
        return oss.str();
    }

    inline std::string now_str() {
        using namespace std::chrono;
        return to_string(system_clock::now());
    }


    inline time_t steady_clock_to_time_t(const std::chrono::steady_clock::time_point& t)
    {
        using std::chrono::steady_clock;
        using std::chrono::system_clock;
        return system_clock::to_time_t(system_clock::now() +
            std::chrono::duration_cast<system_clock::duration>(t - std::chrono::steady_clock::now()));
    }

    inline std::string to_string(const time_point_steady& ts)
    {
        using namespace std::chrono;
        const auto ms = duration_cast<milliseconds>(ts.time_since_epoch()) % 1000;
        const auto timer = steady_clock_to_time_t(ts);
        const std::tm bt = localtime_xp(timer);
        std::ostringstream oss;
        oss << std::put_time(&bt, "%H:%M:%S"); // HH:MM:SS
        oss << '.' << std::setfill('0') << std::setw(3) << ms.count();
        return oss.str();
    }
}
#endif //TIME_UTILS_H