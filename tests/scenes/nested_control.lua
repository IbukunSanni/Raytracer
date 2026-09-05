-- Control for the nested-transform regression test.
-- The child `s` is parented to a plain gr.node. This path was always correct.
-- Rendered output must match nested_under_geometry.lua exactly.
mat1 = gr.material({0.7, 1.0, 0.7}, {0.5, 0.7, 0.5}, 25)
mat2 = gr.material({1.0, 0.6, 0.1}, {0.5, 0.7, 0.5}, 25)

scene  = gr.node('scene')
parent = gr.node('parent')
parent:translate(150, 0, -400)
scene:add_child(parent)

p = gr.nh_sphere('p', {0, 0, 0}, 100)   -- sibling of s, not its ancestor
p:set_material(mat1)
parent:add_child(p)

s = gr.nh_sphere('s', {0, 150, 0}, 50)
s:set_material(mat2)
parent:add_child(s)

l = gr.light({-100, 150, 400}, {0.9, 0.9, 0.9}, {1, 0, 0})
gr.render(scene, 'tests/out/nested_control.png', 200, 200,
          {0, 0, 800}, {0, 0, -800}, {0, 1, 0}, 50, {0.3, 0.3, 0.3}, {l})
