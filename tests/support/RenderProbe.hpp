// Run the renderer on a scene and read back the picture it wrote.
//
// The renderer runs as a subprocess rather than being linked in. It owns
// global state, a thread pool and a Lua interpreter, so driving it
// in-process would exercise a configuration no user ever has. CMake passes
// the path to the binary, so there is nothing to guess and no stale copy
// to pick up.

#pragma once

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
inline void setSceneParameter(const char * name, const std::string & value)
{
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

// Case-insensitive substring search. A test that matches renderer output
// should not stop matching when someone recapitalises the message.
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
// A decoded PNG, and the questions the tests ask of one.
class Image {
public:
	// Loads `path`, or stays empty if it cannot be decoded.
	explicit Image(const std::string & path)
	{
		std::vector<unsigned char> rgba;
		unsigned w = 0, h = 0;
		if (lodepng::decode(rgba, w, h, path) != 0)
			return;

		m_width = w;
		m_height = h;
		m_rgba = std::move(rgba);

		for (size_t i = 0; i + 3 < m_rgba.size(); i += 4) {
			for (int c = 0; c < 3; ++c) { // RGBA, alpha ignored
				const int v = m_rgba[i + (size_t) c];
				m_min = std::min(m_min, v);
				m_max = std::max(m_max, v);
			}
		}
	}

	bool loaded() const { return m_width > 0 && m_height > 0; }
	unsigned width() const { return m_width; }
	unsigned height() const { return m_height; }

	// Lowest and highest colour byte anywhere in the image. An image of
	// one flat colour has them equal.
	int minByte() const { return m_min; }
	int maxByte() const { return m_max; }

	bool operator==(const Image & other) const
	{
		return m_width == other.m_width && m_height == other.m_height &&
		       m_rgba == other.m_rgba;
	}

private:
	std::vector<unsigned char> m_rgba;
	unsigned m_width = 0;
	unsigned m_height = 0;
	int m_min = 255;
	int m_max = 0;
};

// Render and load, asserting on both steps so a missing file fails as a
// missing file rather than as a blank image.
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
