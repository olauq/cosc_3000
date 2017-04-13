/****************************************************************************/
/* Copyright (c) 2016, Ola Olsson */
/****************************************************************************/
#include <GL/freeglut.h>

#include <glm/glm.hpp>

#include <stdio.h>
#include <vector>
#include <algorithm>
#include <functional>

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

// Forward declaraion so we can use the name 
class Object;

// Scene object list (initialized in main)
std::vector<Object*> g_objects;

// Data types:

/**
 * The Camera structure contains all the information needed to describe the camera model.
 */
struct Camera
{
	int width;
	int height;
	vec3 dir;
	vec3 left;
	vec3 up;
	vec3 position;
	float fovY;
	float aspectRatio;
};

/**
 * Initializes the Camera structure. Calculates all derived properties of the camera.
 */
Camera makeCamera(int screenWidth, int screenHeight, const vec3 &cameraPos, const vec3 &cameraTarget, const vec3 &cameraUp, float  verticalFOV)
{
	Camera camera;
	camera.width = screenWidth;
	camera.height = screenHeight;

	camera.position = cameraPos;
	// 1. Form camera forwards vector
	camera.dir = normalize(cameraTarget - cameraPos);
	// 2. Figure out image plane sideways (left?) direction
	camera.left = normalize(cross(cameraUp, camera.dir));
	// 3. Figure out image up direction
	camera.up = cross(camera.dir, camera.left);
	camera.aspectRatio = float(camera.width) / float(camera.height);
	camera.fovY = verticalFOV;

	return camera;
}

/**
 * Structure representing a parametric ray with an origin and a direction.
 */
struct Ray
{
	vec3 origin;
	vec3 direction;
};

/**
 * Structure information about a hit point. By default initialized to represent not having hit anything.
 * We chose to repreesnt this using the maximum number floats can representation.
 * This struct could be extended with more information about the hit point, for example the normal is usuall very useful, and instead of the object
 * maybe representing the material (an object might have several materials) is more useful, or maybe just a function object that returns the shading?
 */
struct HitInfo
{
	HitInfo() : time(std::numeric_limits<float>::max()), object(nullptr) { }

	float time;
	Object *object;
};


// base class for objects that can be traced, as an excercise, why not add a plane?
// TODO: do this a bit more C++11ish and use lambdas instead for both?
class Object
{
public:
	//std::function<HitInfo(const Ray &ray)> intersect;

	/**
	 * intersect is a pure virtual function, it must be overridden in all derived classes. Derive to implent different object types
	 * (see Sphere below).
	 */
	virtual HitInfo intersect(const Ray &ray) = 0;

	vec3 colour; // This is sensibly replaced with a more comprehensive material class, or some such that takes care of the shading calculation!
};


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


/**
 * Implementation of sphere object type
 */
class Sphere : public Object
{
public:

	/**
	 * Overrides the base implementation for a sphere. 
	 */
	virtual HitInfo intersect(const Ray &ray) override
	{
		// Initially a miss!
		HitInfo hit;
		float t = 0.0f;

		if (intersectRaySphere(ray.origin, ray.direction, position, radius, t))
		{
			hit.object = this;
			hit.time = t;
		}
		return hit;
	}

	// Data for representing a sphere.
	vec3 position;
	float radius;
};

/**
* Helper function to make a sphere.
* (Yes, allocating using new and returning sets someone else up to deal with the memory, leak-prone! std::unique_ptr/shared_ptr might be the answer)
*/
Sphere *makeSphere(const vec3 &position, float radius, const vec3 &colour)
{
	Sphere *sphere = new Sphere;

	sphere->position = position;
	sphere->radius = radius;
	sphere->colour = colour;

	return sphere;
}


/**
 * Generates a ray through the pixel (x,y). The ray has unit length and origin at the camera position.
 * Uses the pin-hole camera model, to change the model we could just generate a different distribution.
 */
Ray generatePinHolePrimaryRay(int x, int y, const Camera &c)
{
	Ray r;

	vec2 pixelNormCoord = vec2(float(x) / float(c.width), float(y) / float(c.height)) * 2.0f - 1.0f;

	r.origin = c.position;

	r.direction = normalize(c.dir +
		tanf(degreesToRadians(c.fovY / 2.0f)) * c.up * pixelNormCoord.y +
		tanf(degreesToRadians(c.fovY / 2.0f)) * c.aspectRatio * c.left* pixelNormCoord.x);

	return r;
}

/**
 * Traces a ray through the scene (here represented by a list of objects), returns information about the intersection point.
 * In a recursive ray tracer this information would include the shading at the intersection point.
 */
HitInfo trace(const Ray &ray, const std::vector<Object*> &objects)
{
	HitInfo best;

	for (auto o : objects)
	{
		HitInfo h = o->intersect(ray);

		// Check if new hit time is better
		if (h.time < best.time)
		{
			best = h;
		}
	}

	return best;
}

/**
 * This funciton is called to calculate shading. It should really be dependent on the material of the object hit,
 * and it should absolutely not use 'g_spherePos' etc.
 * In a recursive ray tracer it might want to call trace again to sample shadows, for example, 
 * although to make this neat, would require a bit more re-shuffling (e.g., trace needs to call shade etc...).
 */
vec3 shade(const Ray &ray, const HitInfo &hit)
{
	float objectDistance = length(ray.origin - g_spherePos);
	float shading = 1.0f - (hit.time - objectDistance + g_sphereRadius) / (2.0f * g_sphereRadius);
	return hit.object->colour * shading;
}


// Callback that is called by GLUT system when a frame needs to be drawn, set up in main() using 'glutDisplayFunc'
static void onGlutDisplay()
{
	Camera camera = makeCamera(glutGet(GLUT_WINDOW_WIDTH), glutGet(GLUT_WINDOW_HEIGHT), g_viewPosition, g_viewTarget, g_viewUp, g_fov);

	// memory to use to store the pixels to
	std::vector<vec3> pixels(camera.width * camera.height, g_backGroundColour);
	
	// loop over all the pixel locations.
	for (int y = 0; y < camera.height; ++y)
	{
		for (int x = 0; x < camera.width; ++x)
		{
			Ray r = generatePinHolePrimaryRay(x, y, camera);

			HitInfo h = trace(r, g_objects);

			if (h.time < std::numeric_limits<float>::max())
			{
				pixels[y * camera.width + x] = shade(r, h);
			}
		}
	}

	// Must check for empty vector (if window is made 0 size in either dimension)
	if (!pixels.empty())
	{
		// just copy the pixel data to the frame buffer.
		glDrawPixels(camera.width, camera.height, GL_RGB, GL_FLOAT, &pixels[0]);
	}

	// tell GLUT to get the OS to swap the back and front buffers.
	glutSwapBuffers();
}




int main(int argc, char* argv[])
{
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_RGB | GLUT_DEPTH | GLUT_DOUBLE);
	glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_GLUTMAINLOOP_RETURNS);

	glutInitWindowSize(g_startWidth, g_startHeight);
	glutCreateWindow("A somewhat more structured and extensible ray tracer");

	glutSwapBuffers();

	printf("--------------------------------------\nOpenGL\n  Vendor: %s\n  Renderer: %s\n  Version: %s\n--------------------------------------\n", glGetString(GL_VENDOR), glGetString(GL_RENDERER), glGetString(GL_VERSION));

	// Set up scene:
	g_objects.push_back(makeSphere(g_spherePos - vec3(3.2f, 0.0f, 0.0f), g_sphereRadius / 2.0f, g_sphereColour));
	g_objects.push_back(makeSphere(g_spherePos + vec3(0.0f, 2.0f, 0.0f), g_sphereRadius / 2.0f, vec3(0.2f, 0.9f, 0.3f)));
	g_objects.push_back(makeSphere(g_spherePos + vec3(3.2f, 0.0f, 0.0f), g_sphereRadius / 2.0f, vec3(0.8f, 0.4f, 0.1f)));

	glutDisplayFunc(onGlutDisplay);

	glutMainLoop();

	return 0;
}
