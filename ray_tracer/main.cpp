/****************************************************************************/
/* Copyright (c) 2016, Ola Olsson */
/****************************************************************************/
#include <GL/freeglut.h>

#include <glm/glm.hpp>

#include <stdio.h>
#include <vector>
#include <algorithm>

// We're using the 2 & 3 dimensional vectors of GLM so alias these type names to 'vec2' and 'vec3'.
// The type of the elements of these types is 'float' i.e., single precision (32-bit) floating point numbers.
using glm::vec3;
using glm::vec2;

// 1. Initial window size for GLUT
const int g_startWidth  = 1280;
const int g_startHeight = 720;

// 2. Definition of sphere.
static vec3 g_spherePos = { 0.0f, 0.0f, 0.0f };
static float g_sphereRadius = 3.0f;
const vec3 g_sphereColour = { 0.2f, 0.3f, 1.0f};

// 3. Definition of virtual camera
static vec3 g_viewPosition = { 0.0f, 0.0f, -10.0f };
static vec3 g_viewTarget = { 0.0f, 0.0f, 0.0f };
static vec3 g_viewUp = { 0.0f, 1.0f, 0.0f };
static float g_fov = 45.0f;

// 4. Misc...
const vec3 g_backGroundColour = { 0.1f, 0.2f, 0.1f};
static const float g_pi = 3.1415f;
inline float degreesToRadians(float degs)
{
	return degs * g_pi / 180.0f;
}

// 5. Routine that calculates the intersection of a ray (parametric 3D line), (origin, direction) and a sphere (centre, radius)
//    if an intersection is found, hitDistance contains the distance to the hit point.
//    Note: hitDistance is only the distance iff rayD is of unit length, strictly speaking it is the parameter of the parametric line that gives the intersection point.
inline bool intersectRaySphere(const vec3 &rayO, const vec3 &rayD, const vec3 &spherePos, float sphereRad, float &hitDistance)
{
	// vector from sphere to ray
	vec3 m = rayO - spherePos;
	// Project on ray direction.
	float b = dot(m, rayD);
	// Hm, not sure, best check the book
	float c = dot(m, m) - sphereRad * sphereRad;

	// Exit if r’s origin outside s (c > 0) and r pointing away from s (b > 0) 
	if (c > 0.0f && b > 0.0f)
	{
		return false;
	}
	float discr = b * b - c;

	// A negative discriminant corresponds to ray missing sphere 
	if (discr < 0.0f)
	{
		return false;
	}
	// Ray now found to intersect sphere, compute smallest t value of intersection
	// If t is negative, ray started inside sphere so clamp t to zero 
	hitDistance = std::max(0.0f, -b - sqrtf(discr));

	return true;
}


// Callback that is called by GLUT system when a frame needs to be drawn, set up in main() using 'glutDisplayFunc'
static void onGlutDisplay()
{
	int height = glutGet(GLUT_WINDOW_HEIGHT);
	int width = glutGet(GLUT_WINDOW_WIDTH);

	// 1. Form camera forwards vector
	vec3 viewDir = normalize(g_viewTarget - g_viewPosition);
	// 2. Figure out image plane sideways (left?) direction
	vec3 viewLeft = normalize(cross(g_viewUp, viewDir));
	// 3. Figure out image up direction
	vec3 viewUp = cross(viewDir, viewLeft);

	const float aspectRatio = float(width) / float(height);

	// memory to use to store the pixels to
	std::vector<vec3> pixels(width * height, g_backGroundColour);
	for (int y = 0; y < height; ++y)
	{
		for (int x = 0; x < width; ++x)
		{
			vec2 pixelNormCoord = vec2(float(x) / float(width), float(y) / float(height)) * 2.0f - 1.0f;

			vec3 rayOrigin = g_viewPosition;

			vec3 rayDir = normalize(viewDir +
				tanf(degreesToRadians(g_fov / 2.0f)) * viewUp * pixelNormCoord.y +
				tanf(degreesToRadians(g_fov / 2.0f)) * aspectRatio * viewLeft * pixelNormCoord.x);

			float hitDistance = 0.0f;
			if (intersectRaySphere(rayOrigin, rayDir, g_spherePos, g_sphereRadius, hitDistance))
			{
				// Calculate some totally arbitrary shading based on the distance:
				float sphereDistance = length(rayOrigin - g_spherePos);
				float shading = 1.0f - (hitDistance - sphereDistance + g_sphereRadius) / (2.0f * g_sphereRadius);

				pixels[y * width + x] = g_sphereColour * shading;
			}
		}
	}

	// Must check for empty vector
	if (!pixels.empty())
	{
		glDrawPixels(width, height, GL_RGB, GL_FLOAT, &pixels[0]);
	}
	glutSwapBuffers();
}



int main(int argc, char* argv[])
{
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_RGB | GLUT_DEPTH | GLUT_DOUBLE);
	glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_GLUTMAINLOOP_RETURNS);

	glutInitWindowSize(g_startWidth, g_startHeight);
	glutCreateWindow("A rather short \"ray tracer\"");

	glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	glutSwapBuffers();

	printf("--------------------------------------\nOpenGL\n  Vendor: %s\n  Renderer: %s\n  Version: %s\n--------------------------------------\n", glGetString(GL_VENDOR), glGetString(GL_RENDERER), glGetString(GL_VERSION));

	glutDisplayFunc(onGlutDisplay);

	glutMainLoop();

	return 0;
}
