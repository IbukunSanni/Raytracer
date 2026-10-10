-- The two kinds of checker side by side, on a spatially checkered floor.
--     ./build/raytracer assets/scenes/uv_checker.lua
--     RT_VIEW=uv ./build/raytracer assets/scenes/uv_checker.lua
--
-- Left and middle: gr.uv_checkered, painted in each sphere's (u, v). Its
-- cells follow longitude and latitude, narrow toward the poles, and turn
-- with the sphere. Right: gr.checkered, cut from space by the hit point, so
-- its cells ignore the surface and a curved one slices them unevenly.

local paint = gr.uv_checkered{ columns = 16, rows = 8,
                               yin = {0.85, 0.85, 0.85}, yang = {0.1, 0.25, 0.6} }
local cubes = gr.checkered{ scale = 0.4,
                            yin = {0.85, 0.85, 0.85}, yang = {0.6, 0.15, 0.1} }
local floor = gr.checkered{ scale = 1.5,
                            yin = {0.2, 0.3, 0.1}, yang = {0.9, 0.9, 0.9} }

local scene = gr.node('root')

-- Radius 1000 centred 1000 below the origin, so its top is the y = 0 plane.
local ground = gr.nh_sphere('ground', {0, -1000, 0}, 1000)
ground:set_material(gr.lambertian{ kd = floor })
scene:add_child(ground)

-- Facing the camera as built: the camera sees u = 0.5, and the seam, where
-- u wraps from 1 to 0, is on the far side.
local plain = gr.nh_sphere('plain', {0, 1, 2.5}, 1)
plain:set_material(gr.lambertian{ kd = paint })
scene:add_child(plain)

-- The same texture on a turned and tipped sphere: the pattern goes with it,
-- because (u, v) is computed in the sphere's own space. The tip shows the
-- cells pinching toward the top pole. The turn brings the seam into view,
-- but only RT_VIEW=uv shows it, as a hard edge from red back to black: a
-- whole number of columns meets itself exactly where u wraps, so the
-- checker hides its own seam.
local turned = gr.sphere('turned')
turned:rotate('Y', 150)
turned:rotate('X', 35)
turned:translate(0, 1, 0)
turned:set_material(gr.lambertian{ kd = paint })
scene:add_child(turned)

local spatial = gr.nh_sphere('spatial', {0, 1, -2.5}, 1)
spatial:set_material(gr.lambertian{ kd = cubes })
scene:add_child(spatial)

-- Pure sky light; gr.render wants a non-empty light list.
local black_light = gr.light({0, 0, -100}, {0, 0, 0}, {1, 0, 0})

gr.set_background('')
gr.set_tonemap{ operator = 'reinhard' }

gr.render{
  root = scene, output = 'renders/uv_checker.png',
  width = 640, height = 360,

  eye = {13, 2, 3}, view = {-13, -2, -3}, up = {0, 1, 0}, fov = 25,

  ambient = {0.5, 0.7, 1.0},
  lights = { black_light },

  samples = 64,
}
