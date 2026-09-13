// Logging.
//
//     LOG_INFO(RENDER) << "rendering " << n << " spp";
//     LOG_DEBUG(GEOM)  << "mesh " << name << ": " << faces << " faces";
//     LOG_ERROR(LUA)   << "could not open " << path;
//
// Controlled at runtime by the RT_LOG environment variable, so verbosity
// changes without editing a scene or rebuilding:
//
//     RT_LOG=debug              ./build/raytracer scene.lua
//     RT_LOG=geom:trace,lua:off ./build/raytracer scene.lua
//     RT_LOG=off                ./build/raytracer scene.lua
//     RT_LOG_FILE=run.log       ./build/raytracer scene.lua   (also to a file)
//
// Levels are error < warn < info < debug < trace. The default is info, so a
// normal render prints a few lines and nothing else.
//
// Three properties worth knowing about, because they are the reasons this is
// not just a wrapper around std::cout:
//
// 1. DISABLED LOGGING EVALUATES NOTHING. These are macros, so the streamed
//    arguments are never evaluated when the level is off -- LOG_TRACE(GEOM)
//    << ExpensiveDump() costs nothing. A function cannot do this; C++
//    evaluates arguments before the call.
//
// 2. DISABLED LOGGING IS STILL COMPILED. The dead branch is type-checked, so
//    logging cannot silently rot when a variable is renamed. The optimiser
//    removes it.
//
// 3. LINES ARE ATOMIC. `std::cout << a << b` is several calls, so with 20
//    render threads two lines interleave into garbage. Each log statement
//    builds its whole line in a local buffer and performs exactly one
//    guarded write when the statement ends.

#ifndef RAYTRACER_SRC_CORE_LOG_H_
#define RAYTRACER_SRC_CORE_LOG_H_

#include <sstream>
#include <string>

// Compile-time floor. Anything more verbose than this is removed by the
// optimiser and cannot be re-enabled at runtime. Release keeps debug (it is
// only a branch when off) but compiles out trace, which is the level that
// would sit inside inner loops.
#ifndef RT_LOG_LEVEL
#ifdef NDEBUG
#define RT_LOG_LEVEL 3  // through debug
#else
#define RT_LOG_LEVEL 4  // through trace
#endif
#endif

namespace rt {
namespace log {

enum class Level : int {
  kOff = -1,
  kError = 0,
  kWarn = 1,
  kInfo = 2,
  kDebug = 3,
  kTrace = 4,
};

// Categories exist because a single verbosity knob stops being useful the
// moment two subsystems are both noisy: you want BVH traversal without Lua
// binding chatter. Keep this list short.
enum class Cat : int {
  kRender = 0,  // the render loop, camera, sampling
  kScene,       // scene graph construction and traversal
  kGeom,        // primitives, meshes, acceleration structures
  kLua,         // the binding layer and scene files
  kImage,       // framebuffer resolve and file output
  kCount
};

// True if a message at this level and category would be emitted. Reads a
// plain int per category; no locking on the hot path.
bool Enabled(Level level, Cat cat);

// Set programmatically. Normally you want the RT_LOG env var instead.
void SetLevel(Cat cat, Level level);
void SetLevelAll(Level level);

// Parse a spec such as "debug" or "geom:trace,lua:off". Applied automatically
// from RT_LOG on first use; exposed so a scene or a test can override it.
void Configure(const std::string& spec);

// One log statement. Formats into its own buffer and writes once on
// destruction, so a line from one thread never splits across another's.
class Line {
 public:
  Line(Level level, Cat cat);
  ~Line();

  std::ostream& Stream() { return buf_; }

  Line(const Line&) = delete;
  Line& operator=(const Line&) = delete;

 private:
  // Set from Enabled() in the constructor, so a Line built directly -- to
  // assemble one sentence from several statements -- still respects the
  // configured level. The macros additionally avoid evaluating their
  // arguments, which a runtime flag cannot do.
  bool active_;
  Level level_;
  Cat cat_;
  std::ostringstream buf_;
};

}  // namespace log
}  // namespace rt

// The `if (...) {} else` shape rather than a bare `if` is deliberate: it makes
// LOG_X(...) safe to use as the body of an unbraced if/else without the
// trailing else binding to the wrong statement.
//
// The first test is a compile-time constant, so when a level is above the
// floor the whole branch folds away while still being compiled.
#define RT_LOG_AT(lvl, cat)                                              \
  if (static_cast<int>(::rt::log::Level::lvl) > RT_LOG_LEVEL ||          \
      !::rt::log::Enabled(::rt::log::Level::lvl, ::rt::log::Cat::cat)) { \
  } else                                                                 \
    ::rt::log::Line(::rt::log::Level::lvl, ::rt::log::Cat::cat).Stream()

#define LOG_ERROR(cat) RT_LOG_AT(kError, cat)
#define LOG_WARN(cat) RT_LOG_AT(kWarn, cat)
#define LOG_INFO(cat) RT_LOG_AT(kInfo, cat)
#define LOG_DEBUG(cat) RT_LOG_AT(kDebug, cat)
#define LOG_TRACE(cat) RT_LOG_AT(Trace, cat)

#endif  // RAYTRACER_SRC_CORE_LOG_H_
