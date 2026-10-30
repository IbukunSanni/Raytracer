"""Put the SMG's UV islands on the gun above its unwrap, for the textures post.

    python scripts/make_uv_unwrap_figure.py

Splits the OBJ's vt/f lines into islands and gives each island its own
colour. Writes those colours as a texture, renders assets/scenes/
mesh_uv_islands.lua with RT_VIEW=albedo so the gun wears them, and writes
docs/images/textures/step9-04-mesh-uvs-smg-unwrap.png: the gun, and below it the
unwrap in the same colours.
"""
import colorsys
import os
import subprocess

import numpy as np
from PIL import Image, ImageDraw, ImageFont

OBJ = "assets/models/smg_gun.obj"
TEXTURE = "assets/textures/smg_gun_uv_islands.png"
SCENE = "assets/scenes/mesh_uv_islands.lua"
RENDER = "renders/mesh_uv/mesh_uv_islands.png"
RAYTRACER = "build/raytracer" + (".exe" if os.name == "nt" else "")
OUT = "docs/images/textures/step9-04-mesh-uvs-smg-unwrap.png"

TEXTURE_SIZE = 2048
GAP, PAD, BAR = 12, 12, 34
BG, FG = (26, 30, 42), (214, 219, 232)
SUPER = 4  # draw at 4x and shrink, so edges are antialiased


def label_font():
    for path in ("C:/Windows/Fonts/segoeui.ttf", "C:/Windows/Fonts/arial.ttf"):
        try:
            return ImageFont.truetype(path, 19)
        except OSError:
            continue
    return ImageFont.load_default()


def uv_triangles():
    """Each face's corners in (u, v), fanned from the first as mesh.cc does."""
    uvs, tris = [], []
    with open(OBJ) as f:
        for line in f:
            parts = line.split()
            if not parts:
                continue
            if parts[0] == "vt":
                uvs.append((float(parts[1]), float(parts[2])))
            elif parts[0] == "f":
                corners = []
                for corner in parts[1:]:
                    k = int(corner.split("/")[1])
                    corners.append(uvs[k - 1] if k > 0 else uvs[k])
                for i in range(1, len(corners) - 1):
                    tris.append((corners[0], corners[i], corners[i + 1]))
    return tris


def edges_of(tri):
    # Keyed by rounded position, so two vt lines at one spot join their faces.
    key = [tuple(round(c, 6) for c in corner) for corner in tri]
    return [tuple(sorted((key[a], key[b]))) for a, b in ((0, 1), (1, 2), (2, 0))]


def islands(tris):
    """Each triangle's island number: faces sharing a UV edge are one piece."""
    parent = list(range(len(tris)))

    def find(i):
        while parent[i] != i:
            parent[i] = parent[parent[i]]
            i = parent[i]
        return i

    first_owner = {}
    for t, tri in enumerate(tris):
        for edge in edges_of(tri):
            if edge in first_owner:
                parent[find(t)] = find(first_owner[edge])
            else:
                first_owner[edge] = t
    roots = {}
    return [roots.setdefault(find(t), len(roots)) for t in range(len(tris))]


def island_colours(count):
    # Hue steps by the golden ratio, so islands numbered close together are
    # far apart in hue; saturation and value cycle so equal hues still differ.
    shades = ((0.75, 0.95), (0.55, 0.80), (0.90, 0.70))
    colours = []
    for i in range(count):
        s, v = shades[i % len(shades)]
        rgb = colorsys.hsv_to_rgb((i * 0.6180339887) % 1.0, s, v)
        colours.append(tuple(int(255 * c) for c in rgb))
    return colours


def island_edges(tris):
    """Edges used by one triangle only: the outlines of the UV islands."""
    count = {}
    for tri in tris:
        for a, b in ((tri[0], tri[1]), (tri[1], tri[2]), (tri[2], tri[0])):
            key = tuple(sorted((a, b)))
            count[key] = count.get(key, 0) + 1
    return [edge for edge, n in count.items() if n == 1]


def to_pixels(tri, size):
    # v runs bottom to top; image rows run top to bottom.
    return [(u * size, (1.0 - v) * size) for u, v in tri]


def paint_islands(tris, owner, colours, size):
    image = Image.new("RGB", (size, size), (0, 0, 0))
    draw = ImageDraw.Draw(image)
    for tri, island in zip(tris, owner):
        draw.polygon(to_pixels(tri, size), fill=colours[island])
    return image


def unwrap_panel(tris, owner, colours, side):
    panel = paint_islands(tris, owner, colours, side * SUPER)
    draw = ImageDraw.Draw(panel)
    for edge in island_edges(tris):
        draw.line(to_pixels(edge, side * SUPER), fill=(0, 0, 0), width=SUPER)
    return panel.resize((side, side), Image.LANCZOS)


def gun_render():
    """The scene under RT_VIEW=albedo, put back into sRGB like the texture."""
    subprocess.run([os.path.abspath(RAYTRACER), SCENE], check=True,
                   env={**os.environ, "RT_VIEW": "albedo"})
    # The renderer decodes the texture to linear and a debug view writes that
    # as-is, so encode it again or every colour comes out darker.
    linear = np.asarray(Image.open(RENDER).convert("RGB"), np.float64) / 255.0
    srgb = np.where(linear <= 0.0031308, 12.92 * linear,
                    1.055 * linear ** (1.0 / 2.4) - 0.055)
    return Image.fromarray(np.round(srgb * 255.0).astype(np.uint8))


def main():
    tris = uv_triangles()
    owner = islands(tris)
    colours = island_colours(max(owner) + 1)
    print(f"{max(owner) + 1} islands in {len(tris)} triangles")

    os.makedirs(os.path.dirname(TEXTURE), exist_ok=True)
    paint_islands(tris, owner, colours, TEXTURE_SIZE).save(TEXTURE)

    render = gun_render()
    w, h = render.size
    unwrap = unwrap_panel(tris, owner, colours, w)

    out = Image.new("RGB", (PAD * 2 + w, PAD * 2 + BAR * 2 + h + GAP + w), BG)
    draw = ImageDraw.Draw(out)
    font = label_font()
    for y, panel, text in ((PAD, render, "On the gun"),
                           (PAD + BAR + h + GAP, unwrap, "The unwrap")):
        out.paste(panel, (PAD, y + BAR))
        draw.text((PAD + 2, y + 5), text, font=font, fill=FG)

    out.save(OUT)
    print(f"wrote {OUT}  {out.size[0]}x{out.size[1]}")


if __name__ == "__main__":
    main()
