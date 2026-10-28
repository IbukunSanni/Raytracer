-- Perlin noise on a ball and a cube the ball's size, side by side, on a
-- plain grey ground.
-- The book's scene is the ground and ball alone. The cube's flat faces show
-- the texture is solid -- cut from space, not wrapped -- which a sphere
-- cannot: the pattern runs straight across each face and on round the edge.
--     ./build/raytracer assets/scenes/perlin.lua

local noise = gr.noise(4)
local scene = gr.node('root')

local ground = gr.nh_sphere('ground', {0, -1000, 0}, 1000)
-- Plain grey, so the eye goes to the two textured objects.
ground:set_material(gr.lambertian{ kd = {0.5, 0.5, 0.5} })
scene:add_child(ground)

local ball = gr.nh_sphere('ball', {0, 2, 3}, 2)
ball:set_material(gr.lambertian{ kd = noise })
scene:add_child(ball)

-- Side 4, the ball's diameter, resting on the ground. nh_box takes the
-- minimum corner, so this centres it at (0, 2, -3).
local cube = gr.nh_box('cube', {-2, 0, -5}, 4)
cube:set_material(gr.lambertian{ kd = noise })
scene:add_child(cube)

gr.set_background('')
gr.set_tonemap{ operator = 'reinhard' }

local eye, lookat = {18, 10, 9}, {0, 1.5, 0}
gr.render{
  root = scene, output = 'renders/perlin/perlin_marble.png',
  width = 480, height = 270,
  eye = eye,
  view = {lookat[1] - eye[1], lookat[2] - eye[2], lookat[3] - eye[3]},
  up = {0, 1, 0}, fov = 30,
  ambient = {0.70, 0.80, 1.00},
  lights = { gr.light({0, 0, 0}, {0, 0, 0}, {1, 0, 0}) },
  samples = 64,
}