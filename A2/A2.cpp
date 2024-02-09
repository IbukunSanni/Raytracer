// Termm--Fall 2020
// TODO: look for all couts before submission and TODOs

#include "A2.hpp"
#include "cs488-framework/GlErrorCheck.hpp"

#include <iostream>
#include <typeinfo>

#include <imgui/imgui.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/io.hpp>
using namespace glm;
using namespace std;
static const GLfloat MAX_RGB = 255.0f;// MAXIMUM color of RGB #FF in decimal
static const GLfloat PI = 3.14159265f; //  PI for sphere calculations
static const mat4 IDENTITY = mat4(1.0f) ;// Identity for defaults

static const GLfloat ROTATION_MODEL_CONST  = 0.02f; // rotation factor for model
static const GLfloat ROTATION_VIEW_CONST  = 0.005f; // rotation factor for view
static const GLfloat TRANSLATE_CONST = 0.02f; // translation factor
static const GLfloat SCALE_CONST = 0.02f; // scale factor
static const GLfloat SCALE_MAX = 10.0f; // Max scaling
static const GLfloat SCALE_MIN = 0.01f; // Min scaling

static const GLfloat LOOK_FROM_Z = 20.0f; // Look from z - axis position
static const GLfloat LOOK_AT_Z = -0.5f; // Look at z - axis position
static const GLfloat UP_VEC_OFFSET = -0.05f; // Look at z - axis position

static const GLfloat NEAR_DEFAULT = 1.0f;
static const GLfloat FAR_DEFAULT = 25.0f;
static const GLfloat NEAR_MIN = 0.5f;
static const GLfloat FAR_MAX = 50.0f;
static const GLfloat NEAR_FAR_CONST = 0.5f;

static const GLfloat FOV_Y_DEFAULT = 30.0f *(PI /180.0f); // TODO: default is 30 degrees
static const GLfloat FOV_Y_MAX = 5.0f *(PI /180.0f);
static const GLfloat FOV_Y_MIN = 160.0f *(PI /180.0f);
static const GLfloat FOV_Y_CONST = 1.0f *(PI /180.0f);




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
	: m_currentLineColour(vec3(0.0f))
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
	initCoord();
	resetWorld();

	//---------------------

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
void A2::resetView(){
	// View frame is left hand view:    right +x, up +y, out the window -z
	// World frame is right hande view: right +x, up +y, out the window +z
	// TODO: compare opposing z axes
	glm::vec3 look_at(0.0f,0.0f,LOOK_AT_Z);
	glm::vec3 lookfrom(0.0f,0.0f,LOOK_FROM_Z);
	glm::vec3 up(0.0f, 0.0f + UP_VEC_OFFSET, LOOK_FROM_Z);

	glm::vec3 vz((look_at - lookfrom)/ glm::length(look_at - lookfrom));// view_z
	// TODO: check crossing, which way should it happen
	glm::vec3 vx(glm::cross(up,vz)/ glm::length(glm::cross(up,vz)));// view_x
	glm::vec3 vy(glm::cross(vz,vx)); // view_y

	// This declaration results in column vectors, each vec4 is a column of the
	// matrix R
	mat4 R {
					glm::vec4(vx[0],vx[1],vx[2],0.0f),
					glm::vec4(vy[0],vy[1],vy[2],0.0f),
					glm::vec4(vz[0],vz[1],vz[2],0.0f),
					glm::vec4( 0.0f, 0.0f, 0.0f,1.0f)
				 };

	// This declaration results in column vectors, each vec4 is a column of the
	// matrix T
	mat4 T {
				 	glm::vec4( 1.0f, 0.0f, 0.0f,-lookfrom[0]),
				 	glm::vec4( 0.0f, 1.0f, 0.0f,-lookfrom[1]),
				 	glm::vec4( 0.0f, 0.0f, 1.0f,-lookfrom[2]),
				 	glm::vec4( 0.0f, 0.0f, 0.0f,1.0f)
				 };
	// Transpose to achieve correct matrices for both
	view = glm::transpose(R) * glm::transpose(T);
}
//---------------------------------------------------------------------------------------
void A2::resetProjection(){
	// aspect = W/H;
	aspect = abs((viewportCoords[0][0] -viewportCoords[1][0])/(viewportCoords[0][1] -viewportCoords[1][1]));

	GLfloat theta_half = fovy/2.0f; // fovy in radians
	GLfloat cot_theta_half = 1 / tan(theta_half);
	GLfloat sign = +1.0f; // TODO: pick whether +ve or -ve

	// This declaration results in column vectors, each vec4 is a column of the
	// matrix P
	mat4 P{
					glm::vec4( cot_theta_half/aspect, 0.0f, 0.0f,0.0f),
					glm::vec4( 0.0f, cot_theta_half, 0.0f,0.0f),
					glm::vec4( 0.0f, 0.0f, sign *(far + near)/(far - near),(-2*far*near)/(far-near)),
					glm::vec4( 0.0f, 0.0f, sign,0.0f)
				};
	// Transpose P to get correct matrix
	projection = glm::transpose(P);
}

//---------------------------------------------------------------------------------------
void A2::resetWorld(){
	model_trans_rot = IDENTITY;
	model_trans_rot = IDENTITY;
	model_scale = IDENTITY;
	mode_selection = rm_mode;
	resetView();
	view_trans_rot = IDENTITY;
	near = NEAR_DEFAULT;
	far = FAR_DEFAULT;
	fovy = FOV_Y_DEFAULT;
	glfwGetFramebufferSize(m_window, &window_width, &window_height);
	viewportCoords[0][0] = 0.05 * window_width;
	viewportCoords[0][1] = 0.05 * window_height;
	viewportCoords[1][0] = window_width  - 0.05 * window_width;
	viewportCoords[1][1] = window_height - 0.05 * window_height;
	aspect = abs((viewportCoords[0][0] -viewportCoords[1][0])/(viewportCoords[0][1] -viewportCoords[1][1]));

	resetProjection();
}

//---------------------------------------------------------------------------------------
// returns min_val if value less than
// returns max_val if value greater than
// else returns original value
double A2:: limitPos(
	double val,
	double min_val,
	double max_val
){
	if(val < min_val){
		return min_val;
	}else if (val > max_val){
		return max_val;
	}
	return val;
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
	GLfloat edge_sz = 1.3f;
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

//---------------------------------------------------------------------------------------
void A2::initCoord(){
	GLfloat edge_sz = 1.2f;
	// using right hand rule: right +x, left +y, out of window +z

	coord_verts.push_back(vec4( 0.0f,0.0f,0.0f,1.0f)); // origin
	coord_verts.push_back(vec4( edge_sz,0.0f,0.0f,1.0f)); // local x-axis
	coord_verts.push_back(vec4( 0.0f,edge_sz,0.0f,1.0f)); // local y-axis
	coord_verts.push_back(vec4( 0.0f,0.0f,edge_sz,1.0f)); // local z-axis
}
//----------------------------------------------------------------------------------------
bool A2::clippingNearPlane(
	vec4  &A,   // Line Start
	vec4  &B    // Line End
){
	vec4 n(0.0f,0.0f,1.0f,0.0f); // normal on near plane
	vec4 P(0.0f,0.0f,near,1.0f);// point on plane
	GLfloat wecA = glm::dot((A-P),n);
	GLfloat wecB = glm::dot((B-P),n);
	if (wecA < 0.0f && wecB < 0.0f){// trivially reject
		return false;
	}

	if (wecA >= 0.0f && wecB >= 0.0f){// trivially accept
		return true;
	}

	GLfloat t = wecA/(wecA - wecB);
	if (wecA < 0.0f){
		A = A + t*(B-A);
	}
	else{
		B = A + t*(B-A);
	}
	return true;
}

//----------------------------------------------------------------------------------------
bool A2::clippingFarPlane(
	vec4  &A,   // Line Start
	vec4  &B    // Line End
){
	vec4 n(0.0f,0.0f,-1.0f,0.0f); // normal on far plane
	vec4 P(0.0f,0.0f,far,1.0f);// point on plane
	GLfloat wecA = glm::dot((A-P),n);
	GLfloat wecB = glm::dot((B-P),n);
	if (wecA < 0.0f && wecB < 0.0f){// trivially reject
		return false;
	}

	if (wecA >= 0.0f && wecB >= 0.0f){// trivially accept
		return true;
	}

	GLfloat t = wecA/(wecA - wecB);
	if (wecA < 0.0f){
		A = A + t*(B-A);
	}
	else{
		B = A + t*(B-A);
	}
	return true;
}

//----------------------------------------------------------------------------------------
bool A2::clippingTopPlane(
	vec4  &A,   // Line Start
	vec4  &B    // Line End
){
	GLfloat wecA = -A.y + A.w;
	GLfloat wecB = -B.y + B.w;
	if (wecA < 0.0f && wecB < 0.0f){// trivially reject
		return false;
	}

	if (wecA >= 0.0f && wecB >= 0.0f){// trivially accept
		return true;
	}

	GLfloat t = wecA/(wecA - wecB);
	if (wecA < 0.0f){
		A = A + t*(B-A);
	}
	else{
		B = A + t*(B-A);
	}
	return true;
}

//----------------------------------------------------------------------------------------
bool A2::clippingBottomPlane(
	vec4  &A,   // Line Start
	vec4  &B    // Line End
){
	GLfloat wecA = A.y + A.w;
	GLfloat wecB = B.y + B.w;
	if (wecA < 0.0f && wecB < 0.0f){// trivially reject
		return false;
	}

	if (wecA >= 0.0f && wecB >= 0.0f){// trivially accept
		return true;
	}

	GLfloat t = wecA/(wecA - wecB);
	if (wecA < 0.0f){
		A = A + t*(B-A);
	}
	else{
		B = A + t*(B-A);
	}
	return true;
}

//----------------------------------------------------------------------------------------
bool A2::clippingLeftPlane(
	vec4  &A,   // Line Start
	vec4  &B    // Line End
){
	GLfloat wecA = A.x + A.w;
	GLfloat wecB = B.x + B.w;
	if (wecA < 0.0f && wecB < 0.0f){// trivially reject
		return false;
	}

	if (wecA >= 0.0f && wecB >= 0.0f){// trivially accept
		return true;
	}

	GLfloat t = wecA/(wecA - wecB);
	if (wecA < 0.0f){
		A = A + t*(B-A);
	}
	else{
		B = A + t*(B-A);
	}
	return true;
}

//----------------------------------------------------------------------------------------
bool A2::clippingRightPlane(
	vec4  &A,   // Line Start
	vec4  &B    // Line End
){
	GLfloat wecA = -A.x + A.w;
	GLfloat wecB = -B.x + B.w;
	if (wecA < 0.0f && wecB < 0.0f){// trivially reject
		return false;
	}

	if (wecA >= 0.0f && wecB >= 0.0f){// trivially accept
		return true;
	}

	GLfloat t = wecA/(wecA - wecB);
	if (wecA < 0.0f){
		A = A + t*(B-A);
	}
	else{
		B = A + t*(B-A);
	}
	return true;
}
//----------------------------------------------------------------------------------------
glm::vec4 A2::homogenize(
	glm::vec4 A
){
	return vec4(A.x/A.w,
							A.y/A.w,
							A.z/A.w,
							A.w/A.w);
}

//----------------------------------------------------------------------------------------
// Draws the edge or line for a block based on transformations
void A2::drawBlockEdge(
	vec4  V1,   // Line Start
	vec4  V2,   // Line End
	float x_mid,
	float y_mid,
	float x_scale,
	float y_scale
){
	// TODO: add projection transformation
	vec4 A = view_trans_rot * view * model_trans_rot * model_scale * V1;
	vec4 B = view_trans_rot * view * model_trans_rot * model_scale * V2;
	// TODO: add clipping for near and far
	// and the clippings only when all are true drawLine
	// if one is false(trivially reject) do not draw

	// False represents trivial rejection
	if(clippingNearPlane(A,B) == false){
		 return;
	}

	if(clippingFarPlane(A,B) == false){
		 return;
	}

	// TODO: do Projection
	resetProjection();
	A = projection * A;
	B = projection * B;

	// TODO: clip left, right, top and bottom
	if(clippingLeftPlane(A,B) == false){
		 return;
	}

	if(clippingRightPlane(A,B) == false){
		 return;
	}

	if(clippingTopPlane(A,B) == false){
		 return;
	}

	if(clippingBottomPlane(A,B) == false){
		 return;
	}

	// TODO: homogenize
	A = homogenize(A);
	B = homogenize(B);

	// Then draw
	drawLine(vec2(A.x * x_scale + x_mid, A.y * y_scale + y_mid),
					 vec2(B.x * x_scale + x_mid, B.y * y_scale + y_mid));

}

//----------------------------------------------------------------------------------------
// Draws each of the local-axes for the block based on transformations
void A2::drawModelCoordAxis(
	vec4  V1,   // Line Start
	vec4  V2,    // Line End
	float x_mid,
	float y_mid,
	float x_scale,
	float y_scale
){
	// TODO: add projection transformation
	vec4 A = view_trans_rot * view * model_trans_rot * V1;
	vec4 B = view_trans_rot * view * model_trans_rot * V2;
	// TODO: add clipping for near and far
	// and the clippings only when all are true drawLine
	// if one is false(trivially reject) do not draw

	// False represents trivial rejection
	if(clippingNearPlane(A,B) == false){
		 return;
	}

	if(clippingFarPlane(A,B) == false){
		 return;
	}

	// TODO: do Projection
	resetProjection();
	A = projection * A;
	B = projection * B;

	// TODO: clip left, right, top and bottom
	if(clippingLeftPlane(A,B) == false){
		 return;
	}

	if(clippingRightPlane(A,B) == false){
		 return;
	}

	if(clippingTopPlane(A,B) == false){
		 return;
	}

	if(clippingBottomPlane(A,B) == false){
		 return;
	}

	// TODO: homogenize
	A = homogenize(A);
	B = homogenize(B);

	// Then draw
	drawLine(vec2(A.x * x_scale + x_mid, A.y * y_scale + y_mid),
					 vec2(B.x * x_scale + x_mid, B.y * y_scale + y_mid));

}

//----------------------------------------------------------------------------------------
// Draws each axis for the World  based on transformations
void A2::drawWorldCoordAxis(
	vec4  V1,   // Line Start
	vec4  V2,   // Line End
	float x_mid,
	float y_mid,
	float x_scale,
	float y_scale
){
	// TODO: add projection transformation
	vec4 A = view_trans_rot * view * V1;
	vec4 B = view_trans_rot * view * V2;
	// TODO: add clipping for near and far
	// and the clippings only when all are true drawLine
	// if one is false(trivially reject) do not draw

	// False represents trivial rejection
	if(clippingNearPlane(A,B) == false){
		 return;
	}

	if(clippingFarPlane(A,B) == false){
		 return;
	}

	// TODO: do Projection
	resetProjection();
	A = projection * A;
	B = projection * B;

	// TODO: clip left, right, top and bottom
	if(clippingLeftPlane(A,B) == false){
		 return;
	}

	if(clippingRightPlane(A,B) == false){
		 return;
	}

	if(clippingTopPlane(A,B) == false){
		 return;
	}

	if(clippingBottomPlane(A,B) == false){
		 return;
	}

	// TODO: homogenize
	A = homogenize(A);
	B = homogenize(B);

	// Then draw
	drawLine(vec2(A.x * x_scale + x_mid, A.y * y_scale + y_mid),
					 vec2(B.x * x_scale + x_mid, B.y * y_scale + y_mid));

}

float A2::scaleConverter(
	float val,
	float og_min,
	float og_max,
	float new_min,
	float new_max
){
	float norm_val = (val - og_min)/(og_max - og_min);
	float scaled_val = new_min + norm_val *(new_max - new_min);
	return scaled_val;
}

//----------------------------------------------------------------------------------------
/*
 * Called once per frame, before guiLogic().
 */
void A2::appLogic()
{
	// Place per frame, application logic here ...

	// New Viewport variables to store
	// top-left (0,0) and bottom-right (window_width,window_height)
	// left
	float viewport_left = std::min(viewportCoords[0][0],viewportCoords[1][0]);
	// top
	float viewport_top = std::min(viewportCoords[0][1],viewportCoords[1][1]);
	// right
	float viewport_right = std::max(viewportCoords[0][0],viewportCoords[1][0]);
	// bottom
	float viewport_bottom = std::max(viewportCoords[0][1],viewportCoords[1][1]);

	glfwGetFramebufferSize(m_window, &window_width, &window_height);
	float x_scale = (viewport_right - viewport_left)/window_width;
	float y_scale = (viewport_bottom - viewport_top)/window_height;

	// scale new Viewport with a range (-1 to 1)
	// left
	viewport_left = scaleConverter(viewport_left,0.0f,window_width,-1.0f,1.0f);
	// top
	viewport_top = scaleConverter(viewport_top,0.0f,window_height,1.0f,-1.0f);// inverted cause of window
	// right
	viewport_right = scaleConverter(viewport_right,0.0f,window_width,-1.0f,1.0f);
	// bottom
	viewport_bottom = scaleConverter(viewport_bottom,0.0f,window_height,1.0f,-1.0f);// inverted cause of window

	float x_mid = 0.5 * (viewport_left + viewport_right);
	float y_mid = 0.5 * (viewport_top + viewport_bottom);
	// Call at the beginning of frame, before drawing lines:
	initLineData();

	// Draw Block
	setLineColour(vec3(91.0f/MAX_RGB, 12.0f/MAX_RGB, 112.0f/MAX_RGB));// purple
	drawBlockEdge(block_verts[0],block_verts[1],x_mid,y_mid,x_scale,y_scale);// top front
	drawBlockEdge(block_verts[1],block_verts[2],x_mid,y_mid,x_scale,y_scale);// top right
	drawBlockEdge(block_verts[2],block_verts[3],x_mid,y_mid,x_scale,y_scale);// top back
	drawBlockEdge(block_verts[3],block_verts[0],x_mid,y_mid,x_scale,y_scale);// top left

	drawBlockEdge(block_verts[4],block_verts[5],x_mid,y_mid,x_scale,y_scale);// bottom front
	drawBlockEdge(block_verts[5],block_verts[6],x_mid,y_mid,x_scale,y_scale);// bottom right
	drawBlockEdge(block_verts[6],block_verts[7],x_mid,y_mid,x_scale,y_scale);// bottom back
	drawBlockEdge(block_verts[7],block_verts[4],x_mid,y_mid,x_scale,y_scale);// bottom left

	drawBlockEdge(block_verts[0],block_verts[4],x_mid,y_mid,x_scale,y_scale);// left front
	drawBlockEdge(block_verts[1],block_verts[5],x_mid,y_mid,x_scale,y_scale);// right front
	drawBlockEdge(block_verts[2],block_verts[6],x_mid,y_mid,x_scale,y_scale);// right back
	drawBlockEdge(block_verts[3],block_verts[7],x_mid,y_mid,x_scale,y_scale);// left back

	// Draw model Coordinate - xyz -> rgb
	setLineColour(vec3(180.0f/MAX_RGB, 0.0f/MAX_RGB, 0.0f/MAX_RGB));// red x
	drawModelCoordAxis(coord_verts[0],coord_verts[1],x_mid,y_mid,x_scale,y_scale);// local x-axis

	setLineColour(vec3(0.0f/MAX_RGB, 180.0f/MAX_RGB, 0.0f/MAX_RGB));// green y
  drawModelCoordAxis(coord_verts[0],coord_verts[2],x_mid,y_mid,x_scale,y_scale);// local y-axis

	setLineColour(vec3(0.0f/MAX_RGB, 0.0f/MAX_RGB, 180.0f/MAX_RGB));// blue z
  drawModelCoordAxis(coord_verts[0],coord_verts[3],x_mid,y_mid,x_scale,y_scale);// local z-axis

	// Draw World Coordinate - xyz -> cmy (cyan, magenta, yellow)
	setLineColour(vec3(0.0f/MAX_RGB, 180.0f/MAX_RGB, 180.0f/MAX_RGB));// cyan x
	drawWorldCoordAxis(coord_verts[0],coord_verts[1],x_mid,y_mid,x_scale,y_scale);// world x-axis

	setLineColour(vec3(180.0f/MAX_RGB, 0.0f/MAX_RGB, 180.0f/MAX_RGB));// magenta y
  drawWorldCoordAxis(coord_verts[0],coord_verts[2],x_mid,y_mid,x_scale,y_scale);// world y-axis

	setLineColour(vec3(180.0f/MAX_RGB, 180.0f/MAX_RGB, 0.0f/MAX_RGB));// yellow z
  drawWorldCoordAxis(coord_verts[0],coord_verts[3],x_mid,y_mid,x_scale,y_scale);// world z-axis


	// TODO: Draw viewport
	setLineColour(vec3(237.0f/MAX_RGB, 134.0f/MAX_RGB, 7.0f/MAX_RGB)); // orange
	// Top line
	drawLine(vec2(viewport_left, viewport_top), vec2(viewport_right, viewport_top));
	// Bottom line
	drawLine(vec2(viewport_left, viewport_bottom), vec2(viewport_right, viewport_bottom));
	// Left line
	drawLine(vec2(viewport_left, viewport_top), vec2(viewport_left, viewport_bottom));
  // Right line
	drawLine(vec2(viewport_right, viewport_top), vec2(viewport_right, viewport_bottom));
}
//----------------------------------------------------------------------------------------
void A2::rotateView(
	double xDiff
){
	// Change the rotation view
	// world axis superimposed on view
	// essentially everything rotates around the view
	if (!ImGui::IsMouseHoveringAnyWindow()) {
		mat4 R = IDENTITY;
		if(left_click){ // about x - axis
			R = IDENTITY;
			R[1][1] =  cos((xDiff) * ROTATION_VIEW_CONST);
			R[1][2] =  sin((xDiff) * ROTATION_VIEW_CONST);
			R[2][1] = -sin((xDiff) * ROTATION_VIEW_CONST);
			R[2][2] =  cos((xDiff) * ROTATION_VIEW_CONST);

			view_trans_rot = glm::inverse(R) * view_trans_rot;
		}

		if(mid_click){ // about y - axis
			R = IDENTITY;

			R[0][0] =  cos((xDiff) * ROTATION_VIEW_CONST);
			R[0][2] = -sin((xDiff) * ROTATION_VIEW_CONST);
			R[2][0] =  sin((xDiff) * ROTATION_VIEW_CONST);
			R[2][2] =  cos((xDiff) * ROTATION_VIEW_CONST);

			view_trans_rot = glm::inverse(R) * view_trans_rot;
		}

		if(right_click){ // about z - axis
			R = IDENTITY;

			R[0][0] =  cos((xDiff) * ROTATION_VIEW_CONST);
			R[0][1] =  sin((xDiff) * ROTATION_VIEW_CONST);
			R[1][0] = -sin((xDiff) * ROTATION_VIEW_CONST);
			R[1][1] =  cos((xDiff) * ROTATION_VIEW_CONST);

			view_trans_rot = glm::inverse(R) * view_trans_rot;
		}

	}
}

//----------------------------------------------------------------------------------------
void A2::translateView(
	double xDiff
){
	if (!ImGui::IsMouseHoveringAnyWindow()) {
		mat4 T = IDENTITY;
		// Change the translation view
		// world axis superimposed on view
		// essentially everything translates around the view
		if (left_click){// on the x-axis
			T = IDENTITY;
			T[3][0] = xDiff * TRANSLATE_CONST;
			view_trans_rot = T * view_trans_rot;
		}

		if (mid_click){// on the y-axis
			T = IDENTITY;
			T[3][1] = xDiff * TRANSLATE_CONST;
			view_trans_rot = T * view_trans_rot;
		}

		if(right_click){// on the z-axis
			// TODO: check movement in perspective move
			T = IDENTITY;
			T[3][2] = xDiff * TRANSLATE_CONST;
			view_trans_rot = T * view_trans_rot;
		}
 	}
}

//----------------------------------------------------------------------------------------
void A2::perspective(
	double xDiff
){
	if (!ImGui::IsMouseHoveringAnyWindow()) {
		if (left_click){// affect fovy
			if (fovy + xDiff * FOV_Y_CONST <= FOV_Y_MAX &&
			  	fovy + xDiff * FOV_Y_CONST >= FOV_Y_MIN){
			fovy += xDiff * FOV_Y_CONST;
			}
			resetProjection();
		}

		if(mid_click){// affect near
			if (near + xDiff * NEAR_FAR_CONST < far &&
			  	near + xDiff * NEAR_FAR_CONST >= NEAR_MIN){
			near += xDiff * NEAR_FAR_CONST;
			}
			resetProjection();
		}

		if(right_click){// affect far
			if (far + xDiff * NEAR_FAR_CONST <= FAR_MAX &&
			  	far + xDiff * NEAR_FAR_CONST > near){
			far += xDiff * NEAR_FAR_CONST;
			}
			resetProjection();
		}
	}

}

//----------------------------------------------------------------------------------------
void A2::rotateModel(
	double xDiff
){
	if (!ImGui::IsMouseHoveringAnyWindow()) {
		mat4 R = IDENTITY;
		// Change the rotation model
		if (left_click){// about x-axis
			R = IDENTITY;

			R[1][1] =  cos((xDiff) * ROTATION_MODEL_CONST);
			R[1][2] =  sin((xDiff) * ROTATION_MODEL_CONST);
			R[2][1] = -sin((xDiff) * ROTATION_MODEL_CONST);
			R[2][2] =  cos((xDiff) * ROTATION_MODEL_CONST);

			// rotate around world axis then apply original rotation
			// As a result rotates around its local axis
			model_trans_rot = model_trans_rot * R;
		}

		if (mid_click){// about y-axis
			R = IDENTITY;

			R[0][0] =  cos((xDiff) * ROTATION_MODEL_CONST);
			R[0][2] = -sin((xDiff) * ROTATION_MODEL_CONST);
			R[2][0] =  sin((xDiff) * ROTATION_MODEL_CONST);
			R[2][2] =  cos((xDiff) * ROTATION_MODEL_CONST);

			// rotate around world axis then apply original rotation
			// As aresult rotates around its local axis
			model_trans_rot = model_trans_rot * R;
		}

		if(right_click){// about z-axis
			R = IDENTITY;

			R[0][0] =  cos((xDiff) * ROTATION_MODEL_CONST);
			R[0][1] =  sin((xDiff) * ROTATION_MODEL_CONST);
			R[1][0] = -sin((xDiff) * ROTATION_MODEL_CONST);
			R[1][1] =  cos((xDiff) * ROTATION_MODEL_CONST);

			// rotate around world axis then apply original rotation
			// As aresult rotates around its local axis
			model_trans_rot = model_trans_rot * R;
		}
	}
}

//----------------------------------------------------------------------------------------
void A2::translateModel(
	double xDiff
){
	if (!ImGui::IsMouseHoveringAnyWindow()) {
		mat4 T = IDENTITY;
		// Change the translation model
		if (left_click){// on the x-axis
			T = IDENTITY;
			T[3][0] = xDiff * TRANSLATE_CONST;

			// translate on the world axis then apply original translation
			// As a result translates on its local axis
			model_trans_rot = model_trans_rot * T;
		}

		if (mid_click){// on the y-axis
			T = IDENTITY;
			T[3][1] = xDiff * TRANSLATE_CONST;

			// translate on the world axis then apply original translation
			// As a result translates on its local axis
			model_trans_rot = model_trans_rot * T;
		}

		if(right_click){// on the z-axis
			// TODO: check movement in perspective move
			T = IDENTITY;
			T[3][2] = xDiff * TRANSLATE_CONST;

			// translate on the world axis then apply original translation
			// As a result translates on its local axis
			model_trans_rot = model_trans_rot * T;
		}
 	}
}

//----------------------------------------------------------------------------------------
void A2::scaleModel(
	double xDiff
){
	if (!ImGui::IsMouseHoveringAnyWindow()) {
		mat4 S = IDENTITY;
		// Change the scale model
		if (left_click){// on the x-axis
			S = IDENTITY;
			if (S[0][0] + xDiff * SCALE_CONST > SCALE_MIN &&
				 	S[0][0] + xDiff * SCALE_CONST < SCALE_MAX ){
				S[0][0] += xDiff * SCALE_CONST;
			}
			model_scale = S * model_scale;
		}

		if (mid_click){// on the y-axis
			S = IDENTITY;
			if (S[1][1] + xDiff * SCALE_CONST > SCALE_MIN &&
				 	S[1][1] + xDiff * SCALE_CONST < SCALE_MAX ){
				S[1][1] += xDiff * SCALE_CONST;
			}
			model_scale = S * model_scale;
		}

		if(right_click){// on the z-axis
			S = IDENTITY;
			if (S[2][2] + xDiff * SCALE_CONST > SCALE_MIN &&
					S[2][2] + xDiff * SCALE_CONST < SCALE_MAX ){
				S[2][2] += xDiff * SCALE_CONST;
			}
			model_scale = S * model_scale;
		}
 	}

}
//----------------------------------------------------------------------------------------
void A2::alterViewport(
	double xPos,
	double yPos
){
	if (!ImGui::IsMouseHoveringAnyWindow()) {
		// Release drops point
		// Store second corner
		if (left_click){// on the x-axis
			glfwGetFramebufferSize(m_window, &window_width, &window_height);
			viewportCoords[1][0] = limitPos((float) xPos,
																			0.0f,
																			window_width);// second click x location
			viewportCoords[1][1] = limitPos((float) yPos,
																			0.0f,
																			window_height);// second click y location
			}
		}

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
		// TODO: create pointer for (int*)&mode_selection, call it mode_ptr
		ImGui::PushID( 0 );
		// Rotate View Mode
		ImGui::RadioButton( "Rotate View Mode     (O)", (int*)&mode_selection, rv_mode);

		// Translate View Mode
		ImGui::RadioButton( "Translate View Mode  (E)", (int*)&mode_selection, tv_mode) ;

		// Perspective Mode
		ImGui::RadioButton( "Perspective Mode     (P)", (int*)&mode_selection, p_mode);

		// Rotate Model Mode
		ImGui::RadioButton( "Rotate Model Mode    (R)", (int*)&mode_selection, rm_mode);

		// Translate Model Mode
		ImGui::RadioButton( "Translate Model Mode (T)", (int*)&mode_selection, tm_mode);

		// Scale Model Mode
		ImGui::RadioButton( "Scale Model Mode     (S)", (int*)&mode_selection, sm_mode);

		// Viewport Mode
		ImGui::RadioButton( "Viewport Mode        (V)", (int*)&mode_selection, v_mode);

		ImGui::PopID();

		// Reset Application
		if( ImGui::Button( "Reset Application (A)" ) ) {
			resetWorld();
		}

		// Quit Application
		if( ImGui::Button( "Quit Application (Q)" ) ) {
			glfwSetWindowShouldClose(m_window, GL_TRUE);
		}

		ImGui::Text( "Framerate: %.1f FPS", ImGui::GetIO().Framerate );
		// Display near and far plane locations from view
		ImGui::Text( "Near: %.2f, \nFar: %.2f",near, far);

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
	if (!ImGui::IsMouseHoveringAnyWindow()){
		switch(mode_selection){
			case rv_mode:
				rotateView(xPos - prev_mouse_xPos);
				eventHandled = true;
				break;
			case tv_mode:
				translateView(xPos - prev_mouse_xPos);
				eventHandled = true;
				break;
			case p_mode:
				perspective(xPos - prev_mouse_xPos);
				eventHandled = true;
				break;
			case rm_mode:
				rotateModel(xPos - prev_mouse_xPos);
				eventHandled = true;
				break;
			case tm_mode:
				translateModel(xPos - prev_mouse_xPos);
				eventHandled = true;
				break;
			case sm_mode:
				scaleModel(xPos - prev_mouse_xPos);
				eventHandled = true;
				break;
			case v_mode:
				alterViewport(xPos, yPos);
				eventHandled = true;
				break;
			default:
				break;
		}

		prev_mouse_xPos = xPos;
	}

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
	if (!ImGui::IsMouseHoveringAnyWindow()) {
		if (actions == GLFW_PRESS){// user clicked in the window
			if (button == GLFW_MOUSE_BUTTON_LEFT){ // left click
				if (mode_selection == v_mode ){
					// TODO: update the first viewport location
					// update the second to both zeros, possible L taken

					double xPos, yPos;
					glfwGetCursorPos(m_window, &xPos,&yPos);
					glfwGetFramebufferSize(m_window, &window_width, &window_height);
					// No need of Clamping Because of first click
					viewportCoords[0][0] = (float) xPos; // first click x position
					viewportCoords[0][1] = (float) yPos; // first click y position
					viewportCoords[1][0] = (float) xPos; // second click x position
					viewportCoords[1][1] = (float) yPos; // second click y position


					// cout << window_width << endl;
					// cout << window_height << endl;
					// cout << xPos <<endl;
					// cout << typeid(viewportCoords[0][0]).name() << endl;
					// cout << yPos << endl;
					// cout << typeid(viewportCoords[0][1]).name() << endl;
					// cout << ""<<endl;


				}
				left_click = true;
			}
			if (button == GLFW_MOUSE_BUTTON_MIDDLE){// middle click
				mid_click = true;
			}
			if (button == GLFW_MOUSE_BUTTON_RIGHT){ // right click
				right_click = true;
			}
		}

		if (actions == GLFW_RELEASE){// button released
			if (button == GLFW_MOUSE_BUTTON_LEFT){// left release
				left_click = false;
			}
			if (button == GLFW_MOUSE_BUTTON_MIDDLE){// middle release
				mid_click = false;
			}
			if (button == GLFW_MOUSE_BUTTON_RIGHT){// right release
				right_click = false;
			}
		}
	}

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
	// Respond to some key events.
	if( action == GLFW_PRESS ) {
		// Reset program
		if (key == GLFW_KEY_A) {
			cout << "A key pressed" << endl;
			resetWorld();
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
			mode_selection = rv_mode;
			eventHandled = true;
		}

		// Translate View Mode
		if (key == GLFW_KEY_E) {
			cout << "E key pressed" << endl;
			mode_selection = tv_mode;
			eventHandled = true;
		}

		// Perspective Mode
		if (key == GLFW_KEY_P) {
			cout << "P key pressed" << endl;
			mode_selection = p_mode;
			eventHandled = true;
		}

		// Rotate Model Mode
		if (key == GLFW_KEY_R) {
			cout << "R key pressed" << endl;
			mode_selection = rm_mode;
			eventHandled = true;
		}

		// Translate Model Mode
		if (key == GLFW_KEY_T) {
			cout << "T key pressed" << endl;
			mode_selection = tm_mode;
			eventHandled = true;
		}

		// Scale Model Mode
		if (key == GLFW_KEY_S) {
			cout << "S key pressed" << endl;
			mode_selection = sm_mode;
			eventHandled = true;
		}

		// Viewport Mode
		if (key == GLFW_KEY_V) {
			cout << "V key pressed" << endl;
			mode_selection = v_mode;
			eventHandled = true;
		}
	}

	return eventHandled;
}
