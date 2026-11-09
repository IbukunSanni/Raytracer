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
-- LIGHT_SAMPLING_LAMP=sphere makes the far lamp a ball, so the shapes aimed at
-- are a quad and a sphere, and a direction's density sums over both kinds.
--
-- LIGHT_SAMPLING_BALL=glass makes the ball glass, which focuses the lamps into
-- a caustic on the floor; =glass_sampled also marks it to be aimed at, as the
-- book does. Glass gives off nothing, so aiming at it may move the noise but
-- never the mean.
--
-- Radiance stays low enough that no pixel clips, and the output is linear, so
-- a mean over the frame is a mean of radiance.

local lamp_shape = os.getenv('LIGHT_SAMPLING_LAMP') or 'quad'
local ball_kind = os.getenv('LIGHT_SAMPLING_BALL') or 'black'
assert(lamp_shape == 'quad' or lamp_shape == 'sphere',
       'unknown LIGHT_SAMPLING_LAMP: ' .. lamp_shape)
assert(ball_kind == 'black' or ball_kind == 'glass' or
       ball_kind == 'glass_sampled', 'unknown LIGHT_SAMPLING_BALL: ' .. ball_kind)

-- The tests run in parallel, so each variant writes a file of its own.
local tag = (lamp_shape == 'quad' and '' or '_' .. lamp_shape)
    .. (ball_kind == 'black' and '' or '_' .. ball_kind)

local scene = gr.node('root')

local floor = gr.quad('floor', {-10, 0, -10}, {20, 0, 0}, {0, 0, 20})
floor:set_material(gr.lambertian{ kd = {0.5, 0.5, 0.5} })
scene:add_child(floor)

local ball = gr.nh_sphere('ball', {0.5, 0.4, 0.3}, 0.4)
if ball_kind == 'black' then
  ball:set_material(gr.lambertian{ kd = {0, 0, 0} })
else
  ball:set_material(gr.dielectric{ ior = 1.5 })
  ball:set_sampled(ball_kind == 'glass_sampled')
end
scene:add_child(ball)

-- Turning +z to -y faces the lamp's front down, at the floor.
local mount = gr.node('mount')
mount:rotate('X', 90)
mount:translate(-1, 1.5, -1)
scene:add_child(mount)

local near_lamp = gr.quad('near_lamp', {0, 0, 0}, {1, 0, 0}, {0, 1, 0})
near_lamp:set_material(gr.lambertian{ kd = {0, 0, 0} })
near_lamp:set_emission{ radiance = {2, 2, 2} }
near_lamp:scale(2, 2, 1)
mount:add_child(near_lamp)

local far_lamp
if lamp_shape == 'quad' then
  far_lamp = gr.quad('far_lamp', {-2, 2.5, -2}, {4, 0, 0}, {0, 0, 4})
  far_lamp:set_emission{ radiance = {1, 1, 1} }
else
  -- Behind the camera, which looks straight down, so it lights the floor
  -- without being seen.
  far_lamp = gr.nh_sphere('far_lamp', {0, 2.6, 0}, 0.8)
  far_lamp:set_emission{ radiance = {3, 3, 3} }
end
far_lamp:set_material(gr.lambertian{ kd = {0, 0, 0} })
scene:add_child(far_lamp)

gr.set_background('')
gr.set_tonemap{ operator = 'none', srgb = false }

gr.render{
  root = scene, output = 'tests/out/light_sampling' .. tag .. '.png',
  width = 64, height = 64,
  eye = {0, 1.4, 0}, view = {0, -1, 0}, up = {0, 0, 1}, fov = 90,
  ambient = {0, 0, 0},
  lights = {},
  samples = 256,
}
