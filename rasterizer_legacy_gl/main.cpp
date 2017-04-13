/****************************************************************************/
/* Copyright (c) 2016, Ola Olsson */
/****************************************************************************/

// GLEW, GL Extension Wrangler, is a library that is pretty well kept up to date with new versions and vendor extensions to OpenGL
// we need (or somethis similar) it to access pretty much all post OpenGL 2.0 functionality: http://glew.sourceforge.net/
// All OpenGL extensions (and the standard specification) are available in the OpenGL registry: http://www.opengl.org/registry/
#include <GL/glew.h>
#include <GL/freeglut.h>

// Any graphics program needs a vector maths library!
// GLM is a library implementing most of what one needs to do graphics, with the additional benefit of sharing specification and names with GLSL - the 
// domain specific language that is used to write shaders. This makes it easier to move code from one to the other and means the GLSL spec can be used as
// guide to usage.
#include <glm/glm.hpp>

#include <stdio.h>
#include <vector>
#include <algorithm>

// We're using the 3 dimensional vector type of GLM, so we alias it into the global name space.
// The type of the elements of these types is 'float' i.e., single precision (32-bit) floating point numbers.
using glm::vec3;

// Initial window size for GLUT
const int g_startWidth  = 1280;
const int g_startHeight = 720;

// Definition of sphere.
static vec3 g_spherePos = { 0.0f, 0.0f, 0.0f }; // World Space position of sphere
static float g_sphereRadius = 3.0f; 
const vec3 g_sphereColour = { 0.2f, 0.3f, 1.0f };

// Definition of virtual camera
static vec3 g_viewPosition = { 0.0f, 0.0f, -10.0f }; // location of the eye/camera
static vec3 g_viewTarget = { 0.0f, 0.0f, 0.0f }; // target of the camera (this position will be centered on screen)
static vec3 g_viewUp = { 0.0f, 1.0f, 0.0f }; // World space up direction to constrain the view/camera to.
static float g_fov = 45.0f; // vertical field of view, in degrees.
const float g_nearDistance = 0.1f;
const float g_farDistance = 100000.0f;
// 
const vec3 g_backGroundColour = { 0.1f, 0.2f, 0.1f };

// Called by GLUT system when a frame needs to be drawn
static void onGlutDisplay()
{
	int height = glutGet(GLUT_WINDOW_HEIGHT);
	int width = glutGet(GLUT_WINDOW_WIDTH);

	glViewport(0, 0, width, height);

	glClearColor(g_backGroundColour.x, g_backGroundColour.y, g_backGroundColour.z, 1.0f);
	// We don't own the pixels anymore, tell OpenGL to clear them
	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

	const float aspectRatio = float(width) / float(height);

	// In legacy OpenGL there are two separate matrix stacks, one for projection (view to clip space transform) and one for everything up to view space (
	glMatrixMode(GL_PROJECTION);
	// Ensure identity is loaded, matrix ops typically append to the current matrix.
	glLoadIdentity();
	// Create a perspective matrix and multiply with previous (identity) on the current stack
	gluPerspective(g_fov, aspectRatio, g_nearDistance, g_farDistance);

	// Switch to other matrix stack
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	// Append look-at matrix to transform geometry to view space from world space.
	gluLookAt(g_viewPosition.x, g_viewPosition.y, g_viewPosition.z, g_viewTarget.x, g_viewTarget.y, g_viewTarget.z, g_viewUp.x, g_viewUp.y, g_viewUp.z);

	// Push / pop matrix to avoid affecting other things (which we don't have any, but still)
	glPushMatrix();
		// Construct the model-to-world transform for the sphere.
		// Create a translation matrix and append to the current stack MODELVIEW in this case. Moves the sphere to its proper location.
		glTranslatef(g_spherePos.x, g_spherePos.y, g_spherePos.z);
		// Soecify a surface colour to use for each vertex, this affects the shading. 
		// (This version of the program is differet in that there is no shading, just a colour, some things are much harder in legacy OpenGL
		glColor3f(g_sphereColour.x, g_sphereColour.y, g_sphereColour.z);
		// This is part of GLUT and does something similar to the code I provided in the modern OpenGL example. However, it takes a radius as a parameter
		// which removes the need for scaling.
		glutSolidSphere(g_sphereRadius, 16, 16;
	glPopMatrix();

	// Instruct the windowing system (by way of GLUT) that the back & front buffer should be exchanged, i.e., we're done drawing this frame
	glutSwapBuffers();
}



/**
 * This function is called by OpenGL whenever an error is detected, right now it just prints a message.
 */
void GLAPIENTRY debugMessageCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam)
{
	const char *severityStr = "GL_DEBUG_SEVERITY_NOTIFICATION";
	switch (severity)
	{
	case GL_DEBUG_SEVERITY_HIGH:
		severityStr = "GL_DEBUG_SEVERITY_HIGH";
		break;
	case GL_DEBUG_SEVERITY_MEDIUM:
		severityStr = "GL_DEBUG_SEVERITY_MEDIUM";
		break;
	case GL_DEBUG_SEVERITY_LOW:
		severityStr = "GL_DEBUG_SEVERITY_LOW";
		break;
	default:
		break;
	};
	fprintf(stderr, "OpenGL Error: %s\n  Severity: %s\n", message, severityStr);
#ifdef _MSC_VER
	// Visual C++ specific break-point trigger, which makes the debugger break on this very line and does not cause the application to exit (unlike 'assert(false)').
	__debugbreak();
#endif //_MSC_VER
}



int main(int argc, char* argv[])
{
	// Note: creating a debug context impacts performance and should thus usually not be done in a release build of your applications
	// Will cause GLUT to create the platform specific OpenGL context using the appropriate version of 'GLX_DEBUG_CONTEXT_BIT', see https://www.khronos.org/opengl/wiki/Debug_Output. 
	glutInitContextFlags(GLUT_DEBUG | GLUT_COMPATIBILITY_PROFILE);

	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_RGB | GLUT_DEPTH | GLUT_DOUBLE);
	glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_GLUTMAINLOOP_RETURNS);

	glutInitWindowSize(g_startWidth, g_startHeight);

	// NOTE: Before the window is created, there is probably no OpenGL context created, so any call to OpenGL will probably fail.
	glutCreateWindow("A simple OpenGL <= 2.0 program");

	// glewInit sets up all the function pointers that map to pretty much all of modern OpenGL functionality, so any calls to those 
	// before GLEW is intialized will fail (we're only using the debugging part in this example program).
	glewInit();

	// Set up OpenGL debug callback and turn on... (This is modern OpenGL so may not work on very old machines)
	glDebugMessageCallback(debugMessageCallback, 0);
	// (although this glEnable(GL_DEBUG_OUTPUT) should not have been needed when using the GLUT_DEBUG flag above...)
	glEnable(GL_DEBUG_OUTPUT);
	// This ensures that the callback is done in the context of the calling function, which means it will be on the stack in the debugger, which makes it
	// a lot easier to figure out why it happened.
	glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);

	// Turn on backface culling, depth testing and set the depth function (possibly the degfault already, but why take any changes?)
	glEnable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);

	// Tell GLUT to call 'onGlutDisplay' whenever it needs to re-draw the window.
	glutDisplayFunc(onGlutDisplay);

	glutMainLoop();

	return 0;
}
