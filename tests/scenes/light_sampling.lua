-- A floor under two lamps, with nothing that can bounce light a second time.
--
-- The floor is flat, so it cannot see itself. The lamps reflect nothing. The
-- ball is black, so it casts a shadow and sends nothing back. Every path that
-- carries light is therefore camera, floor, lamp: direct light is all there is,
-- and aiming at the lamps must converge to the same picture as bouncing at
-- random.
--
-- The near lamp is a unit square under three transforms, so it only lands in
-- the right place at the right size if they are all composed. The far lamp is
-- wider and hangs right above it, so rays aimed at the far one cross the near
-- one: a direction's density has to count both.
--
-- Radiance stays low enough that no pixel clips, and the output is linear, so
-- a mean over the frame is a mean of radiance.

local scene = gr.node('root')

local floor = gr.quad('floor', {-10, 0, -10}, {20, 0, 0}, {0, 0, 20})
floor:set_material(gr.lambertian{ kd = {0.5, 0.5, 0.5} })
scene:add_child(floor)

local ball = gr.nh_sphere('ball', {0.5, 0.4, 0.3}, 0.4)
ball:set_material(gr.lambertian{ kd = {0, 0, 0} })
scene:add_child(ball)

-- Turning +z to -y faces the lamp's front down, at the floor.
local mount = gr.node('mount')
mount:rotate('X', 90)
mount:translate(-1, 1.5, -1)
scene:add_child(mount)

local near_lamp = gr.quad('near_lamp', {0, 0, 0}, {1, 0, 0}, {0, 1, 0})
near_lamp:set_material(gr.diffuse_light{ radiance = {2, 2, 2} })
near_lamp:scale(2, 2, 1)
mount:add_child(near_lamp)

local far_lamp = gr.quad('far_lamp', {-2, 2.5, -2}, {4, 0, 0}, {0, 0, 4})
far_lamp:set_material(gr.diffuse_light{ radiance = {1, 1, 1} })
scene:add_child(far_lamp)

gr.set_background('')
gr.set_tonemap{ operator = 'none', srgb = false }

gr.render{
  root = scene, output = 'tests/out/light_sampling.png',
  width = 64, height = 64,
  eye = {0, 1.4, 0}, view = {0, -1, 0}, up = {0, 0, 1}, fov = 90,
  ambient = {0, 0, 0},
  lights = {},
  samples = 256,
}
