"""Paints assets/textures/byakugan.png: a pale, almost pupil-less eye.

    python scripts/byakugan_texture.py

A lat-long texture for assets/scenes/byakugan.lua, drawn procedurally so it
is original art rather than a copy: a near-white eyeball, a pale lavender
iris whose rim and fibres are only a shade darker, a pupil barely there, and
faint veins on the white. The iris is centred on +z, the side of the sphere
that faces the furnace camera.

Texel to direction inverts src/geometry/sphere_uv.h: u = phi / 2pi and
v = theta / pi, with y = -cos(theta), x = -sin(theta) cos(phi) and
z = sin(theta) sin(phi). ImageTexture reads row (1 - v) * height, so row 0
is the top of the sphere.
"""

import numpy as np
from PIL import Image

W, H = 2048, 1024
rng = np.random.default_rng(7)

u = (np.arange(W) + 0.5) / W
v = 1.0 - (np.arange(H) + 0.5) / H
phi = 2.0 * np.pi * u[None, :]
theta = np.pi * v[:, None]
x = -np.sin(theta) * np.cos(phi)
y = -np.cos(theta) * np.ones_like(phi)
z = np.sin(theta) * np.sin(phi)

# Angle from the eye's axis, +z, and the direction around it.
angle = np.degrees(np.arccos(np.clip(z, -1.0, 1.0)))
around = np.arctan2(y, x)

WHITE = np.array([0.97, 0.97, 0.98])
IRIS = np.array([0.86, 0.83, 0.95])
RIM = np.array([0.66, 0.62, 0.80])
PUPIL = np.array([0.78, 0.75, 0.90])
VEIN = np.array([0.80, 0.78, 0.88])

IRIS_DEG, PUPIL_DEG = 34.0, 7.0
img = np.broadcast_to(WHITE, (H, W, 3)).copy()

# Iris: pale lavender with radial fibres, darkening into a soft rim.
fibres = 0.035 * np.sin(around * 90.0 + 3.0 * np.sin(around * 7.0))
iris_t = np.clip(angle / IRIS_DEG, 0.0, 1.0)[..., None]
iris = IRIS * (1.0 + fibres[..., None]) * (1.0 - 0.06 * iris_t)
rim = np.clip((angle - (IRIS_DEG - 4.0)) / 4.0, 0.0, 1.0)[..., None]
iris = iris * (1.0 - rim) + RIM * rim
inside = (angle < IRIS_DEG)[..., None]
img = np.where(inside, iris, img)

# Pupil: there, but only just.
pupil = np.clip((PUPIL_DEG - angle) / 1.5, 0.0, 1.0)[..., None]
img = img * (1.0 - pupil) + PUPIL * pupil

# Veins: wandering lines starting at the iris rim, fading as they spread.
for _ in range(14):
    start = rng.uniform(-np.pi, np.pi)
    wobble = rng.uniform(5.0, 12.0)
    path = start + 0.04 * np.sin((angle - IRIS_DEG) / wobble)
    d = np.abs(np.angle(np.exp(1j * (around - path))))
    width = 0.012 + 0.0006 * (angle - IRIS_DEG)
    reach = rng.uniform(60.0, 95.0)
    strength = np.clip(1.0 - (angle - IRIS_DEG) / (reach - IRIS_DEG), 0.0, 1.0)
    line = (np.clip(1.0 - d / width, 0.0, 1.0) * strength
            * (angle > IRIS_DEG))[..., None]
    img = img * (1.0 - 0.8 * line) + VEIN * 0.8 * line

# The texture is sRGB-encoded on disk; ImageTexture decodes it on load.
encoded = np.where(img <= 0.0031308, 12.92 * img,
                   1.055 * np.power(img, 1.0 / 2.4) - 0.055)
Image.fromarray(np.uint8(np.clip(encoded, 0, 1) * 255 + 0.5)).save(
    "assets/textures/byakugan.png")
print("wrote assets/textures/byakugan.png")
