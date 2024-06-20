//#define WIN32_LEAN_AND_MEAN  

#include <stdlib.h>
#include <iostream>
#include <string>

#include "proxy_server.h"

int main(int argc, char* argv[])
{
    constexpr const char* default_url = "http://www.fxcorporate.com/Hosts.jsp";
    constexpr const char* real_connection = "Real";
    constexpr const char* demo_connection = "Demo";

    if (argc != 1)
    {
        std::cout << "usage: " << argv[0] << " settings_file market_config_file." << std::endl;
        return 0;
    }

    size_t len = 0;
    char fxcm_login[256];
    char fxcm_password[256];
    getenv_s(&len, fxcm_login, sizeof(fxcm_login), "FIX_USER_NAME");
    getenv_s(&len, fxcm_password, sizeof(fxcm_password), "FIX_PASSWORD");

    auto server_host = "0.0.0.0";
    auto server_port = 8080;

    fxcm::ProxyServer server(
        fxcm_login,
        fxcm_password,
        demo_connection,
        default_url,
        server_host,
        server_port
    );

    if (server.is_ready()) {
        server.run();
    } 

    spdlog::info("server stopped");

    return 0;
}

 