#include "log.h"

#include <filesystem>

#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"

namespace fxcm {

    // BUG does not log to file, although it should see https://github.com/gabime/spdlog/issues/1037
    std::shared_ptr<spdlog::logger> create_logger() {
        auto cwd = std::filesystem::current_path().string();

        auto logger_name = "combined";
        auto logger_file = "fxcm_proxy_server.log";
        auto log_level = spdlog::level::debug;

        std::vector<spdlog::sink_ptr> sinks{
            std::make_shared<spdlog::sinks::stdout_color_sink_mt>(),
            std::make_shared<spdlog::sinks::basic_file_sink_mt>(logger_file),
        };
        auto spd_logger = std::make_shared<spdlog::logger>(logger_name, begin(sinks), end(sinks));
        spdlog::register_logger(spd_logger);
        spdlog::set_level(log_level);
        spdlog::flush_every(std::chrono::seconds(1));

        spdlog::debug("Logging started, logger_name={}, level={}, cwd={}", logger_name, (int)spd_logger->level(), cwd);

        return spd_logger;
    }

}