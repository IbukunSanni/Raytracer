-- A second textured OBJ against Blender, the clean case: one material on
-- every face, its own UVs over the whole of [0, 1], and its own 4096 x 4096
-- base-colour map.
--     ./build/raytracer assets/scenes/smg_gun.lua
--
-- Model: "SMG GUN Remastered" (https://sketchfab.com/3d-models/smg-gun-
-- remastered-2e9bf758703847c88d854fa204384203) by ROHIT3DMODELS
-- (https://sketchfab.com/Rohit3Dasset), licensed under CC-BY-4.0
-- (http://creativecommons.org/licenses/by/4.0/). See
-- assets/models/smg_gun_license.txt.
--
-- Only the base colour is read: this renderer's diffuse is Lambertian, so
-- the set's normal, roughness, metallic, AO, height and emission maps have
-- nothing to drive yet. Its Blender twin makes the same simplification -- a
-- Diffuse BSDF on the base colour -- so the two compare like with like. The textures are not in git: without
-- them the gun renders the grey-and-white placeholder checker.

local paint = gr.image_texture{
  path = 'assets/textures/smg_gun/Gun_Gun_Tex_BaseColor.png' }
local floor = gr.checkered{ scale = 1.5,
                            yin = {0.2, 0.3, 0.1}, yang = {0.9, 0.9, 0.9} }

local scene = gr.node('root')

-- Radius 1000 centred 1000 below the origin, so its top is the y = 0 plane.
local ground = gr.nh_sphere('ground', {0, -1000, 0}, 1000)
ground:set_material(gr.lambertian{ kd = floor })
scene:add_child(ground)

-- The model spans 2.13 x 11.83 x 27.60, its bbox centred at (0, 8.70, 1.05)
-- with its lowest point at y = 2.79. Scaled by 1/7 about the origin, then
-- moved so it is centred in z and hovers 0.3 above the floor.
local GUN_SCALE = 1.0 / 7.0
local gun = gr.mesh('gun', 'assets/models/smg_gun.obj')
gun:set_material(gr.lambertian{ kd = paint })
gun:scale(GUN_SCALE, GUN_SCALE, GUN_SCALE)
gun:translate(0.0, -2.7866 * GUN_SCALE + 0.3, -1.0478 * GUN_SCALE)
scene:add_child(gun)

-- Pure sky light; gr.render wants a non-empty light list.
local black_light = gr.light({0, 0, -100}, {0, 0, 0}, {1, 0, 0})

gr.set_background('')
gr.set_tonemap{ operator = 'reinhard' }

local eye    = {5.0, 2.0, 1.5}
local lookat = {0, 1.1, 0}

gr.render{
  root = scene, output = 'renders/smg_gun.png',
  width = 640, height = 360,

  eye = eye,
  view = {lookat[1] - eye[1], lookat[2] - eye[2], lookat[3] - eye[3]},
  up = {0, 1, 0}, fov = 35,

  ambient = {0.5, 0.7, 1.0},
  lights = { black_light },

  samples = 64,
}
