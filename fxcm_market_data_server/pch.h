#pragma once

#include <string>
#include <iostream>
#include <list>
#include <map>
#include <vector>
#include <ctime>
#include <cstring>
#include <cmath>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <algorithm>

#include "ForexConnect.h"

//#include "httplib/httplib.h"
//#include "nlohmann/json.h"
//#include "spdlog/spdlog.h"

//#include "spdlog/sinks/basic_file_sink.h"
//#include "spdlog/sinks/stdout_color_sinks.h"

//#include "common/bar.h"
//#include "common/utils.h"

constexpr auto _TIMEOUT = 30000;

#ifndef WIN32
#define _stricmp strcasecmp
#endif

#define WIN32_LEAN_AND_MEAN  // Exclude rarely-used stuff from Windows headers

