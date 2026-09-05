// Termm--Fall 2020

#pragma once

#include <glm/glm.hpp>

#include "SceneNode.hpp"
#include "Light.hpp"
#include "Image.hpp"

struct LoadedPng {
	std::vector<unsigned char> RGBA;
	unsigned loadedWidth, loadedHeight;
};


// Camera / sampling settings, driven from Lua before gr.render.
//   apertureRadius 0 => pinhole camera (the default, no depth of field)
//   focusDistance    => distance along the view axis that stays sharp
//   samples          => lens samples per pixel
void A4_SetLens(float apertureRadius, float focusDistance, int samples);

// samples per pixel for anti-aliasing; 1 disables it (the default).
void A4_SetAntiAliasing(int samples);

void A4_Render(
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
