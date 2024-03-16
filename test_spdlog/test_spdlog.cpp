#include <iostream>

#include "spdlog/spdlog.h"

int main()
{
    std::string a = "abc";
    std::string b = "123";
    spdlog::debug("a={}, b={}", a, b);
}

