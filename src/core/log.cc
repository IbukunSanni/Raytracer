#include "core/log.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <mutex>

namespace rt {
namespace log {
namespace {

const int kCatCount = static_cast<int>(Cat::kCount);

const char* cat_names[kCatCount] = {"render", "scene", "geom", "lua", "image"};

const char* level_tags[] = {"ERROR", "WARN ", "INFO ", "DEBUG", "TRACE"};

// Per-category levels. Plain ints rather than atomics: they are written once
// during configuration and only read afterwards, and a torn read of an int
// would at worst emit or drop one line.
int g_level[kCatCount];

std::mutex g_write_mutex;
std::ofstream g_file;
bool g_to_file = false;

Level ParseLevel(const std::string& s, bool& ok) {
  ok = true;
  if (s == "off") return Level::kOff;
  if (s == "error") return Level::kError;
  if (s == "warn") return Level::kWarn;
  if (s == "info") return Level::kInfo;
  if (s == "debug") return Level::kDebug;
  if (s == "trace") return Level::kTrace;
  ok = false;
  return Level::kInfo;
}

int ParseCat(const std::string& s) {
  for (int i = 0; i < kCatCount; ++i) {
    if (s == cat_names[i]) return i;
  }
  return -1;
}

std::string Lower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return s;
}

std::string Trim(const std::string& s) {
  size_t a = s.find_first_not_of(" \t");
  if (a == std::string::npos) return std::string();
  size_t b = s.find_last_not_of(" \t");
  return s.substr(a, b - a + 1);
}

// The level setters, without the initialisation check the public ones carry.
// Start-up uses these: going through the public entry points would re-enter
// the initialisation still running, which deadlocks.
void SetLevelRaw(Cat cat, Level level) {
  const int i = static_cast<int>(cat);
  if (i >= 0 && i < kCatCount) g_level[i] = static_cast<int>(level);
}

void SetLevelAllRaw(Level level) {
  for (int i = 0; i < kCatCount; ++i) g_level[i] = static_cast<int>(level);
}

void ConfigureRaw(const std::string& spec);

// Run once, on the first log statement of the process. A function-local
// static is thread-safe to initialise in C++11, which is what makes this
// usable from the render threads without any explicit guard.
bool Initialise() {
  SetLevelAllRaw(Level::kInfo);

  if (const char* spec = std::getenv("RT_LOG")) {
    ConfigureRaw(spec);
  }

  if (const char* path = std::getenv("RT_LOG_FILE")) {
    g_file.open(path, std::ios::out | std::ios::trunc);
    if (g_file.is_open()) {
      g_to_file = true;
    } else {
      std::cerr << "WARN  [log  ] could not open RT_LOG_FILE '" << path << "'"
                << std::endl;
    }
  }
  return true;
}

void EnsureInit() {
  static const bool kDone = Initialise();
  (void)kDone;
}

}  // namespace

// The public setters initialise first, so that a caller who sets a level
// before anything has been logged is not overwritten a moment later by the
// environment being read. Whoever speaks last wins, and that is the caller.
void SetLevel(Cat cat, Level level) {
  EnsureInit();
  SetLevelRaw(cat, level);
}

void SetLevelAll(Level level) {
  EnsureInit();
  SetLevelAllRaw(level);
}

void Configure(const std::string& spec) {
  EnsureInit();
  ConfigureRaw(spec);
}

namespace {

// "debug"                  -> everything at debug
// "geom:trace,lua:off"     -> per category
// "info,geom:trace"        -> a default, then overrides
void ConfigureRaw(const std::string& spec) {
  size_t pos = 0;
  while (pos <= spec.size()) {
    const size_t comma = spec.find(',', pos);
    const std::string item = Trim(Lower(spec.substr(
        pos, comma == std::string::npos ? std::string::npos : comma - pos)));
    if (!item.empty()) {
      const size_t colon = item.find(':');
      bool ok = false;
      if (colon == std::string::npos) {
        const Level lv = ParseLevel(item, ok);
        if (ok) {
          SetLevelAllRaw(lv);
        } else {
          std::cerr << "WARN  [log  ] unknown RT_LOG level '" << item << "'"
                    << std::endl;
        }
      } else {
        const std::string cat_name = item.substr(0, colon);
        const std::string lv_name = item.substr(colon + 1);
        const int c = ParseCat(cat_name);
        const Level lv = ParseLevel(lv_name, ok);
        if (c < 0) {
          std::cerr << "WARN  [log  ] unknown RT_LOG category '" << cat_name
                    << "'" << std::endl;
        } else if (!ok) {
          std::cerr << "WARN  [log  ] unknown RT_LOG level '" << lv_name << "'"
                    << std::endl;
        } else {
          SetLevelRaw(static_cast<Cat>(c), lv);
        }
      }
    }
    if (comma == std::string::npos) break;
    pos = comma + 1;
  }
}

}  // namespace

bool Enabled(Level level, Cat cat) {
  EnsureInit();
  const int i = static_cast<int>(cat);
  if (i < 0 || i >= kCatCount) return false;
  return static_cast<int>(level) <= g_level[i];
}

Line::Line(Level level, Cat cat)
    : active_(Enabled(level, cat)), level_(level), cat_(cat) {}

Line::~Line() {
  if (!active_) return;

  const int lv = static_cast<int>(level_);
  const char* tag = (lv >= 0 && lv <= 4) ? level_tags[lv] : "?????";

  std::string text;
  text.reserve(buf_.str().size() + 16);
  text += tag;
  text += " [";
  // Pad so the message column lines up regardless of category name length.
  std::string cat = cat_names[static_cast<int>(cat_)];
  cat.resize(6, ' ');
  text += cat;
  text += "] ";
  text += buf_.str();
  text += '\n';

  // One write, under one lock, so a line is never split by another thread.
  // Errors and warnings go to stderr so they survive stdout being piped.
  std::lock_guard<std::mutex> guard(g_write_mutex);
  std::ostream& out = (level_ <= Level::kWarn) ? std::cerr : std::cout;
  out << text;
  out.flush();
  if (g_to_file) {
    g_file << text;
    g_file.flush();
  }
}

}  // namespace log
}  // namespace rt
