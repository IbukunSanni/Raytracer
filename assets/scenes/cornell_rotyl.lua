-- The Cornell box as Ray Tracing: The Rest of Your Life's chapter 12 leaves
-- it, for the mixture density (RT_MIXTURE=1):
--     RT_MIXTURE=1 ./build/raytracer assets/scenes/cornell_rotyl.lua
--
-- ROTYL_SCENE picks the stage:
--
--   aluminum  the tall block is a perfect mirror (metal, fuzz 0). A mirror has
--             no density, only one direction, so its bounces skip the mixture
--             and keep their own; the walls around it still aim at the lamp.
--   glass     the short block becomes a glass ball, which focuses the lamp
--             into a caustic on the floor. The default.
--
-- ROTYL_SAMPLE_GLASS=1 also aims at the ball, as the book does, though it
-- gives off nothing. Bounces off the floor then reach the lamp through the
-- glass more often, so the caustic converges sooner; the mean cannot move,
-- because the density divided by is always the one the direction was drawn
-- from.
--
-- The room is the one in cornell_blocks.lua: five walls and a lamp, all
-- quads, with blocks of six quads turned and placed by node transforms.

local stage = os.getenv('ROTYL_SCENE') or 'glass'
assert(stage == 'aluminum' or stage == 'glass',
       'unknown ROTYL_SCENE: ' .. stage)
local sample_glass = os.getenv('ROTYL_SAMPLE_GLASS') == '1'

local function wall(name, q, u, v, material)
  local quad = gr.quad(name, q, u, v)
  quad:set_material(material)
  return quad
end

-- The six faces of the box with opposite corners a and b, under one node,
-- as the book's box(). A face turns its normal toward the ray, which a
-- mirror does not mind: it only ever reflects.
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
local black = gr.lambertian{ kd = {0, 0, 0} }

-- Black, so it reflects nothing: all its light is the emission.
local lamp = wall('light', {343, 554, 332}, {-130, 0, 0}, {0, 0, -105}, black)
lamp:set_emission{ radiance = {15, 15, 15} }

local scene = gr.node('root')

scene:add_child(wall('green',   {555, 0, 0},     {0, 555, 0},  {0, 0, 555},   green))
scene:add_child(wall('red',     {0, 0, 0},       {0, 555, 0},  {0, 0, 555},   red))
scene:add_child(lamp)
scene:add_child(wall('floor',   {0, 0, 0},       {555, 0, 0},  {0, 0, 555},   white))
scene:add_child(wall('ceiling', {555, 555, 555}, {-555, 0, 0}, {0, 0, -555},  white))
scene:add_child(wall('back',    {0, 0, 555},     {555, 0, 0},  {0, 555, 0},   white))

-- Each call composes onto the left of the node's matrix, so the calls apply
-- in the order written: turn the box about its own corner first, then move
-- it.
local tall_material = white
if stage == 'aluminum' then
  tall_material = gr.metal{ albedo = {0.8, 0.85, 0.88}, fuzz = 0.0 }
end
local tall = box('tall', {0, 0, 0}, {165, 330, 165}, tall_material)
tall:rotate('Y', 15)
tall:translate(265, 0, 295)
scene:add_child(tall)

local output
if stage == 'aluminum' then
  local short = box('short', {0, 0, 0}, {165, 165, 165}, white)
  short:rotate('Y', -18)
  short:translate(130, 0, 65)
  scene:add_child(short)
  output = 'renders/cornell_rotyl/cornell_rotyl_aluminum.png'
else
  local ball = gr.nh_sphere('glass', {190, 90, 190}, 90)
  ball:set_material(gr.dielectric{ ior = 1.5 })
  ball:set_sampled(sample_glass)
  scene:add_child(ball)
  output = 'renders/cornell_rotyl/cornell_rotyl_glass'
      .. (sample_glass and '_sampled' or '') .. '.png'
end

gr.set_background('')
gr.set_tonemap{ operator = 'none' }

gr.render{
  root = scene, output = output,
  width = 600, height = 600,
  eye = {278, 278, -800}, view = {0, 0, 1}, up = {0, 1, 0}, fov = 40,
  ambient = {0, 0, 0},
  lights = {},
  samples = 1000, max_depth = 50,
}
