-- A mesh's own texture coordinates, made visible.
--     ./build/raytracer assets/scenes/mesh_uv.lua
--     RT_VIEW=uv ./build/raytracer assets/scenes/mesh_uv.lua
--
-- The OBJ's vt lines give every corner of every face a (u, v), and a hit
-- interpolates its face's three. A UV checker shows that layout on the surface:
-- cells that stay square where the artist unwrapped the surface evenly,
-- stretch where it was not, and break off along the seams, where one edge
-- of the model sits in two places in UV space. RT_VIEW=uv shows the same
-- seams as hard jumps in the red and green ramps.

local paint = gr.uv_checkered{ columns = 24, rows = 24,
                               yin = {0.85, 0.85, 0.85}, yang = {0.1, 0.25, 0.6} }
local floor = gr.checkered{ scale = 1.5,
                            yin = {0.2, 0.3, 0.1}, yang = {0.9, 0.9, 0.9} }

local scene = gr.node('root')

-- Radius 1000 centred 1000 below the origin, so its top is the y = 0 plane.
local ground = gr.nh_sphere('ground', {0, -1000, 0}, 1000)
ground:set_material(gr.lambertian{ kd = floor })
scene:add_child(ground)

-- The SMG's UVs cover the whole of [0, 1], unwrapped by its artist for its
-- 4096 x 4096 texture set (smg_gun.lua wears that set). The spaceship would
-- show almost nothing here: all its vt lines fall in a strip 0.06 wide and
-- 0.002 tall, a palette mapping that points each face at one swatch of a
-- small colour image instead of unwrapping the surface.
--
-- Model: "SMG GUN Remastered" by ROHIT3DMODELS, CC-BY-4.0; see
-- assets/models/smg_gun_license.txt. Placed as in smg_gun.lua: it spans
-- 2.13 x 11.83 x 27.60 about (0, 8.70, 1.05), lowest point y = 2.79, so it
-- is scaled by 1/7 and lifted to hover 0.3 above the floor.
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
  root = scene, output = 'renders/mesh_uv.png',
  width = 640, height = 360,

  eye = eye,
  view = {lookat[1] - eye[1], lookat[2] - eye[2], lookat[3] - eye[3]},
  up = {0, 1, 0}, fov = 35,

  ambient = {0.5, 0.7, 1.0},
  lights = { black_light },

  samples = 64,
}
