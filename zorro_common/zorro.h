#pragma once

#include <windows.h>   

typedef double DATE;
#include "zorro/trading.h"	

#include <string>

#define DLLFUNC extern __declspec(dllexport)
#define DLLFUNC_C extern "C" __declspec(dllexport)

namespace zorro {

    extern int(__cdecl* BrokerError)(const char* txt);

    std::string broker_command_string(int command);

}