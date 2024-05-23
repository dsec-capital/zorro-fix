#pragma once

#include "zorro.h"

#include <string>
#include <chrono>
#include <format>

#include "common/time_utils.h"

// Days between midnight 1899-12-30 and midnight 1970-01-01 is 25569
#define DAYS_BETWEEN_1899_12_30_1979_01_01	25569.0
#define SECONDS_PER_DAY						86400.0
#define MILLIS_PER_DAY                      86400000.0 
#define MICROS_PER_DAY                      86400000000.0 

namespace zorro {

	template <typename... Args>
	void show(std::format_string<Args...> const& fmt, Args &&...args) {
		if (!BrokerError) return;
		auto msg = std::vformat(fmt.get(), std::make_format_args(args...));
		auto tmsg = "[" + common::now_str() + "] " + msg + "\n";
		BrokerError(tmsg.c_str());
	}

	// From Zorro documentation:
	//		If not stated otherwise, all dates and times reflect the time stamp of the Close price at the end of the bar. 
	//		Dates and times are normally UTC. Timestamps in historical data are supposed to be either UTC, or to be a date 
	//		only with no additional time part, as for daily bars. 
	//		It is possible to use historical data with local timestamps in the backtest, but this must be considered in the 
	//		script since the date and time functions will then return the local time instead of UTC, and time zone functions 
	//		cannot be used.

	// DATE is fractional time in days since midnight 1899-12-30
	// The type __time32_t is representing the time as seconds elapsed since midnight 1970-01-01
	DATE convert_time(__time32_t t32);

	__time32_t convert_time(DATE date);

	std::chrono::nanoseconds convert_time_chrono(DATE date);

	DATE convert_time_chrono(const std::chrono::nanoseconds& t);

	std::string zorro_date_to_string(DATE date, bool millis = false);

}