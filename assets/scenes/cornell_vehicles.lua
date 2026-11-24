-- The Cornell box with the tall block and the glass ball, lit only by its
-- ceiling lamp, with three vehicles added: a car on the floor, a drone
-- hovering, a spaceship near the ceiling. Render it with the mixture density,
-- so half of each diffuse bounce aims at the lamp:
--     RT_MIXTURE=1 ./build/raytracer assets/scenes/cornell_vehicles.lua
--
-- Spaceship: "Spaceship" by Quaternius (https://poly.pizza/m/u105mYHLHU), CC0.
-- Car: "CAR Model" by Ignition Labs (https://poly.pizza/m/5zUWP5UsLg-),
-- CC BY 3.0.
-- Drone: "Drone" by NateGazzard (https://poly.pizza/m/DNbUoMtG3H),
-- CC BY 3.0.
--
-- The room spans 0..555 on every axis, open at z = 0, and the camera looks
-- down +z. With y up, that puts +x on the LEFT of the frame: the green wall at
-- x = 555 is on the left, the red one at x = 0 on the right.
--
-- All three vehicles are metal, and a metal is specular here, fuzz or not, so
-- its bounces keep their own direction and never aim at the lamp. Their
-- reflections converge at the BSDF's rate, not the mixture's.

local function wall(name, q, u, v, material)
  local quad = gr.quad(name, q, u, v)
  quad:set_material(material)
  return quad
end

-- The six faces of the box with opposite corners a and b, under one node.
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
-- it. Its footprint covers about x 265..467, z 252..454, and its top is at
-- y = 330.
local tall = box('tall', {0, 0, 0}, {165, 330, 165}, white)
tall:rotate('Y', 15)
tall:translate(265, 0, 295)
scene:add_child(tall)

-- Covers x and z 100..280, up to y = 180.
local ball = gr.nh_sphere('glass', {190, 90, 190}, 90)
ball:set_material(gr.dielectric{ ior = 1.5 })
scene:add_child(ball)

-- ---------------------------------------------------------------- meshes
-- Every model has its bounding box somewhere other than the origin, so a
-- bare translate would land it by that offset. Carry the offset through the
-- scale and the yaw first, then cancel it: that makes `at` mean the bbox
-- centre, which is the only placement unit worth reasoning in.
local function place_mesh(name, path, scale, yaw_deg, centre, at, material)
  local node = gr.mesh(name, path)
  node:set_material(material)
  node:scale(scale, scale, scale)
  node:rotate('Y', yaw_deg)

  local yaw = math.rad(yaw_deg)
  local cx, cy, cz = centre[1] * scale, centre[2] * scale, centre[3] * scale
  local rx =  cx * math.cos(yaw) + cz * math.sin(yaw)
  local rz = -cx * math.sin(yaw) + cz * math.cos(yaw)

  node:translate(at[1] - rx, at[2] - cy, at[3] - rz)
  scene:add_child(node)
  return node
end

-- Lamborghini: 230.20 x 117.55 x 489.44, bbox centre (-19.54, 58.82, -27.05).
-- Its underside already sits at y ~ 0 in model space, so centring the bbox at
-- half its scaled height puts the wheels on the floor. 220 long, it fits the
-- free floor in front of the block and left of the ball: its turned footprint
-- covers about x 325..535, z 8..248.
local CAR_SCALE = 220 / 489.44
place_mesh('car', 'assets/models/Lamborghini_Aventador.obj',
           CAR_SCALE, 215, {-19.54, 58.82, -27.05},
           {430, 117.55 * CAR_SCALE / 2, 128},
           gr.metal{ albedo = {0.72, 0.12, 0.10}, fuzz = 0.08 })

-- Drone: 0.57 x 0.21 x 0.78 about (0, -0.06, -0.13); about 100 wide here.
-- It hovers behind and above the ball, clear of its top.
place_mesh('drone', 'assets/models/Drone.obj',
           177.6, 205, {0.0, -0.06, -0.13}, {153, 278, 305},
           gr.metal{ albedo = {0.45, 0.47, 0.52}, fuzz = 0.20 })

-- Spaceship: 73.37 x 57.03 x 49.58 about (0, 6.70, -6.15); 170 wide here.
-- High over the block and toward the green wall, so it neither sits under
-- the lamp nor shadows everything below it.
local SHIP_SCALE = 170 / 73.37
place_mesh('spaceship', 'assets/models/spaceship.obj',
           SHIP_SCALE, 20, {0.0, 6.701, -6.153}, {440, 466, 300},
           gr.metal{ albedo = {0.80, 0.82, 0.86}, fuzz = 0.03 })

gr.set_background('')
gr.set_tonemap{ operator = 'none' }

gr.render{
  root = scene, output = 'renders/cornell_box/cornell_vehicles.png',
  width = 1200, height = 1200,
  eye = {278, 278, -800}, view = {0, 0, 1}, up = {0, 1, 0}, fov = 40,
  ambient = {0, 0, 0},
  lights = {},
  samples = 16, max_depth = 50,
}
