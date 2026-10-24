-- A black sphere in a white environment, rendered still and then moving.
--
-- No tone map and no sRGB, and the sphere absorbs everything, so a pixel is
-- 255 times the fraction of its samples that missed: 255 minus its coverage.
-- Motion blur spreads the silhouette over the path the sphere took, but the
-- sphere covers the same area at every instant of the shutter, so the
-- frame's total coverage -- its mean -- must not change. Both renders keep
-- the sphere fully in frame for that to hold.
--
-- MOTION_TAG names the output files. Two tests render this scene, and
-- ctest runs them in parallel: on one shared name, one test can read a PNG
-- while the other is still writing it.
local tag = os.getenv('MOTION_TAG') or 'coverage'

local function render(name, start_centre, end_centre)
  local scene = gr.node('root')
  local ball = end_centre
      and gr.nh_sphere('ball', start_centre, 60, end_centre)
      or gr.nh_sphere('ball', start_centre, 60)
  ball:set_material(gr.lambertian{ kd = {0, 0, 0} })
  scene:add_child(ball)

  gr.render{
    root = scene, output = 'tests/out/motion_' .. tag .. '_' .. name .. '.png',
    width = 128, height = 128,
    eye = {0, 0, 0}, view = {0, 0, -1}, up = {0, 1, 0}, fov = 30,
    ambient = {1, 1, 1},
    lights = {},
    samples = 64,
  }
end

gr.set_background('')
gr.set_tonemap{ operator = 'none', srgb = false }

render('still', {0, 0, -500})
render('moving', {-30, 0, -500}, {30, 0, -500})
