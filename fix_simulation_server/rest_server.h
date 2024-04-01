#pragma once

#include <thread>
#include <mutex>
#include <format>

#include "httplib/httplib.h"
#include "nlohmann/json.h"

#include "common/time_utils.h"

namespace fix_sim {
   using namespace httplib;
   using namespace nlohmann;

   class RestServer {

      std::string host;
      int port;
      std::map<std::string, Market>& markets;
      std::mutex& mutex;

      Server server;
      std::atomic_bool done{ false };
      std::thread thread;

   public:

      RestServer(
          const std::string& host,
          int port,
          std::map<std::string, Market>& markets,
          std::mutex& mutex
      ) : host(host)
        , port(port)
        , markets(markets)
        , mutex(mutex)
      {
         server.Get("/symbols", [this](const Request& req, Response& res) {
            std::string symbols;
            for (const auto& market : this->markets) {
               symbols += market.first + ",";
            }            
            res.set_content(std::format("symbols={}", symbols), "text/plain");
         });

         // for example http://localhost:8080/bars?symbol=EUR/USD&from=2024-03-30 12:00:00&to=2024-03-30 16:00:00
         server.Get("/bars", [this](const Request& req, Response& res) {
            std::string msg = "====> /bar";

            std::string symbol = "nan";
            std::chrono::nanoseconds from{0};
            std::chrono::nanoseconds to = common::get_current_system_clock();

            if (req.has_param("symbol")) {
               symbol = req.get_param_value("symbol");
               msg += std::format(" symbol={}", symbol);
            }
            if (req.has_param("from")) {
               auto from_param = req.get_param_value("from");
               from = common::parse_datetime(from_param);
               msg += std::format(" from={}", from_param);
            }
            if (req.has_param("to")) {
               auto to_param = req.get_param_value("to");
               to = common::parse_datetime(to_param);
               msg += std::format(" to={}", to_param);
            }
            auto it = this->markets.find(symbol);
            if (it != this->markets.end()) {
               auto [content, n] = it->second.get_bars_as_json(from, to);
               res.set_content(content.dump(), "application/json");
               msg += std::format(" response bars={}", n);
            }
            else {
               json j;
               j["error"] = std::format("no bar data for symbol={}", symbol);
               auto body = j.dump();
               res.set_content(body, "application/json");
               msg += std::format("error={}", body);
            }

            std::cout << msg << std::endl;
         });

         server.Get("/bar_range", [this](const Request& req, Response& res) {
            std::string msg = "====> /bar_range";

            std::string symbol = "nan";
            if (req.has_param("symbol")) {
               symbol = req.get_param_value("symbol");
               msg += std::format(" symbol={}", symbol);
            }
            auto it = this->markets.find(symbol);
            
            if (it != this->markets.end()) {
               auto [from, to, num_bars] = it->second.get_bar_range();
               json j;
               j["symbol"] = symbol;
               j["from"] = from.count();
               j["to"] = to.count();
               j["num_bars"] = num_bars;
               auto body = j.dump();
               res.set_content(body, "application/json");
               msg += std::format("response={}", body);
            }
            else {
               json j;
               j["error"] = std::format("no bar data for symbol={}", symbol);
               auto body = j.dump();
               res.set_content(body, "application/json");
               msg += std::format("error={}", body);
            }

            std::cout << msg << std::endl;
         });
      }

      void run() {
         thread = std::thread([&]() {
            server.listen(host, port);
            done = true;
            });

         //while (!server.is_running() && !done)
         //    std::this_thread::sleep_for(std::chrono::milliseconds{ 10 });
         //server.stop();
         //thread.join();

         std::cout << "====> rest server stated on " << host << ":" << port << std::endl;
      }
   };
}
