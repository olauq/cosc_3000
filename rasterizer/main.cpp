/****************************************************************************/
/* Copyright (c) 2016, Ola Olsson */
/****************************************************************************/

// GLEW, GL Extension Wrangler, is a library that is pretty well kept up to date with new versions and vendor extensions to OpenGL
// we need (or somethis similar) it to access pretty much all post OpenGL 2.0 functionality: http://glew.sourceforge.net/
// All OpenGL extensions (and the standard specification) are available in the OpenGL registry: http://www.opengl.org/registry/
#include <GL/glew.h>
// Freeglut is an implementation of GLUT, with some notable extensions!
#include <GL/freeglut.h>


// Any graphics program needs a vector maths library!
// GLM is a library implementing most of what one needs to do graphics, with the additional benefit of sharing specification and names with GLSL - the 
// domain specific language that is used to write shaders. This makes it easier to move code from one to the other and means the GLSL spec can be used as
// guide to usage.
#include <glm/glm.hpp>
// Include for matrix building operations, these are extensions that go beyond what is found in the glsl spec, but that we need to write useful graphics programs.
#include <glm/gtx/transform.hpp>
// To be able to get a pointer to upload to OpenGL
#include <glm/gtc/type_ptr.hpp>

// some basic C/C++ standard library includes
#include <stdio.h>
#include <vector>
#include <algorithm>

// We're using the 3 dimensional vector & 4x4 dimensional matrix types of GLM, so we alias them into the global name space.
// The type of the elements of these types is 'float' i.e., single precision (32-bit) floating point numbers.
using glm::vec3;
using glm::mat4;

// Initial window size for GLUT
const int g_startWidth  = 1280;
const int g_startHeight = 720;

// Definition of sphere.
static vec3 g_spherePos = { 0.0f, 0.0f, 0.0f };
static float g_sphereRadius = 3.0f;
const vec3 g_sphereColour = { 0.2f, 0.3f, 1.0f };

// Definition of virtual camera
static vec3 g_viewPosition = { 0.0f, 0.0f, -10.0f };
static vec3 g_viewTarget = { 0.0f, 0.0f, 0.0f };
static vec3 g_viewUp = { 0.0f, 1.0f, 0.0f };
static float g_fov = 45.0f;
const float g_nearDistance = 0.1f;
const float g_farDistance = 100000.0f;

// 
const vec3 g_backGroundColour = { 0.1f, 0.2f, 0.1f };

// OpenGL objects and suchlike

// We explicitly specify the attribute locations to use for the different attributes, we must also ensure the shader knows about this using glBindAttribLocation()
enum VertexAttribLocations
{
	VAL_Position = 0,
};

GLuint g_sphereVertexDataBuffer = 0U;
GLuint g_sphereVertexArrayObject = 0U;
GLuint g_simpleShader = 0U;

// 4. Misc...
static const float g_pi = 3.1415f;
inline float degreesToRadians(float degs)
{
	return degs * g_pi / 180.0f;
}

const int g_numSphereSubdivs = 4;
static int g_numSphereVerts = -1; /**< This must be initialized somewhere! (see main) */

/**
 * Recursively subdivide a triangle into four equally sized sub-triangles.
 * Input vertices are assumed to be on the surface of the unit sphere amd the new vertices also are on part of the unit sphere.
 */
static void subDivide(std::vector<vec3> &dest, const vec3 &v0, const vec3 &v1, const vec3 &v2, int level)
{
	// If the level index/counter is non-zero...
	if (level)
	{
		// ...we subdivide the input triangle into four equal sub-triangles
		// The mid points are the half way between to vertices, which is really (v0 + v2) / 2, but 
		// instead we normalize the vertex to 'push' it out to the surface of the unit sphere.
		vec3 v3 = normalize(v0 + v1);
		vec3 v4 = normalize(v1 + v2);
		vec3 v5 = normalize(v2 + v0);

		// ...and then recursively call this function for each of those (with the level decreased by one)
		subDivide(dest, v0, v3, v5, level - 1);
		subDivide(dest, v3, v4, v5, level - 1);
		subDivide(dest, v3, v1, v4, level - 1);
		subDivide(dest, v5, v4, v2, level - 1);
	}
	else
	{
		// If we have reached the terminating level, just output the vertex position
		dest.push_back(v0);
		dest.push_back(v1);
		dest.push_back(v2);
	}
}


/**
 * This function creates the geometry for a unit sphere by using the subDivide helper function to recursively subdivide 
 * a double unit pyramid (try giving subdivisions == 0). The resulting number of vertices is thus 3*4^numSubDivisionLevels
 * (^ here meaning to the power of). 
 */
static std::vector<vec3> createUnitSphereVertices(int numSubDivisionLevels)
{
	std::vector<vec3>	sphereVerts;

	// The root level sphere is formed from 8 triangles in a diamond shape (two pyramids)
	subDivide(sphereVerts, vec3(0, 1, 0), vec3(0, 0, 1), vec3(1, 0, 0), numSubDivisionLevels);
	subDivide(sphereVerts, vec3(0, 1, 0), vec3(1, 0, 0), vec3(0, 0, -1), numSubDivisionLevels);
	subDivide(sphereVerts, vec3(0, 1, 0), vec3(0, 0, -1), vec3(-1, 0, 0), numSubDivisionLevels);
	subDivide(sphereVerts, vec3(0, 1, 0), vec3(-1, 0, 0), vec3(0, 0, 1), numSubDivisionLevels);

	subDivide(sphereVerts, vec3(0, -1, 0), vec3(1, 0, 0), vec3(0, 0, 1), numSubDivisionLevels);
	subDivide(sphereVerts, vec3(0, -1, 0), vec3(0, 0, 1), vec3(-1, 0, 0), numSubDivisionLevels);
	subDivide(sphereVerts, vec3(0, -1, 0), vec3(-1, 0, 0), vec3(0, 0, -1), numSubDivisionLevels);
	subDivide(sphereVerts, vec3(0, -1, 0), vec3(0, 0, -1), vec3(1, 0, 0), numSubDivisionLevels);

	return sphereVerts;
}


/**
 * This function takes an array of vertex positions and creates a buffer object to store this data, which is then initialized,
 * and the data is uploaded. Then a vertex array object is created to reference the buffer as an attribute array 
 * and provide type information. The attribute array used is 'VAL_Position' declared in global scope.
 * The attribute array is also enabled.
 * For an animated presentation of how this works with OpenGL, take a look at the power point slide deck
 *  createVertexArrayObject.pptx (which should be in the docs folder of this repo) - it should help explaining how OpenGL likes to do things.
 */
void createVertexArrayObject(const std::vector<vec3> &vertexPositions, GLuint &positionBuffer, GLuint &vertexArrayObject)
{
	// glGen*(<count>, <array of GLuint>) is the typical pattern for creating objects in OpenGL. Do pay attention to this idiosyncrasy as the first parameter indicates the
	// number of objects we want created. Usually this is just one, but if you were to change the below code to '2' OpenGL would happily overwrite whatever is after
	// 'positionBuffer' on the stack (this leads to nasty bugs that are sometimes very hard to detect - i.e., this was a poor design choice!)
	glGenBuffers(1, &positionBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, positionBuffer);
	// Upload data to the currently bound GL_ARRAY_BUFFER, note that this is completely anonymous binary data, no type information is retained (we'll supply that later in glVertexAttribPointer)
	glBufferData(GL_ARRAY_BUFFER, sizeof(vec3) * vertexPositions.size(), &vertexPositions[0], GL_STATIC_DRAW);

	glGenVertexArrays(1, &vertexArrayObject);
	glBindVertexArray(vertexArrayObject);

	// The positionBuffer is already bound to the GL_ARRAY_BUFFER location. This is typycal OpenGL style - you bind the buffer to GL_ARRAY_BUFFER,
	// and the vertex array object using 'glBindVertexArray', and then glVertexAttribPointer implicitly uses both of these. You often need to read the manual
	// or find example code.
	// 'VAL_Position' is an integer, which tells it which array we want to attach this data to, this must be the same that we set up our shader
	// using glBindAttribLocation. Next provide we type information about the data in the buffer: there are three components (x,y,z) per element (position)
	// and they are of type 'float'. The last arguments can be used to describe the layout in more detail (stride  & offset).
	// Note: The last adrgument is 'pointer' and has type 'const void *', however, in modern OpenGL, the data ALWAYS comes from the current GL_ARRAY_BUFFER object, 
	// and 'pointer' is interpreted as an offset (which is somewhat clumsy).
	glVertexAttribPointer(VAL_Position, 3, GL_FLOAT, false, 0, 0);
	// For the currently bound vertex array object, enable the VAL_Position'th vertex array (otherwise the data is not fed to the shader)
	glEnableVertexAttribArray(VAL_Position);

	// Unbind the buffers again to avoid unintentianal GL state corruption (this is something that can be rather inconventient to debug)
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
}



// Called by GLUT system when a frame needs to be drawn (we provide GLUT with a pointer in main() )
static void onGlutDisplay()
{
	int height = glutGet(GLUT_WINDOW_HEIGHT);
	int width = glutGet(GLUT_WINDOW_WIDTH);

	glViewport(0, 0, width, height);

	glClearColor(g_backGroundColour.x, g_backGroundColour.y, g_backGroundColour.z, 1.0f);
	// We don't own the pixels anymore, tell OpenGL to clear them
	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

	// Transform from world space to view space.
	const mat4 worldToViewTransform = glm::lookAt(g_viewPosition, g_viewTarget, g_viewUp);
	
	const float aspectRatio = float(width) / float(height);
	// Projection (view to clip space transform)
	const mat4 viewToClipTransform = glm::perspective(degreesToRadians(g_fov), aspectRatio, g_nearDistance, g_farDistance);

	// define transformation matrix from sphere model space (which we take to be the origin).
	// We'll also assume that it is a unit sphere, and so scale it to the specified radius
	const mat4 sphereModelToWorldTransform = glm::translate(g_spherePos) * glm::scale(vec3(g_sphereRadius));

	// Concatenate the transformations to take vertices directly from model space to clip space
	const mat4 modelToClipTransform = viewToClipTransform * worldToViewTransform * sphereModelToWorldTransform;
	// Transform to view space from model space (used for the shading)
	const mat4 modelToViewTransform = worldToViewTransform * sphereModelToWorldTransform;

	// Bind 'use' current shader program 
	glUseProgram(g_simpleShader);
	// Set uniform argument in currently bound shader. glGetUniformLocation is typically not a super-fast operation and ought to be done ahead of time (much like other binding)
	glUniformMatrix4fv(glGetUniformLocation(g_simpleShader, "modelToClipTransform"), 1, GL_FALSE, glm::value_ptr(modelToClipTransform));
	glUniformMatrix4fv(glGetUniformLocation(g_simpleShader, "modelToViewTransform"), 1, GL_FALSE, glm::value_ptr(modelToViewTransform));
	glUniform1f(glGetUniformLocation(g_simpleShader, "sphereRadius"), g_sphereRadius);
	glUniform1f(glGetUniformLocation(g_simpleShader, "sphereDistance"), length(g_viewPosition - g_spherePos));
	glUniform3fv(glGetUniformLocation(g_simpleShader, "sphereColour"), 1, glm::value_ptr(g_sphereColour));
	
	// Bind gl object storing the shphere mesh data (it is set up in main)
	glBindVertexArray(g_sphereVertexArrayObject);

	// https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/glDrawArrays.xhtml
	// Tell OpenGL to draw triangles using data from the currently bound vertex array object by grabbing
	// three at a time vertices from the array up to g_numSphereVerts vertices, for(int i = 0; i < g_numSphereVerts; i += 3) ... draw triangle ...
	glDrawArrays(GL_TRIANGLES, 0, g_numSphereVerts);
	
	// Unbind the shader program & vertex array object to ensure it does not affect anything else (in this simple program, no great risk, but otherwise it pays to be careful)
	glBindVertexArray(0);
	glUseProgram(0);

	// Instruct the windowing system (by way of GLUT) that the back & front buffer should be exchanged, i.e., we're done drawing this frame
	glutSwapBuffers();
}

// Two nearly identical functions to get the error log from the shader compilation process
static std::string getShaderInfoLog(GLuint obj)
{
	int logLength = 0;
	glGetShaderiv(obj, GL_INFO_LOG_LENGTH, &logLength);

	std::string log(logLength, ' ');
	if (logLength > 0)
	{
		int charsWritten = 0;
		glGetShaderInfoLog(obj, logLength, &charsWritten, &log[0]);
	}

	return log;
}
static std::string getProgramInfoLog(GLuint obj)
{
	int logLength = 0;
	glGetProgramiv(obj, GL_INFO_LOG_LENGTH, &logLength);

	std::string log(logLength, ' ');
	if (logLength > 0)
	{
		int charsWritten = 0;
		glGetProgramInfoLog(obj, logLength, &charsWritten, &log[0]);
	}

	return log;
}


/**
 * This function performs the steps needed to compile the source code for a shader stage
 */
static bool compileAndAttachShader(GLuint shaderProgram, GLenum shaderType, const char *source)
{
	// Create the opengl shader object
	GLuint shader = glCreateShader(shaderType);

	GLint length = GLint(strlen(source));
	// upload the source code for the shader
	// Note the function takes an array of source strings and lengths.
	glShaderSource(shader, 1, &source, &length);
	glCompileShader(shader);

	// If there is a syntax or other compiler error during shader compilation, we'd like to know
	int compileOk = 0;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compileOk);
	if (!compileOk)
	{
		std::string err = getShaderInfoLog(shader);
		printf("SHADER COMPILE ERROR: '%s'", err.c_str());
		return false;
	}
	glAttachShader(shaderProgram, shader);
	glDeleteShader(shader);
	return true;
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
#if defined(_MSC_VER) && defined(_DEBUG)
	// Visual C++ specific break-point trigger, which makes the debugger break on this very line and does not cause the application to exit (unlike 'assert(false)').
	__debugbreak();
#endif //_MSC_VER
}


int main(int argc, char* argv[])
{
	// Note: creating a debug context impacts performance and should thus usually not be done in a release build of your applications
	// Will cause GLUT to create the platform specific OpenGL context using the appropriate version of 'GLX_DEBUG_CONTEXT_BIT', see https://www.khronos.org/opengl/wiki/Debug_Output. 
	glutInitContextFlags(GLUT_DEBUG);

	glutInit(&argc, argv);
	glutSetOption(GLUT_MULTISAMPLE, 8);
	glutInitDisplayMode(GLUT_RGB | GLUT_DEPTH | GLUT_DOUBLE | GLUT_MULTISAMPLE);
	glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_GLUTMAINLOOP_RETURNS);

	glutInitWindowSize(g_startWidth, g_startHeight);

	// NOTE: Before the window is created, there is probably no OpenGL context created, so any call to OpenGL will probably fail.
	glutCreateWindow("A 'simple' OpenGL >= 3.0 program");

	// glewInit sets up all the function pointers that map to pretty much all of modern OpenGL functionality, so any calls to those 
	// before GLEW is intialized will fail.
	glewInit();

	// Set up OpenGL debug callback and turn on...
	glDebugMessageCallback(debugMessageCallback, 0);
	// (although this glEnable(GL_DEBUG_OUTPUT) should not have been needed when using the GLUT_DEBUG flag above...)
	glEnable(GL_DEBUG_OUTPUT);
	// This ensures that the callback is done in the context of the calling function, which means it will be on the stack in the debugger, which makes it
	// a lot easier to figure out why it happened.
	glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);

	printf("--------------------------------------\nOpenGL\n  Vendor: %s\n  Renderer: %s\n  Version: %s\n--------------------------------------\n", glGetString(GL_VENDOR), glGetString(GL_RENDERER), glGetString(GL_VERSION));

	// The source code for the vertex shader. The Vertex shader defines a program which is executed for every vertex in the models being drawn (in this case the sphere).
	// The role of the vertex shader is to transform vertices into clip space, from which the fixed function hardware takes over for a while.
	const char *vertexShader =
		R"SOMETAG(
#version 330
in vec3 positionIn;
uniform mat4 modelToClipTransform;
uniform mat4 modelToViewTransform;

// Out variables decalred in a vertex shader can be accessed in the subsequent stages.
// For a pixel shader the variable is interpolated (the type of interpolation can be modified, try placing 'flat' in front, and also in the fragment shader!).
out VertexData
{
	float v2f_distance;
};

void main() 
{
  // Transform the vertex position to view space and calculate the distance, as this is used in the shading calculation
	vec4 viewPos = modelToViewTransform * vec4(positionIn, 1.0);
	v2f_distance = length(viewPos);

	// gl_Position is a buit in out variable that gets passed on to the clipping and rasterization stages.
  // it must be written in order to produce any drawn geometry. 
  // We transform the position using one matrix multiply from model to clip space, note the added 1 at the end of the position.
	gl_Position = modelToClipTransform * vec4(positionIn, 1.0);
}
)SOMETAG";

	// The source code of the fragment shader (sometimes called pixel shader). The fragment shader is executed once for each fragment produced by the rasterizer, for all the triangles drawn. 
	// A fragment is produced for each covered pixel. The role of the fragment shader is to produce the final colour for the pixel, which can then be merged into the frame buffer (or discarded if depth test fails).
	const char *fragmentShader =
		R"SOMETAG(
#version 330
// Input from the vertex shader, will contain the interpolated (i.e., distance weighted average) vaule out put for each of the three vertex shaders that 
// produced the vertex data for the triangle this fragmet is part of.
in VertexData
{
	float v2f_distance;
};

uniform float sphereDistance; // these are needed for the shading and are manually uploaded to the shader.
uniform float sphereRadius; // 
uniform vec3 sphereColour; // 

out vec4 fragmentColor;

void main() 
{
  // Shading is designed to go from 1 at the nearest point of the sphere, to 0 at the furthest point
	float shading = 1.0 - (v2f_distance - sphereDistance + sphereRadius) / (2.0 * sphereRadius);
	fragmentColor = vec4(sphereColour, 1.0) * shading;
}
)SOMETAG";

	// A 'program' object in OpenGL is a pipeline of the different shader stages, in this case we'll attach a typical setup of one vertex shader and one fragment shader
	g_simpleShader = glCreateProgram();
	// The helper function performs the compilation of the source and attaches it to the program object.
	if (compileAndAttachShader(g_simpleShader, GL_VERTEX_SHADER, vertexShader)
		&& compileAndAttachShader(g_simpleShader, GL_FRAGMENT_SHADER, fragmentShader))
	{
		// Link the name we used in the vertex shader 'positionIn' to the integer index we chose in 'VAL_Position'
		// This ensures that when the shader executes, data fed into 'positionIn' will be sourced from the VAL_Position'th generic attribute stream
		// This seemingly backwards way of telling the shader where to look allows OpenGL programs to swap vertex buffers without needing to do any string lookups at run time. 
		glBindAttribLocation(g_simpleShader, VAL_Position, "positionIn");
		
		// If we have multiple images bound as render targets, we need to specify which 'out' variable in the fragment shader goes where.
		// In this case it is totally redundant as we only have one (the default render target, or frame buffer) and the default binding is always zero.
		glBindFragDataLocation(g_simpleShader, 0, "fragmentColor");

		glLinkProgram(g_simpleShader);
		GLint linkStatus = 0;
		glGetProgramiv(g_simpleShader, GL_LINK_STATUS, &linkStatus);
		if (!linkStatus)
		{
			std::string err = getProgramInfoLog(g_simpleShader);
			printf("SHADER LINKER ERROR: '%s'", err.c_str());
			return 1;
		}
	}
	else
	{
		// bungled!
			return 1;
	}

	// Create sphere vertex data and upload to OpenGL
	std::vector<vec3> sphereVerts = createUnitSphereVertices(g_numSphereSubdivs);
	g_numSphereVerts = int(sphereVerts.size());
	createVertexArrayObject(sphereVerts, g_sphereVertexDataBuffer, g_sphereVertexArrayObject);

	// Turn on backface culling, depth testing and set the depth function (possibly the degfault already, but why take any changes?)
	glEnable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);

	// Tell GLUT to call 'onGlutDisplay' whenever it needs to re-draw the window.
	glutDisplayFunc(onGlutDisplay);

	glutMainLoop();

	return 0;
}
