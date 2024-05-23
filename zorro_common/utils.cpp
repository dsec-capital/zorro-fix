#include "pch.h"
#include "utils.h"

#include "common/time_utils.h"

namespace zorro {

	DATE convert_time(__time32_t t32) {
		return (DATE)t32 / SECONDS_PER_DAY + DAYS_BETWEEN_1899_12_30_1979_01_01;
	}

	__time32_t convert_time(DATE date) {
		return (__time32_t)((date - DAYS_BETWEEN_1899_12_30_1979_01_01) * SECONDS_PER_DAY);
	}

	std::chrono::nanoseconds convert_time_chrono(DATE date) {
		auto count = (long long)((date - DAYS_BETWEEN_1899_12_30_1979_01_01) * MICROS_PER_DAY);
		auto us = std::chrono::microseconds(count);
		return std::chrono::duration_cast<std::chrono::nanoseconds>(us);
	}

	DATE convert_time_chrono(const std::chrono::nanoseconds& t) {
		auto us = std::chrono::duration_cast<std::chrono::microseconds>(t).count();
		return (DATE)us / MICROS_PER_DAY + DAYS_BETWEEN_1899_12_30_1979_01_01;
	}

	std::string zorro_date_to_string(DATE date, bool millis) {
		auto ts = convert_time(date);
		auto ms = millis ? (long)(date * 1000) % 1000 : 0;
		return common::time32_to_string(ts, ms);
	}

}