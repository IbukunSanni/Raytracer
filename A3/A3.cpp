// Termm-Fall 2020

#include "A3.hpp"
#include "scene_lua.hpp"
using namespace std;

#include "cs488-framework/GlErrorCheck.hpp"
#include "cs488-framework/MathUtils.hpp"
#include "GeometryNode.hpp"
#include "JointNode.hpp"

#include <imgui/imgui.h>

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/io.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <queue>

using namespace glm;
using namespace std;

static bool show_gui = true;

const size_t CIRCLE_PTS = 48;
static const mat4 IDENTITY = mat4(1.0f) ;// Identity for defaults
static const vec3 ZERO_VEC  = vec3(0.0f); // Zero vector for defualts
static const GLfloat TRANSLATE_CONST = 0.02f; // translation factor
static const GLfloat ROTATE_CONST = 1.1f; // rotation factor
static const GLfloat JOINT_ROTATE_CONST = 0.25f; // rotation factor
static const GLfloat MAX_RGB = 255.0f;// MAXIMUM color of RGB #FF in decimal

//----------------------------------------------------------------------------------------
// Constructor
A3::A3(const std::string & luaSceneFile)
	: m_luaSceneFile(luaSceneFile),
	  m_positionAttribLocation(0),
	  m_normalAttribLocation(0),
	  m_vao_meshData(0),
	  m_vbo_vertexPositions(0),
	  m_vbo_vertexNormals(0),
	  m_vao_arcCircle(0),
	  m_vbo_arcCircle(0),
	  trackBall(false),
	  zBuffer(true),
	  backFaceCulling(false),
	  frontFaceCulling(false),
	  inter_mode_selection(position_mode),
	  localRot(1.0f),
	  viewTransRot(1.0f),
	  left_click(false),
	  mid_click(false),
	  right_click(false),
	  picking(false)
{

}

//----------------------------------------------------------------------------------------
// Destructor
A3::~A3()
{

}
//----------------------------------------------------------------------------------------
// Helper function definitions
// debugPrint() definition
template <class T>
void A3:: dbgPrint(T statement){
	cout << "debug-- ";
	cout << statement << endl;
}

//----------------------------------------------------------------------------------------
/*
 * Called once, at program start.
 */
void A3::init()
{	// TODO: change background color
	// Set the background colour.
	glClearColor(0.85, 0.85, 0.85, 1.0);

	createShaderProgram();

	glGenVertexArrays(1, &m_vao_arcCircle);
	glGenVertexArrays(1, &m_vao_meshData);
	enableVertexShaderInputSlots();

	processLuaSceneFile(m_luaSceneFile);

	// Load and decode all .obj files at once here.  You may add additional .obj files to
	// this list in order to support rendering additional mesh types.  All vertex
	// positions, and normals will be extracted and stored within the MeshConsolidator
	// class.
	unique_ptr<MeshConsolidator> meshConsolidator (new MeshConsolidator{
			getAssetFilePath("cube.obj"),
			getAssetFilePath("sphere.obj"),
			getAssetFilePath("suzanne.obj")
	});


	// Acquire the BatchInfoMap from the MeshConsolidator.
	meshConsolidator->getBatchInfoMap(m_batchInfoMap);

	// Take all vertex data within the MeshConsolidator and upload it to VBOs on the GPU.
	uploadVertexDataToVbos(*meshConsolidator);

	mapVboDataToVertexShaderInputLocations();

	initPerspectiveMatrix();

	initViewMatrix();

	initLightSources();


	// Exiting the current scope calls delete automatically on meshConsolidator freeing
	// all vertex data resources.  This is fine since we already copied this data to
	// VBOs on the GPU.  We have no use for storing vertex data on the CPU side beyond
	// this point.
}

//----------------------------------------------------------------------------------------
void A3::processLuaSceneFile(const std::string & filename) {
	// This version of the code treats the Lua file as an Asset,
	// so that you'd launch the program with just the filename
	// of a puppet in the Assets/ directory.
	// std::string assetFilePath = getAssetFilePath(filename.c_str());
	// m_rootNode = std::shared_ptr<SceneNode>(import_lua(assetFilePath));

	// This version of the code treats the main program argument
	// as a straightforward pathname.
	m_rootNode = std::shared_ptr<SceneNode>(import_lua(filename));
	if (!m_rootNode) {
		std::cerr << "Could Not Open " << filename << std::endl;
	}
}

//----------------------------------------------------------------------------------------
void A3::createShaderProgram()
{
	m_shader.generateProgramObject();
	m_shader.attachVertexShader( getAssetFilePath("VertexShader.vs").c_str() );
	m_shader.attachFragmentShader( getAssetFilePath("FragmentShader.fs").c_str() );
	m_shader.link();

	m_shader_arcCircle.generateProgramObject();
	m_shader_arcCircle.attachVertexShader( getAssetFilePath("arc_VertexShader.vs").c_str() );
	m_shader_arcCircle.attachFragmentShader( getAssetFilePath("arc_FragmentShader.fs").c_str() );
	m_shader_arcCircle.link();
}

//----------------------------------------------------------------------------------------
void A3::enableVertexShaderInputSlots()
{
	//-- Enable input slots for m_vao_meshData:
	{
		glBindVertexArray(m_vao_meshData);

		// Enable the vertex shader attribute location for "position" when rendering.
		m_positionAttribLocation = m_shader.getAttribLocation("position");
		glEnableVertexAttribArray(m_positionAttribLocation);

		// Enable the vertex shader attribute location for "normal" when rendering.
		m_normalAttribLocation = m_shader.getAttribLocation("normal");
		glEnableVertexAttribArray(m_normalAttribLocation);

		CHECK_GL_ERRORS;
	}


	//-- Enable input slots for m_vao_arcCircle:
	{
		glBindVertexArray(m_vao_arcCircle);

		// Enable the vertex shader attribute location for "position" when rendering.
		m_arc_positionAttribLocation = m_shader_arcCircle.getAttribLocation("position");
		glEnableVertexAttribArray(m_arc_positionAttribLocation);

		CHECK_GL_ERRORS;
	}

	// Restore defaults
	glBindVertexArray(0);
}

//----------------------------------------------------------------------------------------
void A3::uploadVertexDataToVbos (
		const MeshConsolidator & meshConsolidator
) {
	// Generate VBO to store all vertex position data
	{
		glGenBuffers(1, &m_vbo_vertexPositions);

		glBindBuffer(GL_ARRAY_BUFFER, m_vbo_vertexPositions);

		glBufferData(GL_ARRAY_BUFFER, meshConsolidator.getNumVertexPositionBytes(),
				meshConsolidator.getVertexPositionDataPtr(), GL_STATIC_DRAW);

		glBindBuffer(GL_ARRAY_BUFFER, 0);
		CHECK_GL_ERRORS;
	}

	// Generate VBO to store all vertex normal data
	{
		glGenBuffers(1, &m_vbo_vertexNormals);

		glBindBuffer(GL_ARRAY_BUFFER, m_vbo_vertexNormals);

		glBufferData(GL_ARRAY_BUFFER, meshConsolidator.getNumVertexNormalBytes(),
				meshConsolidator.getVertexNormalDataPtr(), GL_STATIC_DRAW);

		glBindBuffer(GL_ARRAY_BUFFER, 0);
		CHECK_GL_ERRORS;
	}

	// Generate VBO to store the trackball circle.
	{
		glGenBuffers( 1, &m_vbo_arcCircle );
		glBindBuffer( GL_ARRAY_BUFFER, m_vbo_arcCircle );

		float *pts = new float[ 2 * CIRCLE_PTS ];
		for( size_t idx = 0; idx < CIRCLE_PTS; ++idx ) {
			float ang = 2.0 * M_PI * float(idx) / CIRCLE_PTS;
			pts[2*idx] = cos( ang );
			pts[2*idx+1] = sin( ang );
		}

		glBufferData(GL_ARRAY_BUFFER, 2*CIRCLE_PTS*sizeof(float), pts, GL_STATIC_DRAW);

		glBindBuffer(GL_ARRAY_BUFFER, 0);
		CHECK_GL_ERRORS;
	}
}

//----------------------------------------------------------------------------------------
void A3::mapVboDataToVertexShaderInputLocations()
{
	// Bind VAO in order to record the data mapping.
	glBindVertexArray(m_vao_meshData);

	// Tell GL how to map data from the vertex buffer "m_vbo_vertexPositions" into the
	// "position" vertex attribute location for any bound vertex shader program.
	glBindBuffer(GL_ARRAY_BUFFER, m_vbo_vertexPositions);
	glVertexAttribPointer(m_positionAttribLocation, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

	// Tell GL how to map data from the vertex buffer "m_vbo_vertexNormals" into the
	// "normal" vertex attribute location for any bound vertex shader program.
	glBindBuffer(GL_ARRAY_BUFFER, m_vbo_vertexNormals);
	glVertexAttribPointer(m_normalAttribLocation, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

	//-- Unbind target, and restore default values:
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	CHECK_GL_ERRORS;

	// Bind VAO in order to record the data mapping.
	glBindVertexArray(m_vao_arcCircle);

	// Tell GL how to map data from the vertex buffer "m_vbo_arcCircle" into the
	// "position" vertex attribute location for any bound vertex shader program.
	glBindBuffer(GL_ARRAY_BUFFER, m_vbo_arcCircle);
	glVertexAttribPointer(m_arc_positionAttribLocation, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

	//-- Unbind target, and restore default values:
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	CHECK_GL_ERRORS;
}

//----------------------------------------------------------------------------------------
void A3::initPerspectiveMatrix()
{
	float aspect = ((float)m_windowWidth) / m_windowHeight;
	m_perpsective = glm::perspective(degreesToRadians(60.0f), aspect, 0.1f, 100.0f);
}


//----------------------------------------------------------------------------------------
void A3::initViewMatrix() {
	m_view = glm::lookAt(vec3(0.0f, 0.0f, 10.0f), vec3(0.0f, 0.0f, -1.0f),
			vec3(0.0f, 1.0f, 0.0f));
}

//----------------------------------------------------------------------------------------
void A3::initLightSources() {
	// World-space position
	// TODO: change position and intensity
	m_light.position = vec3(-0.5f, 0.0f, -1.0f);
	m_light.rgbIntensity = vec3(0.5f); // light
}

//----------------------------------------------------------------------------------------
void A3::uploadCommonSceneUniforms() {
	m_shader.enable();
	{
		//-- Set Perpsective matrix uniform for the scene:
		GLint location = m_shader.getUniformLocation("Perspective");
		glUniformMatrix4fv(location, 1, GL_FALSE, value_ptr(m_perpsective));
		CHECK_GL_ERRORS;

		location = m_shader.getUniformLocation("picking");
		glUniform1i( location, picking ? 1 : 0 );
		// TODO: add changes for picking

		if(!picking){
			//-- Set LightSource uniform for the scene:
			{
				location = m_shader.getUniformLocation("light.position");
				glUniform3fv(location, 1, value_ptr(m_light.position));
				location = m_shader.getUniformLocation("light.rgbIntensity");
				glUniform3fv(location, 1, value_ptr(m_light.rgbIntensity));
				CHECK_GL_ERRORS;
			}

			//-- Set background light ambient intensity
			{
				location = m_shader.getUniformLocation("ambientIntensity");
				vec3 ambientIntensity(0.25f);
				glUniform3fv(location, 1, value_ptr(ambientIntensity));
				CHECK_GL_ERRORS;
			}
		}
	}
	m_shader.disable();
}

//----------------------------------------------------------------------------------------
/*
 * Called once per frame, before guiLogic().
 */
void A3::appLogic()
{
	// Place per frame, application logic here ...

	uploadCommonSceneUniforms();
}

//----------------------------------------------------------------------------------------
/*
 * Called once per frame, after appLogic(), but before the draw() method.
 */
void A3::guiLogic()
{
	if( !show_gui ) {
		return;
	}

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


		// Add more gui elements here here ...
		if(ImGui::BeginMainMenuBar()){
			if(ImGui::BeginMenu("Application")){
				if(ImGui::MenuItem("(I) - Reset Position")){
					resetPosition();
				}

				if(ImGui::MenuItem("(O) - Reset Orientation ")){
					resetRotation();
				}

				if(ImGui::MenuItem("(S) - Reset Joints")){
					// TODO: add resetJoints()
				}

				if(ImGui::MenuItem("(A) - Reset All")){
					resetWorld();
				}

				if(ImGui::MenuItem("(Q) - Quit Application")){
					glfwSetWindowShouldClose(m_window, GL_TRUE);
				}
			
				ImGui::EndMenu();

			}

			if(ImGui::BeginMenu("Edit")){
				if(ImGui::MenuItem("(U) - Undo")){
					// TODO: add functionality
				}

				if(ImGui::MenuItem("(R) - Redo")){
					// TODO: add functionality
				}
				
				ImGui::EndMenu();
				
			}

			if(ImGui::BeginMenu("Options")){
				ImGui::Checkbox("(C) - Circle", &trackBall);
				ImGui::Checkbox("(Z) - Z-buffer", &zBuffer);
				ImGui::Checkbox("(B) - Backface Culling", &backFaceCulling);
				ImGui::Checkbox("(F) - Frontface Culling", &frontFaceCulling);

				ImGui::EndMenu();	
			}

			ImGui::EndMainMenuBar();
		}

		ImGui::RadioButton( "Position/Orientation mode (P)", (int*)&inter_mode_selection, position_mode);
		ImGui::RadioButton( "Joints mode               (J)", (int*)&inter_mode_selection, joints_mode);

		ImGui::Text( "Framerate: %.1f FPS", ImGui::GetIO().Framerate );

	ImGui::End();
}

//----------------------------------------------------------------------------------------
// Update mesh specific shader uniforms:
static void updateShaderUniforms(
		const ShaderProgram & shader,
		const GeometryNode & node,
		const glm::mat4 & viewMatrix,
		const glm::mat4 & nodeTransformation,
		const glm::mat4 & viewTransformation,
		bool picking
) {
	shader.enable();
	{
		//-- Set ModelView matrix:
		GLint location = shader.getUniformLocation("ModelView");
		mat4 modelView = viewTransformation * viewMatrix * nodeTransformation * node.trans;
		glUniformMatrix4fv(location, 1, GL_FALSE, value_ptr(modelView));
		CHECK_GL_ERRORS;
		if (picking){
			unsigned int id = node.m_nodeId;
			cout<< "MeshID: "<< id << endl;
			GLfloat r = float(id &0xff)/ MAX_RGB;
			GLfloat g = float((id>>8) &0xff)/ MAX_RGB;
			GLfloat b = float((id>>16) &0xff)/ MAX_RGB;

			location = shader.getUniformLocation("material.kd");
			glUniform3f(location, r, g, b);
			CHECK_GL_ERRORS;

		}else{
			//-- Set NormMatrix:
			location = shader.getUniformLocation("NormalMatrix");
			mat3 normalMatrix = glm::transpose(glm::inverse(mat3(modelView)));
			glUniformMatrix3fv(location, 1, GL_FALSE, value_ptr(normalMatrix));
			CHECK_GL_ERRORS;

			//-- Set Material values:
			// Handle diffuse reflection co-efficients
			location = shader.getUniformLocation("material.kd");
			vec3 kd = node.material.kd;
			if (node.isSelected){ // give slected materials a different value
				kd = {0.0,0.2,0.2};
			}
			glUniform3fv(location, 1, value_ptr(kd));
			CHECK_GL_ERRORS;

			// TODO: Handle co-efficients

			// // TODO: Handle specular reflection co-efficients
			// location = shader.getUniformLocation("material.ks");
			// vec3 ks = node.material.ks;
			// glUniform3fv(location, 1, value_ptr(ks));
			// CHECK_GL_ERRORS;

			// // TODO: Handle phong co-efficients
			// location = shader.getUniformLocation("material.shininess");
			// float p = node.material.shininess;
			// glUniform1f(location,node.material.shininess);
			// CHECK_GL_ERRORS;

		}

		
	}
	shader.disable();

}
//----------------------------------------------------------------------------------------
// TODO: Add new functions  below updateShaderUniforms()


//----------------------------------------------------------------------------------------
void A3::findSelectedNodes(std::shared_ptr<SceneNode>  root){
	if (root == nullptr){
		return;
	}
	SceneNode* rootPtr = root.get();
	// Traverse Scene using BFS
	queue<SceneNode*> q;
	q.push(rootPtr);
	
	while(!q.empty()){
		SceneNode* node = q.front();
		q.pop();
		if (node->isSelected){
			cout<< node->m_name;
			cout<< " isSelected"<<endl;
		}

		for (SceneNode * child : node->children) {
			q.push(child);		
		}
	
	}
}

//----------------------------------------------------------------------------------------
void A3::traverseNodes(std::shared_ptr<SceneNode>  root){
	if (root == nullptr){
		return;
	}
	SceneNode* rootPtr = root.get();
	// Traverse Scene using BFS
	queue<SceneNode*> q;
	q.push(rootPtr);
	
	while(!q.empty()){
		SceneNode* node = q.front();
		q.pop();
		cout << node->m_name;
		cout << " ";
		cout << node->isSelected;
		cout << " selection status"<<endl;

		for (SceneNode * child : node->children) {
			q.push(child);	
		}
	
	}
}

//----------------------------------------------------------------------------------------
void A3::selectJointNodeByMeshId(std::shared_ptr<SceneNode>  root, unsigned int  meshNodeId ){
	if (root == nullptr){
		return;
	}
	
	SceneNode* rootPtr = root.get();
	// Traverse Scene using BFS
	queue<SceneNode*> q;
	q.push(rootPtr);
	
	while(!q.empty()){
		SceneNode* node = q.front();
		q.pop();
		bool isJointSelect(false);
		for (SceneNode * child : node->children) {
			if(child->m_nodeType == NodeType::GeometryNode){
				if (child->m_nodeId == meshNodeId ){
					child->isSelected = !(child->isSelected);
				}
				isJointSelect = child->isSelected || isJointSelect;	
			}
			q.push(child);
		}

		if (node->m_nodeType == NodeType::JointNode){
				node->isSelected = isJointSelect;
		}

		if (node->isSelected){
			cout<< node->m_name;
			cout<< " is selected"<<endl;
		}
	
	}


}
//----------------------------------------------------------------------------------------
void A3::rotateSelectedJoints(std::shared_ptr<SceneNode>  root,double yDiff){
	if (root == nullptr){
		return;
	}
	SceneNode* rootPtr = root.get();
	// Traverse Scene using BFS
	queue<SceneNode*> q;
	q.push(rootPtr);
	
	while(!q.empty()){
		SceneNode* node = q.front();
		q.pop();
		if (node->isSelected and node->m_nodeType == NodeType::JointNode){
			JointNode * jointNode = static_cast < JointNode *>(node);
			double initVal   = jointNode->m_joint_y.init;
			double currAngle = initVal + yDiff * JOINT_ROTATE_CONST;
			double maxVal    = jointNode->m_joint_y.max;
			double minVal    = jointNode->m_joint_y.min;
			currAngle = clamp(currAngle,minVal,maxVal);
			
			mat4 T = jointNode->get_transform();
			jointNode->translate(vec3(-T[3].x,-T[3].y, -T[3].z));// remove translation using negated translation

			//Perform rotation
			jointNode->rotate('y', currAngle - initVal);// Takes degrees
			jointNode->m_joint_y.init = currAngle; // TODO: how to update
			jointNode->translate(vec3(T[3].x,T[3].y, T[3].z));// restore translation using original translation

		}

		for (SceneNode * child : node->children) {
			q.push(child);	
		}
	
	}

}
//----------------------------------------------------------------------------------------
void A3::selectMeshbyPicking(double xPos, double yPos){
	picking = true; // make picking true to draw flase colors for storing Id info
	uploadCommonSceneUniforms();
	glClearColor(1.0, 1.0, 1.0, 1.0 );
	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
	glClearColor(0.35, 0.35, 0.35, 1.0);

	draw();
	CHECK_GL_ERRORS;

	// Ugly -- FB coordinates might be different than Window coordinates
	// (e.g., on a retina display).  Must compensate.
	xPos *= double(m_framebufferWidth) / double(m_windowWidth);
	// WTF, don't know why I have to measure y relative to the bottom of
	// the window in this case.
	yPos = m_windowHeight - yPos;
	yPos *= double(m_framebufferHeight) / double(m_windowHeight);

	GLubyte buffer[ 4 ] = { 0, 0, 0, 0 };
	// A bit ugly -- don't want to swap the just-drawn false colours
	// to the screen, so read from the back buffer.
	glReadBuffer( GL_BACK );
	// Actually read the pixel at the mouse location.
	glReadPixels( int(xPos), int(yPos), 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, buffer );
	CHECK_GL_ERRORS;

	// Reassemble the object ID.
	unsigned int meshNodeId = buffer[0] + (buffer[1] << 8) + (buffer[2] << 16);

	picking = false; // make picking false to draw normal way
	CHECK_GL_ERRORS;
	glClearColor(0.85, 0.85, 0.85, 1.0);

	//TODO: find JointNode by MeshId
	selectJointNodeByMeshId(m_rootNode,meshNodeId);

}

//----------------------------------------------------------------------------------------
void A3::handlePosition(double xPos, double yPos){
	double xDiff = xPos - prev_mouse_xPos;
	double yDiff = yPos - prev_mouse_yPos;
	if (!ImGui::IsMouseHoveringAnyWindow()) {
		mat4 T;
		if(left_click){ // change x and y translations globally
			T = translate(IDENTITY, vec3(xDiff * TRANSLATE_CONST , -yDiff * TRANSLATE_CONST,0.0f ));
			viewTransRot = T * viewTransRot;

		}

		if (mid_click){// change z translations globally
			T = translate(IDENTITY, vec3(0.0f,0.0f,yDiff * TRANSLATE_CONST));
			viewTransRot = T * viewTransRot;

		}

		if (right_click){// rotate about the z axis for view when outside
			double ball_x0_center = m_framebufferWidth / 2.0f;
			double ball_y0_center = m_framebufferHeight / 2.0f;
			// Radius is the smaller of the distance to the border of the frame
			double ball_radius = std::min((m_framebufferWidth  - ball_x0_center) / 2.0f,
										  (m_framebufferHeight - ball_y0_center) / 2.0f);
			// New vector for based on curent mouse position
			vec3 newVecTrackBall((xPos - ball_x0_center)/ ball_radius,
								 (yPos - ball_y0_center)/ ball_radius,
								 0);
			// Subtract sqaure of cursor distance from 1 and store in z
			newVecTrackBall.z = (1.0f - (newVecTrackBall.x * newVecTrackBall.x) - (newVecTrackBall.y * newVecTrackBall.y));
			
			// Prev vector for based on previous mouse position
			vec3 prevVecTrackBall((prev_mouse_xPos - ball_x0_center)/ ball_radius,
								  (prev_mouse_yPos - ball_y0_center)/ ball_radius,
								  0);
			// Subtract sqaure of cursor distance from 1 and store in z
			prevVecTrackBall.z = (1.0f - (prevVecTrackBall.x * prevVecTrackBall.x) - (prevVecTrackBall.y * prevVecTrackBall.y));
			
			// cross vector to generate rotation axis
			vec3 crossVecTrackball;

			// TODO: get rotation matrix
			mat4 R = IDENTITY;
			// angle between vectors
			float angle_rads;

			// Checks whether newVecTrackBall and prevVecTrackball are inside trackball 
			// and makes changes accordingly to the vector
			if (newVecTrackBall.z < 0.0f and newVecTrackBall.z < 0.0f ){ // mouse is outside the trackball, rotation about z-axis

				// Changes for current mouse position
				newVecTrackBall.x = newVecTrackBall.x / sqrt(1.0f - newVecTrackBall.z);
				newVecTrackBall.y = newVecTrackBall.y / sqrt(1.0f - newVecTrackBall.z);
				newVecTrackBall.z = 0.0f;

				// Changes for previous mouse position
				prevVecTrackBall.x = prevVecTrackBall.x / sqrt(1.0f - prevVecTrackBall.z);
				prevVecTrackBall.y = prevVecTrackBall.y / sqrt(1.0f - prevVecTrackBall.z);
				prevVecTrackBall.z = 0.0f;

				/* Generate rotation vector by calculating cross product:
				 * 
				 * fOldVec x fNewVec.
				 * newVecTrackBall x prevVecTrackball
				 * 
				 * The rotation vector is the axis of rotation
				 * and is non-unit length since the length of a crossproduct
				 * is related to the angle between fOldVec and fNewVec which we need
				 * in order to perform the rotation.
				 */
				
				crossVecTrackball = cross( newVecTrackBall, prevVecTrackBall);
				angle_rads = acos(dot(newVecTrackBall,prevVecTrackBall));

				if (crossVecTrackball != ZERO_VEC){
					R = rotate(IDENTITY,angle_rads,crossVecTrackball);
					viewTransRot = R * viewTransRot;
				}
			// TODO: Exclude to only inside for new and prev
			}else { // mouse is inside the trackball, rotation about local origin
				newVecTrackBall.z = sqrt(newVecTrackBall.z);
				prevVecTrackBall.z = sqrt(prevVecTrackBall.z);

				/* Generate rotation vector by calculating cross product:
				 * 
				 * fOldVec x fNewVec.
				 * newVecTrackBall x prevVecTrackball
				 * 
				 * The rotation vector is the axis of rotation
				 * and is non-unit length since the length of a crossproduct
				 * is related to the angle between fOldVec and fNewVec which we need
				 * in order to perform the rotation.
				 */
				
				crossVecTrackball = cross( prevVecTrackBall,newVecTrackBall);
				angle_rads = acos(dot(newVecTrackBall,prevVecTrackBall));

				if (crossVecTrackball != ZERO_VEC){
					R = rotate(IDENTITY,angle_rads * ROTATE_CONST,crossVecTrackball);
					localRot = R * localRot ;
				}

			}
	
		}
	}

}

//----------------------------------------------------------------------------------------
void A3::handleJoints(double xPos, double yPos){
	//TODO: finish handleJoints()
	double xDiff = xPos - prev_mouse_xPos;
	double yDiff = yPos - prev_mouse_yPos;
	if (!ImGui::IsMouseHoveringAnyWindow()) {
		// selects/deselects individual joints handled
		// earlier in mousebutton input 
		// no

		if (mid_click){// change the angles of selected joints 
			rotateSelectedJoints(m_rootNode,yDiff);
		
		}

		if (right_click){// rotate the head left or right ONLY FOR THE HEAD

		}
	}
	
}
//----------------------------------------------------------------------------------------
void A3::resetPosition(){
	viewTransRot[3].x = 0;
	viewTransRot[3].y = 0;
	viewTransRot[3].z = 0;
}

//----------------------------------------------------------------------------------------
void A3::resetRotation(){
	localRot = IDENTITY;
	// Store prior translation
	vec3 vecTranslation (viewTransRot[3].x,
						 viewTransRot[3].y,
						 viewTransRot[3].z);
	// Reset matrix
	viewTransRot = IDENTITY;
	// Add prior tranlsation
	viewTransRot[3].x = vecTranslation.x;
	viewTransRot[3].y = vecTranslation.y;
	viewTransRot[3].z = vecTranslation.z;
}
// TODO: add resetJoints()

//----------------------------------------------------------------------------------------
void A3::resetWorld(){
	resetPosition();
	resetRotation();
	// TODO: add resetJoint()
}


//----------------------------------------------------------------------------------------
/*
 * Called once per frame, after guiLogic().
 */
void A3::draw() {

	// specifiy options method drawing scene
	if(zBuffer){
		glEnable( GL_DEPTH_TEST );
	}

	if (backFaceCulling && frontFaceCulling){
		glEnable(GL_CULL_FACE);
		glCullFace(GL_FRONT_AND_BACK);
	}
	else if (backFaceCulling){
		glEnable(GL_CULL_FACE);
		glCullFace(GL_BACK);
	}
	else if (frontFaceCulling){
		glEnable(GL_CULL_FACE);
		glCullFace(GL_FRONT);
	}
	
	// Draws the scene
	renderSceneGraph(*m_rootNode);

	// disable options method after drawing scene
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	
	if (trackBall){
		renderArcCircle();
	}
	
}

void A3::renderSceneNode(const SceneNode & root, mat4 nodeTransformation){
	if (&root == nullptr){
		return;
	}

	for (const SceneNode * node : root.children) {

		if (node->m_nodeType == NodeType::GeometryNode){
			const GeometryNode * geometryNode = static_cast<const GeometryNode *>(node);
			// TODO: correct globalTrans, it is acting as view Transformation which could be wrong
			updateShaderUniforms(m_shader, *geometryNode, m_view, nodeTransformation * root.get_transform(),viewTransRot,picking);

			// Get the BatchInfo corresponding to the GeometryNode's unique MeshId.
			BatchInfo batchInfo = m_batchInfoMap[geometryNode->meshId];

			//-- Now render the mesh:
			m_shader.enable();
			glDrawArrays(GL_TRIANGLES, batchInfo.startIndex, batchInfo.numIndices);
			m_shader.disable();
		}

		renderSceneNode( *node, nodeTransformation * root.get_transform());
	}

}

//----------------------------------------------------------------------------------------
void A3::renderSceneGraph(const SceneNode & root) {

	// Bind the VAO once here, and reuse for all GeometryNode rendering below.
	glBindVertexArray(m_vao_meshData);

	// This is emphatically *not* how you should be drawing the scene graph in
	// your final implementation.  This is a non-hierarchical demonstration
	// in which we assume that there is a list of GeometryNodes living directly
	// underneath the root node, and that we can draw them in a loop.  It's
	// just enough to demonstrate how to get geometry and materials out of
	// a GeometryNode and onto the screen.

	// You'll want to turn this into recursive code that walks over the tree.
	// You can do that by putting a method in SceneNode, overridden in its
	// subclasses, that renders the subtree rooted at every node.  Or you
	// could put a set of mutually recursive functions in this class, which
	// walk down the tree from nodes of different types.
	
	// Implemented recursive call for rendering scene 
	if (root.children.size() != 0){// check if root has children
		// Call recursively for each nodes mesh
		// localRot for origin rotation
		// TODO: correct localRot, it is correct -ish remeber to transorm the root
		mat4 torsoTransformation = root.get_transform();
		localRot = torsoTransformation * localRot * inverse(torsoTransformation);
		renderSceneNode(root,localRot);
	}
	

	glBindVertexArray(0);
	CHECK_GL_ERRORS;
}

//----------------------------------------------------------------------------------------
// Draw the trackball circle.
void A3::renderArcCircle() {
	glBindVertexArray(m_vao_arcCircle);

	m_shader_arcCircle.enable();
		GLint m_location = m_shader_arcCircle.getUniformLocation( "M" );
		float aspect = float(m_framebufferWidth)/float(m_framebufferHeight);
		glm::mat4 M;
		if( aspect > 1.0 ) {
			M = glm::scale( glm::mat4(), glm::vec3( 0.5/aspect, 0.5, 1.0 ) );
		} else {
			M = glm::scale( glm::mat4(), glm::vec3( 0.5, 0.5*aspect, 1.0 ) );
		}
		glUniformMatrix4fv( m_location, 1, GL_FALSE, value_ptr( M ) );
		glDrawArrays( GL_LINE_LOOP, 0, CIRCLE_PTS );
	m_shader_arcCircle.disable();

	glBindVertexArray(0);
	CHECK_GL_ERRORS;
}

//----------------------------------------------------------------------------------------
/*
 * Called once, after program is signaled to terminate.
 */
void A3::cleanup()
{

}

//----------------------------------------------------------------------------------------
/*
 * Event handler.  Handles cursor entering the window area events.
 */
bool A3::cursorEnterWindowEvent (
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
bool A3::mouseMoveEvent (
		double xPos,
		double yPos
) {
	bool eventHandled(false);

	// Fill in with event handling code...
	if(!ImGui::IsMouseHoveringAnyWindow()){
		switch(inter_mode_selection){
			case position_mode:
				handlePosition(xPos, yPos);
				eventHandled = true;
				break;
			case joints_mode:
				handleJoints(xPos, yPos);
				eventHandled = true;
				break;
			default:
				break;

		}
		prev_mouse_xPos = xPos;
		prev_mouse_yPos = yPos;

	}

	return eventHandled;
}

//----------------------------------------------------------------------------------------
/*
 * Event handler.  Handles mouse button events.
 */
bool A3::mouseButtonInputEvent (
		int button,
		int actions,
		int mods
) {
	bool eventHandled(false);

	// Fill in with event handling code...
	if (!ImGui::IsMouseHoveringAnyWindow()) {
		// TODO: Add joints interaction made details
		if (actions == GLFW_PRESS){// user clicked in the window
			if (button == GLFW_MOUSE_BUTTON_LEFT){ // left click
				left_click = true;
				if (inter_mode_selection == joints_mode){
					double xPos,yPos;
					glfwGetCursorPos( m_window, &xPos, &yPos );
					selectMeshbyPicking(xPos,yPos);
					
				}
				eventHandled = true;

			}
			if (button == GLFW_MOUSE_BUTTON_MIDDLE){// middle click
				mid_click = true;
				
				eventHandled = true;
			}
			if (button == GLFW_MOUSE_BUTTON_RIGHT){ // right click
				right_click = true;
				eventHandled = true;
			}
		}

		if (actions == GLFW_RELEASE){// button released
			if (button == GLFW_MOUSE_BUTTON_LEFT){// left release
				left_click = false;
				eventHandled = true;
			}
			if (button == GLFW_MOUSE_BUTTON_MIDDLE){// middle release
				mid_click = false;
				eventHandled = true;
			}
			if (button == GLFW_MOUSE_BUTTON_RIGHT){// right release
				right_click = false;
				eventHandled = true;
			}
		}
	}

	return eventHandled;
}

//----------------------------------------------------------------------------------------
/*
 * Event handler.  Handles mouse scroll wheel events.
 */
bool A3::mouseScrollEvent (
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
bool A3::windowResizeEvent (
		int width,
		int height
) {
	bool eventHandled(false);
	initPerspectiveMatrix();
	return eventHandled;
}

//----------------------------------------------------------------------------------------
/*
 * Event handler.  Handles key input events.
 */
bool A3::keyInputEvent (
		int key,
		int action,
		int mods
) {
	bool eventHandled(false);
	// Fill in with event handling code...

	if( action == GLFW_PRESS ) {
		// Show GUI
		if( key == GLFW_KEY_M ) {
			dbgPrint("M pressed");
			show_gui = !show_gui;
			eventHandled = true;
		}

		// Reset Position
		if( key == GLFW_KEY_I ) {
			dbgPrint("I pressed");
			resetPosition();
			eventHandled = true;
		}

		// Reset Orientation
		if( key == GLFW_KEY_O) {
			dbgPrint("O pressed");
			resetRotation();
			eventHandled = true;
		}

		// Reset Joints
		if (key == GLFW_KEY_S) {
			dbgPrint("S pressed");
			// TODO: add resetJoints()
			eventHandled = true;
		}

		// Reset All
		if (key == GLFW_KEY_A) {
			dbgPrint("A pressed");
			resetWorld();
			eventHandled = true;
		}

		// Quit program
		if (key == GLFW_KEY_Q) {
			dbgPrint("Q pressed");
			glfwSetWindowShouldClose(m_window, GL_TRUE);
			eventHandled = true;
		}

		// Undo transformations
		if (key == GLFW_KEY_U) {
			dbgPrint("U pressed");
			// TODO: add functionality
			eventHandled = true;
		}

		// Redo transformations
		if (key == GLFW_KEY_R) {
			dbgPrint("R pressed");
			// TODO: add functionality
			eventHandled = true;
		}

		// Draw circle for trackball
		if (key == GLFW_KEY_C) {
			dbgPrint("C pressed");
			trackBall = !trackBall;
			eventHandled = true;
		}

		// Use Z-buffer
		if (key == GLFW_KEY_Z) {
			dbgPrint("Z pressed");
			zBuffer = !zBuffer;
			eventHandled = true;
		}
		// Use Backface culling
		if (key == GLFW_KEY_B) {
			dbgPrint("B pressed");
			backFaceCulling = !backFaceCulling;
			eventHandled = true;
		}

		// Use Frontface culling
		if (key == GLFW_KEY_F) {
			dbgPrint("F pressed");
			frontFaceCulling = !frontFaceCulling;
			eventHandled = true;
		}

		// Use position/orientation
		// for Translation and rotation
		if (key == GLFW_KEY_P) {
			dbgPrint("P pressed");
			inter_mode_selection = position_mode;
			eventHandled = true;
		}

		// Use joints(basically pose mode)
		if (key == GLFW_KEY_J) {
			dbgPrint("J pressed");
			inter_mode_selection = joints_mode;
			eventHandled = true;
		}

		// Debug print a specific value
		if (key == GLFW_KEY_D) {
			dbgPrint("D pressed for debug");
			// TODO: debug print specific values
			dbgPrint(inter_mode_selection);
			eventHandled = true;
		}

		// TODO: remove tests
		if (key == GLFW_KEY_G) {
			dbgPrint("G pressed for test");
			traverseNodes(m_rootNode );// prints all nodes
			eventHandled = true;
		}

		if (key == GLFW_KEY_H) {
			dbgPrint("H pressed for test");
			findSelectedNodes(m_rootNode);// finds Selected node
			eventHandled = true;
		}

		if (key == GLFW_KEY_K) {
			dbgPrint("K pressed for test");
			selectJointNodeByMeshId(m_rootNode,0);// selects/deselects
			eventHandled = true;
		}

		if (key == GLFW_KEY_L) {
			dbgPrint("L pressed for test");
			selectJointNodeByMeshId(m_rootNode,0);// selects/deselects
			eventHandled = true;
		}
	}

	return eventHandled;
}
