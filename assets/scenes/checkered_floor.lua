-- A spatial checker on the ground under Ray Tracing in One Weekend's three
-- big spheres, adapted from The Next Week, 4.2.
--     ./build/raytracer assets/scenes/checkered_floor.lua

-- Cubes 1.5 units on a side, chosen from the hit point alone: no UVs.
local checkered = gr.checkered{ scale = 1.5,
                                yin = {0.2, 0.3, 0.1}, yang = {0.9, 0.9, 0.9} }

local scene = gr.node('root')

-- Radius 1000 centred 1000 below the origin, so its top is the y = 0 plane.
local ground = gr.nh_sphere('ground', {0, -1000, 0}, 1000)
ground:set_material(gr.lambertian{ kd = checkered })
scene:add_child(ground)

local glass = gr.nh_sphere('glass', {0, 1, 0}, 1)
glass:set_material(gr.dielectric{ ior = 1.5 })
scene:add_child(glass)

local diffuse = gr.nh_sphere('diffuse', {-4, 1, 0}, 1)
diffuse:set_material(gr.lambertian{ kd = {0.4, 0.2, 0.1} })
scene:add_child(diffuse)

local metal = gr.nh_sphere('metal', {4, 1, 0}, 1)
metal:set_material(gr.metal{ albedo = {0.7, 0.6, 0.5}, fuzz = 0.0 })
scene:add_child(metal)

-- Pure sky light, as in the book; gr.render wants a non-empty light list.
local black_light = gr.light({0, 0, -100}, {0, 0, 0}, {1, 0, 0})

gr.set_background('')
gr.set_tonemap{ operator = 'reinhard' }

gr.render{
  root = scene, output = 'renders/checkered_floor.png',
  width = 400, height = 225,

  eye = {13, 2, 3}, view = {-13, -2, -3}, up = {0, 1, 0}, fov = 20,

  ambient = {0.5, 0.7, 1.0},
  lights = { black_light },

  samples = 64,
}
