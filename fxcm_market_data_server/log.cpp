#include "log.h"

#include <filesystem>

#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"

namespace fxcm {

    std::shared_ptr<spdlog::logger> create_logger() {
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

        return spd_logger;
    }

}