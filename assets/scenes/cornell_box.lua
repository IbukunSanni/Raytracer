-- A Cornell box holding three vehicles stacked by altitude: the car on the
-- floor, the drone hovering, the spaceship near the ceiling.
--
--     ./build/raytracer assets/scenes/cornell_box.lua
--
-- Interior is x[-1,1], y[0,2], z[-1,1], open at the front toward the camera.
-- Enclosed geometry is the point: rays bounce instead of escaping to a sky,
-- so a ray keeps paying for full mesh scans until the depth cap stops it.
-- Measured at 7.13 scans per pixel-sample against 2.84 for macho-cows, which
-- is what makes this the most demanding scene here for step 8.
--
-- This scene is what exposed the shadow-ray far bound: with kMaxT, geometry
-- behind a light occludes it, and in a closed room the ceiling is always
-- behind a ceiling lamp. Every surface shadowed itself and the frame was
-- black. The bound is 1.0 now, because the light sits at t = 1.

local WIDTH     = 400
local HEIGHT    = 400
local SAMPLES   = 32
local MAX_DEPTH = 8

local scene = gr.node('root')

local white = gr.lambertian{ kd = {0.73, 0.73, 0.73} }
local red   = gr.lambertian{ kd = {0.65, 0.05, 0.05} }
local green = gr.lambertian{ kd = {0.12, 0.45, 0.15} }

-- nh_box is a CUBE -- one size, and `pos` is its min corner -- so a wall is a
-- unit cube scaled into a slab and then moved. Transforms pre-multiply, so
-- the scale applies first and the translate places the scaled corner.
local T = 0.05  -- wall thickness

local function slab(name, sx, sy, sz, tx, ty, tz, material)
  local b = gr.nh_box(name, {0, 0, 0}, 1)
  b:set_material(material)
  b:scale(sx, sy, sz)
  b:translate(tx, ty, tz)
  scene:add_child(b)
end

slab('floor',   2.0, T,   2.0,  -1.0,    -T,   -1.0,     white)
slab('ceiling', 2.0, T,   2.0,  -1.0,    2.0,  -1.0,     white)
slab('back',    2.0, 2.0, T,    -1.0,    0.0,  -1.0 - T, white)
slab('left',    T,   2.0, 2.0,  -1.0 - T, 0.0, -1.0,     red)
slab('right',   T,   2.0, 2.0,   1.0,    0.0,  -1.0,     green)

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
-- half its scaled height puts the wheels on the floor.
local CAR_SCALE = 1.45 / 489.44   -- 1.45 units long
place_mesh('car', 'assets/models/Lamborghini_Aventador.obj',
           CAR_SCALE, 35, {-19.54, 58.82, -27.05},
           {0.02, 117.55 * CAR_SCALE / 2.0, 0.14},
           gr.metal{ albedo = {0.72, 0.12, 0.10}, fuzz = 0.08 })

-- Drone: already small, 0.57 x 0.21 x 0.78 about (0, -0.06, -0.13).
place_mesh('drone', 'assets/models/Drone.obj',
           0.64, 25, {0.0, -0.06, -0.13}, {0.45, 1.00, -0.10},
           gr.metal{ albedo = {0.45, 0.47, 0.52}, fuzz = 0.20 })

-- Spaceship highest, offset in x so it does not sit directly under the lamp
-- and shadow everything below it.
place_mesh('spaceship', 'assets/models/spaceship.obj',
           1.0 / 90.0, 200, {0.0, 6.701, -6.153}, {-0.35, 1.52, -0.25},
           gr.metal{ albedo = {0.80, 0.82, 0.86}, fuzz = 0.03 })

-- One dielectric, for a BSDF the three metals do not cover. A sphere is
-- analytic, so it costs no triangles.
local glass_ball = gr.nh_sphere('glass_ball', {-0.62, 0.22, 0.45}, 0.22)
glass_ball:set_material(gr.dielectric{ ior = 1.5 })
scene:add_child(glass_ball)

-- -------------------------------------------------------------- lighting
-- A point light under the ceiling. There is no emissive geometry here, so
-- this is the only way to light a closed room; the real Cornell box uses an
-- area light in the ceiling, which is step 10.
local lamp = gr.light({0.0, 1.90, 0.30}, {3.2, 3.2, 3.1}, {1, 0, 0})

-- Barely anything leaks in through the open front, but not zero, so the
-- opening does not read as a black void.
gr.set_background('')
gr.set_tonemap{ operator = 'reinhard' }

gr.render{
  root = scene,
  output = 'renders/cornell_box.png',
  width = WIDTH, height = HEIGHT,
  eye = {0, 1.0, 4.0},
  view = {0, 0, -4.0},
  up = {0, 1, 0},
  fov = 36,
  ambient = {0.03, 0.03, 0.04},
  lights = { lamp },

  samples = SAMPLES,
  max_depth = MAX_DEPTH,
}
