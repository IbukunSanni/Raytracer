-- Just for fun: the furnace, with a Byakugan for a sphere.
--     python scripts/byakugan_texture.py
--     ./build/raytracer assets/scenes/byakugan.lua
--
-- The scene is tests/scenes/furnace.lua at radiance 1: a white diffuse
-- sphere there vanishes, so everything left visible here is the texture's
-- albedo falling short of 1 -- the eye that sees everything, in the one
-- scene built to show nothing. The texture is generated, not in git;
-- without it the sphere renders the grey-and-white placeholder checker.

local eye = gr.image_texture{ path = 'assets/textures/byakugan.png' }

local scene = gr.node('root')
local ball = gr.nh_sphere('ball', {0, 0, -500}, 100)
ball:set_material(gr.lambertian{ kd = eye })
scene:add_child(ball)

gr.set_background('')
gr.set_tonemap{ operator = 'none', srgb = true }

gr.render{
  root = scene, output = 'renders/byakugan.png',
  width = 512, height = 512,
  eye = {0, 0, 0}, view = {0, 0, -1}, up = {0, 1, 0}, fov = 25,
  ambient = {1.0, 1.0, 1.0},
  lights = {},
  samples = 16,
}
