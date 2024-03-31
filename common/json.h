#pragma once

#include <map>
#include <chrono>

#include "nlohmann/json.h"

#include "bar_builder.h"

namespace common {

	using json = nlohmann::json;

	inline std::pair<json, int> to_json(
		const std::chrono::nanoseconds& from, 
		const std::chrono::nanoseconds& to, 
		const std::map<std::chrono::nanoseconds, Bar>& bars
	) {
		std::vector<long long> end;
		std::vector<double> open, close, high, low;
		int count = 0;
		for (const auto& [key, bar] : bars) {
			if (from <= bar.end && bar.end <= to) {
				end.emplace_back(
					std::chrono::duration_cast<std::chrono::nanoseconds>(bar.end).count()
				);
				open.emplace_back(bar.open);
				high.emplace_back(bar.high);
				low.emplace_back(bar.low);
				close.emplace_back(bar.close);
				++count;
			}
		}

		json j;
		j["end"] = end;
		j["open"] = open;
		j["high"] = high;
		j["low"] = low;
		j["close"] = close;

		return std::make_pair(j, count);
	}

	inline void from_json(const json& j, std::map<std::chrono::nanoseconds, Bar>& bars) {
		auto end = j["end"].get<std::vector<long long>>();
		auto open = j["open"].get<std::vector<double>>();
		auto high = j["high"].get<std::vector<double>>();
		auto low = j["low"].get<std::vector<double>>();
		auto close = j["close"].get<std::vector<double>>();
		auto n = end.size();
		assert(open.size() == n && high.size() == n && low.size() == n && close.size() == n);

		bars.clear();
		for (size_t i = 0; i < n; ++i) {
			bars.try_emplace(
				std::chrono::nanoseconds(end[i]),
				std::chrono::nanoseconds(end[i]),
				open[i],
				high[i],
				low[i],
				close[i]
			);
		}
	}

	inline void to_json(json& j, const Bar& b) {
		j = json{
			{"end", b.end.count()},
			{"open", b.open},
			{"high", b.high},
			{"low", b.low},
			{"close", b.close}
		};
	}

	inline void from_json(const json& j, Bar& b) {
		b.end = std::chrono::nanoseconds(j["end"].template get<long long>());
		j.at("open").get_to(b.open);
		j.at("high").get_to(b.high);
		j.at("low").get_to(b.low);
		j.at("close").get_to(b.close);
	}
}