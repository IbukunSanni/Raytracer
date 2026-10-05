// Raytracer -- opt-in switch for render-path work counters

#ifndef RAYTRACER_SRC_CORE_STATS_H_
#define RAYTRACER_SRC_CORE_STATS_H_

#include <cstdlib>
#include <cstring>

namespace rt {
namespace stats {

// True when RT_STATS is set to anything other than "" or "0".
inline bool ReadEnabled() {
  const char* value = std::getenv("RT_STATS");
  return value != nullptr && *value != '\0' && std::strcmp(value, "0") != 0;
}

// RT_STATS=1 turns on every counter on the render path. Off by default:
// publishing counts costs ~17-20% of render time, so timed runs must not
// count. Read once before main, so a check is a load, not a getenv.
inline const bool kEnabled = ReadEnabled();

}  // namespace stats
}  // namespace rt

#endif  // RAYTRACER_SRC_CORE_STATS_H_
