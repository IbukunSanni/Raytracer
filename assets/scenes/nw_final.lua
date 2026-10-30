-- The Next Week's final scene (10): every feature of the book in one frame.
--     ./build/raytracer assets/scenes/nw_final.lua
--
--   ground of 400 boxes, random heights   quads under nodes (8)
--   the ceiling lamp                      diffuse_light quad (6, 7)
--   orange sphere sliding right           motion blur (2)
--   glass sphere                          dielectric
--   brushed-metal sphere                  metal, fuzz 1
--   blue glass ball with fog inside       constant_medium in a dielectric (9)
--   faint haze over everything            constant_medium, radius 5000 (9)
--   planet                                image texture (4)
--   grey marble-ish ball                  Perlin noise (5)
--   cluster of 1000 white spheres         rotate + translate a node (8)
--
-- Two substitutions: jupiter_map.png for the book's earthmap.jpg, and this
-- renderer's grey Perlin noise for the book's marble.
--
-- Slow on purpose, for now. The scene graph tests a node's children one by
-- one, and here that is ~2400 quads and ~1000 spheres per ray. The book
-- wraps both groups in a bvh_node; that is the top-level BVH chapter 10
-- brings, and this scene is its benchmark. Until then, keep it small.

local WIDTH     = 300   -- the book: 800
local SAMPLES   = 16    -- the book: 10000
local MAX_DEPTH = 8     -- the book: 40

-- A fixed LCG rather than math.random, so the frame is identical on any Lua.
local seed = 20261029
local function rand()
  seed = (1103515245 * seed + 12345) % 2147483648
  return seed / 2147483648
end
local function between(lo, hi) return lo + (hi - lo) * rand() end

local function wall(name, q, u, v, material)
  local quad = gr.quad(name, q, u, v)
  quad:set_material(material)
  return quad
end

-- The six faces of the box with opposite corners a and b, as the book's
-- box(); the same helper as cornell_blocks.lua.
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

local scene = gr.node('root')

-- The ground: a 20 x 20 grid of boxes 100 wide, each 1 to 101 tall.
local ground_material = gr.lambertian{ kd = {0.48, 0.83, 0.53} }
local ground = gr.node('ground')
for i = 0, 19 do
  for j = 0, 19 do
    local x0, z0 = -1000 + i * 100, -1000 + j * 100
    ground:add_child(box('ground_' .. i .. '_' .. j, {x0, 0, z0},
                         {x0 + 100, between(1, 101), z0 + 100},
                         ground_material))
  end
end
scene:add_child(ground)

scene:add_child(wall('light', {123, 554, 147}, {300, 0, 0}, {0, 0, 265},
                     gr.diffuse_light{ radiance = {7, 7, 7} }))

-- Moves 30 to the right while the shutter is open.
local moving = gr.nh_sphere('moving', {400, 400, 200}, 50, {430, 400, 200})
moving:set_material(gr.lambertian{ kd = {0.7, 0.3, 0.1} })
scene:add_child(moving)

local glass = gr.nh_sphere('glass', {260, 150, 45}, 50)
glass:set_material(gr.dielectric{ ior = 1.5 })
scene:add_child(glass)

local metal = gr.nh_sphere('metal', {0, 150, 145}, 50)
metal:set_material(gr.metal{ albedo = {0.8, 0.8, 0.9}, fuzz = 1.0 })
scene:add_child(metal)

-- Subsurface: a glass shell, and blue fog filling it. Unlike cornell_smoke,
-- the boundary IS in the scene, so its surface refracts and the medium
-- scatters what gets through.
local shell = gr.nh_sphere('shell', {360, 150, 145}, 70)
shell:set_material(gr.dielectric{ ior = 1.5 })
scene:add_child(shell)
local fog = gr.constant_medium('fog', shell, 0.2)
fog:set_material(gr.volume_scatter{ albedo = {0.2, 0.4, 0.9} })
scene:add_child(fog)

-- Haze over the whole scene, the camera inside it. The boundary is only a
-- boundary, so it stays out of the scene.
local haze_boundary = gr.nh_sphere('haze_boundary', {0, 0, 0}, 5000)
local haze = gr.constant_medium('haze', haze_boundary, 0.0001)
haze:set_material(gr.volume_scatter{ albedo = {1, 1, 1} })
scene:add_child(haze)

local planet = gr.nh_sphere('planet', {400, 200, 400}, 100)
planet:set_material(gr.lambertian{
  kd = gr.image_texture{ path = 'assets/textures/jupiter_map.png' } })
scene:add_child(planet)

local marble = gr.nh_sphere('marble', {220, 280, 300}, 80)
marble:set_material(gr.lambertian{ kd = gr.noise(0.2) })
scene:add_child(marble)

-- 1000 small spheres in a 165 cube, turned and moved as one instance.
local white = gr.lambertian{ kd = {0.73, 0.73, 0.73} }
local cluster = gr.node('cluster')
for k = 1, 1000 do
  local s = gr.nh_sphere('cluster_' .. k,
                         {between(0, 165), between(0, 165), between(0, 165)},
                         10)
  s:set_material(white)
  cluster:add_child(s)
end
cluster:rotate('Y', 15)
cluster:translate(-100, 270, 395)
scene:add_child(cluster)

gr.set_background('')
gr.set_tonemap{ operator = 'none' }

local eye, lookat = {478, 278, -600}, {278, 278, 0}
gr.render{
  root = scene, output = 'renders/nw_final/nw_final.png',
  width = WIDTH, height = WIDTH,
  eye = eye,
  view = {lookat[1] - eye[1], lookat[2] - eye[2], lookat[3] - eye[3]},
  up = {0, 1, 0}, fov = 40,
  ambient = {0, 0, 0},
  lights = {},
  samples = SAMPLES, max_depth = MAX_DEPTH,
}
