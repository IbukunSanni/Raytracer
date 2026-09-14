-- Five spheres: a diffuse ground and centre, a hollow glass sphere on the
-- left and a rough metal one on the right -- Ray Tracing in One Weekend's
-- "positioning the camera" scene, seen from off to one side.
--     ./build/raytracer assets/scenes/glass_spheres_camera.lua
--
-- Renders the same scene twice, at a narrow and a wide vertical field of
-- view, because the point of the scene is the framing rather than the
-- geometry. The eye is identical in both; only the fov changes, which is
-- what separates a telephoto crop from a wide-angle one. Two near-identical
-- files used to carry these; the scene graph is built once and rendered
-- twice instead.

local material_ground = gr.lambertian{ kd = {0.8, 0.8, 0.0} }
local material_center = gr.lambertian{ kd = {0.1, 0.2, 0.5} }
local material_left   = gr.dielectric{ ior = 1.50 }

-- An index below 1 is glass seen from the inside: this smaller sphere sits
-- inside the one above and hollows it out into a bubble.
local material_bubble = gr.dielectric{ ior = 1.00 / 1.50 }

-- Fuzz 1 is the widest lobe this material has, so the reflection is a
-- smear rather than a mirror image.
local material_right  = gr.metal{ albedo = {0.8, 0.6, 0.2}, fuzz = 1.0 }

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

local bubble = gr.nh_sphere('bubble', {-1.0, 0.0, -1.0}, 0.4)
bubble:set_material(material_bubble)
scene:add_child(bubble)

local right = gr.nh_sphere('right', {1.0, 0.0, -1.0}, 0.5)
right:set_material(material_right)
scene:add_child(right)

-- No point lights in the book's version of this scene -- pure environment
-- light -- but gr.render wants a non-empty tuple, so pass one at zero
-- intensity rather than an empty table.
local black_light = gr.light({0, 0, -100}, {0, 0, 0}, {1, 0, 0})

gr.set_background('')        -- no texture: ambient below is a flat sky
gr.set_tonemap{ operator = 'reinhard' }

-- The two framings. 20 degrees is the book's telephoto view; 90 is its
-- wide-angle one, from the same eye.
local framings = {
  { fov = 20, output = 'renders/glass_spheres_camera_near.png' },
  { fov = 90, output = 'renders/glass_spheres_camera_far.png'  },
}

for _, shot in ipairs(framings) do
  gr.render{
    root = scene, output = shot.output,
    width = 400, height = 225,   -- 16:9, matching the book's framing

    -- The book gives the camera a lookfrom and a lookat point; this renderer
    -- takes a view DIRECTION, so view is lookat - eye rather than lookat.
    -- Passing the lookat point straight through aims the camera down -z from
    -- an eye that is no longer on that axis, which is a different picture.
    eye = {-2, 2, 1},
    view = {2, -2, -2},  -- lookat {0, 0, -1} minus eye
    up = {0, 1, 0},
    fov = shot.fov,      -- vfov, as the book means it

    ambient = {0.5, 0.7, 1.0},  -- flat sky blue; this renderer has no
                                -- direction-based gradient, unlike the book's
    lights = { black_light },

    samples = 100,
  }
end
