#include "pch.h"

#include "fodra_pham.h"
#include "time_utils.h"

namespace common {

    std::shared_ptr<PriceSampler> price_sampler_factory(
        std::mt19937& generator,
        toml::table& tbl,
        const std::string& symbol,
        double price,
        double spread,
        double tick_size,
        int initial_dir 
    ) {
		auto model = tbl["model"].value<std::string>();
        if (model == "fodra-pham") {
            std::vector<double> tick_probs;
            auto bid_volume = tbl["bid_volume"].value<double>();
            auto ask_volume = tbl["ask_volume"].value<double>();
            auto alpha_plus = tbl["alpha_plus"].value<double>();
            auto alpha_neg = tbl["alpha_neg"].value<double>();
            auto tick_probs_arr = tbl["tick_probs"].as_array();
            tick_probs_arr->for_each([&tick_probs](toml::value<double>& elem)
                {
                    tick_probs.push_back(elem.get());
                });
            auto now = get_current_system_clock();
            auto sampler = std::make_shared<FodraPhamSampler<std::mt19937>>(
                generator,
                symbol,
                now,
                alpha_plus.value(),
                alpha_neg.value(),
                tick_probs,
                tick_size,
                price,
                spread,
                bid_volume.value(),
                ask_volume.value(),
                initial_dir
            );
            auto h = std::chrono::hours(tbl["history_period_hours"].value<int>().value());
            auto history_period = std::chrono::duration_cast<std::chrono::nanoseconds>(h);
            auto sample_period = std::chrono::milliseconds(tbl["history_sample_period_millis"].value<int>().value());
            sampler->initialize_history(now - history_period, now, sample_period);

            sampler->create_bars(std::chrono::minutes(1));
            return sampler;
        }
        else {
            return std::shared_ptr<PriceSampler>();
        }
	}
}