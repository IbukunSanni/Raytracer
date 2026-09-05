// A4 -- thin-lens camera
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

#pragma once

#include "RayTracer.hpp"
#include "Sampling.hpp"

#include <glm/glm.hpp>

// Lens settings.  apertureRadius == 0 reproduces the old pinhole
// camera exactly, which is the default so existing scenes are
// unaffected.
struct LensConfig {
	float apertureRadius = 0.0f;   // world units; 0 => pinhole, no blur
	float focusDistance  = 500.0f; // distance along the view axis that stays sharp
	int   samples        = 16;     // lens samples per pixel

	bool enabled() const { return apertureRadius > 0.0f && samples > 0; }
};

// The camera's orthonormal basis, built once in A4_Render and passed
// down.  uVec/vVec span the film plane (and therefore the aperture
// disk); wVec points along the view direction.
struct CameraBasis {
	glm::vec3 eye;
	glm::vec3 uVec; // right
	glm::vec3 vVec; // up
	glm::vec3 wVec; // forward (normalized view)
};

// ---------------------------------------------------------------------
// >>> YOU IMPLEMENT THIS <<<
//
// Given the pinhole ray for a pixel, produce the ray that a lens of
// radius cfg.apertureRadius would have sent instead.
//
// Inputs:
//   cam      -- camera basis; cam.eye is the lens centre, cam.uVec and
//               cam.vVec span the aperture plane, cam.wVec is forward.
//   pinDir   -- the UNNORMALIZED direction of the existing pinhole ray
//               for this pixel (as built in generatePixelColors).
//   cfg      -- aperture radius, focus distance, sample count.
//   rng      -- this thread's generator.
//
// The three steps:
//
//   1. FIND THE FOCAL POINT.  Walk along the pinhole ray until you
//      reach the focal plane -- the plane perpendicular to cam.wVec at
//      distance cfg.focusDistance in front of cam.eye.  Careful: you
//      want the distance measured ALONG THE VIEW AXIS, not along the
//      ray, otherwise pixels at the edge of frame focus nearer than
//      pixels at the centre (a curved focal surface instead of a flat
//      one).  So scale by focusDistance / dot(pinDir, cam.wVec), not
//      by focusDistance / length(pinDir).
//
//      This is precisely what the old code got wrong: it used
//      dirVec.z, the raw z component, which only coincides with the
//      view axis when the camera happens to look straight down z.
//
//   2. PICK A POINT ON THE LENS.  Take sampleUnitDisk(rng), scale by
//      cfg.apertureRadius, and map it into world space through the
//      camera basis -- offset = d.x * cam.uVec + d.y * cam.vVec.
//      Building the offset from world axes instead of the camera basis
//      is the other thing the old code got wrong; it tilts the
//      aperture relative to the film whenever the camera is rotated.
//
//   3. AIM.  The new origin is cam.eye + offset.  The new direction is
//      (focalPoint - newOrigin).  Every lens sample for this pixel
//      shares one focalPoint, so geometry sitting at the focus
//      distance is hit by all of them and stays sharp, while anything
//      else is struck at a spread of positions and blurs.
//
// Until you fill this in, sampleUnitDisk returns (0,0), the offset is
// zero, and this collapses back to the pinhole ray -- correct image,
// no blur.
inline RayTracer thinLensRay(
		const CameraBasis & cam,
		const glm::vec3 & pinDir,
		const LensConfig & cfg,
		Rng & rng)
{
	// TODO: implement steps 1-3 above.
	//
	// Scaffolding placeholder: the pinhole ray, unchanged.
	RayTracer ray;
	ray.setOrigin(cam.eye);
	ray.setDirection(pinDir);

	(void) cfg;
	(void) rng;
	return ray;
}
