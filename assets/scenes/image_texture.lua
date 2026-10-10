-- Image textures on spheres, the way Ray Tracing: The Next Week wraps its
-- earth map (4.5).
--     ./build/raytracer assets/scenes/image_texture.lua
--
-- Left: uv_grid.png, the orientation check. Seen from this camera it must
-- read as the image does -- red top left, green top right, blue bottom left,
-- yellow bottom right. Anything else is a flipped u or v.
--
-- Right: a lat-long map of Jupiter. Its bands must run around the sphere,
-- parallel to the floor, and the poles must sit at the top and bottom.
--
-- Map: "Merged Cassini and Juno global map of Jupiter", processed by Björn
-- Jónsson (https://www.planetary.org/space-images/merged-cassini-and-juno).
-- NASA / JPL-Caltech / SSI / SwRI / MSSS / ASI / INAF / JIRAM / Björn
-- Jónsson, CC BY 3.0; see assets/textures/jupiter_map_license.txt.

local grid = gr.image_texture{ path = 'assets/textures/uv_grid.png' }
local jupiter = gr.image_texture{ path = 'assets/textures/jupiter_map.png' }
local floor = gr.checkered{ scale = 1.5,
                            yin = {0.2, 0.3, 0.1}, yang = {0.9, 0.9, 0.9} }

local scene = gr.node('root')

-- Radius 1000 centred 1000 below the origin, so its top is the y = 0 plane.
local ground = gr.nh_sphere('ground', {0, -1000, 0}, 1000)
ground:set_material(gr.lambertian{ kd = floor })
scene:add_child(ground)

local grid_ball = gr.nh_sphere('grid', {0, 1.2, 1.5}, 1.2)
grid_ball:set_material(gr.lambertian{ kd = grid })
scene:add_child(grid_ball)

local planet = gr.nh_sphere('jupiter', {0, 1.2, -1.5}, 1.2)
planet:set_material(gr.lambertian{ kd = jupiter })
scene:add_child(planet)

-- Pure sky light; gr.render wants a non-empty light list.
local black_light = gr.light({0, 0, -100}, {0, 0, 0}, {1, 0, 0})

gr.set_background('')
gr.set_tonemap{ operator = 'reinhard' }

gr.render{
  root = scene, output = 'renders/image_texture.png',
  width = 640, height = 360,

  eye = {13, 2, 3}, view = {-13, -2, -3}, up = {0, 1, 0}, fov = 25,

  ambient = {0.5, 0.7, 1.0},
  lights = { black_light },

  samples = 64,
}
