#ifndef RAYTRACER_SRC_RENDER_RENDERER_H_
#define RAYTRACER_SRC_RENDER_RENDERER_H_

#include <glm/glm.hpp>
#include <string>

#include "core/image.h"
#include "core/tone_map.h"
#include "render/aperture.h"
#include "scene/light.h"
#include "scene/scene_node.h"

// Lens, set from Lua. aperture_radius 0 is a pinhole (the default);
// focus_distance is the sharp plane; samples is per pixel; shape sets bokeh.
void SetLens(float aperture_radius, float focus_distance, int samples,
             ApertureShape shape);

// When on, each camera sample draws a time in [0, 1) and moving geometry is
// hit where it is then. Off by default so a static scene's random stream,
// and so its image, is unchanged; the moving-geometry binding turns it on.
void SetMotionBlur(bool enabled);

// Jittered samples per pixel: anti-aliasing and convergence budget. Default 1.
void SetSamplesPerPixel(int samples);

// Hard bounce cap, a safety valve; Russian roulette is the unbiased
// termination rule. Raise it for glass-heavy scenes. Default 8.
void SetMaxDepth(int bounces);

// Also write a snapshot every N samples. 0 (the default) disables it.
void SetSnapshotInterval(int samples);

// Final image path, so snapshots can be named alongside it.
void SetOutputPath(const std::string& path);

// Lat-long PNG environment, decoded here (logging on failure) so Render's
// timer excludes it. Empty (the default) makes `ambient` a uniform emissive
// environment, which the furnace test relies on.
void SetBackground(const std::string& path);

// Tone map + transfer at write-out. Defaults: no tone map, sRGB on.
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
