-- The furnace test, at scene level.
--
-- An albedo-1 sphere inside a uniform emissive environment must be
-- INVISIBLE. Every ray that misses geometry returns the environment
-- radiance, and a surface that neither loses nor gains energy has to send
-- back exactly that same radiance from every point. So every pixel
-- resolves to the environment value, sphere and surround alike. Darker
-- means energy is being lost in the throughput loop; brighter means it is
-- being counted twice.
--
-- The material comes from FURNACE_MATERIAL, because the claim is about the
-- integrator and not about any one BSDF. A material that vanishes only at
-- one roughness is a material whose weight depends on the direction it
-- scattered.
--
-- Rough metal is the exception, and deliberately so. A wide fuzz lobe
-- straddles the horizon at grazing angles, and a perturbation that tips
-- the direction into the surface absorbs the ray rather than scattering
-- it. So a rough sphere is DARKER than its environment near the
-- silhouette, by design. It still must never be brighter, which is the
-- half of the criterion that survives.
--
-- Rendered twice. Radiance 1 is the criterion as written, but it lands on
-- byte 255 and clips, so a too-bright result would be hidden. Radiance 0.5
-- lands on 128 with headroom in both directions.
--
-- No background texture, so `ambient` is the uniform environment. The
-- light is black and exists only because the renderer requires one: a
-- point light is the single thing BSDF sampling cannot reach, so it could
-- not explain a departure from uniformity either way.
--
-- The BSDF checks in tests/bsdf_test.cpp cannot catch this. They integrate
-- a material; this integrates a path.

local kind = os.getenv('FURNACE_MATERIAL') or 'lambertian'

local materials = {
  -- Deliberately a gr.lambertian rather than a Blinn-Phong with a zeroed
  -- specular lobe: numerically the same material, but this one's closed
  -- form leaves nothing to argue about.
  lambertian = function() return gr.lambertian{ kd = {1.0, 1.0, 1.0} } end,
  mirror     = function() return gr.mirror{ albedo = {1.0, 1.0, 1.0} } end,

  -- Fuzz 0 cannot absorb: the lobe is a single direction, so this one is
  -- held to the same exact standard as the mirror.
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

-- 256 is a deliberate floor, not a default. The drift this test exists to
-- catch is a fraction of one byte, so it only shows on the pixels whose
-- geometry rounds the wrong way; too few pixels and a real error renders
-- clean. A 32x32 version of this scene passed while a 2048x2048 one failed
-- on the same code.
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
