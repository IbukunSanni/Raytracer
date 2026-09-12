-- Three spheres: a diffuse ground and centre, flanked by two mirrors --
-- adapted from Ray Tracing in One Weekend's "metal" scene. Unlike
-- tests/scenes/mirror_furnace.lua (a uniform environment, checking energy
-- conservation), this one gives the mirrors actual geometry to reflect --
-- the ground, the centre sphere, each other -- so it is a visual check
-- rather than a numeric one. No pass/fail signal, so it lives here rather
-- than in tests/scenes/, and is not wired into run_tests.sh.
--
--     ./build/raytracer assets/scenes/mirror_spheres.lua

local material_ground = gr.lambertian{ kd = {0.8, 0.8, 0.0} }
local material_center = gr.lambertian{ kd = {0.1, 0.2, 0.5} }
local material_left   = gr.mirror{ albedo = {0.8, 0.8, 0.8} }
local material_right  = gr.mirror{ albedo = {0.8, 0.6, 0.2} }

local scene = gr.node('root')

local ground = gr.nh_sphere('ground', {0.0, -100.5, -1.0}, 100.0)
ground:set_material(material_ground)
scene:add_child(ground)

local center = gr.nh_sphere('center', {0.0, 0.0, -1.2}, 0.5)
center:set_material(material_center)
scene:add_child(center)

local left = gr.nh_sphere('left', {-1.0, 0.0, -1.0}, 0.5)
left:set_material(material_left)
scene:add_child(left)

local right = gr.nh_sphere('right', {1.0, 0.0, -1.0}, 0.5)
right:set_material(material_right)
scene:add_child(right)

-- No point lights in the book's version of this scene -- pure environment
-- light -- but gr.render wants a non-empty tuple, so pass one at zero
-- intensity rather than an empty table.
local black_light = gr.light({0, 0, -100}, {0, 0, 0}, {1, 0, 0})

gr.set_samples(128)
gr.set_background('')        -- no texture: ambient below is a flat sky
gr.set_tonemap{ operator = 'reinhard' }

gr.render{
  root = scene, output = 'renders/mirror_spheres.png',
  width = 400, height = 225,   -- 16:9, matching the book's framing

  -- The book uses vfov 90 here deliberately: close, wide-angle spheres.
  eye = {0, 0, 0}, view = {0, 0, -1}, up = {0, 1, 0}, fov = 90,

  ambient = {0.5, 0.7, 1.0},  -- flat sky blue; this renderer has no
                              -- direction-based gradient, unlike the book's
  lights = { black_light },
}
