-- Defocused highlights, for the aperture-shape writeup.
--     ./build/raytracer assets/scenes/bokeh.lua
--
-- A field of small glossy spheres far behind the focal plane, each carrying
-- one tight specular highlight. Out of focus, every highlight spreads into a
-- disc the shape of the aperture -- which is the picture the post needs.
--
-- The highlights have to come from a specular lobe rather than a mirror: a
-- delta BSDF has zero chance of sampling a point light, so a mirror sphere in
-- a dark room stays black. A high-shininess Blinn-Phong lobe is reachable by
-- next event estimation and gives a near-point highlight instead.

local eye    = {0.0, 0.0, 2.0}
local lookat = {0.0, 0.0, -1.0}
local view   = {lookat[1] - eye[1], lookat[2] - eye[2], lookat[3] - eye[3]}

local scene = gr.node('root')

-- A fixed LCG rather than math.random, so the frame is identical on any Lua.
local seed = 20260923
local function rand()
  seed = (1103515245 * seed + 12345) % 2147483648
  return seed / 2147483648
end
local function between(lo, hi) return lo + (hi - lo) * rand() end

-- The subject: sharp, on the focal plane, so the blur behind it has something
-- to be measured against.
local subject = gr.nh_sphere('subject', {-0.55, -0.30, 0.0}, 0.22)
subject:set_material(gr.lambertian{ kd = {0.025, 0.028, 0.038} })
scene:add_child(subject)

-- The highlight field. Small and far back: the blur circle grows with the
-- distance from the focal plane, and a small sphere keeps each highlight
-- close to a point source so the aperture shape stays crisp.
local tints = {
  {1.00, 0.86, 0.62},  -- warm
  {0.62, 0.80, 1.00},  -- cool
  {1.00, 0.70, 0.75},  -- pink
  {0.78, 1.00, 0.80},  -- green
  {1.00, 1.00, 0.95},  -- white
}

for i = 1, 46 do
  local z = between(-15.0, -6.0)
  local s = gr.nh_sphere('spark' .. i, {between(-4.2, 4.2), between(-2.6, 2.6), z}, 0.22)
  local tint = tints[math.floor(rand() * #tints) + 1]
  s:set_material(gr.blinn_phong{
    kd = {0.0, 0.0, 0.0},
    ks = tint,
    shininess = 40.0,
  })
  scene:add_child(s)
end

-- Constant attenuation, so every sphere in the field is lit alike however far
-- back it sits and the bokeh discs stay the same brightness across the frame.
local key = gr.light({0.0, 0.6, 2.4}, {65.0, 65.0, 65.0}, {1, 0, 0})

gr.set_background('')
gr.set_tonemap{ operator = 'reinhard-extended', white_point = 3.5 }

-- The subject sits on the view axis, so its focus distance is just its offset
-- along that axis from the eye.
local focus = 2.0

gr.render{
  root = scene, output = 'renders/bokeh.png',
  width = 720, height = 420,
  eye = eye, view = view, up = {0, 1, 0}, fov = 32,
  ambient = {0.015, 0.018, 0.028},
  lights = { key },

  samples = 80,
  defocus_angle = 9.0,
  focus_dist    = focus,
  lens_samples  = 48,
}
