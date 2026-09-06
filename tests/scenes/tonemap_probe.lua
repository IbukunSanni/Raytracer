-- Checks the sRGB transfer is correct and switchable.
--
-- Matte grey sphere (kd 0.5, ks 0), ambient 0, light on the view axis so
-- the front pole has N.L = 1 and a diffuse term of 0.5. It lands on the
-- centre pixel. run_tests.sh renders twice via PROBE_SRGB: the images must
-- differ, and decodeSRGB(srgb centre) must equal the linear centre.
--


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
