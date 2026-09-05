-- Regression test: a child parented to a GEOMETRY node.
--
-- GeometryNode::isHit used to transform the ray into local space and then
-- delegate to SceneNode::isHit, which applied the same inverse a second time
-- (and both restored it on the way out). Anything parented to a GeometryNode
-- was therefore displaced by roughly twice its parent's transform.
--
-- This scene is geometrically identical to nested_control.lua, so the two
-- renders must be byte-identical. Before the fix they differed by 1265 bytes.
mat1 = gr.material({0.7, 1.0, 0.7}, {0.5, 0.7, 0.5}, 25)
mat2 = gr.material({1.0, 0.6, 0.1}, {0.5, 0.7, 0.5}, 25)

scene = gr.node('scene')

p = gr.nh_sphere('p', {0, 0, 0}, 100)   -- a GeometryNode, carrying the transform
p:translate(150, 0, -400)
p:set_material(mat1)
scene:add_child(p)

s = gr.nh_sphere('s', {0, 150, 0}, 50)  -- child of a GeometryNode
s:set_material(mat2)
p:add_child(s)

l = gr.light({-100, 150, 400}, {0.9, 0.9, 0.9}, {1, 0, 0})
gr.render(scene, 'tests/out/nested_under_geometry.png', 200, 200,
          {0, 0, 800}, {0, 0, -800}, {0, 1, 0}, 50, {0.3, 0.3, 0.3}, {l})
