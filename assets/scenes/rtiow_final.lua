-- The "Ray Tracing in One Weekend" cover scene, with two substitutions:
-- part of the small-object field is boxes rather than spheres, and the
-- centrepiece glass sphere is replaced by the spaceship mesh, hovering.
--
--     ./build/raytracer assets/scenes/rtiow_final.lua
--
-- Cost is dominated by the ship: Mesh linear-scans its 6208 triangles until
-- step 8 lands, so every ray that reaches the sky pays for all of them. The
-- defaults below are a few minutes; the book's own 1200x675 at 500 spp is
-- roughly two orders of magnitude more.

local WIDTH        = 480
local HEIGHT       = 270  -- 16:9, as the book frames it
local SAMPLES      = 8    -- multiplied by LENS_SAMPLES for the ray count
local LENS_SAMPLES = 4
local MAX_DEPTH    = 12

-- Fixed seed: the field is random but the scene is reproducible, so two
-- renders are comparable and a regression shows up as a difference.
math.randomseed(20260922)

local scene = gr.node('root')

-- ---------------------------------------------------------------- ground
-- Radius 1000 centred 1000 below the origin, so its top is the y = 0
-- plane. Cheaper than a mesh, and it curves away at the horizon.
local ground = gr.nh_sphere('ground', {0, -1000, 0}, 1000)
ground:set_material(gr.lambertian{ kd = {0.5, 0.5, 0.5} })
scene:add_child(ground)

-- ------------------------------------------------------- the object field
-- Two diffuse colours multiplied together, as the book does it: the product
-- biases toward the darker, more saturated end of the cube.
local function random_albedo(lo, hi)
  local function c() return lo + (hi - lo) * math.random() end
  return {c(), c(), c()}
end

-- The book's mix: 80% diffuse, 15% metal, 5% glass.
local function random_material()
  local roll = math.random()
  if roll < 0.8 then
    local a = random_albedo(0.0, 1.0)
    local b = random_albedo(0.0, 1.0)
    return gr.lambertian{ kd = {a[1] * b[1], a[2] * b[2], a[3] * b[3]} }
  elseif roll < 0.95 then
    return gr.metal{ albedo = random_albedo(0.5, 1.0),
                     fuzz = 0.5 * math.random() }
  else
    return gr.dielectric{ ior = 1.5 }
  end
end

local RADIUS       = 0.2
local BOX_FRACTION = 0.5  -- drawn separately, so the material mix is intact
local CLEARANCE    = 1.4  -- keeps the field off the three centrepieces

local count_sphere, count_box = 0, 0

-- 22x22 cells, one object jittered inside each.
for a = -11, 10 do
  for b = -11, 10 do
    local cx = a + 0.9 * math.random()
    local cz = b + 0.9 * math.random()

    local clear = true
    for _, keep in ipairs({{-4, 0}, {0, 0}, {4, 0}}) do
      local dx, dz = cx - keep[1], cz - keep[2]
      if math.sqrt(dx * dx + dz * dz) <= CLEARANCE then clear = false end
    end

    if clear then
      local name = 'obj_' .. a .. '_' .. b
      local node
      if math.random() < BOX_FRACTION then
        -- nh_box takes its MIN CORNER, not its centre, so back off by the
        -- half-size in x and z and start at y = 0 to sit on the ground.
        node = gr.nh_box(name, {cx - RADIUS, 0.0, cz - RADIUS}, 2 * RADIUS)
        count_box = count_box + 1
      else
        node = gr.nh_sphere(name, {cx, RADIUS, cz}, RADIUS)
        count_sphere = count_sphere + 1
      end
      node:set_material(random_material())
      scene:add_child(node)
    end
  end
end

print(string.format('field: %d spheres, %d boxes', count_sphere, count_box))

-- --------------------------------------------------- the two big spheres
-- The book's third centrepiece, glass at (0, 1, 0), is the ship below.
local brown = gr.nh_sphere('brown', {-4, 1, 0}, 1.0)
brown:set_material(gr.lambertian{ kd = {0.4, 0.2, 0.1} })
scene:add_child(brown)

local chrome = gr.nh_sphere('chrome', {4, 1, 0}, 1.0)
chrome:set_material(gr.metal{ albedo = {0.7, 0.6, 0.5}, fuzz = 0.0 })
scene:add_child(chrome)

-- ---------------------------------------------------------- the spaceship
-- The model spans 73.4 x 57.0 x 49.6 with its bounding-box centre at
-- (0, 6.701, -6.153), so a bare translate would land it by that offset
-- rather than where asked. Transforms pre-multiply here (scale, then yaw,
-- then translate), so the offset has to be carried through the first two
-- before it can be cancelled.
local SHIP_SCALE = 1.0 / 36.0    -- 73.4 units long becomes about 2.0
local SHIP_YAW   = 205           -- degrees about y, nose toward the camera
local SHIP_AT    = {0.0, 2.7, 0.9}
local MODEL_CENTRE = {0.0, 6.701, -6.153}

local yaw = math.rad(SHIP_YAW)
local sx = MODEL_CENTRE[1] * SHIP_SCALE
local sy = MODEL_CENTRE[2] * SHIP_SCALE
local sz = MODEL_CENTRE[3] * SHIP_SCALE
local rx = sx * math.cos(yaw) + sz * math.sin(yaw)
local rz = -sx * math.sin(yaw) + sz * math.cos(yaw)

local ship = gr.mesh('spaceship', 'assets/models/spaceship.obj')
ship:set_material(gr.metal{ albedo = {0.72, 0.74, 0.80}, fuzz = 0.04 })
ship:scale(SHIP_SCALE, SHIP_SCALE, SHIP_SCALE)
ship:rotate('Y', SHIP_YAW)
ship:translate(SHIP_AT[1] - rx, SHIP_AT[2] - sy, SHIP_AT[3] - rz)
scene:add_child(ship)

-- ------------------------------------------------------------- lighting
-- The book's sky is a vertical gradient with no light source at all; a
-- uniform environment is the nearest thing here. gr.render still wants a
-- non-empty light list, so pass one at zero intensity.
--
-- For a directional look instead -- long shadows, a glint off the hull --
-- swap in the sun-and-sky map:
--   gr.set_background('assets/textures/sun_sky.png')
gr.set_background('')
local no_light = gr.light({0, 0, 0}, {0, 0, 0}, {1, 0, 0})

gr.set_tonemap{ operator = 'reinhard' }

-- ---------------------------------------------------------------- camera
-- The book gives lookfrom/lookat and 20 degrees; gr.render takes a view
-- DIRECTION. Aimed above the horizon rather than at it, because at 20
-- degrees a hovering ship does not otherwise fit in frame.
local eye    = {13, 2, 3}
local lookat = {0, 1.2, 0}

gr.render{
  root = scene,
  output = 'renders/rtiow_final_box.png',
  width = WIDTH, height = HEIGHT,
  eye = eye,
  view = {lookat[1] - eye[1], lookat[2] - eye[2], lookat[3] - eye[3]},
  up = {0, 1, 0},
  fov = 20,
  ambient = {0.70, 0.80, 1.00},
  lights = { no_light },

  samples = SAMPLES,
  max_depth = MAX_DEPTH,
  defocus_angle = 0.6,
  focus_dist = 10.0,
  lens_samples = LENS_SAMPLES,
}
