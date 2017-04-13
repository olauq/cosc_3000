# cosc_3000
Extra sample code for the studens of the (graphics part) of the course COSC3000 at the University of Queensland. 

## Projects:
All of the examples come with their own visual studio 2015 projects and solutions, found in the respective sub-folder.
They all use FreeGLUT [1], the OpenGL Extension Wrangler (GLEW) [2] and the OpenGL Math Library (GLM) [3].

The bin/win_x64 folder contains the dlls for 64 bit windows, this is where all the solutions output their executables.

In lib/win_x64 you'll find the glut and glew librariers for 64-bit windows.

The include/GL folder contains the headers for the FreeGLUT and GLEW librariers.

GLM is also included in the glm folder, this is a header-only library so there is no corresponding library or dll.


### ray_tracer
This proect implements a ray tracer that is as simple as I could make it. It directly ray-traces a single hard coded sphere.
For a more well-structurd, and extensible (but somewhat longer) ray tracer, please see the 'structured_ray_tracer' proect.

### rasterizer
Same graphical output as the ray-tracer, using a minimum of non-legacy OpenGL. This means not using any fixed funciton for shading
or glVertex and the like for geometry. This in turn means that the program has to contain the code to generate a triangle mesh
for the sphere. For an explanation of part of the program, see the docs/createVertexArrayObject.ppsx power point slide deck.

### rasterizer_legacy_gl
Same again, but this time using legacy OpenGL, and glutSolidSphere to simplify the program. Notably, the shading is different though
since the fixed function pipeline cannot readily re-create my distance based shading I hacked up for the previous examples.

### rasterizer_python
Python implementation of the rasterizer project (that is modern OpenGL) - NOT currently working for some reason.

### structured_ray_tracer
ray tracer implementation with more structure that makes the main loop more easy to read, it is also a much better starting point for 
building something supporting more than one object and multiple materials. Or if one wanted to plug in an acceleration structure say.

## References
[1] FreeGLUT http://freeglut.sourceforge.net/
[2] GLEW http://glew.sourceforge.net/
[3]	GLM  http://glew.sourceforge.net/
