// Raytracer -- thin-lens camera
//
// A pinhole aperture is a single point, so every depth is equally sharp.
// An aperture with area is not: rays leaving one object point reconverge
// only if that point lies on the focal plane, and spread into a disk
// otherwise.  That spread is the depth of field.

#ifndef RAYTRACER_SRC_RENDER_CAMERA_H_
#define RAYTRACER_SRC_RENDER_CAMERA_H_

#include <glm/glm.hpp>

#include "core/ray.h"
#include "render/aperture.h"
#include "render/sampling.h"

// Lens settings.  A zero radius is the pinhole default, so a scene that
// asks for nothing gets no blur.
struct LensConfig {
  float aperture_radius = 0.0f;   // world units; 0 => pinhole
  float focus_distance = 500.0f;  // along the view axis; stays sharp
  int samples = 16;               // lens samples per pixel
  ApertureShape shape = ApertureShape::kDisk;  // the shape of the bokeh

  bool Enabled() const { return aperture_radius > 0.0f && samples > 0; }
};

// The camera's orthonormal basis, built once in Render.  u_vec/v_vec span
// the film plane and therefore the aperture disk.  All three are unit
// length, so an offset built from them is already in world units.  The
// triple is left-handed (u x v = -w) because w points along the view.
struct CameraBasis {
  glm::vec3 eye;    // lens centre
  glm::vec3 u_vec;  // screen right
  glm::vec3 v_vec;  // screen up
  glm::vec3 w_vec;  // forward (normalized view)
};

// The ray a lens of radius cfg.aperture_radius sends for the pixel whose
// pinhole direction is pin_dir (unnormalized).
//
// The shape comes from SampleAperture rather than from SampleUnitDisk,
// which also supplies cosine-weighted hemisphere sampling via Malley's
// method and would carry a cut-out straight into every BSDF's pdf.
inline Ray ThinLensRay(const CameraBasis& cam, const glm::vec3& pin_dir,
                       const LensConfig& cfg, Rng& rng) {
  // Projecting onto the view axis, rather than dividing by length(pin_dir),
  // is what makes the focal surface a plane instead of a sphere around the
  // eye -- otherwise the frame edges focus nearer than its centre.
  const float focus_t = cfg.focus_distance / glm::dot(pin_dir, cam.w_vec);
  const glm::vec3 focal_point_vec = cam.eye + focus_t * pin_dir;

  // A point on the aperture, as an offset from the eye.
  const glm::vec2 lens_uv =
      SampleAperture(cfg.shape, rng) * cfg.aperture_radius;
  const glm::vec3 lens_offset_vec =
      lens_uv.x * cam.u_vec + lens_uv.y * cam.v_vec;

  // Every sample for this pixel aims at the one focal point, so geometry at
  // the focus distance is struck by all of them and stays sharp.  Anything
  // else is struck at a spread of positions and blurs.
  const glm::vec3 lens_origin_vec = cam.eye + lens_offset_vec;

  Ray ray;
  ray.SetOrigin(lens_origin_vec);
  ray.SetDirection(focal_point_vec - lens_origin_vec);
  return ray;
}

#endif  // RAYTRACER_SRC_RENDER_CAMERA_H_
