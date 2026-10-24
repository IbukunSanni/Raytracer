-- Motion blur (Ray Tracing: The Next Week, 2): three spheres moving during
-- the shutter, over a checkered floor, lit by a point light and a dim sky.
--     ./build/raytracer assets/scenes/motion_blur.lua
--
-- A fourth argument to gr.nh_sphere is where the centre is when the shutter
-- closes; each camera sample sees one instant in between. The point light
-- is there for the shadows: a shadow ray has to be cast at its camera
-- sample's instant too, or the blurred sphere drops a sharp shadow from
-- where it was when the shutter opened.

local checkered = gr.checkered{ scale = 1.5,
                                yin = {0.2, 0.3, 0.1}, yang = {0.9, 0.9, 0.9} }

local scene = gr.node('root')

-- Radius 1000 centred 1000 below the origin, so its top is the y = 0 plane.
local ground = gr.nh_sphere('ground', {0, -1000, 0}, 1000)
ground:set_material(gr.lambertian{ kd = checkered })
scene:add_child(ground)

-- Sideways, fast: the longest streak, and its shadow should streak with it.
local red = gr.nh_sphere('red', {-4.5, 1, 0}, 1, {-2.0, 1, 0})
red:set_material(gr.lambertian{ kd = {0.7, 0.15, 0.1} })
scene:add_child(red)

-- Straight up: the bottom stays sharp where it leaves the floor, the top
-- smears, and the shadow shrinks and softens.
local blue = gr.nh_sphere('blue', {0, 1, 0}, 1, {0, 2.2, 0})
blue:set_material(gr.lambertian{ kd = {0.1, 0.25, 0.7} })
scene:add_child(blue)

-- Still, for comparison: sharp edges, sharp shadow.
local white = gr.nh_sphere('white', {4, 1, 0}, 1)
white:set_material(gr.lambertian{ kd = {0.8, 0.8, 0.8} })
scene:add_child(white)

local lamp = gr.light({1, 9, 4}, {2.4, 2.4, 2.3}, {1, 0, 0})

gr.set_background('')
gr.set_tonemap{ operator = 'reinhard' }

gr.render{
  root = scene, output = 'renders/motion_blur.png',
  width = 480, height = 270,

  eye = {0, 4, 14}, view = {0, -3, -14}, up = {0, 1, 0}, fov = 32,

  ambient = {0.15, 0.18, 0.25},
  lights = { lamp },

  samples = 128,
}
