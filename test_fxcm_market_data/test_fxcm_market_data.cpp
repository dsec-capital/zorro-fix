
#include <string>
#include <vector>
#include <iostream>
#include <cstdlib>
#include <format>

#include "common/bar.h"
#include "common/time_utils.h"
#include "zorro_common/utils.h"
#include "zorro_common/log.h"
#include "fxcm_market_data/fxcm_market_data.h"

namespace zorro {
	int(__cdecl* BrokerError)(const char* txt);
}

int show(const char* txt) {
	std::cout << txt << std::endl;
	return 0;
}

int main(int argc, char* argv[])
{
	zorro::BrokerError = show;

	zorro::create_file_logger("test_fxcm_market_data.log");

	int n_tick_minutes = 1;
	int n_ticks = 2 * 1440;
	std::string instrument = "EUR/USD";
	std::string timeframe = "m1";

	std::cout
		<< "get_historical_prices:" << std::endl
		<< "  n_tick_minutes=" << n_tick_minutes << std::endl
		<< "  n_ticks=" << n_ticks << std::endl
		<< "  instrument=" << instrument << std::endl
		<< "  timeframe=" << timeframe << std::endl;

	std::vector<common::BidAskBar<fxcm::DATE>> bars;

	size_t len = 0;
	char fxcm_login[256];
	char fxcm_password[256];
	::getenv_s(&len, fxcm_login, sizeof(fxcm_login), "FIX_USER_NAME");
	::getenv_s(&len, fxcm_password, sizeof(fxcm_password), "FIX_PASSWORD");

	spdlog::debug("fxcm_login={} fxcm_password={}", fxcm_login, fxcm_password);

	auto now = common::get_current_system_clock();
	fxcm::DATE t_end = zorro::convert_time_chrono(now);
	fxcm::DATE bar_seconds = n_tick_minutes * 60;
	fxcm::DATE t_bar = bar_seconds / SECONDS_PER_DAY;
	fxcm::DATE t_start = t_end - n_ticks * t_bar;

	// alternative range 
	t_end = 45442.10972890354;
	t_start = 45441.41528445909;

	auto success = fxcm::get_historical_prices(
		bars,
		fxcm_login,
		fxcm_password,
		"Demo",
		fxcm::default_url,
		instrument.c_str(),
		timeframe.c_str(),
		t_start,
		t_end
	);

	spdlog::debug("{} bars read", bars.size());
	for (auto it = bars.begin(); it != bars.end(); ++it) {
		auto ts = zorro::zorro_date_to_string(it->timestamp, false);
		spdlog::debug("begin {} bar {}", ts, it->to_string());
	}
}

