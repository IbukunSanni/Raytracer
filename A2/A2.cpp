// Termm--Fall 2020

#include "A2.hpp"
#include "cs488-framework/GlErrorCheck.hpp"

#include <iostream>
using namespace std;

#include <imgui/imgui.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/io.hpp>
using namespace glm;
using namespace std;
static const GLfloat MAX_RGB = 255.0f;// MAXIMUM color of RGB #FF in decimal
static const GLfloat PI = 3.14159265f; //  PI for sphere calculations
static const mat4 IDENTITY = glm::mat4(1.0f) ;// Identity for defaults


//----------------------------------------------------------------------------------------
// Constructor
VertexData::VertexData()
	: numVertices(0),
	  index(0)
{
	positions.resize(kMaxVertices);
	colours.resize(kMaxVertices);
}


//----------------------------------------------------------------------------------------
// Constructor
A2::A2()
	: m_currentLineColour(vec3(0.0f)),curr_mode(0)
{

}

//----------------------------------------------------------------------------------------
// Destructor
A2::~A2()
{

}

//----------------------------------------------------------------------------------------
/*
 * Called once, at program start.
 */
void A2::init()
{
	// Set the background colour.
	glClearColor(0.3, 0.5, 0.7, 1.0);

	createShaderProgram();

	glGenVertexArrays(1, &m_vao);

	enableVertexAttribIndices();

	generateVertexBuffers();

	mapVboDataToVertexAttributeLocation();

	initBlockVerts();
	resetWorld();

}

//----------------------------------------------------------------------------------------
void A2::createShaderProgram()
{
	m_shader.generateProgramObject();
	m_shader.attachVertexShader( getAssetFilePath("VertexShader.vs").c_str() );
	m_shader.attachFragmentShader( getAssetFilePath("FragmentShader.fs").c_str() );
	m_shader.link();
}

//---------------------------------------------------------------------------------------- Spring 2020
void A2::enableVertexAttribIndices()
{
	glBindVertexArray(m_vao);

	// Enable the attribute index location for "position" when rendering.
	GLint positionAttribLocation = m_shader.getAttribLocation( "position" );
	glEnableVertexAttribArray(positionAttribLocation);

	// Enable the attribute index location for "colour" when rendering.
	GLint colourAttribLocation = m_shader.getAttribLocation( "colour" );
	glEnableVertexAttribArray(colourAttribLocation);

	// Restore defaults
	glBindVertexArray(0);

	CHECK_GL_ERRORS;
}

//----------------------------------------------------------------------------------------
void A2::generateVertexBuffers()
{
	// Generate a vertex buffer to store line vertex positions
	{
		glGenBuffers(1, &m_vbo_positions);

		glBindBuffer(GL_ARRAY_BUFFER, m_vbo_positions);

		// Set to GL_DYNAMIC_DRAW because the data store will be modified frequently.
		glBufferData(GL_ARRAY_BUFFER, sizeof(vec2) * kMaxVertices, nullptr,
				GL_DYNAMIC_DRAW);


		// Unbind the target GL_ARRAY_BUFFER, now that we are finished using it.
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		CHECK_GL_ERRORS;
	}

	// Generate a vertex buffer to store line colors
	{
		glGenBuffers(1, &m_vbo_colours);

		glBindBuffer(GL_ARRAY_BUFFER, m_vbo_colours);

		// Set to GL_DYNAMIC_DRAW because the data store will be modified frequently.
		glBufferData(GL_ARRAY_BUFFER, sizeof(vec3) * kMaxVertices, nullptr,
				GL_DYNAMIC_DRAW);


		// Unbind the target GL_ARRAY_BUFFER, now that we are finished using it.
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		CHECK_GL_ERRORS;
	}
}

//----------------------------------------------------------------------------------------
void A2::mapVboDataToVertexAttributeLocation()
{
	// Bind VAO in order to record the data mapping.
	glBindVertexArray(m_vao);

	// Tell GL how to map data from the vertex buffer "m_vbo_positions" into the
	// "position" vertex attribute index for any bound shader program.
	glBindBuffer(GL_ARRAY_BUFFER, m_vbo_positions);
	GLint positionAttribLocation = m_shader.getAttribLocation( "position" );
	glVertexAttribPointer(positionAttribLocation, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

	// Tell GL how to map data from the vertex buffer "m_vbo_colours" into the
	// "colour" vertex attribute index for any bound shader program.
	glBindBuffer(GL_ARRAY_BUFFER, m_vbo_colours);
	GLint colorAttribLocation = m_shader.getAttribLocation( "colour" );
	glVertexAttribPointer(colorAttribLocation, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

	//-- Unbind target, and restore default values:
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	CHECK_GL_ERRORS;
}


//---------------------------------------------------------------------------------------
void A2::resetWorld(){
	model_move = IDENTITY;
}


//---------------------------------------------------------------------------------------
void A2::initLineData()
{
	m_vertexData.numVertices = 0;
	m_vertexData.index = 0;
}

//---------------------------------------------------------------------------------------
void A2::setLineColour (
		const glm::vec3 & colour
) {
	m_currentLineColour = colour;
}

//---------------------------------------------------------------------------------------
void A2::drawLine(
		const glm::vec2 & V0,   // Line Start (NDC coordinate)
		const glm::vec2 & V1    // Line End (NDC coordinate)
) {

	m_vertexData.positions[m_vertexData.index] = V0;
	m_vertexData.colours[m_vertexData.index] = m_currentLineColour;
	++m_vertexData.index;
	m_vertexData.positions[m_vertexData.index] = V1;
	m_vertexData.colours[m_vertexData.index] = m_currentLineColour;
	++m_vertexData.index;

	m_vertexData.numVertices += 2;
}

//---------------------------------------------------------------------------------------
void A2::initBlockVerts(){
	GLfloat edge_sz = 0.3f;
	// using right hand rule: right +x, left +y, out of window +z
	// top face                       x,       y,       z,    1
	block_verts.push_back(vec4(-edge_sz, edge_sz, edge_sz, 1.0f));// idx = 0, left top front
	block_verts.push_back(vec4( edge_sz, edge_sz, edge_sz, 1.0f));// idx = 1, right top front
	block_verts.push_back(vec4( edge_sz, edge_sz,-edge_sz, 1.0f));// idx = 2, right top back
	block_verts.push_back(vec4(-edge_sz, edge_sz,-edge_sz, 1.0f));// idx = 3, left top back

	// bottom face
	block_verts.push_back(vec4(-edge_sz,-edge_sz, edge_sz, 1.0f));// idx = 4, left bottom front
	block_verts.push_back(vec4( edge_sz,-edge_sz, edge_sz, 1.0f));// idx = 5, right bottom front
	block_verts.push_back(vec4( edge_sz,-edge_sz,-edge_sz, 1.0f));// idx = 6, right bottom back
	block_verts.push_back(vec4(-edge_sz,-edge_sz,-edge_sz, 1.0f));// idx = 7, left bottom back
}


//----------------------------------------------------------------------------------------
// Draws the edge or line for a block based on transformations
void A2::drawBlockEdge(
	vec4  V1,   // Line Start
	vec4  V2    // Line End
){
	// TODO: add all missing transformations
	vec4 A = model_move * V1;
	vec4 B = model_move * V2;

	drawLine(vec2(A.x,A.y),vec2(B.x,B.y));

}
//----------------------------------------------------------------------------------------
/*
 * Called once per frame, before guiLogic().
 */
void A2::appLogic()
{
	// Place per frame, application logic here ...

	// Call at the beginning of frame, before drawing lines:
	initLineData();

	// Draw Block
	setLineColour(vec3(91.0f/MAX_RGB, 12.0f/MAX_RGB, 112.0f/MAX_RGB));// purple
	drawBlockEdge(block_verts[0],block_verts[1]);// top front
	drawBlockEdge(block_verts[1],block_verts[2]);// top right
	drawBlockEdge(block_verts[2],block_verts[3]);// top back
	drawBlockEdge(block_verts[3],block_verts[0]);// top left

	drawBlockEdge(block_verts[4],block_verts[5]);// bottom front
	drawBlockEdge(block_verts[5],block_verts[6]);// bottom right
	drawBlockEdge(block_verts[6],block_verts[7]);// bottom back
	drawBlockEdge(block_verts[7],block_verts[4]);// bottom left

	drawBlockEdge(block_verts[0],block_verts[4]);// left front
	drawBlockEdge(block_verts[1],block_verts[5]);// right front
	drawBlockEdge(block_verts[2],block_verts[6]);// right back
	drawBlockEdge(block_verts[3],block_verts[7]);// left back

	// Draw outer square:
	setLineColour(vec3(1.0f, 0.7f, 0.8f));
	drawLine(vec2(-0.5f, -0.5f), vec2(0.5f, -0.5f));
	drawLine(vec2(0.5f, -0.5f), vec2(0.5f, 0.5f));
	drawLine(vec2(0.5f, 0.5f), vec2(-0.5f, 0.5f));
	drawLine(vec2(-0.5f, 0.5f), vec2(-0.5f, -0.5f));


	// Draw inner square:
	setLineColour(vec3(0.2f, 1.0f, 1.0f));
	drawLine(vec2(-0.25f, -0.25f), vec2(0.25f, -0.25f));
	drawLine(vec2(0.25f, -0.25f), vec2(0.25f, 0.25f));
	drawLine(vec2(0.25f, 0.25f), vec2(-0.25f, 0.25f));
	drawLine(vec2(-0.25f, 0.25f), vec2(-0.25f, -0.25f));
}

//----------------------------------------------------------------------------------------
/*
 * Called once per frame, after appLogic(), but before the draw() method.
 */
void A2::guiLogic()
{
	static bool firstRun(true);
	if (firstRun) {
		ImGui::SetNextWindowPos(ImVec2(50, 50));
		firstRun = false;
	}

	static bool showDebugWindow(true);
	ImGuiWindowFlags windowFlags(ImGuiWindowFlags_AlwaysAutoResize);
	float opacity(0.5f);

	ImGui::Begin("Properties", &showDebugWindow, ImVec2(100,100), opacity,
			windowFlags);

		// Create Button, and check if it was clicked:

		// PushID/PopID necessary for multiple radio buttons
		ImGui::PushID( 0 );
		// Rotate View Mode
		if( ImGui::RadioButton( "Rotate View Mode  (O)", &curr_mode, rv_mode) ) {
			// TODO: add functionality

			// Rotate View Mode selected
			mode_selection = rv_mode;
		}

		// Translate View Mode
		if( ImGui::RadioButton( "Translate View Mode  (E)", &curr_mode, tv_mode) ) {
			// TODO: add functionality

			// Translate View Mode selected
			mode_selection = tv_mode;
		}

		// Perspective Mode
		if( ImGui::RadioButton( "Perspective Mode (P)", &curr_mode, p_mode) ) {
			// TODO: add functionality

			// Perspective Mode selected
			mode_selection = p_mode;
		}

		// Rotate Model Mode
		if( ImGui::RadioButton( "Rotate Model Mode (R)", &curr_mode, rm_mode) ) {
			// TODO: add functionality

			// Rotate Model Mode selected
			mode_selection = rm_mode;
		}

		// Translate Model Mode
		if( ImGui::RadioButton( "Translate Model Mode (T)", &curr_mode, tm_mode) ) {
			// TODO: add functionality

			// Translate Model Mode selected
			mode_selection = tm_mode;
		}

		// Scale Model Mode
		if( ImGui::RadioButton( "Scale Model Mode (S)", &curr_mode, sm_mode) ) {
			// TODO: add functionality

			// Scale Model Mode selected
			mode_selection = sm_mode;
		}

		// Viewport Mode
		if( ImGui::RadioButton( "Viewport Mode (V)", &curr_mode, v_mode) ) {
			// TODO: add functionality

			// Viewport Mode selected
			mode_selection = v_mode;
		}

		ImGui::PopID();

		// Reset Application
		if( ImGui::Button( "Reset Application (A)" ) ) {
			// TODO: add functionality
		}

		// Quit Application
		if( ImGui::Button( "Quit Application (Q)" ) ) {
			glfwSetWindowShouldClose(m_window, GL_TRUE);
		}

		ImGui::Text( "Framerate: %.1f FPS", ImGui::GetIO().Framerate );
		// TODO: add near and far plane locations
		// reference framerate
		ImGui::Text( "Near: , Far: ");

	ImGui::End();
}

//----------------------------------------------------------------------------------------
void A2::uploadVertexDataToVbos() {

	//-- Copy vertex position data into VBO, m_vbo_positions:
	{
		glBindBuffer(GL_ARRAY_BUFFER, m_vbo_positions);
		glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vec2) * m_vertexData.numVertices,
				m_vertexData.positions.data());
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		CHECK_GL_ERRORS;
	}

	//-- Copy vertex colour data into VBO, m_vbo_colours:
	{
		glBindBuffer(GL_ARRAY_BUFFER, m_vbo_colours);
		glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vec3) * m_vertexData.numVertices,
				m_vertexData.colours.data());
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		CHECK_GL_ERRORS;
	}
}

//----------------------------------------------------------------------------------------
/*
 * Called once per frame, after guiLogic().
 */
void A2::draw()
{
	uploadVertexDataToVbos();

	glBindVertexArray(m_vao);

	m_shader.enable();
		glDrawArrays(GL_LINES, 0, m_vertexData.numVertices);
	m_shader.disable();

	// Restore defaults
	glBindVertexArray(0);

	CHECK_GL_ERRORS;
}

//----------------------------------------------------------------------------------------
/*
 * Called once, after program is signaled to terminate.
 */
void A2::cleanup()
{

}

//----------------------------------------------------------------------------------------
/*
 * Event handler.  Handles cursor entering the window area events.
 */
bool A2::cursorEnterWindowEvent (
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
bool A2::mouseMoveEvent (
		double xPos,
		double yPos
) {
	bool eventHandled(false);

	// Fill in with event handling code...

	return eventHandled;
}

//----------------------------------------------------------------------------------------
/*
 * Event handler.  Handles mouse button events.
 */
bool A2::mouseButtonInputEvent (
		int button,
		int actions,
		int mods
) {
	bool eventHandled(false);

	// Fill in with event handling code...

	return eventHandled;
}

//----------------------------------------------------------------------------------------
/*
 * Event handler.  Handles mouse scroll wheel events.
 */
bool A2::mouseScrollEvent (
		double xOffSet,
		double yOffSet
) {
	bool eventHandled(false);

	// Fill in with event handling code...

	return eventHandled;
}

//----------------------------------------------------------------------------------------
/*
 * Event handler.  Handles window resize events.
 */
bool A2::windowResizeEvent (
		int width,
		int height
) {
	bool eventHandled(false);

	// Fill in with event handling code...

	return eventHandled;
}

//----------------------------------------------------------------------------------------
/*
 * Event handler.  Handles key input events.
 */
bool A2::keyInputEvent (int key,int action,int mods) {
	bool eventHandled(false);
	// TODO: add necessary key presses
	// Respond to some key events.
	if( action == GLFW_PRESS ) {
		// Reset program
		if (key == GLFW_KEY_A) {
			cout << "A key pressed" << endl;
			// TODO: add functionality
			eventHandled = true;
		}

		// Quit program
		if (key == GLFW_KEY_Q) {
			cout << "Q key pressed" << endl;
			glfwSetWindowShouldClose(m_window, GL_TRUE);
			eventHandled = true;
		}

		// Rotate View Mode
		if (key == GLFW_KEY_O) {
			cout << "O key pressed" << endl;
			// TODO: add functionality
			eventHandled = true;
		}

		// Translate View Mode
		if (key == GLFW_KEY_E) {
			cout << "E key pressed" << endl;
			// TODO: add functionality
			eventHandled = true;
		}

		// Perspective Mode
		if (key == GLFW_KEY_P) {
			cout << "P key pressed" << endl;
			// TODO: add functionality
			eventHandled = true;
		}

		// Rotate Model Mode
		if (key == GLFW_KEY_R) {
			cout << "R key pressed" << endl;
			// TODO: add functionality
			eventHandled = true;
		}

		// Translate Model Mode
		if (key == GLFW_KEY_T) {
			cout << "T key pressed" << endl;
			// TODO: add functionality
			eventHandled = true;
		}

		// Scale Model Mode
		if (key == GLFW_KEY_S) {
			cout << "S key pressed" << endl;
			// TODO: add functionality
			eventHandled = true;
		}

		// Viewport Mode
		if (key == GLFW_KEY_V) {
			cout << "V key pressed" << endl;
			// TODO: add functionality
			eventHandled = true;
		}
	}

	return eventHandled;
}
