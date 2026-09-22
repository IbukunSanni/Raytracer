-- Step 5's exit criterion: rack focus between a near and a far sphere.
--     ./build/raytracer assets/scenes/thin_lens.lua
--
-- Three spheres at three depths, rendered three times from one eye: once
-- through a pinhole, once focused on the near sphere, once on the far one.
-- Correct output is the pinhole shot sharp everywhere, the near shot with the
-- near sphere sharp and the other two soft, and the far shot the reverse.
-- While ThinLensRay still returns the pinhole ray, all three come out
-- byte-identical -- which is itself the check that the stub is the only
-- thing missing.

local eye    = {0.0, 0.5, 3.0}
local lookat = {0.0, 0.0, -1.0}
local view   = {lookat[1] - eye[1], lookat[2] - eye[2], lookat[3] - eye[3]}

-- The focal plane is flat and perpendicular to the view axis, so a sphere's
-- focus distance is its offset from the eye projected onto that axis -- not
-- its straight-line distance. Using the straight-line distance would put the
-- off-axis spheres slightly out of focus even with a correct lens.
local function along_view(p)
  local len = math.sqrt(view[1]^2 + view[2]^2 + view[3]^2)
  return ((p[1] - eye[1]) * view[1] + (p[2] - eye[2]) * view[2]
          + (p[3] - eye[3]) * view[3]) / len
end

local near_pos = {-0.6, 0.0,  1.0}
local mid_pos  = { 0.0, 0.0, -1.0}
local far_pos  = { 1.4, 0.0, -4.0}

local scene = gr.node('root')

local function ball(name, pos, kd)
  local s = gr.nh_sphere(name, pos, 0.5)
  s:set_material(gr.lambertian{ kd = kd })
  scene:add_child(s)
end

local ground = gr.nh_sphere('ground', {0.0, -100.5, -1.0}, 100.0)
ground:set_material(gr.lambertian{ kd = {0.8, 0.8, 0.0} })
scene:add_child(ground)

ball('near', near_pos, {0.8, 0.2, 0.2})
ball('mid',  mid_pos,  {0.2, 0.7, 0.3})
ball('far',  far_pos,  {0.2, 0.3, 0.8})

-- Pure environment light, as in glass_spheres_camera.lua: gr.render wants a
-- non-empty light list, so pass one at zero intensity.
local black_light = gr.light({0, 0, -100}, {0, 0, 0}, {1, 0, 0})

gr.set_background('')
gr.set_tonemap{ operator = 'reinhard' }

-- Every shot passes defocus_angle, the pinhole one included. The lens is
-- global renderer state and a render that omits it keeps whatever the last
-- one set, so a pinhole shot placed after a lens shot would silently blur.
-- An angle of 0 is a lens of radius 0: the pinhole camera.
--
-- A lens shot traces samples x lens_samples rays per pixel and a pinhole shot
-- only samples, so the pinhole shot asks for 16 x 16 = 256 up front. Equal
-- budgets mean equal noise, and blur is then the only thing that differs.
local shots = {
  { output = 'renders/thin_lens_pinhole.png', spp = 256, angle = 0.0, focus = along_view(mid_pos)  },
  { output = 'renders/thin_lens_near.png',    spp = 16,  angle = 4.0, focus = along_view(near_pos) },
  { output = 'renders/thin_lens_far.png',     spp = 16,  angle = 4.0, focus = along_view(far_pos)  },
}

for _, shot in ipairs(shots) do
  gr.render{
    root = scene, output = shot.output,
    width = 400, height = 225,
    eye = eye, view = view, up = {0, 1, 0}, fov = 30,
    ambient = {0.5, 0.7, 1.0},
    lights = { black_light },

    samples = shot.spp,        -- AA samples; a lens multiplies these
    defocus_angle = shot.angle,
    focus_dist    = shot.focus,
    lens_samples  = 16,
  }
end
