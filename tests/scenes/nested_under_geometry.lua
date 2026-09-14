-- A child parented to a GEOMETRY node.
--
-- A geometry node carries both geometry and a transform, so intersection can
-- apply its inverse on the way down and again when it delegates to the plain
-- node path. A child underneath is then displaced by roughly twice its
-- parent transform.
--
-- Geometrically identical to the control scene beside it, so the two renders
-- must come out byte for byte the same.
mat1 = gr.material({0.7, 1.0, 0.7}, {0.5, 0.7, 0.5}, 25)
mat2 = gr.material({1.0, 0.6, 0.1}, {0.5, 0.7, 0.5}, 25)

scene = gr.node('scene')

p = gr.nh_sphere('p', {0, 0, 0}, 100) -- a GeometryNode, carrying the transform
p:translate(150, 0, -400)
p:set_material(mat1)
scene:add_child(p)

s = gr.nh_sphere('s', {0, 150, 0}, 50) -- child of a GeometryNode
s:set_material(mat2)
p:add_child(s)

l = gr.light({-100, 150, 400}, {0.9, 0.9, 0.9}, {1, 0, 0})
gr.render {
    root = scene,
    output = 'tests/out/nested_under_geometry.png',
    width = 200,
    height = 200,
    eye = {0, 0, 800},
    view = {0, 0, -800},
    up = {0, 1, 0},
    fov = 50,
    ambient = {0.3, 0.3, 0.3},
    lights = {l}
}
