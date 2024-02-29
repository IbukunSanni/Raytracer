// Termm-Fall 2020

#pragma once

#include "cs488-framework/CS488Window.hpp"
#include "cs488-framework/OpenGLImport.hpp"
#include "cs488-framework/ShaderProgram.hpp"
#include "cs488-framework/MeshConsolidator.hpp"

#include "SceneNode.hpp"

#include <glm/glm.hpp>
#include <memory>

using namespace glm;
using namespace std;

struct LightSource {
	glm::vec3 position;
	glm::vec3 rgbIntensity;
};


class A3 : public CS488Window {
public:
	A3(const std::string & luaSceneFile);
	virtual ~A3();

protected:
	virtual void init() override;
	virtual void appLogic() override;
	virtual void guiLogic() override;
	virtual void draw() override;
	virtual void cleanup() override;

	//-- Virtual callback methods
	virtual bool cursorEnterWindowEvent(int entered) override;
	virtual bool mouseMoveEvent(double xPos, double yPos) override;
	virtual bool mouseButtonInputEvent(int button, int actions, int mods) override;
	virtual bool mouseScrollEvent(double xOffSet, double yOffSet) override;
	virtual bool windowResizeEvent(int width, int height) override;
	virtual bool keyInputEvent(int key, int action, int mods) override;

	//-- One time initialization methods:
	void processLuaSceneFile(const std::string & filename);
	void createShaderProgram();
	void enableVertexShaderInputSlots();
	void uploadVertexDataToVbos(const MeshConsolidator & meshConsolidator);
	void mapVboDataToVertexShaderInputLocations();
	void initViewMatrix();
	void initLightSources();

	void initPerspectiveMatrix();
	void uploadCommonSceneUniforms();
	void renderSceneNode(const SceneNode & root, mat4 nodeTransformation);
	void renderSceneGraph(const SceneNode &node);
	void renderArcCircle();

	
	// Helper functions
	template <class T>
	void dbgPrint(T statement); // for printing and debugging
	void findSelectedNodes(std::shared_ptr<SceneNode>  root); // finds selected nodes
	void traverseNodes(std::shared_ptr<SceneNode>  root);
	void selectJointNodeByMeshId(std::shared_ptr<SceneNode>  root, unsigned int  meshNodeId );
	void rotateSelectedJoints(std::shared_ptr<SceneNode>  root,double yDiff);
	void rotateSelectedNeck(std::shared_ptr<SceneNode>  root,double yDiff);
	void selectMeshbyPicking(double xPos, double yPos);

	// TODO: new funtions
	// reset Functions
	void resetWorld();
	void resetPosition();
	void resetJoints(std::shared_ptr<SceneNode>  root);
	void resetRotation();

	void handlePosition(double xPos, double yPos);
	void handleJoints(double xPos, double yPos);

	void saveInitJointsTransform(std::shared_ptr<SceneNode>  root);
	void saveCurrJointsTransform(std::shared_ptr<SceneNode>  root);
	void undoChange(std::shared_ptr<SceneNode>  root);
	void redoChange(std::shared_ptr<SceneNode>  root);
	


	glm::mat4 m_perpsective;
	glm::mat4 m_view;

	LightSource m_light;

	//-- GL resources for mesh geometry data:
	GLuint m_vao_meshData;
	GLuint m_vbo_vertexPositions;
	GLuint m_vbo_vertexNormals;
	GLint m_positionAttribLocation;
	GLint m_normalAttribLocation;
	ShaderProgram m_shader;

	//-- GL resources for trackball circle geometry:
	GLuint m_vbo_arcCircle;
	GLuint m_vao_arcCircle;
	GLint m_arc_positionAttribLocation;
	ShaderProgram m_shader_arcCircle;

	// BatchInfoMap is an associative container that maps a unique MeshId to a BatchInfo
	// object. Each BatchInfo object contains an index offset and the number of indices
	// required to render the mesh with identifier MeshId.
	BatchInfoMap m_batchInfoMap;

	std::string m_luaSceneFile;

	std::shared_ptr<SceneNode> m_rootNode;

	// Enumeration to represent different interaction modes
	enum inter_mode{
		position_mode,
		joints_mode
	};

	inter_mode inter_mode_selection; //interaction mode selection variable

	// Options
	bool trackBall;
	bool zBuffer;
	bool backFaceCulling;
	bool frontFaceCulling;

	// mouse variables
	double prev_mouse_xPos;
	double prev_mouse_yPos;
	bool left_click;
	bool mid_click;
	bool right_click;

	// Transformation matrices
	glm::mat4 viewTransRot; // global translation
	glm::mat4 localRot;   // global rotation 
	
	bool picking;

	// Undo/redo variables
	unordered_map<string, glm::mat4> initJointsTransform;
	vector<unordered_map<string, glm::mat4>> jointsTransforms;
	int jointsTransformIdx;
	string jointsTransformStr;

	
};
