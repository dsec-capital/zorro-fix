#ifndef PCH_H
#define PCH_H

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

#include <ForexConnect.h>

#include "CommonSources.h"

#define _TIMEOUT 30000

#ifndef WIN32
#define _stricmp strcasecmp
#endif

#include "framework.h"

#endif //PCH_H
