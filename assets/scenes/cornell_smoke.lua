-- The Cornell box with its two blocks as smoke, the book's scene (Ray
-- Tracing: The Next Week, 9.3). The room is cornell_blocks.lua's with a
-- larger, dimmer lamp; each block is the boundary of a gr.constant_medium.
--     ./build/raytracer assets/scenes/cornell_smoke.lua

local function wall(name, q, u, v, material)
  local quad = gr.quad(name, q, u, v)
  quad:set_material(material)
  return quad
end

-- The six faces of the box with opposite corners a and b, under one node,
-- as the book's box(). Each face is a quad, so its (u, v) runs 0 to 1.
local function box(name, a, b, material)
  local lo = { math.min(a[1], b[1]), math.min(a[2], b[2]), math.min(a[3], b[3]) }
  local hi = { math.max(a[1], b[1]), math.max(a[2], b[2]), math.max(a[3], b[3]) }

  local dx = { hi[1] - lo[1], 0, 0 }
  local dy = { 0, hi[2] - lo[2], 0 }
  local dz = { 0, 0, hi[3] - lo[3] }
  local function neg(e) return { -e[1], -e[2], -e[3] } end

  local node = gr.node(name)
  node:add_child(wall(name .. '_front',  { lo[1], lo[2], hi[3] },  dx,      dy,      material))
  node:add_child(wall(name .. '_right',  { hi[1], lo[2], hi[3] },  neg(dz), dy,      material))
  node:add_child(wall(name .. '_back',   { hi[1], lo[2], lo[3] },  neg(dx), dy,      material))
  node:add_child(wall(name .. '_left',   { lo[1], lo[2], lo[3] },  dz,      dy,      material))
  node:add_child(wall(name .. '_top',    { lo[1], hi[2], hi[3] },  dx,      neg(dz), material))
  node:add_child(wall(name .. '_bottom', { lo[1], lo[2], lo[3] },  dx,      dz,      material))
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
tall:set_material(gr.volume_scatter{ albedo = {0, 0, 0} })
scene:add_child(tall)

local short = gr.constant_medium('short', short_box, 0.01)
short:set_material(gr.volume_scatter{ albedo = {1, 1, 1} })
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
