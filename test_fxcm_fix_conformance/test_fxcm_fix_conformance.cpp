#include <memory>
#include <string>
#include <chrono>
#include <iostream>

#include "zorro_fxcm_fix_lib/fix_client.h"
#include "zorro_fxcm_fix_lib/fix_service.h"

#include "common/market_data.h"
#include "common/exec_report.h"
#include "common/blocking_queue.h"
#include "common/time_utils.h"

#include "log.h"
#include "utils.h"

using namespace fxcm;
using namespace std::chrono_literals;

namespace zorro {

	namespace log {
		std::size_t logging_verbosity = dl1;
	}

	int(__cdecl* BrokerError)(const char* txt) = nullptr;
}

std::string settings_cfg_file_default = "zorro_fxcm_fix_client.cfg";

class FixTest {
public:
	FixTest() : settings_cfg_file(settings_cfg_file_default) {}

	void setup() {
		service = create_fix_service(settings_cfg_file);
		service->start();
		auto fix_login_msg = pop_login_service_message(2, 2s);
		ready = fix_login_msg.has_value();
	}

	void teardown() {
		service->client().logout();
		auto fix_logout_msg = pop_logout_service_message(2s);
		service->cancel();
		completed = fix_logout_msg.has_value();
	}

	virtual void test() = 0;

	void run() {
		setup();
		if (ready) {
			test();
		}
		teardown();
	}

	std::string settings_cfg_file;
	bool ready{ false };
	bool completed{ false };
	std::unique_ptr<FixService> service;
};

void test_login() {
	auto service = create_fix_service(settings_cfg_file_default);

	log::debug<0, true>("FIX service starting...");
	service->start();
	log::debug<0, true>("FIX service running");

	log::debug<0, true>("waiting for FIX login...");
	auto fix_login_msg = pop_login_service_message(2, 2s);

	if (fix_login_msg) {
		log::debug<0, true>("FIX login successful");
	}
	else {
		log::debug<0, true>("FIX login failed");
	}

	std::this_thread::sleep_for(1s);

	log::debug<0, true>("waiting for FIX logout...");
	service->client().logout();

	auto fix_logout_msg = pop_logout_service_message(2s);
	if (fix_logout_msg) {
		log::debug<0, true>("FIX lougout successful");
	}
	else {
		log::debug<0, true>("FIX logout failed");
	}

	service->cancel();
}

class FixMarketDataSubscriptionTest : public FixTest {
public:
	FixMarketDataSubscriptionTest() : FixTest() {};

	void test() {
		service->client().subscribe_market_data(FIX::Symbol("AUD/USD"), false);

		auto wait = 2s;
		int n = 5;
		int success = 0;
		for (int i = 0; i < n; ++i) {
			TopOfBook top;
			if (top_of_book_queue.pop(top, wait)) {
				log::debug<0, true>("{}", top.to_string());
				++success;
			}
		}

		log::debug<0, true>("{} out of {} successful top of book updates within timout time {}", success, n, wait);
	}
};

/*	
	Limitations:

	For unknown reasons QuickFix x86 build has an issue when destructing a SocketInitiator.
	The x64 bit version seems to work all fine.

*/
int main()
{
	auto cwd = std::filesystem::current_path().string();

	auto spd_logger = create_logger("test_fxcm_fix_conformace.log", spdlog::level::debug);

	try {

		//test_login();

		FixMarketDataSubscriptionTest().run();
	}
	catch (FIX::UnsupportedMessageType& e) {
		log::debug<0, true>("unsupported message type {}", e.what());
	}

    std::cout << "tests completed" << std::endl;
}

