#define _SILENCE_ALL_MS_EXT_DEPRECATION_WARNINGS

#include "spdlog/spdlog.h"

int main()
{
    std::string a = "abc";
    std::string b = "123";
    
    spdlog::debug("a={}, b={}", a, b); 

    SPDLOG_DEBUG("Some debug message");
}

