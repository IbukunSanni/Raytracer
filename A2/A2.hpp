// Termm--Fall 2020

#pragma once

#include "cs488-framework/CS488Window.hpp"
#include "cs488-framework/OpenGLImport.hpp"
#include "cs488-framework/ShaderProgram.hpp"

#include <glm/glm.hpp>

#include <vector>

// Set a global maximum number of vertices in order to pre-allocate VBO data
// in one shot, rather than reallocating each frame.
const GLsizei kMaxVertices = 1000;


// Convenience class for storing vertex data in CPU memory.
// Data should be copied over to GPU memory via VBO storage before rendering.
class VertexData {
public:
	VertexData();

	std::vector<glm::vec2> positions;
	std::vector<glm::vec3> colours;
	GLuint index;
	GLsizei numVertices;
};


class A2 : public CS488Window {
public:
	A2();
	virtual ~A2();

protected:
	virtual void init() override;
	virtual void appLogic() override;
	virtual void guiLogic() override;
	virtual void draw() override;
	virtual void cleanup() override;

	virtual bool cursorEnterWindowEvent(int entered) override;
	virtual bool mouseMoveEvent(double xPos, double yPos) override;
	virtual bool mouseButtonInputEvent(int button, int actions, int mods) override;
	virtual bool mouseScrollEvent(double xOffSet, double yOffSet) override;
	virtual bool windowResizeEvent(int width, int height) override;
	virtual bool keyInputEvent(int key, int action, int mods) override;

	void createShaderProgram();
	void enableVertexAttribIndices();
	void generateVertexBuffers();
	void mapVboDataToVertexAttributeLocation();
	void uploadVertexDataToVbos();
	void resetWorld();
	void resetView();
	void resetProjection();


	void initLineData();

	void setLineColour(const glm::vec3 & colour);

	void drawLine (
			const glm::vec2 & v0,
			const glm::vec2 & v1
	);
	// Block function declarations
	void initBlockVerts();
	void initCoord();
	void drawBlockEdge(
		glm::vec4  V1,   // Line Start
		glm::vec4  V2    // Line End
	);
	void drawModelCoordAxis(
		glm::vec4  V1,   // Line Start
		glm::vec4  V2    // Line End
	);
	void drawWorldCoordAxis(
		glm::vec4  V1,   // Line Start
		glm::vec4  V2    // Line End
	);

	float scaleConverter(
		float val,
		float og_min,
		float og_max,
		float new_min,
		float new_max
	);

	// Inteacttion declarations
	void rotateView(
		double xDiff
	);
	void translateView(
		double xDiff
	);
	void perspective(
		double xDiff
	);
	void rotateModel(
		double xDiff
	);
	void translateModel(
		double xDiff
	);
	void scaleModel(
		double xDiff
	);
	void alterViewport(
		double xPos,
		double yPos
	);
	// helper function for clamping
	double limitPos(
		double val,
		double min_val,
		double max_val
	);

	// mouse variables
	double prev_mouse_xPos;
	bool left_click;
	bool mid_click;
	bool right_click;


	// variables to draw
	std::vector<glm::vec4> block_verts;
	std::vector<glm::vec4> coord_verts;

	// transformation matrices
	glm::mat4 model_trans_rot; // model rotation and scale handled in 1 matrix
	glm::mat4 model_scale;
	glm::mat4 view_trans_rot; // view rotation and scale handled in 1 matrix



	// view variable
	glm::mat4 view;

	// Viewport Variable;
	glm::mat2 viewportCoords;

	// projection variables
	GLfloat near;
	GLfloat far;
	GLfloat fovy;// store value in radians, y to represent y-axis from view
	glm::mat4 projection;


	ShaderProgram m_shader;

	GLuint m_vao;            // Vertex Array Object
	GLuint m_vbo_positions;  // Vertex Buffer Object
	GLuint m_vbo_colours;    // Vertex Buffer Object

	VertexData m_vertexData;

	glm::vec3 m_currentLineColour;

	// enumeration to represent different modes
	enum mode{
		rv_mode,
		tv_mode,
		p_mode,
		rm_mode,
		tm_mode,
		sm_mode,
		v_mode
	};

	mode mode_selection; // mode selection variable

	int window_width;
	int window_height;
};
