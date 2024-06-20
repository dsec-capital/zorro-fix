#include "log.h"

#include <stdlib.h>
#include <format>
#include <filesystem>

#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"

#include "common/time_utils.h"

namespace fxcm {

    std::shared_ptr<spdlog::logger> create_logger(const std::string& logger_file, spdlog::level::level_enum log_level) {
        auto cwd = std::filesystem::current_path().string();

        size_t len;
        char log_path[4096];
        getenv_s(&len, log_path, sizeof(log_path), "FXCM_MAKRET_DATA_SERVER_LOG_PATH");
        auto full_log_file_name = len > 0
            ? std::format("{}\\{}", log_path, logger_file)
            : logger_file;
      
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


    std::string normalize_symbol(const std::string& symbol) {
        std::string s = symbol;
        std::replace(s.begin(), s.end(), '/', '_');
        std::replace(s.begin(), s.end(), '-', '_');
        return s;
    }

    std::shared_ptr<spdlog::logger> create_data_logger(const std::string& symbol, const std::string& tag, spdlog::level::level_enum log_level) {
        auto cwd = std::filesystem::current_path().string();

        auto sym = normalize_symbol(symbol);
        auto logger_file = std::format("{}_{}_{}.log", sym, tag, common::timestamp_postfix());

        size_t len;
        char log_path[4096];
        getenv_s(&len, log_path, sizeof(log_path), "FXCM_MAKRET_DATA_SERVER_LOG_PATH");
        auto full_log_file_name = len > 0
            ? std::format("{}\\{}", log_path, logger_file)
            : logger_file;

        auto logger_name = std::format("{}_{}", sym, tag);

        std::vector<spdlog::sink_ptr> sinks{
            std::make_shared<spdlog::sinks::basic_file_sink_mt>(full_log_file_name),
        };
        auto data_logger = std::make_shared<spdlog::logger>(logger_name, begin(sinks), end(sinks));
        spdlog::register_logger(data_logger);
        spdlog::set_level(log_level);
        spdlog::flush_every(std::chrono::seconds(1));

        spdlog::debug("Data logger created, logger_file={}, level={}, cwd={}", full_log_file_name, (int)data_logger->level(), cwd);

        return data_logger;
    }

}