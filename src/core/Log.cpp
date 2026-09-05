#include "core/Log.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <mutex>

namespace rt {
namespace log {
namespace {

const int CAT_COUNT = (int) Cat::COUNT;

const char * CAT_NAMES[CAT_COUNT] = {
	"render", "scene", "geom", "lua", "image"
};

const char * LEVEL_TAGS[] = {
	"ERROR", "WARN ", "INFO ", "DEBUG", "TRACE"
};

// Per-category levels. Plain ints rather than atomics: they are written once
// during configuration and only read afterwards, and a torn read of an int
// would at worst emit or drop one line.
int g_level[CAT_COUNT];

std::mutex    g_writeMutex;
std::ofstream g_file;
bool          g_toFile = false;

Level parseLevel(const std::string & s, bool & ok)
{
	ok = true;
	if (s == "off")   return Level::Off;
	if (s == "error") return Level::Error;
	if (s == "warn")  return Level::Warn;
	if (s == "info")  return Level::Info;
	if (s == "debug") return Level::Debug;
	if (s == "trace") return Level::Trace;
	ok = false;
	return Level::Info;
}

int parseCat(const std::string & s)
{
	for (int i = 0; i < CAT_COUNT; ++i) {
		if (s == CAT_NAMES[i]) return i;
	}
	return -1;
}

std::string lower(std::string s)
{
	std::transform(s.begin(), s.end(), s.begin(),
	               [](unsigned char c){ return (char) std::tolower(c); });
	return s;
}

std::string trim(const std::string & s)
{
	size_t a = s.find_first_not_of(" \t");
	if (a == std::string::npos) return std::string();
	size_t b = s.find_last_not_of(" \t");
	return s.substr(a, b - a + 1);
}

// Run once, on the first log statement of the process. A function-local
// static is thread-safe to initialise in C++11, which is what makes this
// usable from the render threads without any explicit guard.
bool initialise()
{
	setLevelAll(Level::Info);

	if (const char * spec = std::getenv("RT_LOG")) {
		configure(spec);
	}

	if (const char * path = std::getenv("RT_LOG_FILE")) {
		g_file.open(path, std::ios::out | std::ios::trunc);
		if (g_file.is_open()) {
			g_toFile = true;
		} else {
			std::cerr << "WARN  [log  ] could not open RT_LOG_FILE '"
			          << path << "'" << std::endl;
		}
	}
	return true;
}

void ensureInit()
{
	static const bool done = initialise();
	(void) done;
}

} // namespace

void setLevel(Cat cat, Level level)
{
	const int i = (int) cat;
	if (i >= 0 && i < CAT_COUNT) g_level[i] = (int) level;
}

void setLevelAll(Level level)
{
	for (int i = 0; i < CAT_COUNT; ++i) g_level[i] = (int) level;
}

// "debug"                  -> everything at debug
// "geom:trace,lua:off"     -> per category
// "info,geom:trace"        -> a default, then overrides
void configure(const std::string & spec)
{
	size_t pos = 0;
	while (pos <= spec.size()) {
		const size_t comma = spec.find(',', pos);
		const std::string item = trim(lower(
			spec.substr(pos, comma == std::string::npos ? std::string::npos
			                                            : comma - pos)));
		if (!item.empty()) {
			const size_t colon = item.find(':');
			bool ok = false;
			if (colon == std::string::npos) {
				const Level lv = parseLevel(item, ok);
				if (ok) {
					setLevelAll(lv);
				} else {
					std::cerr << "WARN  [log  ] unknown RT_LOG level '"
					          << item << "'" << std::endl;
				}
			} else {
				const std::string catName = item.substr(0, colon);
				const std::string lvName  = item.substr(colon + 1);
				const int c = parseCat(catName);
				const Level lv = parseLevel(lvName, ok);
				if (c < 0) {
					std::cerr << "WARN  [log  ] unknown RT_LOG category '"
					          << catName << "'" << std::endl;
				} else if (!ok) {
					std::cerr << "WARN  [log  ] unknown RT_LOG level '"
					          << lvName << "'" << std::endl;
				} else {
					setLevel((Cat) c, lv);
				}
			}
		}
		if (comma == std::string::npos) break;
		pos = comma + 1;
	}
}

bool enabled(Level level, Cat cat)
{
	ensureInit();
	const int i = (int) cat;
	if (i < 0 || i >= CAT_COUNT) return false;
	return (int) level <= g_level[i];
}

Line::Line(Level level, Cat cat)
	: m_active(enabled(level, cat))
	, m_level(level)
	, m_cat(cat)
{
}

Line::~Line()
{
	if (!m_active) return;

	const int lv = (int) m_level;
	const char * tag = (lv >= 0 && lv <= 4) ? LEVEL_TAGS[lv] : "?????";

	std::string text;
	text.reserve(m_buf.str().size() + 16);
	text += tag;
	text += " [";
	// Pad so the message column lines up regardless of category name length.
	std::string cat = CAT_NAMES[(int) m_cat];
	cat.resize(6, ' ');
	text += cat;
	text += "] ";
	text += m_buf.str();
	text += '\n';

	// One write, under one lock, so a line is never split by another thread.
	// Errors and warnings go to stderr so they survive stdout being piped.
	std::lock_guard<std::mutex> guard(g_writeMutex);
	std::ostream & out = (m_level <= Level::Warn) ? std::cerr : std::cout;
	out << text;
	out.flush();
	if (g_toFile) {
		g_file << text;
		g_file.flush();
	}
}

} // namespace log
} // namespace rt
