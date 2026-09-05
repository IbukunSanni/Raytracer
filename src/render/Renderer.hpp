#pragma once

#include <glm/glm.hpp>

#include "scene/SceneNode.hpp"
#include "scene/Light.hpp"
#include "core/Image.hpp"
#include "core/ToneMap.hpp"

#include <string>

struct LoadedPng {
	std::vector<unsigned char> RGBA;
	unsigned loadedWidth, loadedHeight;
};


// Camera / sampling settings, driven from Lua before gr.render.
//   apertureRadius 0 => pinhole camera (the default, no depth of field)
//   focusDistance    => distance along the view axis that stays sharp
//   samples          => lens samples per pixel
void SetLens(float apertureRadius, float focusDistance, int samples);

// Total samples per pixel. Every sample is jittered inside the pixel
// footprint, so this is both the anti-aliasing quality and, once the
// renderer becomes stochastic, the convergence budget. Default 1.
void SetSamplesPerPixel(int samples);

// Write a progressive snapshot every N samples, in addition to the final
// image. 0 (the default) writes only the final image.
void SetSnapshotInterval(int samples);

// Where the final image will be written. Needed so snapshots can be named
// alongside it; set by the Lua binding before Render runs.
void SetOutputPath(const std::string & path);

// Tone map + transfer applied at write-out, to the final image and every
// snapshot. Defaults to no tone mapping, sRGB on.
void SetToneMap(const tonemap::Config & cfg);
const tonemap::Config & GetToneMap();

void Render(
		// What to render
		SceneNode * root,

		// Image to write to, set to a given width and height
		Image & image,

		// Viewing parameters
		const glm::vec3 & eye,
		const glm::vec3 & view,
		const glm::vec3 & up,
		double fovy,

		// Lighting parameters
		const glm::vec3 & ambient,
		const std::list<Light *> & lights
);
