
#include <string>
#include <vector>
#include <iostream>
#include <cstdlib>
#include <format>
#include "magic_enum/magic_enum.hpp"

#include "fxcm_market_data/forex_connect.h"  // must be before includes from zorro_common

#include "common/bar.h"
#include "common/time_utils.h"
#include "zorro_common/utils.h"
#include "zorro_common/log.h"

namespace zorro {
	int(__cdecl* BrokerError)(const char* txt);

	namespace log {
		std::size_t logging_verbosity = 1;
	}
}

int show(const char* txt) {
	std::cout << txt << std::endl;
	return 0;
}

using namespace zorro;
using namespace std::literals::chrono_literals;

void test_function()
{
	zorro::BrokerError = show;

	auto logger = zorro::create_file_logger("test_fxcm_market_data_function.log");

	int n_tick_minutes = 1;
	int n_ticks = 2 * 1440;
	std::string instrument = "EUR/USD";
	std::string timeframe = "m1";

	std::stringstream ss; ss
		<< "FXCM marktet data download with get_historical_prices:" << std::endl
		<< "  n_tick_minutes=" << n_tick_minutes << std::endl
		<< "  n_ticks=" << n_ticks << std::endl
		<< "  instrument=" << instrument << std::endl
		<< "  timeframe=" << timeframe << std::endl;

	std::cout << ss.str();
	spdlog::info(ss.str());

	std::vector<common::BidAskBar<fxcm::DATE>> bars;

	size_t len = 0;
	char fxcm_login[256];
	char fxcm_password[256];
	getenv_s(&len, fxcm_login, sizeof(fxcm_login), "FIX_USER_NAME");
	getenv_s(&len, fxcm_password, sizeof(fxcm_password), "FIX_PASSWORD");

	log::debug<0, true>("fxcm_login={} fxcm_password={}", fxcm_login, fxcm_password);

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

	for (auto it = bars.begin(); it != bars.end(); ++it) {
		auto ts = zorro::zorro_date_to_string(it->timestamp);
		spdlog::debug("begin {} bar {}", ts, it->to_string());
	}

	log::debug<1, true>("FXCM marktet data download completed: {} bars read", bars.size());
}

void test_class() {
	zorro::BrokerError = show;

	auto logger = zorro::create_file_logger("test_fxcm_market_data_class.log");

	size_t len = 0;
	char fxcm_login[256];
	char fxcm_password[256];
	getenv_s(&len, fxcm_login, sizeof(fxcm_login), "FIX_USER_NAME");
	getenv_s(&len, fxcm_password, sizeof(fxcm_password), "FIX_PASSWORD");

	log::debug<1, true>("fxcm_login={} fxcm_password={}", fxcm_login, fxcm_password);

	auto connection = fxcm::ForexConnect(
		std::string(fxcm_login), 
		std::string(fxcm_password),
		std::string(fxcm::demo_connection),
		std::string(fxcm::default_url)
	);

	connection.login();
	log::debug<1, true>("logged in");

	int n_tick_minutes = 1;
	int n_ticks = 2*1440;
	std::string instrument = "EUR/USD";
	std::string timeframe = "m1";

	auto now = common::get_current_system_clock();
	fxcm::DATE t_end = zorro::convert_time_chrono(now);
	fxcm::DATE bar_seconds = n_tick_minutes * 60;
	fxcm::DATE t_bar = bar_seconds / SECONDS_PER_DAY;
	fxcm::DATE t_start = t_end - n_ticks * t_bar;

	std::vector<common::BidAskBar<fxcm::DATE>> bars1;
	connection.fetch(
		bars1,
		instrument,
		timeframe,
		t_start,
		t_end
	);

	log::debug<1, true>("downloaded {} bars", bars1.size());
	for (auto it = bars1.begin(); it != bars1.end(); ++it) {
		auto ts = zorro::zorro_date_to_string(it->timestamp);
		log::debug<2, true>("begin {} bar {}", ts, it->to_string());
	}
	log::debug<1, true>("FXCM marktet data download completed: {} bars read", bars1.size());

	// other fixed range 
	t_end = 45442.10972890354;
	t_start = 45441.41528445909;

	std::vector<common::BidAskBar<fxcm::DATE>> bars2;
	connection.fetch(
		bars2, 
		instrument,
		timeframe,
		t_start, 
		t_end
	);

	log::debug<1, true>("downloaded {} bars", bars2.size());
	for (auto it = bars2.begin(); it != bars2.end(); ++it) {
		auto ts = zorro::zorro_date_to_string(it->timestamp);
		log::debug<2, true>("begin {} bar {}", ts, it->to_string());
	}
	log::debug<1, true>("FXCM marktet data download completed: {} bars read", bars2.size());

	connection.logout();
	log::debug<1, true>("logged out");
}

void test_login() {
	zorro::BrokerError = show;

	auto logger = zorro::create_file_logger("test_fxcm_market_data_login.log");

	size_t len = 0;
	char fxcm_login[256];
	char fxcm_password[256];
	getenv_s(&len, fxcm_login, sizeof(fxcm_login), "FIX_USER_NAME");
	getenv_s(&len, fxcm_password, sizeof(fxcm_password), "FIX_PASSWORD");

	log::debug<1, true>("fxcm_login={} fxcm_password={}", fxcm_login, fxcm_password);

	auto connection = fxcm::ForexConnect(
		std::string(fxcm_login),
		std::string(fxcm_password),
		std::string(fxcm::demo_connection),
		std::string(fxcm::default_url)
	);

	bool res = connection.login();
	log::debug<1, true>("waiting for login res={}", res);

	auto t0 = std::chrono::high_resolution_clock::now();

	auto logged_in = connection.wait_for_login(10000ms);
	
	auto dt = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - t0);

	log::debug<1, true>("logged in with logged_in={} in dt={}", logged_in, dt);


	connection.logout();
	log::debug<1, true>("waiting for logout");

	t0 = std::chrono::high_resolution_clock::now();

	auto logged_out = connection.wait_for_logout(15000ms);

	dt = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - t0);

	log::debug<1, true>("logged out with logged_out={} in dt={}", logged_out, dt);
}

int main(int argc, char* argv[])
{
	test_login();
	
	//test_class();

	//test_function();
	
	return 0;
}