-- One sphere against the environment texture, at whatever size
-- RESOLUTION asks for.
--
-- Nothing about a scene should depend on how many pixels you point at it.
-- The texture matters here: with no background loaded the environment is a
-- flat colour and the texture lookup never runs, so the test would pass
-- without exercising the thing it is named after.
--
-- The harness renders this at several sizes, chosen to sit either side of
-- the texture below, so the lookup is exercised both within the texture and
-- past its edges.

local size = tonumber(os.getenv('RESOLUTION')) or 512

local matte = gr.blinn_phong{ kd = {0.7, 1.0, 0.7}, ks = {0.2, 0.2, 0.2}, shininess = 25 }

local scene = gr.node('root')
local ball = gr.nh_sphere('ball', {0, 0, -400}, 100)
ball:set_material(matte)
scene:add_child(ball)

local light = gr.light({-100, 150, 400}, {0.9, 0.9, 0.9}, {1, 0, 0})

gr.set_background('assets/textures/kh_stain_glass.png')
gr.set_samples(1)

gr.render{
  root = scene, output = 'tests/out/resolution_' .. size .. '.png',
  width = size, height = size,
  eye = {0, 0, 800}, view = {0, 0, -1}, up = {0, 1, 0}, fov = 50,
  ambient = {0.3, 0.3, 0.3},
  lights = { light },
}
