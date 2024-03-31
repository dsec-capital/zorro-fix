#include "pch.h"

#include "white_noise.h"
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
            auto alpha_plus = tbl["alpha_plus"].value<double>();
            auto alpha_neg = tbl["alpha_neg"].value<double>();
            auto tick_probs_arr = tbl["tick_probs"].as_array();
            tick_probs_arr->for_each([&tick_probs](toml::value<double>& elem)
                {
                    tick_probs.push_back(elem.get());
                });
            auto sampler = std::make_shared<FodraPham>(
               symbol,
               generator,
               alpha_plus.value(),
               alpha_neg.value(),
               tick_probs,
               tick_size,
               initial_dir
            );
            return sampler;
        }
        else if (model == "white-noise") {
           auto sigma = tbl["sigma"].value<double>();
           auto sampler = std::make_shared<WhiteNoise>(
              symbol,
              generator,
              sigma.value()
           );
           return sampler;
        }
        else {
            return std::shared_ptr<PriceSampler>();
        }
	}
}