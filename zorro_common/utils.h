#pragma once

#include "zorro.h"

#include <string>
#include <chrono>
#include <format>

#include "common/time_utils.h"


namespace zorro {

	// Days between midnight 1899-12-30 and midnight 1970-01-01 is 25569
	constexpr auto DAYS_BETWEEN_1899_12_30_1979_01_01 = 25569.0;
	constexpr auto SECONDS_PER_DAY = 86400.0;
	constexpr auto MILLIS_PER_DAY = 86400000.0;
	constexpr auto MICROS_PER_DAY = 86400000000.0;
	constexpr auto NANOS_PER_DAY = 86400000000000.0;
	constexpr auto SECONDS_PER_MINUTE = 60;
	constexpr auto SECONDS_PER_HOURS = 3600;

	template <typename... Args>
	void show(std::format_string<Args...> const& fmt, Args &&...args) {
		if (!BrokerError) return;
		auto msg = std::vformat(fmt.get(), std::make_format_args(args...));
		auto tmsg = "[" + common::now_str() + "] " + msg + "\n";
		BrokerError(tmsg.c_str());
	}

	// DATE is fractional time in days since midnight 1899-12-30
	// The type __time32_t is representing the time as seconds elapsed since midnight 1970-01-01
	DATE convert_time(__time32_t t32);

	__time32_t convert_time(DATE date);

	std::chrono::nanoseconds convert_time_chrono(DATE date);

	DATE convert_time_chrono(const std::chrono::nanoseconds& t);

	std::string zorro_date_to_string(DATE date);
}