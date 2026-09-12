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
-- No background texture, so `ambient` is the uniform environment. The
-- light is black and exists only because the renderer requires one: BSDF
-- sampling cannot reach a point light, so it could not explain a departure
-- from uniformity either way.

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
}

assert(materials[kind], 'unknown FURNACE_MATERIAL: ' .. kind)

local scene = gr.node('root')
local ball = gr.nh_sphere('ball', {0, 0, -500}, 100)
ball:set_material(materials[kind]())
scene:add_child(ball)

local black_light = gr.light({0, 0, -100}, {0, 0, 0}, {1, 0, 0})

gr.set_samples(64)
gr.set_background('')
gr.set_tonemap{ operator = 'none', srgb = false }

-- 256 is a floor, not a default. The error this test looks for is a
-- fraction of a byte, so it only shows on the pixels whose geometry rounds
-- the wrong way. Too few pixels and a real fault renders clean.
local function render(name, radiance)
  gr.render{
    root = scene, output = 'tests/out/furnace_' .. kind .. '_' .. name .. '.png',
    width = 256, height = 256,
    eye = {0, 0, 0}, view = {0, 0, -1}, up = {0, 1, 0}, fov = 30,
    ambient = {radiance, radiance, radiance},
    lights = { black_light },
  }
end

render('full', 1.0)
render('half', 0.5)
