"""Renders a textured-OBJ scene in Blender's Cycles, as a reference.

    "C:/Program Files/Blender Foundation/Blender 5.0/blender.exe" -b \
        --factory-startup -P scripts/make_blender_reference.py -- SCENE

Run from the repo root. SCENE is a key of SCENES below -- textured_ship
(the default) or smg -- each the twin of an assets/scenes/*.lua. Writes
renders/<lua>_blender.exr (linear) and renders/<lua>_blender.png
(tone-mapped as the raytracer does).

Each scene is rebuilt here by hand from its .lua, so the two must be changed
together. What is matched, and how:

  - Geometry: the same OBJ and the same PNG, read by Blender's own importer,
    so the UVs and the image orientation are Blender's reading, not ours.
  - Axes: this renderer is y-up, Blender z-up. A point (x, y, z) here is
    (x, -z, y) there; ToBlender() does that for every position.
  - Camera: vertical field of view, 640 x 360, a box pixel filter one pixel
    wide -- the raytracer jitters uniformly inside each pixel.
  - Materials: Diffuse BSDF, which is Lambertian. The image is sampled
    'Closest' (nearest-neighbour) with 'EXTEND' (clamp) at its edges.
  - Floor: a radius-1000 sphere with the same spatial checker, built from
    math nodes as floor(x/s) + floor(y/s) + floor(z/s), floored mod 2, in
    this renderer's axes.
  - Light: a uniform world of radiance (0.5, 0.7, 1.0) and nothing else.
  - Tone map: Blender has no Reinhard view transform, so the render is saved
    as linear EXR and mapped here exactly as src/core/tone_map.cc does --
    Reinhard on Rec.709 luminance, clamp, then the IEC 61966-2-1 sRGB curve.

Not matched: path termination. Cycles stops at 8 bounces; the raytracer
also plays Russian roulette from the third. Both are unbiased up to that
depth, so the means agree and only the noise differs.
"""

import math
import os
import sys

import bpy
import numpy as np
from mathutils import Vector

# One entry per scene, copied from its .lua. bbox is the OBJ's own bounding
# box in this renderer's axes, before scale and translate, to check the
# import against. Transforms apply as in the .lua: scale about the origin,
# then translate.
SCENES = {
    "textured_ship": dict(
        lua="textured_ship",
        obj="assets/models/cartoon_ship.obj",
        texture="assets/textures/cartoon_ship_base_color.png",
        scale=1.0, translate=(0.0, 0.82 + 0.3, -0.11),
        eye=(5.5, 3.0, 5.0), lookat=(0.0, 1.0, 0.0),
        bbox=((-1.687, -0.824, -1.598), (1.683, 1.045, 1.812))),
    "smg": dict(
        lua="smg_gun",
        obj="assets/models/smg_gun.obj",
        texture="assets/textures/smg_gun/Gun_Gun_Tex_BaseColor.png",
        scale=1.0 / 7.0,
        translate=(0.0, -2.7866 / 7.0 + 0.3, -1.0478 / 7.0),
        eye=(5.0, 2.0, 1.5), lookat=(0.0, 1.1, 0.0),
        bbox=((-1.0655, 2.7866, -12.7506), (1.0655, 14.6181, 14.8462))),
}

NAME = sys.argv[sys.argv.index("--") + 1] if "--" in sys.argv else "textured_ship"
SCENE = SCENES[NAME]

REPO = os.getcwd()
OBJ = os.path.join(REPO, SCENE["obj"])
TEXTURE = os.path.join(REPO, SCENE["texture"])
OUT_EXR = os.path.join(REPO, f"renders/{SCENE['lua']}_blender.exr")
OUT_PNG = os.path.join(REPO, f"renders/{SCENE['lua']}_blender.png")

WIDTH, HEIGHT = 640, 360
FOVY_DEGREES = 35.0
SAMPLES = 256
EYE = SCENE["eye"]
LOOKAT = SCENE["lookat"]
MODEL_SCALE = SCENE["scale"]
MODEL_TRANSLATE = SCENE["translate"]
SKY = (0.5, 0.7, 1.0)
CHECKER_SCALE = 1.5
YIN = (0.2, 0.3, 0.1)
YANG = (0.9, 0.9, 0.9)
OBJ_MIN, OBJ_MAX = SCENE["bbox"]


def ToBlender(p):
    """A y-up position here as a z-up position in Blender."""
    x, y, z = p
    return Vector((x, -z, y))


def ClearScene():
    bpy.ops.wm.read_factory_settings(use_empty=True)


def MakeModel():
    bpy.ops.wm.obj_import(filepath=OBJ, forward_axis="NEGATIVE_Z", up_axis="Y")
    # Blender imports each 'o' object separately -- the ship has six -- while
    # the raytracer reads a file as one mesh, so join them into one here, or
    # only the first is moved and painted. The gun's 78 'g' groups import as
    # one object, which needs no join.
    parts = list(bpy.context.selected_objects)
    bpy.context.view_layer.objects.active = parts[0]
    if len(parts) > 1:
        bpy.ops.object.join()
    model = bpy.context.active_object
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)

    # The import must land the model where ToBlender says, or the camera
    # below frames something else. Checked rather than assumed.
    corners = [model.matrix_world @ Vector(c) for c in model.bound_box]
    lo = Vector([min(c[i] for c in corners) for i in range(3)])
    hi = Vector([max(c[i] for c in corners) for i in range(3)])
    want = [ToBlender(OBJ_MIN), ToBlender(OBJ_MAX)]
    want_lo = Vector([min(w[i] for w in want) for i in range(3)])
    want_hi = Vector([max(w[i] for w in want) for i in range(3)])
    if (lo - want_lo).length > 0.01 or (hi - want_hi).length > 0.01:
        raise SystemExit(f"OBJ axes: got {lo} .. {hi}, "
                         f"want {want_lo} .. {want_hi}")

    model.scale = (MODEL_SCALE, MODEL_SCALE, MODEL_SCALE)
    model.location = ToBlender(MODEL_TRANSLATE)

    mat = bpy.data.materials.new("model_paint")
    mat.use_nodes = True
    nodes, links = mat.node_tree.nodes, mat.node_tree.links
    nodes.clear()
    out = nodes.new("ShaderNodeOutputMaterial")
    diffuse = nodes.new("ShaderNodeBsdfDiffuse")
    image = nodes.new("ShaderNodeTexImage")
    image.image = bpy.data.images.load(TEXTURE)
    image.image.colorspace_settings.name = "sRGB"
    image.interpolation = "Closest"
    image.extension = "EXTEND"
    links.new(image.outputs["Color"], diffuse.inputs["Color"])
    links.new(diffuse.outputs["BSDF"], out.inputs["Surface"])

    # One material for every face, as the raytracer reads no usemtl.
    model.data.materials.clear()
    model.data.materials.append(mat)
    for poly in model.data.polygons:
        poly.material_index = 0


def MakeFloor():
    bpy.ops.mesh.primitive_uv_sphere_add(segments=512, ring_count=256,
                                         radius=1000.0,
                                         location=ToBlender((0, -1000, 0)))
    ground = bpy.context.active_object
    bpy.ops.object.shade_smooth()

    mat = bpy.data.materials.new("floor")
    mat.use_nodes = True
    nodes, links = mat.node_tree.nodes, mat.node_tree.links
    nodes.clear()
    out = nodes.new("ShaderNodeOutputMaterial")
    diffuse = nodes.new("ShaderNodeBsdfDiffuse")
    geometry = nodes.new("ShaderNodeNewGeometry")
    split = nodes.new("ShaderNodeSeparateXYZ")
    links.new(geometry.outputs["Position"], split.inputs["Vector"])

    def Math(op, a, b=None, value=None):
        node = nodes.new("ShaderNodeMath")
        node.operation = op
        links.new(a, node.inputs[0])
        if b is not None:
            links.new(b, node.inputs[1])
        elif value is not None:
            node.inputs[1].default_value = value
        return node.outputs[0]

    def Cell(axis_output, sign):
        scaled = Math("MULTIPLY", axis_output, value=sign / CHECKER_SCALE)
        return Math("FLOOR", scaled)

    # This renderer's (x, y, z) is Blender's (x, z, -y).
    x = Cell(split.outputs["X"], 1.0)
    y = Cell(split.outputs["Z"], 1.0)
    z = Cell(split.outputs["Y"], -1.0)
    parity = Math("FLOORED_MODULO", Math("ADD", Math("ADD", x, y), z),
                  value=2.0)

    mix = nodes.new("ShaderNodeMix")
    mix.data_type = "RGBA"
    mix.inputs["A"].default_value = (*YIN, 1.0)   # even sum
    mix.inputs["B"].default_value = (*YANG, 1.0)  # odd sum
    links.new(parity, mix.inputs["Factor"])
    links.new(mix.outputs["Result"], diffuse.inputs["Color"])
    links.new(diffuse.outputs["BSDF"], out.inputs["Surface"])
    ground.data.materials.append(mat)


def MakeWorld():
    world = bpy.data.worlds.new("sky")
    bpy.context.scene.world = world
    world.use_nodes = True
    background = world.node_tree.nodes["Background"]
    background.inputs["Color"].default_value = (*SKY, 1.0)
    background.inputs["Strength"].default_value = 1.0


def MakeCamera():
    data = bpy.data.cameras.new("camera")
    data.sensor_fit = "VERTICAL"
    data.angle_y = math.radians(FOVY_DEGREES)
    camera = bpy.data.objects.new("camera", data)
    bpy.context.scene.collection.objects.link(camera)
    camera.location = ToBlender(EYE)
    direction = ToBlender(LOOKAT) - camera.location
    camera.rotation_euler = direction.to_track_quat("-Z", "Y").to_euler()
    bpy.context.scene.camera = camera


def Render():
    scene = bpy.context.scene
    scene.render.engine = "CYCLES"
    scene.cycles.device = "CPU"
    scene.cycles.samples = SAMPLES
    scene.cycles.use_adaptive_sampling = False
    scene.cycles.use_denoising = False
    scene.cycles.pixel_filter_type = "BOX"
    scene.cycles.filter_width = 1.0
    scene.cycles.max_bounces = 8
    scene.cycles.diffuse_bounces = 8
    scene.render.resolution_x = WIDTH
    scene.render.resolution_y = HEIGHT
    scene.render.resolution_percentage = 100
    scene.view_settings.view_transform = "Standard"
    scene.view_settings.look = "None"
    scene.render.image_settings.file_format = "OPEN_EXR"
    scene.render.image_settings.color_depth = "32"
    scene.render.filepath = OUT_EXR
    bpy.ops.render.render(write_still=True)


def ToneMap():
    """src/core/tone_map.cc: Reinhard on luminance, clamp, sRGB encode."""
    exr = bpy.data.images.load(OUT_EXR)
    rgba = np.array(exr.pixels[:], dtype=np.float64).reshape(HEIGHT, WIDTH, 4)
    rgb = rgba[..., :3]
    y = rgb @ np.array([0.2126, 0.7152, 0.0722])
    safe = np.where(y > 0.0, y, 1.0)
    mapped = np.where((y > 0.0)[..., None],
                      rgb * ((y / (1.0 + y)) / safe)[..., None], 0.0)
    mapped = np.clip(mapped, 0.0, 1.0)
    encoded = np.where(mapped <= 0.0031308, 12.92 * mapped,
                       1.055 * np.power(mapped, 1.0 / 2.4) - 0.055)

    out = bpy.data.images.new("tonemapped", WIDTH, HEIGHT, alpha=False)
    out.colorspace_settings.name = "Non-Color"  # already encoded: write as is
    pixels = np.ones((HEIGHT, WIDTH, 4))
    pixels[..., :3] = encoded
    out.pixels = pixels.ravel()
    out.filepath_raw = OUT_PNG
    out.file_format = "PNG"
    out.save()


ClearScene()
MakeModel()
MakeFloor()
MakeWorld()
MakeCamera()
Render()
ToneMap()
print("wrote", OUT_PNG)
