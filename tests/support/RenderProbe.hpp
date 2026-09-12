// Drive the renderer and read back what it produced.
//
// These tests are end-to-end on purpose. Everything bsdf_test.cpp proves
// about a material in isolation can still be undone by the integrator, the
// transform stack or the image writer, and only a render exercises those.
//
// The renderer runs as a subprocess rather than being linked in: it owns
// global state, a thread pool and a Lua interpreter, and a test that drove
// it in-process would be testing a configuration no user ever has. The
// path to the binary comes from CMake, so there is nothing to guess.

#pragma once

#include <doctest/doctest.h>
#include <lodepng/lodepng.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

#ifndef _WIN32
#include <cstdlib>
#endif

#ifndef RAYTRACER_EXE
#error "RAYTRACER_EXE must be defined by the build -- see tests/CMakeLists.txt"
#endif

namespace render {

//---------------------------------------------------------------------
// Scenes are ordinary Lua programs, so the ones that need a parameter read
// it from the environment. Set it here, before the child is spawned, and
// the child inherits it.
inline void setSceneParameter(const char * name, const std::string & value)
{
#ifdef _WIN32
	_putenv_s(name, value.c_str());
#else
	setenv(name, value.c_str(), 1);
#endif
}

// Run the renderer on a scene file. Returns true if it exited cleanly, and
// fills `output` with everything it printed when one is asked for.
//
// A pipe rather than a redirect to a file: the renders run in parallel
// under ctest, and a shared log would need a name per process and would
// leave one behind for every test.
inline bool renderScene(const std::string & sceneFile,
                        std::string * output = nullptr)
{
	// The outer quotes are for cmd.exe, which strips one layer before it
	// sees a command whose program path is itself quoted.
	const std::string command =
	    "\"\"" RAYTRACER_EXE "\" \"" + sceneFile + "\" 2>&1\"";

#ifdef _WIN32
	FILE * pipe = _popen(command.c_str(), "r");
#else
	FILE * pipe = popen(command.c_str(), "r");
#endif
	if (pipe == nullptr)
		return false;

	std::string captured;
	char chunk[4096];
	while (std::fgets(chunk, sizeof chunk, pipe) != nullptr)
		captured += chunk;

#ifdef _WIN32
	const int status = _pclose(pipe);
#else
	const int status = pclose(pipe);
#endif

	if (output != nullptr)
		*output = captured;
	return status == 0;
}

// Case-insensitive substring search, so a test asserting on a log message
// does not silently stop matching when someone recapitalises it. The
// original of this check looked for "BVH MISMATCH" while the renderer
// logged "bvh mismatch", and passed for a year without ever comparing
// anything.
inline bool logContains(const std::string & haystack, const std::string & needle)
{
	const auto it = std::search(
	    haystack.begin(), haystack.end(), needle.begin(), needle.end(),
	    [](char a, char b) {
		    return std::tolower((unsigned char) a) == std::tolower((unsigned char) b);
	    });
	return it != haystack.end();
}

//---------------------------------------------------------------------
// A decoded PNG, plus the two questions the tests ask of one.
class Image {
public:
	// Loads `path`, or leaves the image empty if it cannot be decoded.
	explicit Image(const std::string & path)
	{
		unsigned w = 0, h = 0;
		if (lodepng::decode(m_rgba, w, h, path) == 0) {
			m_width = w;
			m_height = h;
		}
	}

	bool loaded() const { return m_width > 0 && m_height > 0; }
	unsigned width() const { return m_width; }
	unsigned height() const { return m_height; }

	// Lowest and highest colour byte anywhere in the image, alpha ignored.
	// A uniform image has min == max, which is how the furnace tests ask
	// whether a sphere is invisible against its environment.
	int minByte() const { return extremes().first; }
	int maxByte() const { return extremes().second; }

	bool operator==(const Image & other) const
	{
		return m_width == other.m_width && m_height == other.m_height &&
		       m_rgba == other.m_rgba;
	}

private:
	std::pair<int, int> extremes() const
	{
		int lo = 255, hi = 0;
		for (size_t i = 0; i + 3 < m_rgba.size(); i += 4) {
			for (int c = 0; c < 3; ++c) { // RGBA, alpha skipped
				const int v = m_rgba[i + (size_t) c];
				lo = std::min(lo, v);
				hi = std::max(hi, v);
			}
		}
		return {lo, hi};
	}

	std::vector<unsigned char> m_rgba;
	unsigned m_width = 0;
	unsigned m_height = 0;
};

//---------------------------------------------------------------------
// Render a scene and load what it wrote, asserting on both steps so a
// missing file fails as a missing file rather than as a blank image.
inline Image renderAndLoad(const std::string & sceneFile,
                           const std::string & outputFile)
{
	INFO("scene ", sceneFile);
	REQUIRE(renderScene(sceneFile));

	Image image(outputFile);
	INFO("output ", outputFile);
	REQUIRE(image.loaded());
	return image;
}

} // namespace render
