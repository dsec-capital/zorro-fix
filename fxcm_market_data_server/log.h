#pragma once

#include <memory>

#include "spdlog/spdlog.h"

namespace fxcm {

    std::shared_ptr<spdlog::logger> create_logger(
        const std::string& logger_file, 
        spdlog::level::level_enum log_level = spdlog::level::debug
    );

    std::shared_ptr<spdlog::logger> create_data_logger(
        const std::string& symbol, 
        const std::string& tag, 
        spdlog::level::level_enum log_level = spdlog::level::debug
    );
}