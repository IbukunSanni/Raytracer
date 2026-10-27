-- Perlin noise (Ray Tracing: The Next Week, 5), on the book's two spheres.
--     ./build/raytracer assets/scenes/perlin.lua

local noise = gr.noise()
local scene = gr.node('root')

local ground = gr.nh_sphere('ground', {0, -1000, 0}, 1000)
ground:set_material(gr.lambertian{ kd = noise })
scene:add_child(ground)

local ball = gr.nh_sphere('ball', {0, 2, 0}, 2)
ball:set_material(gr.lambertian{ kd = noise })
scene:add_child(ball)

gr.set_background('')
gr.set_tonemap{ operator = 'reinhard' }

local eye, lookat = {13, 2, 3}, {0, 0, 0}
gr.render{
  root = scene, output = 'renders/perlin.png',
  width = 480, height = 270,
  eye = eye,
  view = {lookat[1] - eye[1], lookat[2] - eye[2], lookat[3] - eye[3]},
  up = {0, 1, 0}, fov = 20,
  ambient = {0.70, 0.80, 1.00},
  lights = { gr.light({0, 0, 0}, {0, 0, 0}, {1, 0, 0}) },
  samples = 64,
}