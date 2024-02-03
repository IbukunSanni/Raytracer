// Termm--Fall 2020

#include "A1.hpp"
#include "cs488-framework/GlErrorCheck.hpp"

#include <iostream>

#include <sys/types.h>
#include <unistd.h>

#include <imgui/imgui.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

using namespace glm;
using namespace std;

static const size_t DIM = 16; // Dimensions for the maze
static const GLfloat MAX_RGB = 255.0f;// MAXIMUM color of RGB #FF in decimal
static const GLfloat PI = 3.14159265f; //  PI for sphere calculations
static const GLfloat MAX_BLOCK_HEIGHT = 4.0f;// Max block height
static const GLfloat MIN_BLOCK_HEIGHT = 0.0f;// Min block height
static const GLfloat DELTA_BLOCK_UNIT = 1.0f;// Change in block height
static const GLfloat MAX_SCALE = 2.5f; // Max scaling
static const GLfloat MIN_SCALE = 0.3f; // Min scaling
static const glm::vec3 B_RGB_DEFAULT = vec3(100/MAX_RGB,50/MAX_RGB,20/MAX_RGB);// Default block color
static const glm::vec3 A_RGB_DEFAULT = vec3(20/MAX_RGB,200/MAX_RGB,10/MAX_RGB);// Default avatar color
static const glm::vec3 F_RGB_DEFAULT = vec3(44/MAX_RGB,120/MAX_RGB,220/MAX_RGB);// Default floor color
static const GLfloat REVOLUTION = 2048.0f; //a full revolution

//----------------------------------------------------------------------------------------
// Constructor
A1::A1()
	: current_col( 0 ),m(DIM)
{
	colour[0] = B_RGB_DEFAULT.r;
	colour[1] = B_RGB_DEFAULT.g;
	colour[2] = B_RGB_DEFAULT.b;
}

//----------------------------------------------------------------------------------------
// Destructor
A1::~A1()
{}

//----------------------------------------------------------------------------------------
/*
 * Called once, at program start.
 */
void A1::init(){
	// Initialize random number generator
	int rseed=getpid();
	srandom(rseed);
	// Print random number seed in case we want to rerun with
	// same random numbers
	cout << "Random number seed = " << rseed << endl;
	// Reset the World
	resetWorld();

	// TODO: figure out maze
	// Initialize maze
	// DELETE FROM HERE...
	m.digMaze();
	m.printMaze();
	// ...TO HERE

	placeAvatar();

	// Set the background colour.
	glClearColor( 71/MAX_RGB, 35/MAX_RGB, 17/MAX_RGB, 1.0 );

	// Build the shader
	m_shader.generateProgramObject();
	m_shader.attachVertexShader(
		getAssetFilePath( "VertexShader.vs" ).c_str() );
	m_shader.attachFragmentShader(
		getAssetFilePath( "FragmentShader.fs" ).c_str() );
	m_shader.link();

	// Set up the uniforms
	P_uni = m_shader.getUniformLocation( "P" );
	V_uni = m_shader.getUniformLocation( "V" );
	M_uni = m_shader.getUniformLocation( "M" );
	col_uni = m_shader.getUniformLocation( "colour" );

	initGrid();
	initBlock();
	initAvatar();
	initFloor();

	// Set up initial view and projection matrices (need to do this here,
	// since it depends on the GLFW window being set up correctly).
	view = glm::lookAt(
		glm::vec3( 0.0f, 2.*float(DIM)*2.0*M_SQRT1_2, float(DIM)*2.0*M_SQRT1_2 ),
		glm::vec3( 0.0f, 0.0f, 0.0f ),
		glm::vec3( 0.0f, 1.0f, 0.0f ) );

	proj = glm::perspective(
		glm::radians( 30.0f ),
		float( m_framebufferWidth ) / float( m_framebufferHeight ),
		1.0f, 1000.0f );
}

void A1::resetWorld(){
	b_height = 1.0f;
	a_offset_x = 0.0f;
	a_offset_z = 0.0f;
	b_color = B_RGB_DEFAULT;
	a_color = A_RGB_DEFAULT;
	f_color = F_RGB_DEFAULT;
	scale = 1.0f;
	rotate_change = 0.0f;
	rotate_persistence = 0.0f;
}

void A1::placeAvatar(){
	// get starting position
	// Looping through dimensions of maze not grid
	for (int i = 0; i < DIM ; i++){
		for (int j = 0; j < DIM ; j++ ){
				if (((i) % (DIM - 1) == 0 || (j) % (DIM - 1) == 0 )
						&& (m.getValue(j,i) == 0))
				{
					// Place avatar at entrance
					a_offset_x = i+1;
					a_offset_z = j+1;
				}
		}
	}

}

// Initialize the grid
void A1::initGrid(){
	size_t sz = 3 * 2 * 2 * (DIM+3);

	float *verts = new float[ sz ];
	size_t ct = 0;
	for( int idx = 0; idx < DIM+3; ++idx ) {
		verts[ ct ] = -1;
		verts[ ct+1 ] = 0;
		verts[ ct+2 ] = idx-1;
		verts[ ct+3 ] = DIM+1;
		verts[ ct+4 ] = 0;
		verts[ ct+5 ] = idx-1;
		ct += 6;

		verts[ ct ] = idx-1;
		verts[ ct+1 ] = 0;
		verts[ ct+2 ] = -1;
		verts[ ct+3 ] = idx-1;
		verts[ ct+4 ] = 0;
		verts[ ct+5 ] = DIM+1;
		ct += 6;
	}


	// Create the vertex array
	glGenVertexArrays( 1, &m_grid_vao );
	// Binds the vertex array
	glBindVertexArray( m_grid_vao );

	// Create the cube vertex buffer
	glGenBuffers( 1, &m_grid_vbo );
	glBindBuffer( GL_ARRAY_BUFFER, m_grid_vbo );
	glBufferData( GL_ARRAY_BUFFER, sz*sizeof(float),
		verts, GL_STATIC_DRAW );

	// Specify the means of extracting the position values properly.
	GLint posAttrib = m_shader.getAttribLocation( "position" );
	glEnableVertexAttribArray( posAttrib );
	glVertexAttribPointer( posAttrib, 3, GL_FLOAT, GL_FALSE, 0, nullptr );

	// Reset state to prevent rogue code from messing with *my*
	// stuff!
	glBindVertexArray( 0 );
	glBindBuffer( GL_ARRAY_BUFFER, 0 );
	glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, 0 );

	// OpenGL has the buffer now, there's no need for us to keep a copy.
	delete [] verts;

	CHECK_GL_ERRORS;
}

// Initialize a block
void A1::initBlock(){
	// Initialize local orgin 2D for (x,z);
	GLfloat l_origin[] = { -0.5f, -0.5f};

	// Assign values to the respective vertices
	// Each position is in respect to the local orgin
	// Default view right +ve x, down +ve z, towards
	// And +ve y-axis upwards
	GLfloat verts[] = {
		l_origin[0] -0.5f,  b_height,l_origin[1] -0.5f, //top face upper left
		l_origin[0] +0.5f,  b_height,l_origin[1] -0.5f, //top face upper right
		l_origin[0] +0.5f,  b_height,l_origin[1] +0.5f, //top face lower left
		l_origin[0] -0.5f,  b_height,l_origin[1] +0.5f, //top face lower left

		l_origin[0] -0.5f,  0.0f,    l_origin[1] -0.5f, //bottom face upper left
		l_origin[0] +0.5f,  0.0f,    l_origin[1] -0.5f, //bottom face upper right
		l_origin[0] +0.5f,  0.0f,    l_origin[1] +0.5f, //bottom face lower left
		l_origin[0] -0.5f,  0.0f,    l_origin[1] +0.5f  //bottom face lower left
	};


	// Assign values to the respective indices
	// Indices tell GL how to draw the triangles
	// Assume top view from +ve z -axis towards user
	// And +ve y-axis upwards
	GLuint indices[] = {
		// top face
		0, 1, 2,
		0, 2, 3,
		// bottom face
		4, 5, 6,
		4, 6, 7,
		// right face
		2, 1, 5,
		2, 5, 6,
		// left face
		0, 3, 7,
		0, 7, 4,
		// front face
		3, 2, 6,
		3, 6, 7,
		// back face
		0, 1, 5,
		0, 5, 4
	};


	// Create the block vertex array
	glGenVertexArrays( 1, &m_block_vao );
	// Bind the the block vertex array
	glBindVertexArray( m_block_vao );

	// Create the block vertex buffer
	glGenBuffers( 1, &m_block_vbo );
	// Bind the block vertex buffer
	glBindBuffer( GL_ARRAY_BUFFER, m_block_vbo );
	glBufferData( GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW );

	// Create the block element buffer used for indices
	glGenBuffers( 1, &m_block_ebo );
	// Bind the block element buffer
	glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, m_block_ebo );
	glBufferData( GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW );

	// Specify the means of extracting the position values properly.
	GLint posAttrib = m_shader.getAttribLocation( "position" );
	glEnableVertexAttribArray( posAttrib );
	glVertexAttribPointer( posAttrib, 3, GL_FLOAT, GL_FALSE, 0, nullptr );

	// Reset state to prevent rogue code from messing with *my*
	// stuff!
	glBindVertexArray( 0 );
	glBindBuffer( GL_ARRAY_BUFFER, 0 );
	glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, 0 );

	// OpenGL has the buffer now, there's no need for us to keep a copy.
	// delete [] verts;
	// delete [] indices;

	CHECK_GL_ERRORS;
	}

// Initialize Avatar
void A1:: initAvatar(){
	// Size of vertices assuming 18 faces along longitude
	// assuming 36 faces along latitude
	size_t sz = 18 * 36 * 3 *2 *3;
	size_t delta = 10;
	float r = 0.5f;

	// Initialize local orgin 3D for (x,y,z);
	GLfloat l_origin[] = { -0.5f + a_offset_x,0.5f, -0.5f + a_offset_z};

	float *verts = new float[ sz ];
	size_t idx = 0;

	for (int i  = 0; i < 360; i+=delta){
		for(int j  = 90; j > -90; j -=delta){
			// traingle 1 x;y;z;
			verts[idx]     =  l_origin[0] + r * cos(j *PI/180.0f) * cos(i *PI / 180.0f);
			verts[idx + 1] =  l_origin[1] + r * sin(j *PI/180.0f);
			verts[idx + 2] =  l_origin[2] + r * cos(j *PI/180.0f) * sin(i *PI / 180.0f);

			verts[idx + 3] =  l_origin[0] + r * cos((j-delta) *PI/180.0f) * cos(i *PI / 180.0f);
			verts[idx + 4] =  l_origin[1] + r * sin((j-delta) *PI/180.0f);
			verts[idx + 5] =  l_origin[2] + r * cos((j-delta) *PI/180.0f) * sin(i *PI / 180.0f);

			verts[idx + 6] =  l_origin[0] + r * cos(j *PI/180.0f) * cos((i+delta) *PI / 180.0f);
			verts[idx + 7] =  l_origin[1] + r * sin(j *PI/180.0f);
			verts[idx + 8] =  l_origin[2] + r * cos(j *PI/180.0f) * sin((i+delta) *PI / 180.0f);

			// triangle 2 x;y;z;
			verts[idx+ 9]   = l_origin[0] + r * cos((j-delta) *PI/180.0f) * cos(i *PI / 180.0f);
			verts[idx + 10] = l_origin[1] + r * sin((j-delta) *PI/180.0f);
			verts[idx + 11] = l_origin[2] + r * cos((j-delta) *PI/180.0f) * sin(i *PI / 180.0f);

			verts[idx + 12] = l_origin[0] + r * cos((j-delta) *PI/180.0f) * cos((i+delta) *PI / 180.0f);
			verts[idx + 13] = l_origin[1] + r * sin((j-delta) *PI/180.0f);
			verts[idx + 14] = l_origin[2] + r * cos((j-delta) *PI/180.0f) * sin((i+delta) *PI / 180.0f);

			verts[idx + 15] = l_origin[0] + r * cos((j-delta) *PI/180.0f) * cos((i+delta) *PI / 180.0f);
			verts[idx + 16] = l_origin[1] + r * sin((j-delta) *PI/180.0f);
			verts[idx + 17] = l_origin[2] + r * cos((j-delta) *PI/180.0f) * sin((i+delta) *PI / 180.0f);

			idx += 18;

		}
	}


		// Create the vertex array
		glGenVertexArrays( 1, &m_avatar_vao );
		// Binds the vertex array
		glBindVertexArray( m_avatar_vao );

		// Create the cube vertex buffer
		glGenBuffers( 1, &m_avatar_vbo );
		glBindBuffer( GL_ARRAY_BUFFER, m_avatar_vbo );
		glBufferData( GL_ARRAY_BUFFER, sz*sizeof(float),
			verts, GL_STATIC_DRAW );

		// Specify the means of extracting the position values properly.
		GLint posAttrib = m_shader.getAttribLocation( "position" );
		glEnableVertexAttribArray( posAttrib );
		glVertexAttribPointer( posAttrib, 3, GL_FLOAT, GL_FALSE, 0, nullptr );

		// Reset state to prevent rogue code from messing with *my*
		// stuff!
		glBindVertexArray( 0 );
		glBindBuffer( GL_ARRAY_BUFFER, 0 );
		glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, 0 );

		// OpenGL has the buffer now, there's no need for us to keep a copy.
		delete [] verts;

		CHECK_GL_ERRORS;
}

void A1::initFloor(){

		// Initialize local orgin 2D for (x,z);
		GLfloat l_origin[] = { (DIM/2),2.0f + (DIM/2)};

		// Assign values to the respective vertices
		// Each position is in respect to the local orgin
		// Default view right +ve x, down +ve z, towards +ve y

		GLfloat verts[] = {
			l_origin[0] -(DIM/2),  0.0f, l_origin[0] -(DIM/2), //top face upper left
			l_origin[0] +(DIM/2),  0.0f, l_origin[0] -(DIM/2), //top face upper right
			l_origin[0] +(DIM/2),  0.0f, l_origin[0] +(DIM/2), //top face lower left
			l_origin[0] -(DIM/2),  0.0f, l_origin[0] +(DIM/2), //top face lower left
		};


		// Assign values to the respective indices
		// Indices tell GL how to draw the triangles
		// Assume top view from +ve z -axis towards user
		// And +ve y-axis upwards
		GLuint indices[] = {
			// top face
			0, 1, 2,
			0, 2, 3
		};


		// Create the floor vertex array
		glGenVertexArrays( 1, &m_floor_vao );
		// Bind the the floor vertex array
		glBindVertexArray( m_floor_vao );

		// Create the floor vertex buffer
		glGenBuffers( 1, &m_floor_vbo );
		// Bind the floor vertex buffer
		glBindBuffer( GL_ARRAY_BUFFER, m_floor_vbo );
		glBufferData( GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW );

		// Create the floor element buffer used for indices
		glGenBuffers( 1, &m_floor_ebo );
		// Bind the floor element buffer
		glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, m_floor_ebo );
		glBufferData( GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW );

		// Specify the means of extracting the position values properly.
		GLint posAttrib = m_shader.getAttribLocation( "position" );
		glEnableVertexAttribArray( posAttrib );
		glVertexAttribPointer( posAttrib, 3, GL_FLOAT, GL_FALSE, 0, nullptr );

		// Reset state to prevent rogue code from messing with *my*
		// stuff!
		glBindVertexArray( 0 );
		glBindBuffer( GL_ARRAY_BUFFER, 0 );
		glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, 0 );

		// OpenGL has the buffer now, there's no need for us to keep a copy.
		// delete [] verts;
		// delete [] indices;

		CHECK_GL_ERRORS;

}

//----------------------------------------------------------------------------------------
/*
 * Called once per frame, before guiLogic().
 */
void A1::appLogic()
{
	// Place per frame, application logic here ...

	rotate_change += rotate_persistence;
}

//----------------------------------------------------------------------------------------
/*
 * Called once per frame, after appLogic(), but before the draw() method.
 */
void A1::guiLogic()
{
	// We already know there's only going to be one window, so for
	// simplicity we'll store button states in static local variables.
	// If there was ever a possibility of having multiple instances of
	// A1 running simultaneously, this would break; you'd want to make
	// this into instance fields of A1.
	static bool showTestWindow(false);
	static bool showDebugWindow(true);

	ImGuiWindowFlags windowFlags(ImGuiWindowFlags_AlwaysAutoResize);
	float opacity(0.5f);

	ImGui::Begin("Debug Window", &showDebugWindow, ImVec2(100,100), opacity, windowFlags);
		// Create a Quit button and check if it was clicked
		if( ImGui::Button( "Quit Application" ) ) {
			glfwSetWindowShouldClose(m_window, GL_TRUE);
		}
		// Create a Reset button and check if it was clicked
		if( ImGui::Button( "Reset Application" ) ) {

			// reset Maze
			m.reset();
			resetWorld();
			initAvatar();
		}

		// Create a Dig Button and check if it was clicked
		if( ImGui::Button( "Dig" ) ) {

			//
			resetWorld();
			m.digMaze();
			placeAvatar();
			initAvatar();

		}

		// Eventually you'll create multiple colour widgets with
		// radio buttons.  If you use PushID/PopID to give them all
		// unique IDs, then ImGui will be able to keep them separate.
		// This is unnecessary with a single colour selector and
		// radio button, but I'm leaving it in as an example.

		// Prefixing a widget name with "##" keeps it from being
		// displayed.

		ImGui::PushID( 0 );
		if( ImGui::RadioButton( "Block", &current_col, block_select ) ) {
			// Select current block color for the color edit bar
			colour[0] = b_color.r;
			colour[1] = b_color.g;
			colour[2] = b_color.b;
			// block selected
			obj_selection = block_select;
		}
		ImGui::SameLine();
		if( ImGui::RadioButton( "Avatar", &current_col, avatar_select ) ) {
			// Select current avatar color for the color edit bar
			colour[0] = a_color.r;
			colour[1] = a_color.g;
			colour[2] = a_color.b;
			// avatar selected
			obj_selection = avatar_select;
		}
		ImGui::SameLine();
		if( ImGui::RadioButton( "Floor", &current_col, floor_select) ) {
			// Select current floor color for the color edit bar
			colour[0] = f_color.r;
			colour[1] = f_color.g;
			colour[2] = f_color.b;
			// floor selected
			obj_selection = floor_select;
		}
		// Color edit bar
		ImGui::ColorEdit3( "##Colour", colour );

		// change object color according to selection
		switch (obj_selection) {
			case  block_select:
				b_color.r = colour[0];
				b_color.g = colour[1];
				b_color.b = colour[2];
				break;
			case  avatar_select:
				a_color.r = colour[0];
				a_color.g = colour[1];
				a_color.b = colour[2];
				break;
			case  floor_select:
				f_color.r = colour[0];
				f_color.g = colour[1];
				f_color.b = colour[2];
				break;
			default:
				break;
		}
		ImGui::PopID();

/*
		// For convenience, you can uncomment this to show ImGui's massive
		// demonstration window right in your application.  Very handy for
		// browsing around to get the widget you want.  Then look in
		// shared/imgui/imgui_demo.cpp to see how it's done.
		if( ImGui::Button( "Test Window" ) ) {
			showTestWindow = !showTestWindow;
		}

*/
		if( ImGui::Button( "Test Window" ) ) {
			showTestWindow = !showTestWindow;
		}

		ImGui::Text( "Framerate: %.1f FPS", ImGui::GetIO().Framerate );

	ImGui::End();

	if( showTestWindow ) {
		ImGui::ShowTestWindow( &showTestWindow );
	}
}

//----------------------------------------------------------------------------------------
/*
 * Called once per frame, after guiLogic().
 */
void A1::draw()
{
	// Create a global transformation for the model (centre it).
	mat4 W;
	// scale the transformations
	W = glm::scale(W,vec3(scale));
	// rotate the transformations
	// use float not double
	W = glm::rotate(W, radians( 360.0f * rotate_change) ,vec3(0.0f,1.0f,0.0f));
	// centre the transformation
	W = glm::translate( W, vec3( -float(DIM)/2.0f, 0, -float(DIM)/2.0f ) );

	// Store the centre origin
	mat4 centre_origin = W;

	m_shader.enable();
		glEnable( GL_DEPTH_TEST );

		glUniformMatrix4fv( P_uni, 1, GL_FALSE, value_ptr( proj ) );
		glUniformMatrix4fv( V_uni, 1, GL_FALSE, value_ptr( view ) );
		glUniformMatrix4fv( M_uni, 1, GL_FALSE, value_ptr( W ) );

		// Draw Grid Section.
		glBindVertexArray( m_grid_vao );
		glUniform3f( col_uni, 1, 1, 1 );
		glDrawArrays( GL_LINES, 0, (3+DIM)*4 );

		// Draw Blocks Section
		// Translate out of the grid by -1 unit in x and z
		W = glm::translate(W,vec3(-1.0f, 0.0f, -1.0f));

		for (int i = 0; i < DIM +2; i++){
			// Translate out of the grid by 1 unit in +ve z
			W = glm::translate(W,vec3(0.0f, 0.0f,1.0f));
			for (int j = 0; j < DIM +2; j++ ){
				// Translate out of the grid by 1 unit in +ve x
				W = glm::translate(W,vec3(1.0f, 0.0f, 0.0f));
				glUniformMatrix4fv(M_uni, 1,GL_FALSE, value_ptr(W));

				if ( i % (DIM + 1) != 0 && j % (DIM + 1) != 0 && m.getValue(i-1,j-1) == 1){
					// Draw a block
					glBindVertexArray(m_block_vao);
					glUniform3f(col_uni, b_color.r,b_color.g,b_color.b); // TODO: make variables for color
					glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT,0);
				}

			}
			// Reset x location
			W = glm::translate(W,vec3(-float(DIM+2),0.0f,0.0f));
		}

		// Reset W to centre origin
		W = centre_origin;

		// Draw Avatar Section
		W = glm::translate(W,vec3(0.0f, 0.0f, 0.0f));
		glUniformMatrix4fv(M_uni, 1,GL_FALSE, value_ptr(W));

		glBindVertexArray(m_avatar_vao);
		glUniform3f(col_uni, a_color.r,a_color.g,a_color.b); // TODO: make variables for color
		glDrawArrays(GL_TRIANGLES,0, 18 * 36 *2 *3);

		// Reset W to centre origin
		W = centre_origin;

		// Draw Floor Section
		W = glm::translate(W,vec3(0.0f, 0.0f, 0.0f));
		glUniformMatrix4fv(M_uni, 1,GL_FALSE, value_ptr(W));

		glBindVertexArray(m_floor_vao);
		glUniform3f(col_uni, f_color.r,f_color.g,f_color.b); // TODO: make variables for color
		glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT,0);


		// Highlight the active square.
	m_shader.disable();

	// Restore defaults
	glBindVertexArray( 0 );

	CHECK_GL_ERRORS;
}

//----------------------------------------------------------------------------------------
/*
 * Called once, after program is signaled to terminate.
 */
void A1::cleanup()
{}

//----------------------------------------------------------------------------------------
/*
 * Event handler.  Handles cursor entering the window area events.
 */
bool A1::cursorEnterWindowEvent (
		int entered
) {
	bool eventHandled(false);

	// Fill in with event handling code...

	return eventHandled;
}

//----------------------------------------------------------------------------------------
/*
 * Event handler.  Handles mouse cursor movement events.
 */
bool A1::mouseMoveEvent(double xPos, double yPos)
{
	bool eventHandled(false);
	double curr_xPos = xPos;

	if (!ImGui::IsMouseHoveringAnyWindow()) {
		// Put some code here to handle rotations.  Probably need to
		// check whether we're *dragging*, not just moving the mouse.
		// Probably need some instance variables to track the current
		// rotation amount, and maybe the previous X position (so
		// that you can rotate relative to the *change* in X.
		if (ImGui::IsMouseDragging(0)){
			// Change rotation based on change in mouse x position
			rotate_change += (curr_xPos - prev_mouse_xPos)/ REVOLUTION;
			// Presistence variable stored based on change in mouse x position
			rotate_persistence = (curr_xPos - prev_mouse_xPos)/ REVOLUTION;
			mouse_drag = true;
		}
		prev_mouse_xPos = curr_xPos;
	}
	return eventHandled;
}

//----------------------------------------------------------------------------------------
/*
 * Event handler.  Handles mouse button events.
 */
bool A1::mouseButtonInputEvent(int button, int actions, int mods) {
	bool eventHandled(false);

	if (!ImGui::IsMouseHoveringAnyWindow()) {
		// The user clicked in the window.  If it's the left
		// mouse button, initiate a rotation.
		if (actions == GLFW_PRESS){
			mouse_drag = false;
			eventHandled = true;
		}
		if (actions == GLFW_RELEASE){
			if(mouse_drag){
				mouse_drag = false;
			}
			else{
				// TODO: deal with rotate
				rotate_persistence = 0;
			}
			eventHandled = true;
		}
	}

	return eventHandled;
}

//----------------------------------------------------------------------------------------
/*
 * Event handler.  Handles mouse scroll wheel events.
 */
bool A1::mouseScrollEvent(double xOffSet, double yOffSet) {
	bool eventHandled(false);

	// Zoom in or out.
	if (yOffSet <= 0 && scale > MIN_SCALE){
		scale = scale - 0.15f;
	}
	else if(yOffSet> 0 && scale <MAX_SCALE){
		scale = scale + 0.15f;
	}

	return eventHandled;
}

//----------------------------------------------------------------------------------------
/*
 * Event handler.  Handles window resize events.
 */
bool A1::windowResizeEvent(int width, int height) {
	bool eventHandled(false);

	// Fill in with event handling code...

	return eventHandled;
}

//----------------------------------------------------------------------------------------
/*
 * Event handler.  Handles key input events.
 */
bool A1::keyInputEvent(int key, int action, int mods) {
	bool eventHandled(false);
	bool isShiftKeyPressed = false;

	if( action == GLFW_PRESS ) {
		// Respond to some key events.
		if (key == GLFW_KEY_Q) { // Quit program
			cout << "Q key pressed" << endl;
			glfwSetWindowShouldClose(m_window, GL_TRUE);
			eventHandled = true;
		}

		if (key == GLFW_KEY_R) {// Reset world
			cout << "R key pressed" << endl;
			m.reset();
			resetWorld();
			initAvatar();
			eventHandled = true;
		}

		if (key == GLFW_KEY_D) {// Dig Maze
			cout << "D key pressed" << endl;
			resetWorld();
			m.digMaze();
			placeAvatar();
			initAvatar();
			eventHandled = true;
		}

		if (key == GLFW_KEY_SPACE) {// Increase block height
			cout << "SPACE key pressed" << endl;
			if (b_height < MAX_BLOCK_HEIGHT) b_height += DELTA_BLOCK_UNIT;
			initBlock();
			eventHandled = true;
		}

		if (key == GLFW_KEY_BACKSPACE) {// Decrease block height
			cout << "BACKSPACE key pressed" << endl;
			if (b_height > MIN_BLOCK_HEIGHT) b_height -= DELTA_BLOCK_UNIT;
			initBlock();
			eventHandled = true;
		}

		if (key == GLFW_KEY_RIGHT && a_offset_x <= DIM) { // Move right
			cout << "RIGHT ARROW key pressed" << endl;
			// Check if right step is border
			if (int(a_offset_x + 1.0f)%(DIM + 1) == 0|| int(a_offset_z)%(DIM + 1) == 0){
				a_offset_x += DELTA_BLOCK_UNIT;
			}
			// Should be in the Maze
			// Check if a wall to the right
			else if ( m.getValue(int(a_offset_z - 1), int(a_offset_x)) == 0){
				a_offset_x += DELTA_BLOCK_UNIT;
			}
			// Remove wall if present
			else if(glfwGetKey(glfwGetCurrentContext(),GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS||
					glfwGetKey(glfwGetCurrentContext(),GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS){
				cout << "SHIFT + RIGHT ARROW key pressed" << endl;
				m.setValue(int(a_offset_z - 1), int(a_offset_x),0);
				a_offset_x += DELTA_BLOCK_UNIT;
			}

			initAvatar();

			eventHandled = true;
		}

		if (key == GLFW_KEY_LEFT && a_offset_x >= 1) { // Move left
			cout << "LEFT ARROW key pressed" << endl;
			// Check if left step is border
			if (int(a_offset_x - 1.0f)%(DIM + 1) == 0|| int(a_offset_z)%(DIM + 1) == 0){
				a_offset_x -= DELTA_BLOCK_UNIT;
			}
			// Should be in the Maze
			// Check if a wall to the left
			else if ( m.getValue(int(a_offset_z - 1.0f), int(a_offset_x - 2.0f)) == 0){
				a_offset_x -= DELTA_BLOCK_UNIT;
			}
			// Remove wall if present
			else if(glfwGetKey(glfwGetCurrentContext(),GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS||
					glfwGetKey(glfwGetCurrentContext(),GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS){
				cout << "SHIFT + LEFT ARROW key pressed" << endl;
				m.setValue(a_offset_z - 1.0f, a_offset_x - 2.0f, 0);
				a_offset_x -= DELTA_BLOCK_UNIT;
			}

			initAvatar();

			eventHandled = true;
		}


		if (key == GLFW_KEY_UP && a_offset_z >= 1) { // Move up
			cout << "UP ARROW key pressed" << endl;
			// Check if up step is border
			if (int(a_offset_x)%(DIM + 1) == 0|| int(a_offset_z - 1.0f)%(DIM + 1) == 0){
				a_offset_z -= DELTA_BLOCK_UNIT;
			}
			// Should be in the Maze
			// Check if a wall to upwards
			else if ( m.getValue(int(a_offset_z - 2.0f), int(a_offset_x - 1.0f)) == 0){
				a_offset_z -= DELTA_BLOCK_UNIT;
			}
			// Remove wall if present
			else if(glfwGetKey(glfwGetCurrentContext(),GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS||
					glfwGetKey(glfwGetCurrentContext(),GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS){
				cout << "SHIFT + UP ARROW key pressed" << endl;
				m.setValue(a_offset_z - 2.0f, a_offset_x - 1.0f, 0);
				a_offset_z -= DELTA_BLOCK_UNIT;
			}

			initAvatar();

			eventHandled = true;
		}

		if (key == GLFW_KEY_DOWN && a_offset_z <= DIM) { // Move down
			cout << "UP ARROW key pressed" << endl;
			// Check if down step is border
			if (int(a_offset_x)%(DIM + 1) == 0|| int(a_offset_z + 1.0f)%(DIM + 1) == 0){
				a_offset_z += DELTA_BLOCK_UNIT;
			}
			// Should be in the Maze
			// Check if a wall downwards
			else if ( m.getValue(int(a_offset_z), int(a_offset_x - 1.0f)) == 0){
				a_offset_z += DELTA_BLOCK_UNIT;
			}
			// Remove wall if present
			else if(glfwGetKey(glfwGetCurrentContext(),GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS||
					glfwGetKey(glfwGetCurrentContext(),GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS){
				cout << "SHIFT + DOWN ARROW key pressed" << endl;
				m.setValue(a_offset_z , a_offset_x - 1.0f, 0);
				a_offset_z += DELTA_BLOCK_UNIT;
			}

			initAvatar();

			eventHandled = true;
		}


		if (key == GLFW_KEY_DOWN) {
			cout << "DOWN ARROW key pressed" << endl;
			// TODO - handle DOWN ARROW key
			eventHandled = true;
		}

	}

	return eventHandled;
}
