#include <memory>
#include <string>
#include <chrono>
#include <iostream>

#include "zorro_fxcm_fix_lib/fix_client.h"
#include "zorro_fxcm_fix_lib/fix_service.h"

#include "common/market_data.h"
#include "common/exec_report.h"
#include "common/blocking_queue.h"
#include "common/utils.h"
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

auto fxcm_account = common::get_env("FIX_ACCOUNT_ID").value();

class FixTest {
public:
	FixTest() : settings_cfg_file(settings_cfg_file_default) {}

	std::string next_client_order_id() {
		++client_order_id;
		auto ts = common::get_current_system_clock().count() / 1000000;
		return std::format("coid_{}_{}", ts, client_order_id);
	}

	template<class T>
	const std::string& get_position_id(const T& order_or_exec_report) {
		return order_or_exec_report.custom_1;
	}

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
	int client_order_id{ 0 };
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

class MarketDataSubscriptionTest : public FixTest {
public:
	MarketDataSubscriptionTest() : FixTest() {};

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

class CollateralInquiryTest : public FixTest {
public:
	CollateralInquiryTest() : FixTest() {};

	void test() {
		service->client().collateral_inquiry(FIX::SubscriptionRequestType_SNAPSHOT_AND_UPDATES);

		FXCMCollateralReport collateral_report;
		auto success = collateral_report_queue.pop(collateral_report, 1s);
		if (success) {
			log::debug<0, true>("collateral report={}", collateral_report.to_string());
		}
	}
};

class TradingSessionStatusTest : public FixTest {
public:
	TradingSessionStatusTest() : FixTest() {};

	void test() {
		service->client().trading_session_status_request();

		FXCMTradingSessionStatus trading_session_status;
		bool success = trading_session_status_queue.pop(trading_session_status, 1s);
		if (success) {
			log::debug<0, true>("trading session status={}", trading_session_status.to_string());
		}
	}
};

class PositionReportsTest : public FixTest {
public:
	PositionReportsTest() : FixTest() {};

	void test() {
		service->client().request_for_positions(fxcm_account, 0, FIX::SubscriptionRequestType_SNAPSHOT_AND_UPDATES);
		service->client().request_for_positions(fxcm_account, 1, FIX::SubscriptionRequestType_SNAPSHOT_AND_UPDATES);

		for (int i = 0; i < 2; ++i) {
			FXCMPositionReports pos_reports;
			auto success = position_snapshot_reports_queue.pop(pos_reports, 1s);
			if (success) {
				log::debug<0, true>("position reports=\n{}", pos_reports.to_string());
			}
		}
	}
};

class OrderTest : public FixTest {
public:
	int amount;
	bool close_after_confirmed;
	FIX::OrdType ord_type;
	FIX::Price limit_price;
	FIX::TimeInForce tif;
	FIX::Symbol symbol;
	FIX::StopPx stop_price;

	OrderTest(
		int amount = 5000,
		bool close_after_confirmed = true,
		const FIX::OrdType& ord_type = FIX::OrdType(FIX::OrdType_MARKET),
		const FIX::Price& limit_price = FIX::Price(0),
		const FIX::StopPx& stop_price = FIX::StopPx(0),
		const FIX::TimeInForce& tif = FIX::TimeInForce(FIX::TimeInForce_GOOD_TILL_CANCEL),
		const FIX::Symbol& symbol = FIX::Symbol("AUD/USD")
	) : FixTest(), 
		amount(amount), 
		close_after_confirmed(close_after_confirmed),
		ord_type(ord_type), 
		limit_price(limit_price),
		stop_price(stop_price),
		tif(tif), 
		symbol(symbol) 
	{};

	void test() {
		auto cl_ord_id = FIX::ClOrdID(next_client_order_id());
		auto qty = FIX::OrderQty(std::abs(amount));
		auto side = amount > 0 ? FIX::Side(FIX::Side_BUY) : FIX::Side(FIX::Side_SELL);

		service->client().new_order_single(symbol, cl_ord_id, side, ord_type, tif, qty, limit_price, stop_price);

		auto report = ord_type == FIX::OrdType_LIMIT
			? pop_exec_report_new(cl_ord_id.getString(), 1s)
			: pop_exec_report_fill(cl_ord_id.getString(), 1s);

		if (report.has_value()) {
			log::debug<0, true>("exec report=\n{}", report.value().to_string());

			if (close_after_confirmed && ord_type == FIX::OrdType_MARKET) {
				auto other_side = amount > 0 ? FIX::Side(FIX::Side_SELL) : FIX::Side(FIX::Side_BUY);
				auto position_id = get_position_id(report.value());
				auto cl_ord_id = FIX::ClOrdID(next_client_order_id());

				service->client().new_order_single(symbol, cl_ord_id, other_side, ord_type, tif, qty, limit_price, stop_price, std::make_optional(position_id));

				auto closing_report = pop_exec_report_fill(cl_ord_id.getString(), 2s);

				if (closing_report.has_value()) {
					log::debug<0, true>("closing exec report=\n{}", closing_report.value().to_string());
				}
				else {
					log::debug<0, true>("closing exec report timed out in {}", 2s);
				}
			}

			if (close_after_confirmed && ord_type == FIX::OrdType_LIMIT) {
				auto ord_id = FIX::OrderID(report.value().ord_id);
				auto orig_cl_ord_id = FIX::OrigClOrdID(report.value().cl_ord_id);
				auto position_id = get_position_id(report.value());
				auto cl_ord_id = FIX::ClOrdID(next_client_order_id());
				auto leaves_qty = FIX::OrderQty(report.value().leaves_qty);

				service->client().order_cancel_request(
					symbol, ord_id, orig_cl_ord_id, cl_ord_id, side, leaves_qty, position_id
				);

				auto cancel_report = pop_exec_report_cancel(ord_id.getString(), 2s);

				if (cancel_report.has_value()) {
					log::debug<0, true>("cancel exec report=\n{}", cancel_report.value().to_string());
				}
				else {
					log::debug<0, true>("cancel exec report timed out in {}", 2s);
				}
			}
		}
	}
};

class OrderCancelReplaceTest : public FixTest {
public:
	int amount;
	int reduce_amount;
	int num_replaces;
	FIX::OrdType ord_type{ FIX::OrdType(FIX::OrdType_LIMIT) };
	FIX::Price limit_price;
	FIX::TimeInForce tif;
	FIX::Symbol symbol;
	FIX::StopPx stop_price;

	OrderCancelReplaceTest(
		int amount = 8000,
		int reduce_amount = 2000,
		int num_replaces = 2,
		const FIX::Price& limit_price = FIX::Price(0),
		const FIX::StopPx& stop_price = FIX::StopPx(0),
		const FIX::TimeInForce& tif = FIX::TimeInForce(FIX::TimeInForce_GOOD_TILL_CANCEL),
		const FIX::Symbol& symbol = FIX::Symbol("AUD/USD")
	) : FixTest(),
		amount(amount),
		reduce_amount(reduce_amount),
		num_replaces(num_replaces),
		limit_price(limit_price),
		stop_price(stop_price),
		tif(tif),
		symbol(symbol)
	{};

	void test() {
		auto cl_ord_id = FIX::ClOrdID(next_client_order_id());
		auto qty = FIX::OrderQty(std::abs(amount));
		auto side = amount > 0 ? FIX::Side(FIX::Side_BUY) : FIX::Side(FIX::Side_SELL);

		service->client().new_order_single(symbol, cl_ord_id, side, ord_type, tif, qty, limit_price, stop_price);

		auto report = pop_exec_report_new(cl_ord_id.getString(), 1s);

		if (report.has_value()) {
			log::debug<0, true>("exec report=\n{}", report.value().to_string());

			auto ord_id = FIX::OrderID(report.value().ord_id);
			auto orig_cl_ord_id = FIX::OrigClOrdID(report.value().cl_ord_id);
			auto position_id = get_position_id(report.value());
			auto leaves_qty = FIX::OrderQty(report.value().leaves_qty);

			auto new_qty = report.value().leaves_qty;

			std::optional<ExecReport> cancel_replace_report;

			for (int i = 0; i < num_replaces; ++i) {
				auto cl_ord_id = FIX::ClOrdID(next_client_order_id());
				new_qty -= reduce_amount;
				service->client().order_cancel_replace_request(
					symbol, ord_id, orig_cl_ord_id, cl_ord_id, side, ord_type, FIX::OrderQty(new_qty), limit_price, tif, position_id
				);

				cancel_replace_report = pop_exec_report_cancel_replace(ord_id.getString(), 2s);

				if (cancel_replace_report.has_value()) {
					log::debug<0, true>("cancel/replace exec report=\n{}", cancel_replace_report.value().to_string());
				}
				else {
					log::debug<0, true>("cancel/replace exec report timed out in {}", 2s);
				}

				auto orig_cl_ord_id = FIX::OrigClOrdID(cl_ord_id.getString());
			}

			auto report_for_cancel_request = cancel_replace_report.value_or(report.value());
			cl_ord_id = FIX::ClOrdID(next_client_order_id());
			orig_cl_ord_id = FIX::OrigClOrdID(report_for_cancel_request.cl_ord_id);
			leaves_qty = FIX::OrderQty(report_for_cancel_request.leaves_qty);

			service->client().order_cancel_request(
				symbol, ord_id, orig_cl_ord_id, cl_ord_id, side, leaves_qty, position_id
			);

			auto cancel_report = pop_exec_report_cancel(ord_id.getString(), 2s);

			if (cancel_report.has_value()) {
				log::debug<0, true>("cancel exec report=\n{}", cancel_report.value().to_string());
			}
			else {
				log::debug<0, true>("cancel exec report timed out in {}", 2s);
			}
		}
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

		test_login();

		MarketDataSubscriptionTest().run();
		CollateralInquiryTest().run();
		TradingSessionStatusTest().run();
		PositionReportsTest().run();
		OrderTest(5000, true, FIX::OrdType(FIX::OrdType_MARKET)).run();
		OrderTest(-5000, true, FIX::OrdType(FIX::OrdType_MARKET)).run();
		OrderTest(5000, true, FIX::OrdType(FIX::OrdType_LIMIT), FIX::Price(0.64)).run();
		OrderTest(-5000, true, FIX::OrdType(FIX::OrdType_LIMIT), FIX::Price(0.69)).run();
		OrderCancelReplaceTest(8000, 2000, 2, FIX::Price(0.64)).run();
		OrderCancelReplaceTest(-8000, 2000, 2, FIX::Price(0.69)).run();
	}
	catch (FIX::UnsupportedMessageType& e) {
		log::debug<0, true>("unsupported message type {}", e.what());
	}

    std::cout << "tests completed" << std::endl;
}

