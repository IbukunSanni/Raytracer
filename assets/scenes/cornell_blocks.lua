-- The Cornell box with its two blocks, the book's scene (Ray Tracing: The
-- Next Week, 8.3). The room is cornell_quads.lua's; the blocks are boxes of
-- six quads, turned and placed by node transforms.
--     ./build/raytracer assets/scenes/cornell_blocks.lua
--
-- The book wraps an object in `translate` and `rotate_y` classes that move
-- the ray instead of the object. A gr.node transform is the same idea,
-- already built: the node carries the matrix, and a hit is computed in the
-- node's own space. So this chapter needs no C++, only the box helper below.

local function wall(name, q, u, v, material)
  local quad = gr.quad(name, q, u, v)
  quad:set_material(material)
  return quad
end

-- The six faces of the box with opposite corners a and b, under one node,
-- as the book's box(). Each face is a quad, so its (u, v) runs 0 to 1.
-- Lambertian only for now: a Quad turns its normal toward the ray, so a
-- glass box could not tell entering from leaving.
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
local light = gr.diffuse_light{ radiance = {15, 15, 15} }

local scene = gr.node('root')

scene:add_child(wall('green',   {555, 0, 0},     {0, 555, 0},  {0, 0, 555},   green))
scene:add_child(wall('red',     {0, 0, 0},       {0, 555, 0},  {0, 0, 555},   red))
scene:add_child(wall('light',   {343, 554, 332}, {-130, 0, 0}, {0, 0, -105},  light))
scene:add_child(wall('floor',   {0, 0, 0},       {555, 0, 0},  {0, 0, 555},   white))
scene:add_child(wall('ceiling', {555, 555, 555}, {-555, 0, 0}, {0, 0, -555},  white))
scene:add_child(wall('back',    {0, 0, 555},     {555, 0, 0},  {0, 555, 0},   white))

-- Each call composes onto the left of the node's matrix, so the calls apply
-- in the order written: turn the box about its own corner first, then move
-- it. The other order would swing it around the room's origin.
local tall = box('tall', {0, 0, 0}, {165, 330, 165}, white)
tall:rotate('Y', 15)
tall:translate(265, 0, 295)
scene:add_child(tall)

local short = box('short', {0, 0, 0}, {165, 165, 165}, white)
short:rotate('Y', -18)
short:translate(130, 0, 65)
scene:add_child(short)

gr.set_background('')
gr.set_tonemap{ operator = 'none' }

gr.render{
  root = scene, output = 'renders/cornell_blocks/cornell_blocks.png',
  width = 600, height = 600,
  eye = {278, 278, -800}, view = {0, 0, 1}, up = {0, 1, 0}, fov = 40,
  ambient = {0, 0, 0},
  lights = {},
  samples = 200, max_depth = 50,
}
