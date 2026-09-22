#!/usr/bin/env python3
"""Generate the lat-long environment map that lights assets/scenes/caustic.lua.

A caustic needs a concentrated source. A uniform environment cannot produce
one -- refraction turns uniform radiance into uniform radiance -- so this
writes a dim sky with a single small bright disc, which BSDF sampling can
still find. Requires Pillow.

    python scripts/make_sun_sky.py

The mapping matches Environment() in src/render/renderer.cc: u is azimuth
about +y via atan2(x, -z), v is the polar angle from +y, so row 0 is zenith.
"""
import math
import sys

from PIL import Image

WIDTH, HEIGHT = 1024, 512
SUN_POLAR_DEG = 20.0   # angle from straight up
SUN_RADIUS_DEG = 4.0   # angular radius of the disc
SUN_RGB = (255, 255, 245)
SKY_RGB = (10, 16, 34)
OUT = "assets/textures/sun_sky.png"


def main():
    polar = math.radians(SUN_POLAR_DEG)
    sun = (0.0, math.cos(polar), -math.sin(polar))
    cos_radius = math.cos(math.radians(SUN_RADIUS_DEG))

    image = Image.new("RGB", (WIDTH, HEIGHT))
    pixels = image.load()
    for ty in range(HEIGHT):
        theta = (ty + 0.5) / HEIGHT * math.pi
        y, sin_theta = math.cos(theta), math.sin(theta)
        for tx in range(WIDTH):
            phi = ((tx + 0.5) / WIDTH - 0.5) * 2.0 * math.pi
            x = sin_theta * math.sin(phi)
            z = -sin_theta * math.cos(phi)
            dot = x * sun[0] + y * sun[1] + z * sun[2]
            pixels[tx, ty] = SUN_RGB if dot >= cos_radius else SKY_RGB
    image.save(OUT)

    solid_angle = 2.0 * math.pi * (1.0 - cos_radius)
    print("wrote %s (%dx%d)" % (OUT, WIDTH, HEIGHT))
    print("sun solid angle %.4f sr -- a cosine-weighted bounce near the zenith"
          % solid_angle)
    print("finds it about %.2f%% of the time, which is the noise floor."
          % (100.0 * solid_angle / math.pi))


if __name__ == "__main__":
    sys.exit(main())
