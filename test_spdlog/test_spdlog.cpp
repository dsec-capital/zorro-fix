#define _SILENCE_ALL_MS_EXT_DEPRECATION_WARNINGS

#include <format>

#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"

int main()
{
    auto logger = spdlog::basic_logger_mt("basic_logger", "test_log_file.log");
    spdlog::set_level(spdlog::level::debug);

    std::string a = "abc";
    std::string b = "123";
    
    spdlog::debug("a={}, b={}", a, b); 
    
    SPDLOG_DEBUG("Some debug message");

    logger->debug("Should be in file");
}

