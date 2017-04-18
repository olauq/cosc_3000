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

#include "OBJModel.h"

// We're using the 3 dimensional vector & 4x4 dimensional matrix types of GLM, so we alias them into the global name space.
// The type of the elements of these types is 'float' i.e., single precision (32-bit) floating point numbers.
using glm::vec3;
using glm::vec4;
using glm::mat4;

// Initial window size for GLUT
const int g_startWidth  = 1280;
const int g_startHeight = 720;

// Model that is loaded from an OBJ file
OBJModel *g_model = 0;

// Definition of virtual camera
static vec3 g_viewPosition = { -1250.0f, 650.0f, 50.0f };
//static vec3 g_viewPosition = { 1000.0f, 150.0f, 50.0f };
static vec3 g_viewTarget = { 1250.0f, 100.0f, 0.0f };
static vec3 g_viewUp = { 0.0f, 1.0f, 0.0f };
static float g_fov = 70.0f;
const float g_nearDistance = 0.1f; // TODO: Play with near clip distance, what happens if you set it to 100? 500?
const float g_farDistance = 100000.0f; // TODO: Play with far distance, what happens if you set it to 1000?

// 
const vec3 g_backGroundColour = { 0.1f, 0.2f, 0.1f };
const vec3 g_lightDirection = { 0.2f, 1.0f, -0.2f };

GLuint g_simpleShader = 0U;


// 4. Misc...
static const float g_pi = 3.1415f;
inline float degreesToRadians(float degs)
{
	return degs * g_pi / 180.0f;
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

	// define transformation matrix from model space, identity in this case: 
	const mat4 modelToWorldTransform = glm::mat4(1.0f);

	// Concatenate the transformations to take vertices directly from model space to clip space
	const mat4 modelToClipTransform = viewToClipTransform * worldToViewTransform * modelToWorldTransform;
	// Transform to view space from model space (used for the shading)
	const mat4 modelToViewTransform = worldToViewTransform * modelToWorldTransform;

	// Bind 'use' current shader program 
	glUseProgram(g_simpleShader);
	// Set uniform argument in currently bound shader. glGetUniformLocation is typically not a super-fast operation and ought to be done ahead of time (much like other binding)
	glUniformMatrix4fv(glGetUniformLocation(g_simpleShader, "modelToClipTransform"), 1, GL_FALSE, glm::value_ptr(modelToClipTransform));
	glUniformMatrix4fv(glGetUniformLocation(g_simpleShader, "modelToViewTransform"), 1, GL_FALSE, glm::value_ptr(modelToViewTransform));

	vec3 viewSpaceLightDirection = normalize(vec3(modelToViewTransform * vec4(g_lightDirection, 0.0f)));
	glUniform3fv(glGetUniformLocation(g_simpleShader, "viewSpaceLightDirection"), 1, glm::value_ptr(viewSpaceLightDirection));

	// Draw different classes of geometry:
	g_model->render(g_simpleShader, OBJModel::RF_Opaque, worldToViewTransform);
	g_model->render(g_simpleShader, OBJModel::RF_AlphaTested, worldToViewTransform);
	g_model->render(g_simpleShader, OBJModel::RF_Transparent, worldToViewTransform);

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
	//glutSetOption(GLUT_MULTISAMPLE, 8);
	glutInitDisplayMode(GLUT_RGB | GLUT_DEPTH | GLUT_DOUBLE);// | GLUT_MULTISAMPLE
	glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_GLUTMAINLOOP_RETURNS);

	glutInitWindowSize(g_startWidth, g_startHeight);

	// NOTE: Before the window is created, there is probably no OpenGL context created, so any call to OpenGL will probably fail.
	glutCreateWindow("A more complex OpenGL >= 3.0 program");

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

	// The source code for the vertex shader. The Vertex shader defines a program which is executed for every vertex in the models being drawn
	// The role of the vertex shader is to transform vertices into clip space, from which the fixed function hardware takes over for a while.
	const char *vertexShader =
		R"SOMETAG(
#version 330

in vec3 positionAttribute;
in vec3	normalAttribute;
in vec2	texCoordAttribute;

uniform mat4 modelToClipTransform;
uniform mat4 modelToViewTransform;

// Out variables decalred in a vertex shader can be accessed in the subsequent stages.
// For a pixel shader the variable is interpolated (the type of interpolation can be modified, try placing 'flat' in front, and also in the fragment shader!).
out VertexData
{
	vec3 v2f_viewSpaceNormal;
	vec2 v2f_texCoord;
};

void main() 
{
	// gl_Position is a buit in out variable that gets passed on to the clipping and rasterization stages.
  // it must be written in order to produce any drawn geometry. 
  // We transform the position using one matrix multiply from model to clip space, note the added 1 at the end of the position.
	gl_Position = modelToClipTransform * vec4(positionAttribute, 1.0);
	// We transform the normal using the model to view transform, but only the rotation part, this is only OK if we know
	// that there is only ever going ot be uniform scaling involved!
	v2f_viewSpaceNormal = normalize(mat3(modelToViewTransform) * normalAttribute);
	// The texture coordinate is just passed through
	v2f_texCoord = texCoordAttribute;
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
	vec3 v2f_viewSpaceNormal;
	vec2 v2f_texCoord;
};

// Material properties uniform buffer, required by OBJModel.
// 'MaterialProperties' must be bound to a uniform buffer, OBJModel::setDefaultUniformBindings is of help!
layout(std140) uniform MaterialProperties
{
  vec3 material_diffuse_color; 
	float material_alpha;
  vec3 material_specular_color; 
  vec3 material_emissive_color; 
  float material_specular_exponent;
};
// Textures set by OBJModel (names must be bound to the right texture unit, OBJModel::setDefaultUniformBindings helps with that.
uniform sampler2D diffuse_texture;
uniform sampler2D opacity_texture;
uniform sampler2D specular_texture;
uniform sampler2D normal_texture;

// Other uniforms used by the shader
uniform vec3 viewSpaceLightDirection;

out vec4 fragmentColor;

// If we do not convert the colour to srgb before writing it out it looks terrible! All our lighting is done in linear space
// (which it should be!), and the frame buffer is srgb by default. So we must convert, or somehow create a linear frame buffer...
vec3 toSrgb(vec3 color)
{
  return pow(color, vec3(1.0 / 2.2));
}

void main() 
{
	// Manual alpha test (note: alpha test is no longer part of Opengl 3.3).
	if (texture2D(opacity_texture, v2f_texCoord).r < 0.5)
	{
		discard;
	}

	vec3 materialDiffuse = texture(diffuse_texture, v2f_texCoord).xyz * material_diffuse_color;
	vec3 color = materialDiffuse * (0.1 + 0.9 * max(0.0, dot(v2f_viewSpaceNormal, viewSpaceLightDirection))) + material_emissive_color;
	fragmentColor = vec4(toSrgb(color), material_alpha);
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
		OBJModel::bindDefaultAttributes(g_simpleShader);
		
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

		// 
		glUseProgram(g_simpleShader);
		OBJModel::setDefaultUniformBindings(g_simpleShader);
		glUseProgram(0);

	}
	else
	{
		// bungled!
			return 1;
	}

	// Turn on backface culling, depth testing and set the depth function (possibly the degfault already, but why take any changes?)
	glEnable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);


	// load model:
	g_model = new OBJModel;
	g_model->load("data/crysponza/sponza.obj");

	// Tell GLUT to call 'onGlutDisplay' whenever it needs to re-draw the window.
	glutDisplayFunc(onGlutDisplay);

	glutMainLoop();

	return 0;
}
