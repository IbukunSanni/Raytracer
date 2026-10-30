-- The SMG's UV islands, each painted its own colour.
--     python scripts/make_uv_unwrap_figure.py
--
-- That script writes the texture, one flat colour per island of the OBJ's
-- unwrap, then renders this scene with RT_VIEW=albedo, so every pixel is
-- the unlit colour of the island the hit lands in. A patch of one colour on
-- the gun is one island; a change of colour is a seam.
--
-- Model: "SMG GUN Remastered" by ROHIT3DMODELS, CC-BY-4.0; see
-- assets/models/smg_gun_license.txt. Placed and framed as in mesh_uv.lua.

local paint = gr.image_texture{
  path = 'assets/textures/smg_gun_uv_islands.png' }

local scene = gr.node('root')

-- Radius 1000 centred 1000 below the origin, so its top is the y = 0 plane.
-- Plain grey, so the floor takes no colour from the islands.
local ground = gr.nh_sphere('ground', {0, -1000, 0}, 1000)
ground:set_material(gr.lambertian{ kd = {0.18, 0.18, 0.18} })
scene:add_child(ground)

local GUN_SCALE = 1.0 / 7.0
local gun = gr.mesh('gun', 'assets/models/smg_gun.obj')
gun:set_material(gr.lambertian{ kd = paint })
gun:scale(GUN_SCALE, GUN_SCALE, GUN_SCALE)
gun:translate(0.0, -2.7866 * GUN_SCALE + 0.3, -1.0478 * GUN_SCALE)
scene:add_child(gun)

-- Pure sky light; gr.render wants a non-empty light list.
local black_light = gr.light({0, 0, -100}, {0, 0, 0}, {1, 0, 0})

gr.set_background('')

local eye    = {5.0, 2.0, 1.5}
local lookat = {0, 1.1, 0}

gr.render{
  root = scene, output = 'renders/mesh_uv/mesh_uv_islands.png',
  width = 640, height = 360,

  eye = eye,
  view = {lookat[1] - eye[1], lookat[2] - eye[2], lookat[3] - eye[3]},
  up = {0, 1, 0}, fov = 35,

  ambient = {0.5, 0.7, 1.0},
  lights = { black_light },

  samples = 16,
}
