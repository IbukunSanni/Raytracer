-- The book's first lit scene (Ray Tracing: The Next Week, 7.4), plus a
-- marble cube beside the ball, lit only by a square panel and a sphere
-- above. The environment is black, so all the light comes from those two.
--     ./build/raytracer assets/scenes/simple_light.lua

local noise = gr.noise(4)
local scene = gr.node('root')

local ground = gr.nh_sphere('ground', {0, -1000, 0}, 1000)
ground:set_material(gr.lambertian{ kd = noise })
scene:add_child(ground)

local ball = gr.nh_sphere('ball', {0, 2, 0}, 2)
ball:set_material(gr.lambertian{ kd = noise })
scene:add_child(ball)

-- Side 4, the ball's diameter, on the side away from the panel. nh_box
-- takes the minimum corner, so this centres it at (0, 2, 5).
local cube = gr.nh_box('cube', {-2, 0, 3}, 4)
cube:set_material(gr.lambertian{ kd = noise })
scene:add_child(cube)

-- Radiance 4: brighter than white, so a surface it lights can be lit at
-- all. A radiance-1 panel would leave everything it shines on dim.
local lamp = gr.diffuse_light{ radiance = {4, 4, 4} }

local panel = gr.quad('panel', {3, 1, -2}, {2, 0, 0}, {0, 2, 0})
panel:set_material(lamp)
scene:add_child(panel)

local bulb = gr.nh_sphere('bulb', {0, 7, 0}, 2)
bulb:set_material(lamp)
scene:add_child(bulb)

gr.set_background('')
-- No curve: the lights clip to white, as they do in the book's image.
gr.set_tonemap{ operator = 'none' }

local eye, lookat = {26, 3, 6}, {0, 2, 0}
gr.render{
  root = scene, output = 'renders/simple_light/simple_light.png',
  width = 400, height = 225,
  eye = eye,
  view = {lookat[1] - eye[1], lookat[2] - eye[2], lookat[3] - eye[3]},
  up = {0, 1, 0}, fov = 20,
  ambient = {0, 0, 0},
  lights = {},
  samples = 100,
  max_depth = 50,
}
