-- The Cornell box with its two blocks at 300 x 300, for timing: the full
-- scene, loaded as it is, with only the image size and output path changed.
-- Samples per pixel come from the scene or RT_SPP.
--     ./build/raytracer assets/scenes/cornell_blocks_small.lua
--
-- It runs on every build of the renderer. A build whose nodes have no
-- set_emission makes a lamp from a diffuse_light material instead; the two
-- give the same lamp, so every build renders the same room.

local nodes = getmetatable(gr.node('probe'))
if not nodes.set_emission then
  function nodes.set_emission(node, light)
    node:set_material(gr.diffuse_light{ radiance = light.radiance })
  end
end

local render = gr.render
function gr.render(args)
  args.width, args.height = 300, 300
  args.output = 'renders/cornell_blocks/cornell_blocks_small.png'
  render(args)
end

dofile('assets/scenes/cornell_blocks.lua')
