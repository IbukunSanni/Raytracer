-- An area light seen directly, against nothing.
--
-- The environment is black, so a pixel that misses the sphere is 0 and one
-- that hits it carries the light's radiance and nothing else: 0.5, which
-- lands on byte 128 with room to be wrong in either direction.
--
-- A bright point light sits at the eye, aimed straight at the sphere. A
-- light reflects nothing, so it must not brighten the sphere at all; if it
-- does, the light is being shaded as well as counted.

local scene = gr.node('root')
local lamp = gr.nh_sphere('lamp', {0, 0, -500}, 100)
lamp:set_material(gr.diffuse_light{ radiance = {0.5, 0.5, 0.5} })
scene:add_child(lamp)

gr.set_background('')
gr.set_tonemap{ operator = 'none', srgb = false }

gr.render{
  root = scene, output = 'tests/out/diffuse_light.png',
  width = 64, height = 64,
  eye = {0, 0, 0}, view = {0, 0, -1}, up = {0, 1, 0}, fov = 30,
  ambient = {0, 0, 0},
  lights = { gr.light({0, 0, 0}, {10, 10, 10}, {1, 0, 0}) },
  samples = 4,
}
