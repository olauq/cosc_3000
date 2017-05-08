from OpenGL.GL import *
from OpenGL.GLUT import *
from OpenGL.GLU import *

import numpy as np
from ctypes import sizeof, c_float, c_void_p, c_uint, string_at
import math

#import transformations as tr

def normalize(v):
    norm=np.linalg.norm(v)
    return v/norm

def length(v):
    return math.sqrt(np.sum(v ** 2))

def vec3(a, b, c):
    return np.array([a,b,c], np.float32)

def translate(xyz):
    x, y, z = xyz
    return np.matrix([[1,0,0,x],
                      [0,1,0,y],
                      [0,0,1,z],
                      [0,0,0,1]])
 
def scale(xyz):
    x, y, z = xyz
    return np.matrix([[x,0,0,0],
                      [0,y,0,0],
                      [0,0,z,0],
                      [0,0,0,1]])

def mul(m1, m2):
    return np.dot(m1, m2)



def lookAt(eye, target, up):
    F = target[:3] - eye[:3]
    f = normalize(F)
    U = normalize(up[:3])
    s = np.cross(f, U)
    u = np.cross(s, f)
    M = np.matrix(np.identity(4))
    M[:3,:3] = np.vstack([s,u,-f])
    T = translate(-eye)
    return mul(M, T)

def perspective(fovy, aspect, n, f):
    radFovY = math.radians(fovy)
    tanHalfFovY = math.tan(radFovY/2.0)
    sx = 1.0 / (tanHalfFovY * aspect)
    sy = 1.0 / tanHalfFovY
    zz = -(f+n)/(f-n)
    zw = -(2.0*f*n)/(f-n)

    return np.matrix([[sx,0,0,0],
                      [0,sy,0,0],
                      [0,0,zz,zw],
                      [0,0,-1,0]])

g_startWidth = 1280
g_startHeight = 720

# Definition of sphere.
g_spherePos = vec3(0.0, 0.0, 0.0);
g_sphereRadius = 3.0;
g_sphereColour = vec3(0.2, 0.3, 1.0)

# Definition of virtual camera
g_viewPosition = vec3(0.0, 0.0, -10.0)
g_viewTarget = vec3(0.0, 0.0, 0.0)
g_viewUp = vec3(0.0, 1.0, 0.0)
g_fov = 45.0;
g_nearDistance = 0.1;
g_farDistance = 100000.0;

# 
g_backGroundColour = vec3(0.1, 0.2, 0.1)

g_sphereVertexArrayObject = 0
g_simpleShader = 0;

VAL_Position = 0

def getShaderInfoLog(obj):
    logLength = glGetShaderiv(obj, GL_INFO_LOG_LENGTH)

    if logLength > 0:
        return glGetShaderInfoLog(obj, logLength)

    return ""


#
# This function performs the steps needed to compile the source code for a shader stage
#
def compileAndAttachShader(shaderProgram, shaderType, source):
    # Create the opengl shader object
    shader = glCreateShader(shaderType);
    # upload the source code for the shader
    # Note the function takes an array of source strings and lengths.
    glShaderSource(shader, [source])
    glCompileShader(shader)

    # If there is a syntax or other compiler error during shader compilation, we'd like to know
    compileOk = glGetShaderiv(shader, GL_COMPILE_STATUS)

    if not compileOk:
	    err = getShaderInfoLog(shader);
	    print("SHADER COMPILE ERROR: '%s'"%err);
	    return False;

    glAttachShader(shaderProgram, shader)
    glDeleteShader(shader)
    return True

def debugMessageCallback(source, type, id, severity, length, message, userParam):
    print(message)

g_numSphereSubdivs = 4
g_numSphereVerts = -1# This must be initialized somewhere! (see main) 

# Recursively subdivide a triangle with its vertices on the surface of the unit sphere such that the new vertices also are on part of the unit sphere.
def subDivide(dest, v0, v1, v2, level):
	#If the level index/counter is non-zero...
	if level:
		# ...we subdivide the input triangle into four equal sub-triangles
		# The mid points are the half way between to vertices, which is really (v0 + v2) / 2, but 
		# instead we normalize the vertex to 'push' it out to the surface of the unit sphere.
		v3 = normalize(v0 + v1);
		v4 = normalize(v1 + v2);
		v5 = normalize(v2 + v0);

		# ...and then recursively call this function for each of those (with the level decreased by one)
		subDivide(dest, v0, v3, v5, level - 1);
		subDivide(dest, v3, v4, v5, level - 1);
		subDivide(dest, v3, v1, v4, level - 1);
		subDivide(dest, v5, v4, v2, level - 1);
	else:
		# If we have reached the terminating level, just output the vertex position
		dest.append(v0)
		dest.append(v1)
		dest.append(v2)


def createSphere(numSubDivisionLevels):
	sphereVerts = []

	# The root level sphere is formed from 8 triangles in a diamond shape (two pyramids)
	subDivide(sphereVerts, vec3(0, 1, 0), vec3(0, 0, 1), vec3(1, 0, 0), numSubDivisionLevels)
	subDivide(sphereVerts, vec3(0, 1, 0), vec3(1, 0, 0), vec3(0, 0, -1), numSubDivisionLevels)
	subDivide(sphereVerts, vec3(0, 1, 0), vec3(0, 0, -1), vec3(-1, 0, 0), numSubDivisionLevels)
	subDivide(sphereVerts, vec3(0, 1, 0), vec3(-1, 0, 0), vec3(0, 0, 1), numSubDivisionLevels)

	subDivide(sphereVerts, vec3(0, -1, 0), vec3(1, 0, 0), vec3(0, 0, 1), numSubDivisionLevels)
	subDivide(sphereVerts, vec3(0, -1, 0), vec3(0, 0, 1), vec3(-1, 0, 0), numSubDivisionLevels)
	subDivide(sphereVerts, vec3(0, -1, 0), vec3(-1, 0, 0), vec3(0, 0, -1), numSubDivisionLevels)
	subDivide(sphereVerts, vec3(0, -1, 0), vec3(0, 0, -1), vec3(1, 0, 0), numSubDivisionLevels)

	return sphereVerts;

def setShaderMat(shader, name, mat):
    index = glGetUniformLocation(shader, name)
    glUniformMatrix4fv(index, 1, GL_TRUE, np.ascontiguousarray(mat, dtype=np.float32))


def onGlutDisplay2():
    global g_simpleShader
    global g_sphereVertexArrayObject

    height = glutGet(GLUT_WINDOW_HEIGHT)
    width = glutGet(GLUT_WINDOW_WIDTH)

    glViewport(0, 0, width, height)
    glClearColor(0.2, 0.3, 0.1, 1.0)
    # We don't own the pixels anymore, tell OpenGL to clear them
    glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT)

    glMatrixMode(GL_PROJECTION)
    glLoadIdentity()
    glMatrixMode(GL_MODELVIEW)
    glLoadIdentity()

    glColor3f(1.0, 0.0, 0.0);

    glBegin(GL_TRIANGLES)
    glVertex3f(0.5, 0.0, 0.0);
    glVertex3f(0.0, 0.5, 0.0);
    glVertex3f(-0.5, 0.0, 0.0);
    glEnd()

    glutSwapBuffers() 


def onGlutDisplay():
    global g_simpleShader
    global g_sphereVertexArrayObject

    height = glutGet(GLUT_WINDOW_HEIGHT)
    width = glutGet(GLUT_WINDOW_WIDTH)

    glViewport(0, 0, width, height)
    glClearColor(0.2, 0.3, 0.1, 1.0)
    # We don't own the pixels anymore, tell OpenGL to clear them
    glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT)

	# Transform from world space to view space.
    worldToViewTransform = lookAt(g_viewPosition, g_viewTarget, g_viewUp)
    
    aspectRatio = float(width) / float(height)
	# Projection (view to clip space transform)
    viewToClipTransform = perspective(g_fov, aspectRatio, g_nearDistance, g_farDistance)

	# define transformation matrix from sphere model space (which we take to be the origin).
	# We'll also assume that it is a unit sphere, and so scale it to the specified radius
    sphereModelToWorldTransform = mul(translate(g_spherePos), scale(vec3(g_sphereRadius,g_sphereRadius,g_sphereRadius)))

    # Transform to view space from model space (used for the shading)
    modelToViewTransform = mul(worldToViewTransform, sphereModelToWorldTransform)
    # Concatenate the transformations to take vertices directly from model space to clip space
    modelToClipTransform = mul(viewToClipTransform, modelToViewTransform)


    # Bind 'use' current shader program 
    glUseProgram(g_simpleShader)
    # Set uniform argument in currently bound shader. glGetUniformLocation is typically not a super-fast operation and ought to be done ahead of time (much like other binding)
    #glUniformMatrix4fv(glGetUniformLocation(g_simpleShader, "modelToClipTransform"), 1, GL_TRUE, np.ascontiguousarray(modelToClipTransform, dtype=np.float32))
    setShaderMat(g_simpleShader, "modelToClipTransform", modelToClipTransform)
    #glUniformMatrix4fv(glGetUniformLocation(g_simpleShader, "modelToViewTransform"), 1, GL_TRUE, np.ascontiguousarray(modelToViewTransform, dtype=np.float32))
    setShaderMat(g_simpleShader, "modelToViewTransform", modelToViewTransform)
    glUniform1f(glGetUniformLocation(g_simpleShader, "sphereRadius"), g_sphereRadius)
    glUniform1f(glGetUniformLocation(g_simpleShader, "sphereDistance"), length(g_viewPosition - g_spherePos))
    glUniform3fv(glGetUniformLocation(g_simpleShader, "sphereColour"), 1, np.ascontiguousarray(g_sphereColour, dtype=np.float32))

    # Bind gl object storing the shphere mesh data (it is set up in main)
    glBindVertexArray(g_sphereVertexArrayObject)

    # https:#www.khronos.org/registry/OpenGL-Refpages/gl4/html/glDrawArrays.xhtml
    # Tell OpenGL to draw triangles using data from the currently bound vertex array object by grabbing
    # three at a time vertices from the array up to g_numSphereVerts vertices, for(int i = 0; i < g_numSphereVerts; i += 3) ... draw triangle ...
    glDrawArrays(GL_TRIANGLES, 0, g_numSphereVerts)
	
    # Unbind the shader program & vertex array object to ensure it does not affect anything else (in this simple program, no great risk, but otherwise it pays to be careful)
    glBindVertexArray(0);
    glUseProgram(0);


    glutSwapBuffers() 

# Turns a multidimensional array (3d?) into a 1D array, somehow...
def flatten(*lll):
	return [u for ll in lll for l in ll for u in l]

def createVertexArrayObject(vertexPositions):
    #GLuint &positionBuffer, GLuint &vertexArrayObject
    # glGen*(<count>, <array of GLuint>) is the typical pattern for creating objects in OpenGL. Do pay attention to this idiosyncrasy as the first parameter indicates the
    # number of objects we want created. Usually this is just one, but if you were to change the below code to '2' OpenGL would happily overwrite whatever is after
    # 'positionBuffer' on the stack (this leads to nasty bugs that are sometimes very hard to detect - i.e., this was a poor design choice!)
    positionBuffer = glGenBuffers(1)
    glBindBuffer(GL_ARRAY_BUFFER, positionBuffer)

    # re-package python data into something we can send to OpenGL
    flatData = flatten(vertexPositions)
    data_buffer = (c_float*len(flatData))(*flatData)
    # Upload data to the currently bound GL_ARRAY_BUFFER, note that this is completely anonymous binary data, no type information is retained (we'll supply that later in glVertexAttribPointer)
    glBufferData(GL_ARRAY_BUFFER, data_buffer, GL_STATIC_DRAW)

    vertexArrayObject = glGenVertexArrays(1)
    glBindVertexArray(vertexArrayObject)

    # The positionBuffer is already bound to the GL_ARRAY_BUFFER location. This is typycal OpenGL style - you bind the buffer to GL_ARRAY_BUFFER,
    # and the vertex array object using 'glBindVertexArray', and then glVertexAttribPointer implicitly uses both of these. You often need to read the manual
    # or find example code.
    # 'VAL_Position' is an integer, which tells it which array we want to attach this data to, this must be the same that we set up our shader
    # using glBindAttribLocation. Next provide we type information about the data in the buffer: there are three components (x,y,z) per element (position)
    # and they are of type 'float'. The last arguments can be used to describe the layout in more detail (stride  & offset).
    # Note: The last adrgument is 'pointer' and has type 'const void *', however, in modern OpenGL, the data ALWAYS comes from the current GL_ARRAY_BUFFER object, 
    # and 'pointer' is interpreted as an offset (which is somewhat clumsy).
    glVertexAttribPointer(VAL_Position, 3, GL_FLOAT, GL_FALSE, 0, None)
    # For the currently bound vertex array object, enable the VAL_Position'th vertex array (otherwise the data is not fed to the shader)
    glEnableVertexAttribArray(VAL_Position)

    # Unbind the buffers again to avoid unintentianal GL state corruption (this is something that can be rather inconventient to debug)
    glBindBuffer(GL_ARRAY_BUFFER, 0)
    glBindVertexArray(0)
    return (positionBuffer, vertexArrayObject)



glutInitContextFlags(GLUT_DEBUG);

glutInit("")
glutInitDisplayMode(GLUT_RGB | GLUT_DEPTH | GLUT_DOUBLE)
glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_GLUTMAINLOOP_RETURNS)

glutInitWindowSize(g_startWidth, g_startHeight)

glutCreateWindow(b"A 'simple' OpenGL >= 3.0 program")

glDebugMessageCallback(GLDEBUGPROC(debugMessageCallback), None)

# (although this glEnable(GL_DEBUG_OUTPUT) should not have been needed when using the GLUT_DEBUG flag above...)
glEnable(GL_DEBUG_OUTPUT)
# This ensures that the callback is done in the context of the calling function, which means it will be on the stack in the debugger, which makes it
# a lot easier to figure out why it happened.
glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS)

print("--------------------------------------\nOpenGL\n  Vendor: %s\n  Renderer: %s\n  Version: %s\n--------------------------------------\n"%(glGetString(GL_VENDOR).decode("utf8"), glGetString(GL_RENDERER).decode("utf8"), glGetString(GL_VERSION).decode("utf8")));

vertexShader ="""
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
}"""
fragmentShader = """
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
"""

g_simpleShader = glCreateProgram()

if compileAndAttachShader(g_simpleShader, GL_VERTEX_SHADER, vertexShader) and compileAndAttachShader(g_simpleShader, GL_FRAGMENT_SHADER, fragmentShader):
	# Link the name we used in the vertex shader 'positionIn' to the integer index we chose in 'VAL_Position'
	# This ensures that when the shader executes, data fed into 'positionIn' will be sourced from the VAL_Position'th generic attribute stream
	# This seemingly backwards way of telling the shader where to look allows OpenGL programs to swap vertex buffers without needing to do any string lookups at run time. 
    glBindAttribLocation(g_simpleShader, VAL_Position, "positionIn")

	# If we have multiple images bound as render targets, we need to specify which 'out' variable in the fragment shader goes where
	# in this case it is totally redundant as we only have one (the default render target, or frame buffer) and the default binding is always zero.
    glBindFragDataLocation(g_simpleShader, 0, "fragmentColor")

    glLinkProgram(g_simpleShader)
    linkStatus = glGetProgramiv(g_simpleShader, GL_LINK_STATUS)
    if not linkStatus:
        err = glGetProgramInfoLog(g_simpleShader)
        print("SHADER LINKER ERROR: '%s'"% err)
        sys.exit(1)


sphereVerts = createSphere(g_numSphereSubdivs)
g_numSphereVerts = len(sphereVerts)
(g_sphereVertexDataBuffer, g_sphereVertexArrayObject) = createVertexArrayObject(sphereVerts)

glEnable(GL_CULL_FACE)
glEnable(GL_DEPTH_TEST)
glDepthFunc(GL_LEQUAL)

# Tell GLUT to call 'onGlutDisplay' whenever it needs to re-draw the window.
glutDisplayFunc(onGlutDisplay);

glutMainLoop()
