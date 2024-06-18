#pragma once

#include <memory>

#include "spdlog/spdlog.h"

namespace fxcm {

    std::shared_ptr<spdlog::logger> create_logger();

}