// Raytracer -- thin-lens camera
//
// The renderer has always used a PINHOLE camera: the aperture is a
// single point, so exactly one ray reaches each pixel and every depth
// is equally sharp.  Real lenses have area, and that is the whole
// source of depth of field.
//
// The mechanism: a lens focuses all rays leaving one object point back
// to one sensor point -- but only for object points sitting on the
// focal plane.  A point nearer or farther than that plane lands as a
// small disk on the sensor (the "circle of confusion") whose radius
// grows with the aperture radius and with the distance from the focal
// plane.  So: sharp at the focus distance, progressively blurrier away
// from it, and a wider aperture blurs harder.
//
// To simulate it we do not model glass.  We use the fact above
// directly: every ray that leaves the lens aimed at the same focal
// point must converge there.  So pick the focal point from the pinhole
// ray, then jitter only the ORIGIN across the aperture and re-aim.

#ifndef RAYTRACER_SRC_RENDER_CAMERA_H_
#define RAYTRACER_SRC_RENDER_CAMERA_H_

#include <glm/glm.hpp>

#include "core/ray.h"
#include "render/sampling.h"

// Lens settings.  aperture_radius == 0 reproduces the old pinhole
// camera exactly, which is the default so existing scenes are
// unaffected.
struct LensConfig {
  float aperture_radius = 0.0f;  // world units; 0 => pinhole, no blur
  float focus_distance =
      500.0f;        // distance along the view axis that stays sharp
  int samples = 16;  // lens samples per pixel

  bool Enabled() const { return aperture_radius > 0.0f && samples > 0; }
};

// The camera's orthonormal basis, built once in Render and passed
// down.  u_vec/v_vec span the film plane (and therefore the aperture
// disk); w_vec points along the view direction.
struct CameraBasis {
  glm::vec3 eye;
  glm::vec3 u_vec;  // right
  glm::vec3 v_vec;  // up
  glm::vec3 w_vec;  // forward (normalized view)
};

// ---------------------------------------------------------------------
// >>> YOU IMPLEMENT THIS <<<
//
// Given the pinhole ray for a pixel, produce the ray that a lens of
// radius cfg.aperture_radius would have sent instead.
//
// Inputs:
//   cam      -- camera basis; cam.eye is the lens centre, cam.u_vec and
//               cam.v_vec span the aperture plane, cam.w_vec is forward.
//   pin_dir   -- the UNNORMALIZED direction of the existing pinhole ray
//               for this pixel (as built in RenderBand).
//   cfg      -- aperture radius, focus distance, sample count.
//   rng      -- this thread's generator.
//
// The three steps:
//
//   1. FIND THE FOCAL POINT.  Walk along the pinhole ray until you
//      reach the focal plane -- the plane perpendicular to cam.w_vec at
//      distance cfg.focus_distance in front of cam.eye.  Careful: you
//      want the distance measured ALONG THE VIEW AXIS, not along the
//      ray, otherwise pixels at the edge of frame focus nearer than
//      pixels at the centre (a curved focal surface instead of a flat
//      one).  So scale by focus_distance / dot(pin_dir, cam.w_vec), not
//      by focus_distance / length(pin_dir).
//
//      This is precisely what the old code got wrong: it used
//      dir_vec.z, the raw z component, which only coincides with the
//      view axis when the camera happens to look straight down z.
//
//   2. PICK A POINT ON THE LENS.  Take SampleUnitDisk(rng), scale by
//      cfg.aperture_radius, and map it into world space through the
//      camera basis -- offset = d.x * cam.u_vec + d.y * cam.v_vec.
//      Building the offset from world axes instead of the camera basis
//      is the other thing the old code got wrong; it tilts the
//      aperture relative to the film whenever the camera is rotated.
//
//   3. AIM.  The new origin is cam.eye + offset.  The new direction is
//      (focal_point - new_origin).  Every lens sample for this pixel
//      shares one focal_point, so geometry sitting at the focus
//      distance is hit by all of them and stays sharp, while anything
//      else is struck at a spread of positions and blurs.
//
// Until you fill this in, this returns the pinhole ray unchanged --
// correct image, no blur.
//
// NOTE: SampleUnitDisk() is no longer lens-only. Step 3 made it the body
// of cosine-weighted hemisphere sampling too, via Malley's method, so a
// SHAPED aperture (hex bokeh, a bladed iris) would be right here and
// would silently break every BSDF's pdf/sample agreement. Give the lens
// its own sampler first if you want a shaped one.
inline Ray ThinLensRay(const CameraBasis& cam, const glm::vec3& pin_dir,
                       const LensConfig& cfg, Rng& rng) {
  // TODO: implement steps 1-3 above.
  //
  // Scaffolding placeholder: the pinhole ray, unchanged.
  Ray ray;
  ray.SetOrigin(cam.eye);
  ray.SetDirection(pin_dir);

  (void)cfg;
  (void)rng;
  return ray;
}

#endif  // RAYTRACER_SRC_RENDER_CAMERA_H_
