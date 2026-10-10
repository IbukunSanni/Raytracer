-- A sphere whose image texture does not exist, in a uniform environment of
-- radiance 0.5 with no tone map and no sRGB, as in furnace.lua.
--
-- The scene must still render, with the placeholder checker in place of the
-- image. Its white cells have albedo 1, so like the furnace sphere they
-- vanish into the background at exactly 128; its grey cells have albedo
-- 0.604 and read about 77. A render with no grey in it lost the placeholder.
local missing = gr.image_texture{ path = 'assets/textures/does_not_exist.png' }

local scene = gr.node('root')
local ball = gr.nh_sphere('ball', {0, 0, -500}, 100)
ball:set_material(gr.lambertian{ kd = missing })
scene:add_child(ball)

gr.set_background('')
gr.set_tonemap{ operator = 'none', srgb = false }

gr.render{
  root = scene, output = 'tests/out/missing_texture.png',
  width = 128, height = 128,
  eye = {0, 0, 0}, view = {0, 0, -1}, up = {0, 1, 0}, fov = 30,
  ambient = {0.5, 0.5, 0.5},
  lights = {},
  samples = 16,
}
