#pragma once

#include "pch.h"

#include "logger.h"


namespace common {

    extern int(__cdecl* BrokerError)(const char* txt);

    Logger& Logger::instance() {
        static Logger inst;
        return inst;
    }

    void Logger::init(std::string&& name) {
        finit();
        name_ = std::move(name);
#ifdef _DEBUG
        setLevel(LogLevel::L_TRACE);
#endif
        if (level_ != LogLevel::L_OFF && !log_) {
            open_log();
        }
    }

    LogLevel Logger::getLevel() const noexcept {  
        return level_; 
    }

    void Logger::setLevel(LogLevel level) noexcept {
        level_ = level;
        if (level != LogLevel::L_OFF && !log_) {
            open_log();
        }
    }

    Logger::~Logger() {
        finit();
    }

    void Logger::finit() {
        if (log_) {
            fflush(log_);
            fclose(log_);
            log_ = nullptr;
        }
    }

    void Logger::open_log() {
        std::time_t t = std::time(nullptr);
        struct tm _tm;
        localtime_s(&_tm, &t);
        char buf[25];
        std::strftime(buf, sizeof(buf), "%F_%H%M%S", &_tm);
        std::string log_file = "./Log/" + name_ + "_" + std::string(buf) + ".log";
        log_ = fopen(log_file.c_str(), "w");
    }
}
