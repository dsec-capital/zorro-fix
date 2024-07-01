#ifndef UTILS_H
#define UTILS_H

#include <chrono>

namespace common {

    template<typename K, typename T>
    inline const T& get_or_else(const std::map<K, T>& map, const K& key, const T& other) {
        auto it = map.find(key);
        if (it != map.end()) {
            return it->second;
        }
        else {
            return other;
        }
    }

    template<typename K, typename V, typename T>
    inline const T& vget_or_else(const std::map<K, V>& map, const K& key, const T& other) {
        auto it = map.find(key);
        if (it != map.end()) {
            return std::get<T>(it->second);
        }
        else {
            return other;
        }
    }

    std::optional<std::string> get_env(const std::string& name);

    bool is_nan(double value);

    std::string upper_string(const std::string& str);

    std::string toml_str_to_symbol_str(const std::string& toml);

    double round_up(double in, double multiple);

    double round_down(double in, double multiple);

    // https://stackoverflow.com/questions/3407012/rounding-up-to-the-nearest-multiple-of-a-number
    long long round_up(long long in, long long multiple);

    long long round_down(long long in, long long multiple);

    std::chrono::nanoseconds round_up(const std::chrono::nanoseconds& in, const std::chrono::nanoseconds multiple);

    std::chrono::nanoseconds round_down(const std::chrono::nanoseconds& in, const std::chrono::nanoseconds& multiple);
}

#endif
