#ifndef UTILS_H
#define UTILS_H

#include <chrono>

namespace common {

  inline double round_up(double in, double multiple) {
    double m = std::fmod(in, multiple);
    if (m == 0.0) {
      return in;
    }
    else {
      double down = in - m;
      return down + (std::signbit(in) ? -multiple : multiple);
    }
  }

  inline double round_down(double in, double multiple) {
    return in - std::fmod(in, multiple);
  }

  // https://stackoverflow.com/questions/3407012/rounding-up-to-the-nearest-multiple-of-a-number
  inline long long round_up(long long in, long long multiple)
  {
    int is_positive = (int)(in >= 0);
    return ((in + is_positive * (multiple - 1)) / multiple) * multiple;
  }

  inline long long round_down(long long in, long long multiple) {
    return in - in % multiple;
  }

  inline std::chrono::nanoseconds round_up(const std::chrono::nanoseconds& in, const std::chrono::nanoseconds multiple)
  {
      int is_positive = (int)(in.count() >= 0);
      return ((in + is_positive * (multiple - std::chrono::nanoseconds(1))) / multiple) * multiple;
  }

  inline std::chrono::nanoseconds round_down(const std::chrono::nanoseconds& in, const std::chrono::nanoseconds& multiple) {
      return in - in % multiple;
  }
}

#endif
