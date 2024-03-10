#pragma once

#include <cstdio>
#include <cstdint>
#include <ctime>
#include <string>
#include <mutex>

#include "time_utils.h"

namespace zfix {

	enum LogLevel : uint8_t {
		L_OFF,
		L_ERROR,
		L_WARNING,
		L_INFO,
		L_DEBUG,
		L_TRACE,
		L_TRACE2,
	};

	static constexpr const char* to_string(LogLevel level) {
		constexpr const char* s_levels[] = {
			"OFF", "ERROR", "WARNING", "INFO", "DEBUG", "TRACE", "TRACE2"
		};
		return s_levels[level];
	}

	class Logger {
	public:
		static Logger& instance();

		void init(std::string&& name);

		LogLevel getLevel() const noexcept;

		void setLevel(LogLevel level) noexcept;

		template<typename ... Args>
		inline void log(LogLevel level, const char* format, Args... args) {
			using namespace std::chrono;
			mutex_.lock();
			//std::time_t t = std::time(nullptr);
			//struct tm _tm;
			//localtime_s(&_tm, &t);
			//char buf[25];
			//std::strftime(buf, sizeof(buf), "%F %T", &_tm);
			auto now = system_clock::now();
			auto us = duration_cast<microseconds>(now.time_since_epoch()) % 1000000;
			auto timer = system_clock::to_time_t(now);
			std::tm bt = *std::localtime(&timer);
			std::ostringstream oss;
			oss << std::put_time(&bt, "%H:%M:%S");  
			oss << '.' << std::setfill('0') << std::setw(6) << us.count();
			fprintf(log_, "%s | %s | ", oss.str().c_str(), to_string(level));
			fprintf(log_, format, std::forward<Args>(args)...);
			fflush(log_);
			mutex_.unlock();
		}

	private:
		Logger() = default;

		~Logger();

		void finit();

		void open_log();

	private:
		std::mutex mutex_;
		std::string name_;
		FILE* log_ = nullptr;
		LogLevel level_ = LogLevel::L_OFF;
	};


#ifndef _LOG
#define _LOG(level, format, ...)                    \
{                                                   \
    auto& logger = Logger::instance();              \
    auto lvl = logger.getLevel();                   \
    if (lvl >= level) {                             \
        logger.log(level, format, __VA_ARGS__);     \
    }                                               \
}

#define LOG_DEBUG(format, ...) _LOG(L_DEBUG, format, __VA_ARGS__);
#define LOG_INFO(format, ...) _LOG(L_INFO, format, __VA_ARGS__);
#define LOG_WARNING(format, ...) _LOG(L_WARNING, format, __VA_ARGS__);
#define LOG_ERROR(format, ...) _LOG(L_ERROR, format, __VA_ARGS__);
#define LOG_TRACE(format, ...) _LOG(L_TRACE, format, __VA_ARGS__);
#define LOG_TRACE2(format, ...) _LOG(L_TRACE2, format, __VA_ARGS__);
#ifdef _DEBUG
#define LOG_DIAG(format, ...) _LOG(L_DEBUG, format, __VA_ARGS__);
#else
#define LOG_DIAG(format, ...)
#endif
#endif
}
