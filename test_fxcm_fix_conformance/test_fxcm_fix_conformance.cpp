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

std::string settings_cfg_file = "zorro_fxcm_fix_client.cfg";

void test_login() {
	auto service = create_fix_service(settings_cfg_file);

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

	service->cancel();
}

int main()
{
	auto cwd = std::filesystem::current_path().string();

	auto spd_logger = create_logger("test_fxcm_fix_conformace.log", spdlog::level::debug);

	test_login();

    std::cout << "Hello World!\n";
}

