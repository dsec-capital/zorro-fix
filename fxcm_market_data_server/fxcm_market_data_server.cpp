//#define WIN32_LEAN_AND_MEAN  

#include <stdlib.h>
#include <iostream>
#include <string>

#include "spdlog/spdlog.h"

#include "log.h"
#include "proxy_server.h"

constexpr const char* real_connection = "Real";
constexpr const char* demo_connection = "Demo";
constexpr const char* default_url = "http://www.fxcorporate.com/Hosts.jsp";

constexpr const char* FXCM_USER_NAME = "FXCM_USER_NAME";
constexpr const char* FXCM_PASSWORD = "FXCM_PASSWORD";

void usage() {
    std::stringstream ss;
    ss << "usage: " << std::endl;
    ss << "  " << FXCM_USER_NAME << "=FXCM account user name" << std::endl;
    ss << "  " << FXCM_PASSWORD << "=FXCM account password" << std::endl;
    ss << "  fxcm_market_data_server [server_host] [server_port] [Demo|Real] [fxcm_server_url]" << std::endl;
    spdlog::info(ss.str());
}

int main(int argc, char* argv[])
{
    auto spd_logger = fxcm::create_logger("fxcm_market_data_server.log");

    char fxcm_login[256];
    char fxcm_password[256];

    {
        size_t len = 0;
        getenv_s(&len, fxcm_login, sizeof(fxcm_login), FXCM_USER_NAME);
        if (len == 0) {
            spdlog::error("environment variable {} not defined", FXCM_USER_NAME);
            usage();
            return -1;
        }
    }
    {
        size_t len = 0;
        getenv_s(&len, fxcm_password, sizeof(fxcm_password), FXCM_PASSWORD);
        if (len == 0) {
            spdlog::error("environment variable {} not defined", FXCM_PASSWORD);
            usage();
            return -1;
        }
    }

    auto server_host = "0.0.0.0";
    auto server_port = 8080;
    auto connection = demo_connection;
    auto url = default_url;

    if (argc >= 2) {
        if (argv[1] == "-h" || argv[1] == "-help") {
            usage();
            return 0;
        }

        server_host = argv[1];
    }

    if (argc >= 3) {
        server_port = atoi(argv[2]);
    }

    if (argc >= 4) {
        connection = argv[3];
    }

    if (argc >= 5) {
        url = argv[4];
    }

    fxcm::ProxyServer server(
        fxcm_login,
        fxcm_password,
        connection,
        url,
        server_host,
        server_port
    );

    if (server.is_ready()) {
        server.run();
    } 

    spdlog::info("server stopped");

    return 0;
}


 