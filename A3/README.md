# A3 read me
    
## Compilation
The program is located in the downloads folder. As a result, you have to **cd** into **~/Downloads/cs488/A3** .
To run the program with puppet perform following operations:
    $ premake4 gmake
    $ make
    $ ./A3 Assets/puppet.lua

## Assumptions
- Reset All (A) and Reset Joints (S) clear all selected nodes
- Joints have multiple meshes. All meshes parented to a joint need to be deselected to deselect joint node
- Default pose is the T-pose, typically default pose for model files 

## Manual
- No changes made to data structures
- Undo/Redo details appear in the GUI window

structure of tree

root ---- torsoMesh
  |
  ------- neckJoint-- neckMesh
  |              |
  |               --- headMesh
  |
  |------- shoulders-- leftUpperArmJoint -- leftUpperArmMesh01
  |                |                   |
  |                |                   ---- leftUpperArmMesh02
  |                |                   |
  |                |                   ---- leftLowerArmJoint -- leftUpperArmMesh01
  |                |                                       |
  |                |                                       ----- leftUpperArmMesh02
  |                |                                       |
  |                |                                       ----- leftHandJoint -- leftHandMesh01
  |                |                                                        |
  |                |                                                        ----- leftHandMesh02
  |                -- rightUpperArmJoint -- rightUpperArmMesh01
  |                                    |
  |                                    ---- rightUpperArmMesh02
  |                                    |
  |                                    ---- rightLowerArmJoint -- rightUpperArmMesh01
  |                                                        |
  |                                                        ----- rightUpperArmMesh02
  |                                                        |
  |                                                        ----- rightHandJoint -- rightHandMesh01
  |                                                                         |
  |                                                                         ----- rightHandMesh02
  ------ waist-- leftUpperLegJoint -- leftUpperLegMesh01
                  |                   |
                  |                   ---- leftUpperLegMesh02
                  |                   |
                  |                   ---- leftLowerLegJoint -- leftUpperLegMesh01
                  |                                       |
                  |                                       ----- leftUpperLegMesh02
                  |                                       |
                  |                                       ----- leftFootJoint -- leftFootMesh01
                  |                                                        |
                  |                                                        ----- leftFootMesh02
                  -- rightUpperLegJoint -- rightUpperLegMesh01
                                      |
                                      ---- rightUpperLegMesh02
                                      |
                                      ---- rightLowerLegJoint -- rightUpperLegMesh01
                                                          |
                                                          ----- rightUpperLegMesh02
                                                          |
                                                          ----- rightFootJoint -- rightFootMesh01
                                                                           |
                                                                           ----- rightFootMesh02