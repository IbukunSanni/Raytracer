-- Simple Scene:
-- An extremely simple scene that will render right out of the box
-- with the provided skeleton code.  It doesn't rely on hierarchical
-- transformations.

-- Create the top level root node named 'root'.
-- root create at (0,0,0) no need to translate
rootNode = gr.node('root')
------------------------ Materials declarartion--------------------
-- TODO: make your maore materials
red = gr.material({1.0, 0.0, 0.0}, {0.1, 0.1, 0.1}, 10)
blue = gr.material({0.0, 0.0, 1.0}, {0.1, 0.1, 0.1}, 10)
green = gr.material({0.0, 1.0, 0.0}, {0.1, 0.1, 0.1}, 10)
white = gr.material({1.0, 1.0, 1.0}, {0.1, 0.1, 0.1}, 10)

brown = gr.material({0.8,0.5, 0.25}, {0.1, 0.1, 0.1}, 10)
darkbrown = gr.material({0.21, 0.14, 0.02}, {0.1, 0.1, 0.1}, 10)
slightDarkbrown = gr.material({0.70, 0.47, 0.23}, {0.1, 0.1, 0.1}, 10)
black = gr.material({0.0,0.0, 0.0}, {0.1, 0.1, 0.1}, 10)
white = gr.material({1.0,1.0, 1.0}, {0.5, 0.5, 0.5}, 10)
yellow = gr.material({1.0, 1.0, 0.0}, {0.1, 0.1, 0.1}, 10)

--- joint and geometry details
-- follow order
-- create node: joint or geometry 
-- scale
-- rotate
-- translate
-- set material
-- add parent
------------------------ Cube mesh details--------------------
cubeMesh = gr.mesh('cube', 'cube1')
cubeMesh:scale(1.0, 1.0, 1.0) -- unit height cube
cubeMesh:translate(0.0, 0.0, 0.0)
cubeMesh:rotate('y', 0.0)
cubeMesh:set_material(red)
------------------------ Spehere mesh details--------------------
spehereMesh = gr.mesh('sphere','spehere1')
spehereMesh:scale(0.5, 0.5, 0.5) -- unit diamater spehere
spehereMesh:translate(0.0, 0.0, 0.0)
spehereMesh:rotate('y', 0.0)
spehereMesh:set_material(green)

----------------- torso -----------------
torsoMesh = gr.mesh('cube', 'torsoMesh')
torsoMesh:scale(1.0, 2.0, 1.0) 
torsoMesh:translate(0.0, 0.0, 0.0)
torsoMesh:set_material(red)

rootNode:add_child(torsoMesh)


----------------- leftUpperArm -----------------
-- leftUpperArmJoint
leftUpperArmJoint = gr.joint('leftUpperArmJoint', {-45, 0, 45}, {-45, 0, 45});
-- TODO: understand joint translation
leftUpperArmJoint:translate(0.7,0.8,0.0)

rootNode:add_child(leftUpperArmJoint)

-- leftUpperArm01Mesh
leftUpperArm01Mesh = gr.mesh('sphere','leftUpperArm01Mesh')
leftUpperArm01Mesh:scale(0.3, 0.3, 0.3)
leftUpperArm01Mesh:set_material(green)

leftUpperArmJoint: add_child(leftUpperArm01Mesh) -- TODO: correct to joint

-- leftUpperArm02Mesh
leftUpperArm02Mesh = gr.mesh('cube','leftUpperArm02Mesh')
leftUpperArm02Mesh:scale(0.4, 1.2, 0.4)
leftUpperArm02Mesh:rotate('z', 90.0)
leftUpperArm02Mesh:translate(0.7, 0.0, 0.0)
leftUpperArm02Mesh:set_material(red)

leftUpperArmJoint: add_child(leftUpperArm02Mesh) -- TODO: correct to joint

-- ----------------- leftLowerArm -----------------
-- leftLowerArmJoint
leftLowerArmJoint = gr.joint('leftLowerArmJoint', {-45, 0, 45}, {-45, 0, 45});
-- TODO: understand joint translation
leftLowerArmJoint:translate(1.3,0.0,0.0)

leftUpperArmJoint:add_child(leftLowerArmJoint)

-- leftLowerArm01Mesh
leftLowerArm01Mesh = gr.mesh('sphere','leftLowerArm01Mesh')
leftLowerArm01Mesh:scale(0.3, 0.3, 0.3)
leftLowerArm01Mesh:set_material(green)

leftLowerArmJoint: add_child(leftLowerArm01Mesh)

-- leftLowerArm02Mesh
leftLowerArm02Mesh = gr.mesh('cube','leftLowerArm02Mesh')
leftLowerArm02Mesh:scale(0.4, 1.2, 0.4)
leftLowerArm02Mesh:rotate('z', 90.0)
leftLowerArm02Mesh:translate(0.7, 0.0, 0.0)
leftLowerArm02Mesh:set_material(red)

leftLowerArmJoint: add_child(leftLowerArm02Mesh)

-- ----------------- LeftHand -----------------
-- Joint
leftHandJoint = gr.joint('leftHandJoint', {-45, 0, 45}, {-45, 0, 45});
-- TODO: understand joint translation
leftHandJoint:translate(1.5,0.0,0.0)

leftLowerArmJoint:add_child(leftHandJoint)

-- LeftHand01Mesh
leftHand01Mesh = gr.mesh('sphere','leftHand01Mesh')
leftHand01Mesh:scale(0.3, 0.3, 0.3)
leftHand01Mesh:set_material(green)

leftHandJoint: add_child(leftHand01Mesh)

-- LeftHand02Mesh
leftHand02Mesh = gr.mesh('cube','leftHand02Mesh')
leftHand02Mesh:scale(0.3, 0.5, 0.3)
leftHand02Mesh:rotate('z', 90.0)
leftHand02Mesh:translate(0.3, 0.0, 0.0)
leftHand02Mesh:set_material(red)

leftHandJoint: add_child(leftHand02Mesh)

-- Return the root with all of it's childern.  The SceneNode A3::m_rootNode will be set
-- equal to the return value from this Lua script.
return rootNode
