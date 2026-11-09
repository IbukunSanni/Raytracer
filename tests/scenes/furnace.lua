-- The furnace test, at scene level.
--
-- An albedo-1 sphere inside a uniform emissive environment must be
-- INVISIBLE. Every ray that misses geometry returns the environment
-- radiance, and a surface that neither loses nor gains energy sends back
-- exactly that same radiance from every point. So every pixel resolves to
-- the environment value, sphere and surround alike. Darker means energy is
-- being lost; brighter means it is being counted twice.
--
-- FURNACE_MATERIAL picks the material, because the claim is about the
-- integrator rather than any one BSDF. A material that vanishes at only
-- one roughness is a material whose weight depends on which direction it
-- happened to scatter.
--
-- Rough metal is the deliberate exception. Its lobe straddles the horizon
-- at grazing angles, and a perturbation that tips the direction into the
-- surface absorbs the ray, so a rough sphere is darker than its
-- environment near the silhouette. It must still never be brighter.
--
-- Rendered twice. Radiance 1 is the criterion as usually written, but it
-- lands on byte 255 and clips, so a too-bright result would hide there.
-- Radiance 0.5 lands on 128 with headroom in both directions.
--
-- No background texture, so `ambient` is the uniform environment, and no
-- light is needed at all: BSDF sampling cannot reach a point light, so one
-- couldn't explain a departure from uniformity either way.

local kind = os.getenv('FURNACE_MATERIAL') or 'lambertian'

local materials = {
  -- A gr.lambertian rather than a Blinn-Phong with a zeroed specular
  -- lobe. Numerically the same material, but this one has a closed form
  -- and so leaves nothing to argue about.
  lambertian = function() return gr.lambertian{ kd = {1.0, 1.0, 1.0} } end,
  mirror     = function() return gr.mirror{ albedo = {1.0, 1.0, 1.0} } end,

  -- Fuzz 0 is a single direction and cannot absorb, so this one is held
  -- to the same exact standard as the mirror.
  metal_sharp = function() return gr.metal{ albedo = {1.0, 1.0, 1.0}, fuzz = 0.0 } end,
  metal_rough = function() return gr.metal{ albedo = {1.0, 1.0, 1.0}, fuzz = 0.3 } end,

  -- index = 1 has no interface to bend at, so this is a test of the delta
  -- plumbing alone, with the Fresnel/Snell physics held at identity.
  dielectric = function() return gr.dielectric{ ior = 1.0 } end,

  -- A real interface, where the eta^2 radiance scaling is not 1. Entering
  -- and leaving apply reciprocal factors, so every path that escapes
  -- carries them both and the mean is unchanged -- see the test.
  dielectric_glass = function() return gr.dielectric{ ior = 1.5 } end,

  -- Smoke rather than a surface: a gr.constant_medium filling the sphere.
  -- Scattering inside it has no cosine, so one left in darkens the ball.
  volume = function() return gr.isotropic{ albedo = {1.0, 1.0, 1.0} } end,
}

assert(materials[kind], 'unknown FURNACE_MATERIAL: ' .. kind)

-- FURNACE_MOTION=1 moves the sphere sideways during the shutter. Motion
-- blur only decides where a sample meets the sphere, never how much light
-- comes back, so a moving sphere must vanish exactly as a still one does.
local moving = os.getenv('FURNACE_MOTION') == '1'

-- FURNACE_LAMP=1 adds a lamp beside the ball, glowing at the sky's own
-- radiance, so light sampling has something to aim at. A lamp face emits
-- from its front only, so the lamp is a closed box of six faces, each front
-- turned outward: from any direction the nearest face is a lit one, and the
-- lamp is as bright as the sky it hides. Two faces back to back would leave
-- a rim where a ray slips between them and meets a dark back.
--
-- With BSDF sampling the frame must still be exactly flat. With the mixture
-- each path's weight is no longer 1, so only the mean is the claim.
local lamp = os.getenv('FURNACE_LAMP') == '1'

-- The tests run in parallel, so a mixture render needs a file of its own, or
-- it overwrites the BSDF one while that test is still reading it.
local mixture = os.getenv('RT_MIXTURE') == '1'

local tag = kind .. (moving and '_moving' or '') .. (lamp and '_lamp' or '')
    .. (mixture and '_mixture' or '')

local scene = gr.node('root')
local ball = moving
    and gr.nh_sphere('ball', {-15, 0, -500}, 100, {15, 0, -500})
    or gr.nh_sphere('ball', {0, 0, -500}, 100)
-- Density 0.01 makes the diameter two mean free paths: most rays that
-- enter scatter, and few scatter often enough to reach the bounce cap.
if kind == 'volume' then
  ball = gr.constant_medium('smoke', ball, 0.01)
end
ball:set_material(materials[kind]())
scene:add_child(ball)

-- Up and to the left, in front of the ball and clear of it: the nearest
-- corner is 137 from the centre, past the radius of 100. Close enough that
-- the ball's upper left sees it filling about a steradian, and partly in
-- frame, so the camera sees it too.
local lamp_faces = {}
if lamp then
  local lo, hi = {-110, 30, -370}, {-30, 110, -290}
  local dx, dy, dz = hi[1] - lo[1], hi[2] - lo[2], hi[3] - lo[3]
  -- A face's front is u x v; each pair below is ordered to point it outward.
  local faces = {
    {'lamp_px', {hi[1], lo[2], lo[3]}, {0, dy, 0}, {0, 0, dz}},
    {'lamp_nx', {lo[1], lo[2], lo[3]}, {0, 0, dz}, {0, dy, 0}},
    {'lamp_py', {lo[1], hi[2], lo[3]}, {0, 0, dz}, {dx, 0, 0}},
    {'lamp_ny', {lo[1], lo[2], lo[3]}, {dx, 0, 0}, {0, 0, dz}},
    {'lamp_pz', {lo[1], lo[2], hi[3]}, {dx, 0, 0}, {0, dy, 0}},
    {'lamp_nz', {lo[1], lo[2], lo[3]}, {0, dy, 0}, {dx, 0, 0}},
  }
  -- Black, so the lamp reflects nothing: all its light is the emission.
  local black = gr.lambertian{ kd = {0, 0, 0} }
  for _, f in ipairs(faces) do
    local face = gr.quad(f[1], f[2], f[3], f[4])
    face:set_material(black)
    scene:add_child(face)
    table.insert(lamp_faces, face)
  end
end

gr.set_background('')
gr.set_tonemap{ operator = 'none', srgb = false }

-- 256 is a floor, not a default. The error this test looks for is a
-- fraction of a byte, so it only shows on the pixels whose geometry rounds
-- the wrong way. Too few pixels and a real fault renders clean.
local function render(name, radiance)
  -- The lamp matches the sky in each pass, so its emission follows the pass.
  for _, face in ipairs(lamp_faces) do
    face:set_emission{ radiance = {radiance, radiance, radiance} }
  end
  gr.render{
    root = scene, output = 'tests/out/furnace_' .. tag .. '_' .. name .. '.png',
    width = 256, height = 256,
    eye = {0, 0, 0}, view = {0, 0, -1}, up = {0, 1, 0}, fov = 30,
    ambient = {radiance, radiance, radiance},
    lights = {},
    samples = 64,
    -- Smoke scatters many times where a surface bounces once or twice,
    -- and every path the cap cuts off darkens the ball.
    max_depth = kind == 'volume' and 64 or nil,
  }
end

render('full', 1.0)
render('half', 0.5)
