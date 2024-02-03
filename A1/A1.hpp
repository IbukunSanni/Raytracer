// Termm--Fall 2020

#pragma once

#include <glm/glm.hpp>

#include "cs488-framework/CS488Window.hpp"
#include "cs488-framework/OpenGLImport.hpp"
#include "cs488-framework/ShaderProgram.hpp"

#include "maze.hpp"

class A1 : public CS488Window {
public:
	A1();
	virtual ~A1();

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

private:
	void resetWorld();
	void placeAvatar();

	void initGrid();
	void initBlock();
	void initAvatar();
	void initFloor();
	Maze m;

	// mouse variables
	bool mouse_drag;
	double prev_mouse_xPos;

	// rotation variables
	float rotate_change;
	float rotate_persistence;


	float scale;

	// Fields related to the shader and uniforms.
	ShaderProgram m_shader;
	GLint P_uni; // Uniform location for Projection matrix.
	GLint V_uni; // Uniform location for View matrix.
	GLint M_uni; // Uniform location for Model matrix.
	GLint col_uni;   // Uniform location for cube colour.

	// Fields related to grid geometry.
	GLuint m_grid_vao; // Vertex Array Object
	GLuint m_grid_vbo; // Vertex Buffer Object

	// Fields related to block geometry.
	GLuint m_block_vao; // Vertex Array Object
	GLuint m_block_vbo; // Vertex Buffer Object
	GLuint m_block_ebo; // Element Buffer Object
	GLfloat b_height;   // block height
	glm::vec3 b_color;  // block color

	// Fields related to avatar geometry.
	GLuint m_avatar_vao; // Vertex Array Object
	GLuint m_avatar_vbo; // Vertex Buffer Object
	GLfloat a_offset_x; // avatar offset in x direction
	GLfloat a_offset_z; // avatar offset in z direction
	glm::vec3 a_color;  // avatar color

	// Fields related to floor geometry.
	GLuint m_floor_vao; // Vertex Array Object
	GLuint m_floor_vbo; // Vertex Buffer Object
	GLuint m_floor_ebo; // Element Buffer Object
	glm::vec3 f_color;  // floor color

	// Matrices controlling the camera and projection.
	glm::mat4 proj;
	glm::mat4 view;

	float colour[3];
	int current_col;

	// object enum to represent object selection
	enum object{
		block_select,
		avatar_select,
		floor_select
	};
	// object slection variable
	object obj_selection;
};
