#pragma once

#include "pch.h"
#include "utils.h"

#include "common/time_utils.h"

#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"

namespace zorro {

	int logging_verbosity = 2;

	namespace log {

		namespace {
			inline void _show(const std::string& msg) {
				if (!BrokerError) return;
				auto tmsg = "[" + common::now_str() + "] " + msg + "\n";
				BrokerError(tmsg.c_str());
			}
		}

		template <std::size_t level = 0, bool display = true>
		struct info final {
			template <typename... Args>
			constexpr info(std::format_string<Args...> const& fmt, Args &&...args) {
				if constexpr (level > 0) {
					if (logging_verbosity < level) [[likely]]
						return;
				}
				auto msg = std::vformat(fmt.get(), std::make_format_args(args...));
				if constexpr (display) {
					_show(msg);
				}
				spdlog::info(msg);
			}
		};


		template <std::size_t level = 0, bool display = true>
		struct debug final {
			template <typename... Args>
			constexpr debug(std::format_string<Args...> const& fmt, Args &&...args) {
				if constexpr (level > 0) {
					if (logging_verbosity < level) [[likely]]
						return;
				}
				auto msg = std::vformat(fmt.get(), std::make_format_args(args...));
				if constexpr (display) {
					_show(msg);
				}
				spdlog::debug(msg);
			}
		};

		template <bool display = true>
		struct error final {
			template <typename... Args>
			constexpr error(std::format_string<Args...> const& fmt, Args &&...args) {
				auto msg = std::vformat(fmt.get(), std::make_format_args(args...));
				if constexpr (display) {
					_show(msg);
				}
				spdlog::error(msg);
			}
		};

	}

}