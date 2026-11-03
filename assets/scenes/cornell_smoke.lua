-- The Cornell box with its two blocks as smoke, the book's scene (Ray
-- Tracing: The Next Week, 9.3), with the book's larger, dimmer lamp; each
-- block is the boundary of a gr.constant_medium.
--     ./build/raytracer assets/scenes/cornell_smoke.lua

local function wall(name, q, u, v, material)
  local quad = gr.quad(name, q, u, v)
  quad:set_material(material)
  return quad
end

-- The box with opposite corners a and b, as one gr.box: a single slab test.
-- A medium tests its boundary twice per ray, so six quads here would be most
-- of the scene's cost. A gr.box has no (u, v), which a boundary never needs.
local function box(name, a, b, material)
  local node = gr.box(name, a, b)
  node:set_material(material)
  return node
end

local red   = gr.lambertian{ kd = {0.65, 0.05, 0.05} }
local white = gr.lambertian{ kd = {0.73, 0.73, 0.73} }
local green = gr.lambertian{ kd = {0.12, 0.45, 0.15} }
local light = gr.diffuse_light{ radiance = {7, 7, 7} }

local scene = gr.node('root')

scene:add_child(wall('green',   {555, 0, 0},     {0, 555, 0},  {0, 0, 555},   green))
scene:add_child(wall('red',     {0, 0, 0},       {0, 555, 0},  {0, 0, 555},   red))
scene:add_child(wall('light',   {113, 554, 127}, {330, 0, 0},  {0, 0, 305},   light))
scene:add_child(wall('floor',   {0, 0, 0},       {555, 0, 0},  {0, 0, 555},   white))
scene:add_child(wall('ceiling', {555, 555, 555}, {-555, 0, 0}, {0, 0, -555},  white))
scene:add_child(wall('back',    {0, 0, 555},     {555, 0, 0},  {0, 555, 0},   white))

-- The boxes are only boundaries: never added to the scene, and their
-- material is never shaded. Turning and moving them is rigid, so the
-- density stays per world unit.
local tall_box = box('tall_box', {0, 0, 0}, {165, 330, 165}, white)
tall_box:rotate('Y', 15)
tall_box:translate(265, 0, 295)

local short_box = box('short_box', {0, 0, 0}, {165, 165, 165}, white)
short_box:rotate('Y', -18)
short_box:translate(130, 0, 65)

local tall = gr.constant_medium('tall', tall_box, 0.01)
tall:set_material(gr.isotropic{ albedo = {0, 0, 0} })
scene:add_child(tall)

local short = gr.constant_medium('short', short_box, 0.01)
short:set_material(gr.isotropic{ albedo = {1, 1, 1} })
scene:add_child(short)

gr.set_background('')
gr.set_tonemap{ operator = 'none' }

gr.render{
  root = scene, output = 'renders/cornell_smoke/cornell_smoke.png',
  width = 600, height = 600,
  eye = {278, 278, -800}, view = {0, 0, 1}, up = {0, 1, 0}, fov = 40,
  ambient = {0, 0, 0},
  lights = {},
  samples = 200, max_depth = 50,
}
