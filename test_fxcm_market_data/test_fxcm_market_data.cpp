
#include <string>
#include <vector>
#include <iostream>
#include <cstdlib>
#include <format>
#include <filesystem>

#include "magic_enum/magic_enum.hpp"

#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"

#include "fxcm_market_data/forex_connect.h"  // must be before includes from zorro_common

#include "common/bar.h"
#include "common/time_utils.h"
#include "zorro_common/utils.h"

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

	auto now = common::get_current_system_clock();
	fxcm::DATE t_end = zorro::convert_time_chrono(now);
	fxcm::DATE bar_seconds = n_tick_minutes * 60;
	fxcm::DATE t_bar = bar_seconds / SECONDS_PER_DAY;
	fxcm::DATE t_start = t_end - n_ticks * t_bar;

	// alternative range 
	t_end = 0;
	t_start = 45460.166666666664;

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
}

void test_class() {
	zorro::BrokerError = show;

	size_t len = 0;
	char fxcm_login[256];
	char fxcm_password[256];
	getenv_s(&len, fxcm_login, sizeof(fxcm_login), "FIX_USER_NAME");
	getenv_s(&len, fxcm_password, sizeof(fxcm_password), "FIX_PASSWORD");

	spdlog::debug("fxcm_login={} fxcm_password={}", fxcm_login, fxcm_password);

	auto connection = fxcm::ForexConnect(
		std::string(fxcm_login), 
		std::string(fxcm_password),
		std::string(fxcm::demo_connection),
		std::string(fxcm::default_url)
	);

	connection.login();
	spdlog::debug("logged in");

	auto t0 = std::chrono::high_resolution_clock::now();

	auto logged_in = connection.wait_for_login(10000ms);

	auto dt = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - t0);

	spdlog::debug("logged in with logged_in={} in dt={}", logged_in, dt.count());

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

	spdlog::debug("downloaded {} bars", bars1.size());
	for (auto it = bars1.begin(); it != bars1.end(); ++it) {
		auto ts = zorro::zorro_date_to_string(it->timestamp);
		spdlog::debug("begin {} bar {}", ts, it->to_string());
	}
	spdlog::debug("FXCM marktet data download completed: {} bars read", bars1.size());

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

	spdlog::debug("downloaded {} bars", bars2.size());
	for (auto it = bars2.begin(); it != bars2.end(); ++it) {
		auto ts = zorro::zorro_date_to_string(it->timestamp);
		spdlog::debug("begin {} bar {}", ts, it->to_string());
	}
	spdlog::debug("FXCM marktet data download completed: {} bars read", bars2.size());

	connection.logout();
	spdlog::debug("logged out");
}

void test_login() {
	zorro::BrokerError = show;

	size_t len = 0;
	char fxcm_login[256];
	char fxcm_password[256];
	getenv_s(&len, fxcm_login, sizeof(fxcm_login), "FIX_USER_NAME");
	getenv_s(&len, fxcm_password, sizeof(fxcm_password), "FIX_PASSWORD");

	spdlog::debug("fxcm_login={} fxcm_password={}", fxcm_login, fxcm_password);

	auto connection = fxcm::ForexConnect(
		std::string(fxcm_login),
		std::string(fxcm_password),
		std::string(fxcm::demo_connection),
		std::string(fxcm::default_url)
	);

	bool res = connection.login();
	spdlog::debug("waiting for login res={}", res);

	auto t0 = std::chrono::high_resolution_clock::now();

	auto logged_in = connection.wait_for_login(10000ms);
	
	auto dt = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - t0);

	spdlog::debug("logged in with logged_in={} in dt={}", logged_in, dt.count());

	connection.logout();
	spdlog::debug("waiting for logout");

	t0 = std::chrono::high_resolution_clock::now();

	auto logged_out = connection.wait_for_logout(15000ms);

	dt = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - t0);

	spdlog::debug("logged out with logged_out={} in dt={}", logged_out, dt.count());
}

int main(int argc, char* argv[])
{
	auto cwd = std::filesystem::current_path().string();

	auto logger_name = "fxcm_proxy_server";
	auto log_level = spdlog::level::debug;
	auto flush_interval = std::chrono::seconds(2);
	std::vector<spdlog::sink_ptr> sinks{
		std::make_shared<spdlog::sinks::stdout_color_sink_mt>(),
		std::make_shared<spdlog::sinks::basic_file_sink_mt>("file", "fxcm_proxy_server"),
	};
	auto spd_logger = std::make_shared<spdlog::logger>("name", begin(sinks), end(sinks));
	spdlog::register_logger(spd_logger);
	spd_logger->set_level(log_level);

	spdlog::set_level(log_level);
	spdlog::flush_every(flush_interval);

	spdlog::debug("Logging started, logger_name={}, level={}, cwd={}", logger_name, (int)spd_logger->level(), cwd);


	//test_login();
	
	//test_class();

	test_function();
	
	return 0;
}