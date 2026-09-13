-- ============================================================================
-- template.lua — the canonical annotated scene. Copy this to start a new one.
--
--     ./build/raytracer assets/scenes/template.lua
--
-- A scene file is not a data format. It is a Lua program that builds a C++
-- object graph by calling constructors, then calls the renderer once. Anything
-- Lua can do — loops, maths, reading a CSV — is available while building it.
-- ============================================================================


-- ---------------------------------------------------------------------------
-- 1. MATERIALS
--
-- Four constructors, all named-table form -- unknown or misspelled fields
-- are an error rather than a silent default:
--
--     gr.lambertian{ kd = {r, g, b} }
--     gr.blinn_phong{ kd = {r, g, b}, ks = {r, g, b}, shininess = n }
--     gr.mirror{ albedo = {r, g, b} }
--     gr.metal{ albedo = {r, g, b}, fuzz = f }
--
-- kd (diffuse) and ks (specular) are 0..1; shininess is the Blinn-Phong
-- exponent -- higher = tighter, harder highlight. Keep kd + ks <= 1 per
-- channel, or the scene loads with an energy-conservation warning.
--
-- gr.mirror is a perfect specular reflector: a delta lobe, so unlike the
-- first two it takes only an albedo -- no ks, no shininess, every photon
-- leaves in exactly one direction. Roadmap step 4.
--
-- gr.metal is that same lobe roughened. Each ray still reflects, but the
-- direction is nudged by a random point drawn from a ball of radius `fuzz`
-- centred on the mirror direction, so a point on the surface reflects a
-- small cone of the scene instead of a single ray. fuzz is 0..1 and out of
-- range is an error, not a silent clamp:
--
--     0.0   a mirror, pixel for pixel
--     0.05  polished steel -- reflections readable but soft
--     0.3   brushed metal -- shapes still placed, edges gone
--     1.0   the widest lobe, nearly diffuse but still tinted by albedo
--
-- Fuzz costs nothing extra per ray, but it turns one sharp reflection into
-- a distribution, so a fuzzy surface needs more samples per pixel than a
-- mirror before it stops looking grainy. Raise gr.set_samples with it.
--
-- These values are linear. The sRGB transfer is applied once, at write-out
-- (see gr.set_tonemap below).
--
-- gr.material(diffuse, specular, shininess) -- the old positional form --
-- still works, kept so pre-step-3 scenes still load. It is gr.blinn_phong
-- under an unchecked, positional spelling.
-- ---------------------------------------------------------------------------
local grass  = gr.lambertian{ kd = {0.3, 0.7, 0.3} }
local ivory  = gr.blinn_phong{ kd = {0.6, 0.6, 0.55}, ks = {0.3, 0.3, 0.3}, shininess = 60 }
local copper = gr.blinn_phong{ kd = {0.5, 0.25, 0.15}, ks = {0.4, 0.3, 0.2}, shininess = 30 }
local chrome  = gr.mirror{ albedo = {0.9, 0.9, 0.9} }
local brushed = gr.metal{ albedo = {0.85, 0.82, 0.78}, fuzz = 0.18 }
local glass = gr.dielectric{ior = 1.5};


-- ---------------------------------------------------------------------------
-- 2. THE SCENE GRAPH
--
-- Everything hangs off one root node. Interior nodes carry transforms;
-- GeometryNodes carry a primitive and a material.
--
-- Rays are transformed into each node's local space on the way down and the
-- hit is transformed back on the way out, so a child inherits every transform
-- above it. See docs/WALKTHROUGH.md, stage 6.
-- ---------------------------------------------------------------------------
local scene = gr.node('root')


-- --- Primitives in world space ("nh" = non-hierarchical) --------------------
--
-- These take their position and size directly, so they ignore any transform
-- you would otherwise need. Convenient for quick scenes.
--
--     gr.nh_sphere(name, centre, radius)
--     gr.nh_box(name, min_corner, side_length)

local ground = gr.nh_sphere('ground', {0, -1000, -500}, 900)  -- a huge sphere
ground:set_material(grass)                                    -- reads as a plane
scene:add_child(ground)


-- --- Unit primitives + transforms -------------------------------------------
--
--     gr.sphere(name)  -- unit sphere at the origin
--     gr.cube(name)    -- unit cube, corner at the origin
--
-- Prefer these when you want to scale or rotate. Note a scaled sphere becomes
-- an ellipsoid, which is exactly why normals need the inverse transpose.
--
-- TRANSFORM ORDER — the one thing that catches everyone:
-- each call PRE-multiplies the node's matrix (new = op * current), so calls
-- apply to the geometry in the order you write them.
--
--     n:scale(2,2,2);  n:translate(0,10,0)   -- scale, THEN move by 10
--     n:translate(0,10,0);  n:scale(2,2,2)   -- move, THEN scale -- which
--                                            -- scales the translation to 20

local ball = gr.sphere('ball')
ball:set_material(ivory)
ball:scale(80, 80, 80)          -- radius 1 -> 80
ball:translate(-120, 20, -400)  -- then move it into place
scene:add_child(ball)

local block = gr.cube('block')
block:set_material(copper)
block:scale(90, 90, 90)
block:rotate('Y', 30)           -- axis is 'X' | 'Y' | 'Z', angle in degrees
block:translate(110, -40, -420)
scene:add_child(block)

local mirror_ball = gr.sphere('mirror_ball')  -- gr.mirror in action
mirror_ball:set_material(chrome)
mirror_ball:scale(70, 70, 70)
mirror_ball:translate(0, 40, -250)            -- nearer camera, between the other two
scene:add_child(mirror_ball)

-- The same sphere in gr.metal, alongside the mirror so the two read as a
-- pair: identical albedo behaviour, one sharp and one roughened.
local metal_ball = gr.sphere('metal_ball')
metal_ball:set_material(brushed)
metal_ball:scale(58, 58, 58)
metal_ball:translate(165, 25, -245)           -- same depth as the mirror, to its right
scene:add_child(metal_ball)


local glass_ball = gr.sphere('glass_ball')
glass_ball:set_material(glass)
glass_ball:scale(58, 58, 58)
glass_ball:translate(165, 150, -245)           -- same depth as the mirror, to its right
scene:add_child(glass_ball)


-- --- Grouping ---------------------------------------------------------------
--
-- A plain gr.node carries a transform but no geometry. Transform the group and
-- everything under it moves together. This is also how you reuse geometry:
-- add the same node under two parents and it is drawn twice.

local cluster = gr.node('cluster')
cluster:translate(0, 120, -350)
scene:add_child(cluster)

for i = 0, 2 do                 -- plain Lua: build geometry procedurally
  local pip = gr.nh_sphere('pip' .. i, {(i - 1) * 45, 0, 0}, 16)
  pip:set_material(copper)
  cluster:add_child(pip)
end


-- --- Meshes -----------------------------------------------------------------
--
--     gr.mesh(name, 'assets/models/foo.obj')
--
-- Paths are relative to the working directory, so run from the repo root.
-- Only 'v' and 'f' lines are read — no vertex normals (meshes are flat
-- shaded) and no UVs yet. Roadmap steps 7 and 9.
--
-- local cow = gr.mesh('cow', 'assets/models/cow.obj')
-- cow:set_material(ivory)
-- cow:scale(40, 40, 40)
-- cow:translate(0, -30, -300)
-- scene:add_child(cow)


-- ---------------------------------------------------------------------------
-- 3. LIGHTS
--
--     gr.light(position, colour, falloff)
--
-- Point lights. colour is {r, g, b} and doubles as intensity — values above 1
-- are allowed and are what a tone mapper would compress.
--
-- falloff is {constant, linear, quadratic}. It is parsed and stored but the
-- shader never reads it, so lights currently do not attenuate with distance.
-- Roadmap step 10. Pass {1, 0, 0} until then.
--
-- Shadows are hard: one ray per light, fully lit or fully black.
-- ---------------------------------------------------------------------------
local key  = gr.light({-200, 300, 400}, {2.5133, 2.5133, 2.3562}, {1, 0, 0})
local fill = gr.light({ 300, 100, 200}, {0.9425, 0.9425, 1.2566},  {1, 0, 0})


-- ---------------------------------------------------------------------------
-- 4. SAMPLING + OUTPUT (all optional)
-- ---------------------------------------------------------------------------

-- Total samples per pixel. Each is jittered inside the pixel footprint, so
-- this is your anti-aliasing quality. 1 is fast and aliased; 64 is smooth.
-- The gr.metal sphere is the noisiest thing in this scene, so it is what
-- sets the floor here: below about 32 its reflection is visibly speckled.
gr.set_samples(64)

-- Write renders/template_NNNNspp.png every N samples as it converges, on top
-- of the final image. 0 (default) writes only the final image.
-- gr.set_snapshot_interval(4)

-- Thin-lens depth of field: aperture radius, focus distance, lens samples.
-- Aperture 0 (default) is a pinhole — everything sharp. Roadmap step 5.
-- gr.set_lens(20.0, 800.0, 16)

-- Tone map + transfer, applied at write-out. operator: 'none' (default),
-- 'reinhard', 'reinhard-extended' or 'aces'. white_point is the
-- ReinhardExtended knob. srgb = false writes a raw linear dump.
-- gr.set_tonemap{ operator = 'reinhard', exposure = 1.0 }


-- ---------------------------------------------------------------------------
-- 5. RENDER
--
-- The named-table form. Every field is required; unknown or misspelled fields
-- are an error rather than a silent default.
--
-- The old positional form still works and is what most existing scenes use:
gr.set_background('assets/textures/kh_stain_glass.png')

--     gr.render(root, output, w, h, eye, view, up, fov, ambient, lights)
-- ---------------------------------------------------------------------------
gr.render{
  root    = scene,
  output  = 'renders/template.png',

  width   = 400,
  height  = 400,

  -- Camera. `view` is a look DIRECTION, not a target point — to aim at
  -- something, subtract: view = target - eye.
  eye     = {0, 0, 500},
  view    = {0, 0, -1},
  up      = {0, 1, 0},

  -- Vertical field of view, in degrees.
  fov     = 50,

  -- Flat ambient term, added to every surface regardless of lighting. A
  -- stand-in for global illumination until there is a real integrator.
  ambient = {0.25, 0.25, 0.25},

  lights  = { key, fill },
}
