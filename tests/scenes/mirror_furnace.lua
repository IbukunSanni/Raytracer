-- The furnace test, for a delta material. Roadmap step 4's "perfect
-- mirror" rung acceptance criterion.
--
-- An albedo-1 mirror sphere inside a uniform emissive environment must be
-- INVISIBLE: a perfect mirror in a uniform environment reflects radiance 1
-- from every direction, so it can never read as anything, but the
-- environment itself. Darker means energy is being lost in sample()'s
-- pdf/brdf split; brighter means it is being double-counted.
--
-- Same setup as tests/scenes/furnace.lua, with gr.mirror in place of
-- gr.lambertian -- see that file for why it is rendered twice and why the
-- light is black.

local mirror = gr.mirror{ albedo = {1.0, 1.0, 1.0} }

local scene = gr.node('root')
local ball = gr.nh_sphere('ball', {0, 0, -500}, 100)
ball:set_material(mirror)
scene:add_child(ball)

local black_light = gr.light({0, 0, -100}, {0, 0, 0}, {1, 0, 0})

gr.set_samples(64)
gr.set_background('')
gr.set_tonemap{ operator = 'none', srgb = false }

gr.render{
  root = scene, output = 'tests/out/mirror_furnace_full.png',
  width = 2048, height = 2048,
  eye = {0, 0, 0}, view = {0, 0, -1}, up = {0, 1, 0}, fov = 30,
  ambient = {1.0, 1.0, 1.0},
  lights = { black_light },
}

gr.render{
  root = scene, output = 'tests/out/mirror_furnace_half.png',
  width = 2048, height = 2048,
  eye = {0, 0, 0}, view = {0, 0, -1}, up = {0, 1, 0}, fov = 30,
  ambient = {0.5, 0.5, 0.5},
  lights = { black_light },
}
