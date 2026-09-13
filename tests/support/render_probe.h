// Run the renderer on a scene and read back the picture it wrote.
//
// The renderer runs as a subprocess rather than being linked in. It owns
// global state, a thread pool and a Lua interpreter, so driving it
// in-process would exercise a configuration no user ever has. CMake passes
// the path to the binary, so there is nothing to guess and no stale copy
// to pick up.

#ifndef RAYTRACER_TESTS_SUPPORT_RENDER_PROBE_H_
#define RAYTRACER_TESTS_SUPPORT_RENDER_PROBE_H_

#include <doctest/doctest.h>
#include <lodepng/lodepng.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#ifndef RAYTRACER_EXE
#error "RAYTRACER_EXE must be defined by the build"
#endif

namespace render {

// Scenes are ordinary Lua programs, so the ones that take a parameter read
// it from the environment. Set it before spawning and the child inherits.
inline void SetSceneParameter(const char* name, const std::string& value) {
#ifdef _WIN32
  _putenv_s(name, value.c_str());
#else
  setenv(name, value.c_str(), 1);
#endif
}

// Render a scene file. Returns true if the renderer exited cleanly, and
// fills `output` with everything it printed when one is asked for.
//
// A pipe rather than a redirect to a file: the tests run in parallel, so a
// log on disk would need a unique name per process and would leave one
// behind for every test.
inline bool RenderScene(const std::string& scene_file,
                        std::string* output = nullptr) {
  // The outer quotes are for cmd.exe, which strips one layer before it
  // sees a command whose program path is itself quoted.
  const std::string command =
      "\"\"" RAYTRACER_EXE "\" \"" + scene_file + "\" 2>&1\"";

#ifdef _WIN32
  FILE* pipe = _popen(command.c_str(), "r");
#else
  FILE* pipe = popen(command.c_str(), "r");
#endif
  if (pipe == nullptr) return false;

  std::string captured;
  char chunk[4096];
  while (std::fgets(chunk, sizeof chunk, pipe) != nullptr) captured += chunk;

#ifdef _WIN32
  const int status = _pclose(pipe);
#else
  const int status = pclose(pipe);
#endif

  if (output != nullptr) *output = captured;
  return status == 0;
}

// Case-insensitive substring search. A test that matches renderer output
// should not stop matching when someone recapitalises the message.
inline bool LogContains(const std::string& haystack,
                        const std::string& needle) {
  const auto it =
      std::search(haystack.begin(), haystack.end(), needle.begin(),
                  needle.end(), [](char a, char b) {
                    return std::tolower(static_cast<unsigned char>(a)) ==
                           std::tolower(static_cast<unsigned char>(b));
                  });
  return it != haystack.end();
}

//---------------------------------------------------------------------
// A decoded PNG, and the questions the tests ask of one.
class Image {
 public:
  // Loads `path`, or stays empty if it cannot be decoded.
  explicit Image(const std::string& path) {
    std::vector<unsigned char> rgba;
    unsigned w = 0, h = 0;
    if (lodepng::decode(rgba, w, h, path) != 0) return;

    width_ = w;
    height_ = h;
    rgba_ = std::move(rgba);

    for (size_t i = 0; i + 3 < rgba_.size(); i += 4) {
      for (int c = 0; c < 3; ++c) {  // RGBA, alpha ignored
        const int v = rgba_[i + static_cast<size_t>(c)];
        min_ = std::min(min_, v);
        max_ = std::max(max_, v);
      }
    }
  }

  bool Loaded() const { return width_ > 0 && height_ > 0; }
  unsigned Width() const { return width_; }
  unsigned Height() const { return height_; }

  // Lowest and highest colour byte anywhere in the image. An image of
  // one flat colour has them equal.
  int MinByte() const { return min_; }
  int MaxByte() const { return max_; }

  bool operator==(const Image& other) const {
    return width_ == other.width_ && height_ == other.height_ &&
           rgba_ == other.rgba_;
  }

 private:
  std::vector<unsigned char> rgba_;
  unsigned width_ = 0;
  unsigned height_ = 0;
  int min_ = 255;
  int max_ = 0;
};

// Render and load, asserting on both steps so a missing file fails as a
// missing file rather than as a blank image.
inline Image RenderAndLoad(const std::string& scene_file,
                           const std::string& output_file) {
  INFO("scene ", scene_file);
  REQUIRE(RenderScene(scene_file));

  Image image(output_file);
  INFO("output ", output_file);
  REQUIRE(image.Loaded());
  return image;
}

}  // namespace render

#endif  // RAYTRACER_TESTS_SUPPORT_RENDER_PROBE_H_
