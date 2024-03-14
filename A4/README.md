# A4 read me
    
## Compilation
The program is located in the downloads folder. As a result, you have to **cd** into **~/Downloads/cs488/A4** .
To run the program with sample.lua script eg simple.lua perform following operations:
    $ premake4 gmake
    $ make
    $ ./A4 Assets/sample.lua
output is sample.png in **~/Downloads/cs488/A4** 

## Note
- OBJ files need to be prefixed with **Assets/** in order to run lua scripts
- **screenshot.png** uses anitialiasing/supersampling
- **sample.png** samples per pixel reduced compared to **screenshot.png**

## Manual
- Anti-Aliasing implemented.
- To run anti aliasing, **ANTI_ALIASING** is a macro defined in **~/Downloads/cs488/A4/A4.cpp**, set to 1 for anti-aliasing and 0 otherwise

- Bounding spheres implemented.
- To run bounding spheres, **RENDER_BOUNDING_VOLUMES** is a macro defined in **~/Downloads/cs488/A4/Mesh.hpp**, set to 1 for bounding sphere and 0 otherwise