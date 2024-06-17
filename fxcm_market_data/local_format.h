#pragma once

class LocalFormat
{
    std::string list_separator;
    std::string decimal_separator;

 public:
    LocalFormat();

    const char *get_list_separator();

    const char *get_decimal_separator();

    std::string format_double(double value, int precision);

    std::string format_date(double value);
};
