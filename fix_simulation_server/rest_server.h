#pragma once

#include <thread>
#include <mutex>
#include <format>

#include <httplib/httplib.h>

using namespace httplib;

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
        server.Get("/test", [](const Request& req, Response& res) {
            std::string arg = "nan";
            if (req.has_param("symbol")) {
                arg = req.get_param_value("arg");
            }
            res.set_content(std::format("test : arg={}", arg), "text/plain");
            });

        server.Get("/bars", [](const Request& req, Response& res) {
            std::string symbol = "nan";
            if (req.has_param("symbol")) {
                symbol = req.get_param_value("symbol");
            }
            res.set_content(std::format("symbol={} body=<{}>", symbol, req.body), "text/plain");
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

        std::cout << "rest server stated on " << host << ":" << port << std::endl;
    }
};