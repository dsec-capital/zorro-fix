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

	inline json to_json(const std::vector<BidAskBar<double>>& bars) {
		std::vector<double> timestamp;
		std::vector<double> bid_open, bid_close, bid_high, bid_low, ask_open, ask_close, ask_high, ask_low, volume;
		int count = 0;
		for (const auto& bar : bars) {
			timestamp.emplace_back(bar.timestamp);
			bid_open.emplace_back(bar.bid_open);
			bid_high.emplace_back(bar.bid_high);
			bid_low.emplace_back(bar.bid_low);
			bid_close.emplace_back(bar.bid_close);
			ask_open.emplace_back(bar.ask_open);
			ask_high.emplace_back(bar.ask_high);
			ask_low.emplace_back(bar.ask_low);
			ask_close.emplace_back(bar.ask_close);
			volume.emplace_back(bar.volume);
		}

		json j;
		j["timestamp"] = timestamp;
		j["bid_open"] = bid_open;
		j["bid_high"] = bid_high;
		j["bid_low"] = bid_low;
		j["bid_close"] = bid_close;
		j["ask_open"] = ask_open;
		j["ask_high"] = ask_high;
		j["ask_low"] = ask_low;
		j["ask_close"] = ask_close;
		j["volume"] = volume;

		return j;
	}

	inline json to_json(const std::vector<Quote<double>>& quotes) {
		std::vector<double> timestamp;
		std::vector<double> bid, ask;
		int count = 0;
		for (const auto& quote : quotes) {
			timestamp.emplace_back(quote.timestamp);
			bid.emplace_back(quote.bid);
			ask.emplace_back(quote.ask);
		}

		json j;
		j["timestamp"] = timestamp;
		j["bid"] = bid;
		j["ask"] = ask;

		return j;
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

	inline void from_json(const json& j, std::vector<BidAskBar<double>>& bars) {
		auto timestamp = j["timestamp"].get<std::vector<double>>();
		auto n = timestamp.size();

		auto bid_open = j["bid_open"].get<std::vector<double>>();
		auto bid_high = j["bid_high"].get<std::vector<double>>();
		auto bid_low = j["bid_low"].get<std::vector<double>>();
		auto bid_close = j["bid_close"].get<std::vector<double>>();
		assert(bid_open.size() == n && bid_high.size() == n && bid_low.size() == n && bid_close.size() == n);

		auto ask_open = j["ask_open"].get<std::vector<double>>();
		auto ask_high = j["ask_high"].get<std::vector<double>>();
		auto ask_low = j["ask_low"].get<std::vector<double>>();
		auto ask_close = j["ask_close"].get<std::vector<double>>();
		auto volume = j["volume"].get<std::vector<double>>();
		assert(ask_open.size() == n && ask_high.size() == n && ask_low.size() == n && ask_close.size() == n && volume.size() == n);

		bars.clear();
		for (size_t i = 0; i < n; ++i) {
			bars.emplace_back(
				timestamp[i],
				bid_open[i],
				bid_high[i],
				bid_low[i],
				bid_close[i],
				ask_open[i],
				ask_high[i],
				ask_low[i],
				ask_close[i],
				volume[i]
				);
		}
	}

	inline void from_json(const json& j, std::vector<Quote<double>>& quotes) {
		auto timestamp = j["timestamp"].get<std::vector<double>>();
		auto n = timestamp.size();

		auto bid = j["bid"].get<std::vector<double>>();
		auto ask = j["ask"].get<std::vector<double>>();
		assert(bid.size() == n && ask.size() == n);

		quotes.clear();
		for (size_t i = 0; i < n; ++i) {
			quotes.emplace_back(
				timestamp[i],
				bid[i],
				ask[i]
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