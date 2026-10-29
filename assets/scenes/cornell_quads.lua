-- The empty Cornell box from quads, the book's scene (Ray Tracing: The Next
-- Week, 7.5). A 555-unit room lit only by the small ceiling quad: no point
-- lights, a black background.
--     ./build/raytracer assets/scenes/cornell_quads.lua

local function wall(name, q, u, v, material)
  local quad = gr.quad(name, q, u, v)
  quad:set_material(material)
  return quad
end

local red   = gr.lambertian{ kd = {0.65, 0.05, 0.05} }
local white = gr.lambertian{ kd = {0.73, 0.73, 0.73} }
local green = gr.lambertian{ kd = {0.12, 0.45, 0.15} }
local light = gr.diffuse_light{ radiance = {15, 15, 15} }

local scene = gr.node('root')

scene:add_child(wall('green',   {555, 0, 0},     {0, 555, 0},  {0, 0, 555},   green))
scene:add_child(wall('red',     {0, 0, 0},       {0, 555, 0},  {0, 0, 555},   red))
scene:add_child(wall('light',   {343, 554, 332}, {-130, 0, 0}, {0, 0, -105},  light))
scene:add_child(wall('floor',   {0, 0, 0},       {555, 0, 0},  {0, 0, 555},   white))
scene:add_child(wall('ceiling', {555, 555, 555}, {-555, 0, 0}, {0, 0, -555},  white))
scene:add_child(wall('back',    {0, 0, 555},     {555, 0, 0},  {0, 555, 0},   white))

-- A uniform environment of `ambient`, which is black: all light is the lamp's.
gr.set_background('')
-- No tone mapping, as in the book: the lamp clips to white.
gr.set_tonemap{ operator = 'none' }

gr.render{
  root = scene, output = 'renders/cornell_quads/cornell_quads.png',
  width = 600, height = 600,
  eye = {278, 278, -800}, view = {0, 0, 1}, up = {0, 1, 0}, fov = 40,
  ambient = {0, 0, 0},
  lights = {},
  samples = 600, max_depth = 50,
}
