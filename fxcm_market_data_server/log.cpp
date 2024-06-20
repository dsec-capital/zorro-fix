#include "log.h"

#include <format>
#include <filesystem>

#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"

#include "common/time_utils.h"

namespace fxcm {

    std::shared_ptr<spdlog::logger> create_logger(const std::string& logger_file, spdlog::level::level_enum log_level) {
        auto cwd = std::filesystem::current_path().string();

        auto logger_name = "default";

        std::vector<spdlog::sink_ptr> sinks{
            std::make_shared<spdlog::sinks::stdout_color_sink_mt>(),
            std::make_shared<spdlog::sinks::basic_file_sink_mt>(logger_file),
        };
        auto spd_logger = std::make_shared<spdlog::logger>(logger_name, begin(sinks), end(sinks));
        spdlog::register_logger(spd_logger);
        spdlog::set_default_logger(spd_logger);
        spdlog::set_level(log_level);
        spdlog::flush_every(std::chrono::seconds(1));

        spdlog::debug("Logging started, logger_name={}, logger_file={}, level={}, cwd={}", logger_name, logger_file, (int)spd_logger->level(), cwd);

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
        auto logger_name = std::format("{}_{}", sym, tag);
        auto logger_file = std::format("{}_{}_{}.log", sym, tag, common::timestamp_postfix());

        std::vector<spdlog::sink_ptr> sinks{
            std::make_shared<spdlog::sinks::basic_file_sink_mt>(logger_file),
        };
        auto data_logger = std::make_shared<spdlog::logger>(logger_name, begin(sinks), end(sinks));
        spdlog::register_logger(data_logger);
        spdlog::set_level(log_level);
        spdlog::flush_every(std::chrono::seconds(1));

        spdlog::debug("Data logger created, logger_file={}, level={}, cwd={}", logger_file, (int)data_logger->level(), cwd);

        return data_logger;
    }

}