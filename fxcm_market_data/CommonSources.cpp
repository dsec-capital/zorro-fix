#include "pch.h"

bool is_nan(double value)
{
    return value != value;
}

std::string upper_string(const std::string &str)
{
    std::string upper;
    std::transform(str.begin(), str.end(), std::back_inserter(upper), toupper);
    return upper;
}