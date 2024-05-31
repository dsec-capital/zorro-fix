#include "pch.h"
#include "LocalFormat.h"

LocalFormat::LocalFormat()
{
    mListSeparator = "";
    mDecimalSeparator = "";
}

const char *LocalFormat::getListSeparator()
{
    return ";";
}

const char *LocalFormat::getDecimalSeparator()
{
    return ".";
}

std::string LocalFormat::formatDouble(double value, int precision)
{
    char format[16];
    char buffer[64];

    sprintf_s(format, 16, "%%.%if", precision);
    sprintf_s(buffer, 64, format, value);
    char *point = strchr(buffer, '.');
    if (point != 0)
        *point = getDecimalSeparator()[0];

    return std::string(buffer);
}

std::string LocalFormat::formatDate(DATE value)
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
