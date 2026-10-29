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
  volume = function() return gr.volume_scatter{ albedo = {1.0, 1.0, 1.0} } end,
}

assert(materials[kind], 'unknown FURNACE_MATERIAL: ' .. kind)

-- FURNACE_MOTION=1 moves the sphere sideways during the shutter. Motion
-- blur only decides where a sample meets the sphere, never how much light
-- comes back, so a moving sphere must vanish exactly as a still one does.
local moving = os.getenv('FURNACE_MOTION') == '1'
local tag = moving and (kind .. '_moving') or kind

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

gr.set_background('')
gr.set_tonemap{ operator = 'none', srgb = false }

-- 256 is a floor, not a default. The error this test looks for is a
-- fraction of a byte, so it only shows on the pixels whose geometry rounds
-- the wrong way. Too few pixels and a real fault renders clean.
local function render(name, radiance)
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
