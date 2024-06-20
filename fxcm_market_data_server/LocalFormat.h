#pragma once

#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>

/** The helper class that controls formatting and localization. */
class LocalFormat
{
    std::string mListSeparator;
    std::string mDecimalSeparator;

 public:
    LocalFormat();

    const char *getListSeparator();
    const char *getDecimalSeparator();
    std::string formatDouble(double value, int precision);
    std::string formatDate(double value);
};
