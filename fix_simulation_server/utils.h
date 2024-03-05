#pragma once

#include <iostream>
#include <ctime>
#include <iostream>
#include <iterator>
#include <locale>
#include <chrono>

std::string getCurrentTimestamp()
{
    using std::chrono::system_clock;

    char buffer[80];
    auto currentTime = system_clock::now();

    auto transformed = currentTime.time_since_epoch().count() / 1000000;
    auto millis = transformed % 1000;

    std::time_t tt = system_clock::to_time_t(currentTime);
    auto timeinfo = std::gmtime(&tt);
    strftime(buffer, 80, "%F %H:%M:%S", std::gmtime(&tt));
    sprintf(buffer, "%s:%03d", buffer, (int)millis);

    return std::string(buffer);
}

std::string getCurrentTimestampIso() {
    std::time_t time = std::time({});
    char buffer[std::size("yyyy-mm-ddThh:mm:ss")];
    std::strftime(std::data(buffer), std::size(buffer), "%FT%TZ", std::gmtime(&time));
    return std::string(buffer);
}

std::string getCurrentDate() {
    std::time_t time = std::time({});
    char buffer[std::size("yyyy-mm-dd")];
    std::strftime(std::data(buffer), std::size(buffer), "%F", std::gmtime(&time));
    return std::string(buffer);
}

std::string getCurrentTime() {
    std::time_t time = std::time({});
    char buffer[std::size("hh:mm:ss")];
    std::strftime(std::data(buffer), std::size(buffer), "%T", std::gmtime(&time));
    return std::string(buffer);
}