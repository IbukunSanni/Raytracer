-- A simple test scene featuring some spherical cows grazing
-- around Stonehenge.  "Assume that cows are spheres..."

-- Create materials
stone = gr.material({0.8, 0.7, 0.7}, {0.5, 0.7, 0.5}, 0)
grass = gr.material({54/255, 120/255, 28/255}, {0.0, 0.0, 0.0}, 0)
wood = gr.material({66/255, 34/255, 10/255}, {0.0, 0.0, 0.0}, 0)

leafMat = gr.material({0.1, 0.8, 0.1}, {0.3, 0.3, 0.3}, 25)
leafMat1 = gr.material({104/255, 120/255, 28/255}, {0.5, 0.7, 0.5}, 25)


hide = gr.material({0.84, 0.6, 0.53}, {0.3, 0.3, 0.3}, 20)
mat1 = gr.material({0.7, 1.0, 0.7}, {0.5, 0.7, 0.5}, 25)
mat2 = gr.material({0.5, 0.5, 0.5}, {0.5, 0.7, 0.5}, 25)
mat3 = gr.material({1.0, 0.6, 0.1}, {0.5, 0.7, 0.5}, 25)

-- ##############################################
-- the TREE
-- ##############################################
tree = gr.node('tree')
tree:translate(0, 0, -10)

trunk = gr.nh_box('trunk', {0, 0, 0}, 1)
trunk:scale(1, 5, 1)
trunk:translate(0.5,0.0,0.5)
tree:add_child(trunk)
trunk:set_material(wood)


leaves1 = gr.mesh( 'leaves1', 'Assets/buckyball.obj' )
leaves1:scale(.8, 0.7, 0.9)
tree:add_child(leaves1)
leaves1:translate(0.5, 3, 0.0)
leaves1:set_material(leafMat)

leaves2 = gr.mesh( 'leaves2', 'Assets/buckyball.obj' )
leaves2:scale(0.7, 0.8, 0.7)
tree:add_child(leaves2)
leaves2:translate(0, 4, 0.5)
leaves2:set_material(leafMat1)

leaves3 = gr.mesh( 'leaves3', 'Assets/buckyball.obj' )
leaves3:scale(1.0, 0.5, 1.0)
tree:add_child(leaves3)
leaves3:translate(1, 5, 1)
leaves3:set_material(leafMat1)


-- ##############################################
-- the scene
-- ##############################################

scene = gr.node('scene')
scene:rotate('X', 10)

-- add trees
for i = 1, 5 do
    a_tree = gr.node('tree' .. tostring(i))
    a_tree:rotate('Y', (i-1) * 72)
    scene:add_child(a_tree)
    a_tree:add_child(tree)
 end


--- CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC



-- #############################################
-- Let's assume that cows are spheres
-- #############################################

cow = gr.node('the_cow')

for _, spec in pairs({
			{'body', {0, 0, 0}, 1.0},
			{'head', {.9, .3, 0}, 0.6},
			{'tail', {-.94, .34, 0}, 0.2},
			{'lfleg', {.7, -.7, -.7}, 0.3},
			{'lrleg', {-.7, -.7, -.7}, 0.3},
			{'rfleg', {.7, -.7, .7}, 0.3},
			{'rrleg', {-.7, -.7, .7}, 0.3}
		     }) do
   part = gr.nh_sphere(table.unpack(spec))
   part:set_material(hide)
   cow:add_child(part)
end



-- the floor

plane = gr.mesh( 'plane', 'Assets/plane.obj' )
scene:add_child(plane)
plane:set_material(grass)
plane:scale(30, 30, 30)

-- Construct a central altar in the shape of a buckyball.  The
-- buckyball at the centre of the real Stonehenge was destroyed
-- in the great fire of 733 AD.

-- buckyball = gr.mesh( 'buckyball', 'Assets/buckyball.obj' )
buckyball = gr.nh_sphere('buckyball', {-100, 25, -300}, 20)
scene:add_child(buckyball)
buckyball:set_material(mat3)
buckyball:scale(1.5, 1.5, 1.5)
buckyball:translate(0.0,0.2,0.0)

-- Use the instanced cow model to place some actual cows in the scene.
-- For convenience, do this in a loop.

cow_number = 1

for _, pt in pairs({
		      {{1,1.3,14}, 20},
		      {{5,1.3,-11}, 200}}) do
   cow_instance = gr.node('cow' .. tostring(cow_number))
   scene:add_child(cow_instance)
   cow_instance:add_child(cow)
   cow_instance:scale(1.4, 1.4, 1.4)
   cow_instance:rotate('Y', pt[2])
   cow_instance:translate(table.unpack(pt[1]))
   
   cow_number = cow_number + 1
end

-- Place a ring of arches.

-- for i = 1, 6 do
--    an_arc = gr.node('arc' .. tostring(i))
--    an_arc:rotate('Y', (i-1) * 60)
--    scene:add_child(an_arc)
--    an_arc:add_child(arc)
-- end

-- white_light = gr.light({-100.0, 150.0, 400.0}, {0.9, 0.9, 0.9}, {1, 0, 0})
magenta_light = gr.light({0.0, 10.0, 10.0}, {0.7, 0.0, 0.7}, {1, 0, 0})

gr.render(scene,
	  'sample.png', 500, 500,
	  {0, 2, 25}, {0, 0, -1}, {0, 1, 0}, 50,
	  {0.4, 0.4, 0.4}, {gr.light({200, 202, 430}, {0.8, 0.8, 0.8}, {1, 0, 0})})
