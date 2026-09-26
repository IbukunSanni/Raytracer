#ifndef RAYTRACER_SRC_RENDER_RENDERER_H_
#define RAYTRACER_SRC_RENDER_RENDERER_H_

#include <glm/glm.hpp>
#include <string>

#include "core/image.h"
#include "core/tone_map.h"
#include "render/aperture.h"
#include "scene/light.h"
#include "scene/scene_node.h"

struct LoadedPng {
  std::vector<unsigned char> rgba;
  unsigned loaded_width, loaded_height;
};

// Camera / sampling settings, driven from Lua before gr.render.
//   aperture_radius 0 => pinhole camera (the default, no depth of field)
//   focus_distance    => distance along the view axis that stays sharp
//   samples          => lens samples per pixel
//   shape            => the lens opening, and so the shape of the bokeh
void SetLens(float aperture_radius, float focus_distance, int samples,
             ApertureShape shape);

// Total samples per pixel. Every sample is jittered inside the pixel
// footprint, so this is both the anti-aliasing quality and, once the
// renderer becomes stochastic, the convergence budget. Default 1.
void SetSamplesPerPixel(int samples);

// Hard cap on path length, counted in bounces. This is a safety valve, not
// the termination rule -- Russian roulette is, and it is unbiased where
// this cut is not. Raise it for scenes built out of glass. Default 8.
void SetMaxDepth(int bounces);

// Write a progressive snapshot every N samples, in addition to the final
// image. 0 (the default) writes only the final image.
void SetSnapshotInterval(int samples);

// Where the final image will be written. Needed so snapshots can be named
// alongside it; set by the Lua binding before Render runs.
void SetOutputPath(const std::string& path);

// Environment texture, as a lat-long PNG sampled by ray direction. An
// empty path (the default) means no texture, and the scene's `ambient`
// becomes a uniform emissive environment instead -- which is what the
// furnace test needs.
void SetBackground(const std::string& path);

// Tone map + transfer applied at write-out, to the final image and every
// snapshot. Defaults to no tone mapping, sRGB on.
void SetToneMap(const tonemap::Config& cfg);
const tonemap::Config& GetToneMap();

void Render(
    // What to render
    SceneNode* root,

    // Image to write to, set to a given width and height
    Image& image,

    // Viewing parameters
    const glm::vec3& eye, const glm::vec3& view, const glm::vec3& up,
    double fovy,

    // Lighting parameters
    const glm::vec3& ambient, const std::list<Light*>& lights);

#endif  // RAYTRACER_SRC_RENDER_RENDERER_H_
