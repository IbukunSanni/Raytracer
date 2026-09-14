// Naming in this file is exempt, via the marker below. The solvers are
// transcriptions of published algorithms (Graphics Gems; Schwarze's quartic)
// and their one-letter identifiers are the ones the papers use -- A/B/C/D for
// coefficients, G and H for the resolvent-cubic terms. Lowercasing them both
// loses that correspondence and collides outright: QuarticRoots declares `h`
// and `g` alongside `H` and `G` in one scope. polyroots.cc also defines a
// `fabs` macro, which the rule would rewrite to FABS -- a real improvement,
// but not one a formatting pass should make silently. The public entry points
// are named to the Google rules by hand instead.
// NOLINTBEGIN(readability-identifier-naming)

/* //-------------------------------------------------------------------------
//
// CS488 -- Introduction to Computer Graphics
//
// polyroots.h/polyroots.cc
//
// Utility functions to solve low-order polynomial equations efficiently
// and robustly.  Very useful when writing ray-object intersection tests.
//
//------------------------------------------------------------------------- */

#ifndef RAYTRACER_SRC_MATH_POLYROOTS_H_
#define RAYTRACER_SRC_MATH_POLYROOTS_H_

#include <stdlib.h>

size_t QuadraticRoots(double A, double B, double C, double roots[2]);
size_t CubicRoots(double A, double B, double C, double roots[3]);
size_t QuarticRoots(double A, double B, double C, double D, double roots[4]);

#endif  // RAYTRACER_SRC_MATH_POLYROOTS_H_

/*
 * Copyright (c) 1990, Graphics and AI Laboratory, University of Washington
 * Copying, use and development for non-commercial purposes permitted.
 *                  All rights for commercial use reserved.
 */
// NOLINTEND(readability-identifier-naming)
