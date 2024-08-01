#include "pch.h"

#include <stdlib.h>

#include "utils.h"

namespace common {

    std::optional<std::string> get_env(const std::string& name) {
        char buffer[4096];
        size_t n;
        auto err = getenv_s(&n, buffer, sizeof(buffer), name.c_str());

        if (err == 0 && n > 0) {
            return std::optional<std::string>(std::string(buffer));
        }
        else {
            return std::optional<std::string>();
        }
    }

    bool is_nan(double value) {
        return value != value;
    }

    std::string upper_string(const std::string& str) {
        std::string upper;
        std::transform(str.begin(), str.end(), std::back_inserter(upper), toupper);
        return upper;
    }

    std::string toml_str_to_symbol_str(const std::string& toml) {
        auto symbol = std::string(toml);
        std::replace(symbol.begin(), symbol.end(), '_', '/');
        return symbol;
    }

    double round_up(double in, double multiple) {
        double m = std::fmod(in, multiple);
        if (m == 0.0) {
            return in;
        }
        else {
            double down = in - m;
            return down + (std::signbit(in) ? -multiple : multiple);
        }
    }

    double round_down(double in, double multiple) {
        return in - std::fmod(in, multiple);
    }

    long long round_up(long long in, long long multiple)
    {
        int is_positive = (int)(in >= 0);
        return ((in + is_positive * (multiple - 1)) / multiple) * multiple;
    }

    long long round_down(long long in, long long multiple) {
        return in - in % multiple;
    }

    std::chrono::nanoseconds round_up(const std::chrono::nanoseconds& in, const std::chrono::nanoseconds multiple)
    {
        int is_positive = (int)(in.count() >= 0);
        return ((in + is_positive * (multiple - std::chrono::nanoseconds(1))) / multiple) * multiple;
    }

    std::chrono::nanoseconds round_down(const std::chrono::nanoseconds& in, const std::chrono::nanoseconds& multiple) {
        return in - in % multiple;
    }

}