-- The Next Week's first render (2.5, "bouncing spheres"), laid out as the
-- book has it: an all-sphere field, glass, brown and chrome centrepieces.
-- rtiow_final.lua's RTIOW_BOUNCE=1 is the same idea on this project's own
-- variant of the scene (boxes, a ship); this one is the book's.
--
--     ./build/raytracer assets/scenes/bouncing_spheres.lua
--
-- Tuned so the blur shows up in reflections, not only on the spheres:
--   * Every field metal is a perfect mirror. The book draws fuzz up to 0.5,
--     which smears a reflection on its own and hides the motion in it.
--   * The chrome centrepiece is neutral and bright, so what it reflects is
--     not tinted bronze.
--   * The camera sits nearer the chrome sphere than the book's, so it fills
--     more of the frame and the field streaks across it.
-- A blurred sphere's reflection streaks the way the sphere does: each bounce
-- ray carries its camera sample's instant.

local WIDTH        = 960
local HEIGHT       = 540
local SAMPLES      = 32   -- multiplied by LENS_SAMPLES for the ray count
local LENS_SAMPLES = 4
local MAX_DEPTH    = 12

math.randomseed(20261024)

local scene = gr.node('root')

local ground = gr.nh_sphere('ground', {0, -1000, 0}, 1000)
ground:set_material(gr.lambertian{ kd = {0.5, 0.5, 0.5} })
scene:add_child(ground)

local function random_albedo(lo, hi)
  local function c() return lo + (hi - lo) * math.random() end
  return {c(), c(), c()}
end

-- How far a diffuse sphere rises during the shutter, in [0, 0.5), as the
-- book has it.
local function bounce_height(a, b)
  local h = math.sin(a * 12.9898 + b * 78.233) * 43758.5453
  return 0.5 * (h - math.floor(h))
end

local RADIUS = 0.2

for a = -11, 10 do
  for b = -11, 10 do
    local cx = a + 0.9 * math.random()
    local cz = b + 0.9 * math.random()
    local roll = math.random()

    -- The book keeps the field 0.9 clear of the chrome sphere only.
    local dx, dz = cx - 4, cz
    if math.sqrt(dx * dx + dz * dz) > 0.9 then
      local name = 'ball_' .. a .. '_' .. b
      local node
      if roll < 0.8 then
        local p = random_albedo(0.0, 1.0)
        local q = random_albedo(0.0, 1.0)
        node = gr.nh_sphere(name, {cx, RADIUS, cz}, RADIUS,
                            {cx, RADIUS + bounce_height(a, b), cz})
        node:set_material(gr.lambertian{ kd = {p[1] * q[1], p[2] * q[2],
                                               p[3] * q[3]} })
      elseif roll < 0.95 then
        node = gr.nh_sphere(name, {cx, RADIUS, cz}, RADIUS)
        node:set_material(gr.metal{ albedo = random_albedo(0.5, 1.0),
                                    fuzz = 0.0 })
      else
        node = gr.nh_sphere(name, {cx, RADIUS, cz}, RADIUS)
        node:set_material(gr.dielectric{ ior = 1.5 })
      end
      scene:add_child(node)
    end
  end
end

local glass = gr.nh_sphere('glass', {0, 1, 0}, 1.0)
glass:set_material(gr.dielectric{ ior = 1.5 })
scene:add_child(glass)

local brown = gr.nh_sphere('brown', {-4, 1, 0}, 1.0)
brown:set_material(gr.lambertian{ kd = {0.4, 0.2, 0.1} })
scene:add_child(brown)

local chrome = gr.nh_sphere('chrome', {4, 1, 0}, 1.0)
chrome:set_material(gr.metal{ albedo = {0.9, 0.9, 0.9}, fuzz = 0.0 })
scene:add_child(chrome)

-- Not in the book: two larger bouncers beside the chrome sphere. A convex
-- mirror shrinks what it shows, so the field's 0.2-radius balls reflect as
-- dots; these sit close enough to reflect large, streaks and all.
for _, b in ipairs({ {'hero_red',  {4.83, 0.35, 1.4}, {0.75, 0.08, 0.06}},
                     {'hero_blue', {5.57, 0.35, -0.44}, {0.08, 0.2, 0.75}} }) do
  local c = b[2]
  local hero = gr.nh_sphere(b[1], c, 0.35, {c[1], c[2] + 0.6, c[3]})
  hero:set_material(gr.lambertian{ kd = b[3] })
  scene:add_child(hero)
end

-- The book's sky has no light source; a uniform environment stands in, and
-- gr.render wants a non-empty light list, so this one is at zero.
gr.set_background('')
local no_light = gr.light({0, 0, 0}, {0, 0, 0}, {1, 0, 0})
gr.set_tonemap{ operator = 'reinhard' }

-- The book: lookfrom (13, 2, 3), lookat (0, 0, 0), 20 degrees, focus 10.
-- Moved in along the same line, and the focus with it, onto the chrome.
local eye    = {11, 1.8, 2.8}
local lookat = {0.8, 0.6, 0}

gr.render{
  root = scene,
  output = 'renders/bouncing_spheres.png',
  width = WIDTH, height = HEIGHT,
  eye = eye,
  view = {lookat[1] - eye[1], lookat[2] - eye[2], lookat[3] - eye[3]},
  up = {0, 1, 0},
  fov = 22,
  ambient = {0.70, 0.80, 1.00},
  lights = { no_light },

  samples = SAMPLES,
  max_depth = MAX_DEPTH,
  defocus_angle = 0.6,
  focus_dist = 7.6,   -- eye to the chrome centre
  lens_samples = LENS_SAMPLES,
}
