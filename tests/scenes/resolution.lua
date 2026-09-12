-- One sphere, rendered at whatever RESOLUTION asks for.
--
-- The background used to be sampled by a 1:1 centre crop of the texture,
-- which indexed out of bounds as soon as the render exceeded the texture's
-- own size. Nothing about a scene should depend on how many pixels you
-- point at it, so the test sweeps sizes either side of that boundary.

local size = tonumber(os.getenv('RESOLUTION')) or 512

local matte = gr.blinn_phong{ kd = {0.7, 1.0, 0.7}, ks = {0.2, 0.2, 0.2}, shininess = 25 }

local scene = gr.node('root')
local ball = gr.nh_sphere('ball', {0, 0, -400}, 100)
ball:set_material(matte)
scene:add_child(ball)

local light = gr.light({-100, 150, 400}, {0.9, 0.9, 0.9}, {1, 0, 0})

-- The texture is what the regression was about, so it has to be loaded:
-- with no background the environment is a uniform colour and the lookup
-- that used to go out of bounds never runs. It is 920x891, and the sizes
-- the test sweeps sit either side of that.
gr.set_background('assets/textures/kh_stain_glass.png')
gr.set_samples(1)

gr.render{
  root = scene, output = 'tests/out/resolution_' .. size .. '.png',
  width = size, height = size,
  eye = {0, 0, 800}, view = {0, 0, -1}, up = {0, 1, 0}, fov = 50,
  ambient = {0.3, 0.3, 0.3},
  lights = { light },
}
