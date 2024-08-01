#pragma once

#include <filesystem>

#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"

#include "zorro_common/log.h"

namespace fxcm {

	constexpr std::size_t dl0 = 0;
	constexpr std::size_t dl1 = 1;
	constexpr std::size_t dl2 = 2;

	std::shared_ptr<spdlog::logger> create_logger(const std::string& logger_file, spdlog::level::level_enum log_level) {
		auto cwd = std::filesystem::current_path().string();

		auto full_log_file_name = logger_file;

		auto logger_name = "default";

		std::vector<spdlog::sink_ptr> sinks{
			std::make_shared<spdlog::sinks::stdout_color_sink_mt>(),
			std::make_shared<spdlog::sinks::basic_file_sink_mt>(full_log_file_name),
		};
		auto spd_logger = std::make_shared<spdlog::logger>(logger_name, begin(sinks), end(sinks));
		spdlog::register_logger(spd_logger);
		spdlog::set_default_logger(spd_logger);
		spdlog::set_level(log_level);
		spdlog::flush_every(std::chrono::seconds(1));

		spdlog::debug("Logging started, logger_name={}, logger_file={}, level={}, cwd={}", logger_name, full_log_file_name, (int)spd_logger->level(), cwd);

		return spd_logger;
	}

}