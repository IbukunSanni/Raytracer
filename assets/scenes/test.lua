-- ============================================================================
-- test.lua — the fixed regression scene for the roadmap staircase.
--
--     ./build/raytracer assets/scenes/test.lua        ->  renders/test.png
--
-- Small, cheap, and deliberately unchanging: keep the camera, geometry and
-- output path stable so two renders are comparable and a step's exit
-- criterion has something concrete to point at. Change SETTINGS below, not
-- the scene body, when testing a step.
--
-- What each part is here to exercise:
--
--   step 1  jitter + accumulation  -- sphere silhouettes and the tilted cube
--                                     edge; raise `samples`, or set `snapshot`
--                                     to watch it converge.
--   step 2  linear colour          -- `probe` is albedo 0.50, matte, lit
--                                     head-on. Set ambient = {0,0,0} and
--                                     key_light colour to {1,1,1}: its
--                                     brightest pixel must read 0.50 in a
--                                     linear dump (~188/255 sRGB), not 0.73.
--   step 5  thin-lens DoF          -- near / mid / far spheres are staggered
--                                     along the view axis; `focus_distance`
--                                     is set to `mid`. Set `aperture` > 0.
--   step 8  AABB + BVH             -- `blob` is a 116-triangle mesh. Run with
--                                     BVH_VERIFY=1 to check the tree against
--                                     the linear scan.
--   always  shadows + reflection   -- every object shadows the floor;
--                                     `chrome` mirrors the coloured spheres.
-- ============================================================================


-- ---------------------------------------------------------------------------
-- SETTINGS  — the only part you should need to touch
-- ---------------------------------------------------------------------------
local samples  = 4      -- per pixel. 1 = fast dev loop, 64 = quality check.
local snapshot = 0      -- >0 writes renders/test_NNNNspp.png every N samples.
local aperture = 0      -- >0 enables depth of field (try 25). 0 = pinhole.

local focus_distance = 900   -- along the view axis; = distance to `mid`.
local lens_samples   = 24

-- Tone map + transfer, applied at write-out. operator: 'none' | 'reinhard'
-- | 'reinhard-extended' | 'aces'. srgb = false writes a raw linear dump
-- (the step-2 probe check reads 0.50 there, ~0.735 with srgb on).
local tonemap_operator = 'none'
local tonemap_exposure = 1.0
local tonemap_srgb     = true


-- ---------------------------------------------------------------------------
-- MATERIALS   gr.material(diffuse, specular, shininess)
-- ---------------------------------------------------------------------------
local floor_mat = gr.material({0.55, 0.55, 0.55}, {0.0, 0.0, 0.0},  0)
local probe_mat = gr.material({0.50, 0.50, 0.50}, {0.0, 0.0, 0.0},  0)  -- step 2
local chrome    = gr.material({0.05, 0.05, 0.05}, {0.9, 0.9, 0.9}, 80)  -- mirror-ish
local red       = gr.material({0.85, 0.20, 0.20}, {0.3, 0.3, 0.3}, 20)
local green     = gr.material({0.20, 0.75, 0.30}, {0.3, 0.3, 0.3}, 20)
local blue      = gr.material({0.25, 0.35, 0.90}, {0.3, 0.3, 0.3}, 20)
local amber     = gr.material({0.90, 0.60, 0.15}, {0.4, 0.4, 0.4}, 25)
local violet    = gr.material({0.55, 0.30, 0.75}, {0.3, 0.3, 0.3}, 20)


-- ---------------------------------------------------------------------------
-- SCENE GRAPH   (floor top sits at y = -60)
-- ---------------------------------------------------------------------------
local scene = gr.node('root')

-- Floor: a very large, low sphere used as a near-flat plane.
local ground = gr.nh_sphere('ground', {0, -10000, -300}, 9940)
ground:set_material(floor_mat)
scene:add_child(ground)

-- Three spheres staggered along the view axis (z) — depth of field, and
-- targets for the mirror to reflect. `mid` is at the focus distance.
local near = gr.nh_sphere('near', {-95, -8, -95}, 46)
near:set_material(red)
scene:add_child(near)

local mid = gr.nh_sphere('mid', {35, 12, -370}, 66)
mid:set_material(green)
scene:add_child(mid)

local far = gr.nh_sphere('far', {210, 60, -650}, 100)
far:set_material(blue)
scene:add_child(far)

-- Mirror sphere, nearest the camera: depth-of-field foreground plus a curved
-- reflector showing the coloured spheres.
local chrome_ball = gr.nh_sphere('chrome', {-25, -18, 60}, 48)
chrome_ball:set_material(chrome)
scene:add_child(chrome_ball)

-- Matte 50%-grey sphere, off on its own and lit head-on — the step-2 probe.
local probe = gr.nh_sphere('probe', {165, -5, -200}, 55)
probe:set_material(probe_mat)
scene:add_child(probe)

-- Tilted cube, back left and raised clear of the spheres: straight edges at
-- an angle, the clearest anti-aliasing target.
local cube = gr.cube('cube')
cube:set_material(amber)
cube:scale(92, 92, 92)
cube:rotate('Y', 25)
cube:rotate('X', 6)
cube:translate(-175, -20, -430)
scene:add_child(cube)

-- Small triangle mesh (60 verts / 116 faces) — the BVH's job. Swap for
-- assets/models/mickey.obj (962 faces) to stress it harder.
local blob = gr.mesh('blob', 'assets/models/buckyball.obj')
blob:set_material(violet)
blob:scale(40, 40, 40)
blob:translate(105, -25, -235)
scene:add_child(blob)


-- ---------------------------------------------------------------------------
-- LIGHTS   gr.light(position, colour, falloff)   -- falloff unused, pass {1,0,0}
-- ---------------------------------------------------------------------------
-- Key: in front of `probe` and slightly above, so that sphere has a point
-- with N·L close to 1 that faces the camera (the step-2 measurement).
local key_light  = gr.light({150, 120, 300}, {0.9, 0.9, 0.9}, {1, 0, 0})
local fill_light = gr.light({-350, 150, 250}, {0.3, 0.3, 0.35}, {1, 0, 0})


-- ---------------------------------------------------------------------------
-- SAMPLING  (driven from SETTINGS above)
-- ---------------------------------------------------------------------------
gr.set_samples(samples)

gr.set_tonemap{
  operator = tonemap_operator,
  exposure = tonemap_exposure,
  srgb     = tonemap_srgb,
}

if snapshot > 0 then
  gr.set_snapshot_interval(snapshot)
end

if aperture > 0 then
  gr.set_lens(aperture, focus_distance, lens_samples)
end


-- ---------------------------------------------------------------------------
-- RENDER  — fixed. 300×300 keeps a full run well under a second at 1 spp.
-- ---------------------------------------------------------------------------
gr.render{
  root    = scene,
  output  = 'renders/test/test_gamma_corrected.png',

  width   = 2048,
  height  = 2048,

  eye     = {0, 50, 520},
  view    = {0, -0.09, -1}, -- look direction, not a target point
  up      = {0, 1, 0},
  fov     = 50,

  ambient = {0.15, 0.15, 0.15},

  lights  = { key_light, fill_light },
}
