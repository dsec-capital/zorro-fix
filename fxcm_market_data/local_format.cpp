#include "pch.h"

#include "local_format.h"

LocalFormat::LocalFormat()
{
    list_separator = "";
    decimal_separator = "";
}

const char *LocalFormat::get_list_separator()
{
    return ";";
}

const char *LocalFormat::get_decimal_separator()
{
    return ".";
}

std::string LocalFormat::format_double(double value, int precision)
{
    char format[16];
    char buffer[64];

    sprintf_s(format, 16, "%%.%if", precision);
    sprintf_s(buffer, 64, format, value);
    char *point = strchr(buffer, '.');
    if (point != 0)
        *point = get_decimal_separator()[0];

    return std::string(buffer);
}

std::string LocalFormat::format_date(double value)
{
    struct tm tmBuf = { 0 };
    CO2GDateUtils::OleTimeToCTime(value, &tmBuf);

    using namespace std;
    stringstream sstream;
    sstream 
        << setw(4) << tmBuf.tm_year + 1900 << "-" \
        << setw(2) << setfill('0') << tmBuf.tm_mon + 1 << "-" \
        << setw(2) << setfill('0') << tmBuf.tm_mday << " " \
        << setw(2) << setfill('0') << tmBuf.tm_hour << ":" \
        << setw(2) << setfill('0') << tmBuf.tm_min << ":" \
        << setw(2) << setfill('0') << tmBuf.tm_sec;

    return sstream.str();
}
