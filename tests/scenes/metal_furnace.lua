-- An albedo-1 metal sphere sits inside a uniform emissive environment.
-- Every ray that misses geometry returns the environment's radiance, and a
-- reflective sphere that loses or gains no energy when it scatters a ray
-- must send back exactly that same radiance from every point on its
-- surface. So the sphere should be completely invisible: every pixel in
-- the render should equal the environment radiance, sphere and surround
-- alike. If the sphere reads darker, energy is being lost somewhere in the
-- scatter; if brighter, energy is being duplicated.
--
-- The scene has one light and it is black, at zero intensity: a point
-- light cannot be hit by a scattered ray sampled from a surface, so it
-- would never explain a departure from uniformity either way -- it exists
-- only because the renderer requires at least one light to be given.
--
-- Rendered twice: once at environment radiance 1, which lands on 255 and
-- would hide a too-bright result by clipping, and once at 0.5, which lands
-- on 128 with headroom to show brightness errors in either direction.

local metal = gr.metal{ albedo = {1.0, 1.0, 1.0}, fuzz = 0.3 }

local scene = gr.node('root')
local ball = gr.nh_sphere('ball', {0, 0, -500}, 100)
ball:set_material(metal)
scene:add_child(ball)

local black_light = gr.light({0, 0, -100}, {0, 0, 0}, {1, 0, 0})

gr.set_samples(64)
gr.set_background('')
gr.set_tonemap{ operator = 'none', srgb = false }

gr.render{
  root = scene, output = 'tests/out/metal_furnace_full.png',
  width = 32, height = 32,
  eye = {0, 0, 0}, view = {0, 0, -1}, up = {0, 1, 0}, fov = 30,
  ambient = {1.0, 1.0, 1.0},
  lights = { black_light },
}

gr.render{
  root = scene, output = 'tests/out/metal_furnace_half.png',
  width = 32, height = 32,
  eye = {0, 0, 0}, view = {0, 0, -1}, up = {0, 1, 0}, fov = 30,
  ambient = {0.5, 0.5, 0.5},
  lights = { black_light },
}
