-- A textured OBJ: the model's own UVs reading its own base-colour image.
--     ./build/raytracer assets/scenes/textured_ship.lua
--     RT_VIEW=albedo ./build/raytracer assets/scenes/textured_ship.lua
--
-- Model: "Cartoon spaceship" (https://sketchfab.com/3d-models/cartoon-
-- spaceship-a377d653202f45a68d441a987aa6dbda) by pinguinoconpulgares
-- (https://sketchfab.com/pinguinoconpulgares), licensed under CC-BY-4.0
-- (http://creativecommons.org/licenses/by/4.0/). Exported to OBJ; see
-- assets/models/cartoon_ship_license.txt.
--
-- The texture is not in git (assets/textures is ignored). Without it the
-- ship renders the grey-and-white placeholder checker instead.
--
-- One material for the whole mesh: OBJ's usemtl is not read. The original
-- has six, and three of them -- the cockpit, a near-black trim and a yellow
-- part -- are flat colours rather than the texture, so here those faces
-- show whatever part of the image their UVs land on.

local ship_paint = gr.image_texture{
  path = 'assets/textures/cartoon_ship_base_color.png' }
local floor = gr.checkered{ scale = 1.5,
                            yin = {0.2, 0.3, 0.1}, yang = {0.9, 0.9, 0.9} }

local scene = gr.node('root')

-- Radius 1000 centred 1000 below the origin, so its top is the y = 0 plane.
local ground = gr.nh_sphere('ground', {0, -1000, 0}, 1000)
ground:set_material(gr.lambertian{ kd = floor })
scene:add_child(ground)

-- The model spans 3.37 x 1.87 x 3.41 about its bbox centre (0.00, 0.11,
-- 0.11), with its lowest point at y = -0.82. Lifted so it hovers 0.3 above
-- the floor.
local ship = gr.mesh('ship', 'assets/models/cartoon_ship.obj')
ship:set_material(gr.lambertian{ kd = ship_paint })
ship:translate(0.0, 0.82 + 0.3, -0.11)
scene:add_child(ship)

-- Pure sky light; gr.render wants a non-empty light list.
local black_light = gr.light({0, 0, -100}, {0, 0, 0}, {1, 0, 0})

gr.set_background('')
gr.set_tonemap{ operator = 'reinhard' }

local eye    = {5.5, 3.0, 5.0}
local lookat = {0, 1.0, 0}

gr.render{
  root = scene, output = 'renders/textured_ship.png',
  width = 640, height = 360,

  eye = eye,
  view = {lookat[1] - eye[1], lookat[2] - eye[2], lookat[3] - eye[3]},
  up = {0, 1, 0}, fov = 35,

  ambient = {0.5, 0.7, 1.0},
  lights = { black_light },

  samples = 64,
}
