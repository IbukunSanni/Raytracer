-- The Next Week's final scene (10): every feature of the book in one frame.
--     ./build/raytracer assets/scenes/nw_final.lua
--
--   ground of 400 boxes, random heights   gr.box, one slab test each (8)
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
-- Cut down from the book's settings (beside each) so a render takes well
-- under a second, which makes it a quick benchmark for the scene graph:
-- 400 boxes and 1,000 spheres in two groups, each searched through a tree.

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

-- The box with opposite corners a and b, as one gr.box: a single slab test
-- where six quads would take six. A gr.box has no (u, v); the ground is a
-- flat colour, so nothing reads one.
local function box(name, a, b, material)
  local node = gr.box(name, a, b)
  node:set_material(material)
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
fog:set_material(gr.isotropic{ albedo = {0.2, 0.4, 0.9} })
scene:add_child(fog)

-- Haze over the whole scene, the camera inside it. The boundary is only a
-- boundary, so it stays out of the scene.
local haze_boundary = gr.nh_sphere('haze_boundary', {0, 0, 0}, 5000)
local haze = gr.constant_medium('haze', haze_boundary, 0.0001)
haze:set_material(gr.isotropic{ albedo = {1, 1, 1} })
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
