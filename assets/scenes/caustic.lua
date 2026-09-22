-- A caustic: sunlight focused through a glass ball onto a diffuse floor.
--     ./build/raytracer assets/scenes/caustic.lua
--
-- The scene is built around what this renderer can actually carry a caustic
-- on. Point lights cannot: the shadow ray from floor to light treats the
-- dielectric as an opaque occluder, so a point light casts a shadow through
-- glass and never a bright spot. A uniform environment cannot either, for the
-- furnace's reason -- refraction redistributes uniform radiance into uniform
-- radiance, so the focus is invisible. What is left is a small, bright region
-- in a lat-long environment map, found by BSDF sampling alone.

local glass  = gr.dielectric{ ior = 1.5 }
local floor  = gr.lambertian{ kd = {0.75, 0.75, 0.72} }

local scene = gr.node('root')

-- A big sphere standing in for a ground plane; its top sits at y = -0.5.
local ground = gr.nh_sphere('ground', {0.0, -100.5, 0.0}, 100.0)
ground:set_material(floor)
scene:add_child(ground)

-- A ball lens of radius R and index n focuses at nR / (2(n-1)) from its
-- centre -- 0.75 here, which puts the focus at y = -0.45, just above the
-- floor. That is what makes the spot small instead of a smear.
local ball = gr.nh_sphere('ball', {0.0, 0.3, 0.0}, 0.5)
ball:set_material(glass)
scene:add_child(ball)

-- gr.render wants a non-empty lights tuple; this scene is lit entirely by
-- the environment, so pass one at zero intensity.
local black_light = gr.light({0, 0, -100}, {0, 0, 0}, {1, 0, 0})

gr.set_background('assets/textures/sun_sky.png')
gr.set_tonemap{ operator = 'reinhard', exposure = 9.0 }

gr.render{
  root = scene, output = 'renders/caustic.png',
  width = 400, height = 300,
  eye = {0, 1.1, 3.2}, view = {0, -1.6, -3.2}, up = {0, 1, 0}, fov = 45,
  ambient = {0, 0, 0},
  lights = { black_light },
  samples = 512,
}
