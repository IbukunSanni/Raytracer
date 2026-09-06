-- The furnace test, at scene level. Roadmap step 3's acceptance criterion.
--
-- An albedo-1 diffuse sphere inside a uniform emissive environment must be
-- INVISIBLE: every pixel resolves to the environment radiance, sphere and
-- surround alike. Darker means energy is being lost in the throughput
-- loop; brighter means it is being double-counted.
--
-- Rendered twice. Radiance 1 is the criterion as written, but it lands on
-- 255 and clips, so a too-bright result would be hidden. Radiance 0.5
-- lands on 128 with headroom in both directions.
--
-- No background texture, so `ambient` is the uniform environment. The
-- light is black: point lights are the one thing BSDF sampling cannot
-- reach, and this test is about the integrator, not next event estimation.
--
-- The BSDF-level checks in tests/furnace.cpp cannot catch this -- they
-- integrate a material, not a path.
--
-- Deliberately a gr.lambertian: a Blinn-Phong with a zeroed specular lobe
-- is numerically the same material, but this one's closed form leaves
-- nothing to argue about.

local white = gr.lambertian{ kd = {1.0, 1.0, 1.0} }

local scene = gr.node('root')
local ball = gr.nh_sphere('ball', {0, 0, -500}, 100)
ball:set_material(white)
scene:add_child(ball)

local black_light = gr.light({0, 0, -100}, {0, 0, 0}, {1, 0, 0})

gr.set_samples(64)
gr.set_background('')
gr.set_tonemap{ operator = 'none', srgb = false }

gr.render{
  root = scene, output = 'tests/out/furnace_full.png',
  width = 32, height = 32,
  eye = {0, 0, 0}, view = {0, 0, -1}, up = {0, 1, 0}, fov = 30,
  ambient = {1.0, 1.0, 1.0},
  lights = { black_light },
}

gr.render{
  root = scene, output = 'tests/out/furnace_half.png',
  width = 32, height = 32,
  eye = {0, 0, 0}, view = {0, 0, -1}, up = {0, 1, 0}, fov = 30,
  ambient = {0.5, 0.5, 0.5},
  lights = { black_light },
}
