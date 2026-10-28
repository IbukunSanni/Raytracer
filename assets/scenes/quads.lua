-- Five quads, the book's scene (Ray Tracing: The Next Week, 6.6): an open
-- box seen through its missing front, each wall a different colour.
-- Each quad is a corner q and the two edges u and v leaving it. The edges'
-- lengths are the side lengths, so every wall here is 4 by 4.
--     ./build/raytracer assets/scenes/quads.lua

local function wall(name, q, u, v, kd)
  local quad = gr.quad(name, q, u, v)
  quad:set_material(gr.lambertian{ kd = kd })
  return quad
end

local scene = gr.node('root')

scene:add_child(wall('left',  {-3, -2, 5}, {0, 0, -4}, {0, 4, 0},  {1.0, 0.2, 0.2}))
scene:add_child(wall('back',  {-2, -2, 0}, {4, 0, 0},  {0, 4, 0},  {0.2, 1.0, 0.2}))
scene:add_child(wall('right', { 3, -2, 1}, {0, 0, 4},  {0, 4, 0},  {0.2, 0.2, 1.0}))
scene:add_child(wall('upper', {-2,  3, 1}, {4, 0, 0},  {0, 0, 4},  {1.0, 0.5, 0.0}))
scene:add_child(wall('lower', {-2, -3, 5}, {4, 0, 0},  {0, 0, -4}, {0.2, 0.8, 0.8}))

gr.set_background('')
gr.set_tonemap{ operator = 'reinhard' }

gr.render{
  root = scene, output = 'renders/quads/quads.png',
  width = 400, height = 400,
  eye = {0, 0, 9}, view = {0, 0, -1}, up = {0, 1, 0}, fov = 80,
  ambient = {0.70, 0.80, 1.00},
  lights = { gr.light({0, 0, 0}, {0, 0, 0}, {1, 0, 0}) },
  samples = 64,
}
