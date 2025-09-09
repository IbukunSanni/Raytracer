#pragma once

#ifdef __APPLE__
	#include <OpenGL/gl3.h>
#elif defined __linux__
    #include <gl3w/GL/gl3w.h>
#elif defined _WIN32
    #include <gl3w/GL/gl3w.h>
#endif
