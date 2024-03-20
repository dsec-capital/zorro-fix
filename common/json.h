#pragma once

#include <map>
#include <chrono>

#include "nlohmann/json.h"

#include "bar_builder.h"

namespace common {

	using json = nlohmann::json;

	inline json to_json(const std::map<std::chrono::nanoseconds, Bar>& bars) {
		std::vector<long long> start;
		std::vector<long long> end;
		std::vector<double> open, close, high, low;
		for (const auto& [key, bar] : bars) {
			start.emplace_back(
				std::chrono::duration_cast<std::chrono::milliseconds>(bar.start).count()
			);
			end.emplace_back(
				std::chrono::duration_cast<std::chrono::milliseconds>(bar.end).count()
			);
			open.emplace_back(bar.open);
			high.emplace_back(bar.high);
			low.emplace_back(bar.low);
			close.emplace_back(bar.close);
		}

		json j;
		j["start"] = start;
		j["end"] = end;
		j["open"] = open;
		j["high"] = high;
		j["low"] = low;
		j["close"] = close;

		return j;
	}

	inline void to_json(json& j, const Bar& b) {
		j = json{
			{"start", b.start.count()},
			{"end", b.end.count()},
			{"open", b.open},
			{"high", b.high},
			{"low", b.low},
			{"close", b.close}
		};
	}

	inline void from_json(const json& j, Bar& b) {
		b.start = std::chrono::nanoseconds(j["start"].template get<long long>());
		b.end = std::chrono::nanoseconds(j["end"].template get<long long>());
		j.at("open").get_to(b.open);
		j.at("high").get_to(b.high);
		j.at("low").get_to(b.low);
		j.at("close").get_to(b.close);
	}

}