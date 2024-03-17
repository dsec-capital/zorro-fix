#pragma once

#include <thread>
#include <httplib/httplib.h>

using namespace httplib;

class RestServer {
    std::string host;
    int port;

    Server server;
    std::atomic_bool done{ false };
    std::thread thread;

public: 
    RestServer(const std::string& host, int port) : host(host), port(port) {
        server.Get("/hi", [](const Request& /*req*/, Response& res) {
            res.set_content("Hello World!", "text/plain");
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