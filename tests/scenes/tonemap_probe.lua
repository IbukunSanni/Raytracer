-- Step 2 check: the linear pipeline is genuinely linear and the sRGB
-- transfer is a correct, invertible curve you can switch off.
--
-- Setup: matte grey sphere (kd 0.5, ks 0), ambient 0. The light sits
-- between the camera and the sphere on the view axis, so the front pole
-- has N.L exactly 1; its diffuse term is kd * lightColour = 0.5. That pole
-- projects to the centre pixel.
--
-- Rendered twice (run_tests.sh sets PROBE_SRGB): the two images must
-- differ, and decodeSRGB(centre of the srgb dump) must equal the centre of
-- the linear dump.
--
-- NOTE: the centre currently reads ~0.375, not 0.5 -- the always-on
-- reflection blend mixes in 25% of a background miss (black): 0.5 * 0.75.
-- The exact-0.5 reading is blocked by that glm::mix hack, which roadmap
-- step 4 replaces with a real BSDF. It is not a tone-mapping problem.

local grey = gr.material({0.5, 0.5, 0.5}, {0.0, 0.0, 0.0}, 0)

local scene = gr.node('root')

local ball = gr.nh_sphere('ball', {0, 0, -500}, 100)
ball:set_material(grey)
scene:add_child(ball)

local light = gr.light({0, 0, -100}, {1.0, 1.0, 1.0}, {1, 0, 0})

gr.set_samples(1)
gr.set_tonemap{ operator = 'none', srgb = (os.getenv('PROBE_SRGB') == '1') }

gr.render{
  root = scene, output = 'tests/out/tonemap_probe.png',
  width = 64, height = 64,
  eye = {0, 0, 0}, view = {0, 0, -1}, up = {0, 1, 0}, fov = 30,
  ambient = {0, 0, 0},
  lights = { light },
}
