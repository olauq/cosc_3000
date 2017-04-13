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
struct Camera
{
	int height;
	int width;
	vec3 viewDir;
	vec3 viewLeft;
	vec3 viewUp;
	vec3 position;
	float fovY;
	float aspectRatio;
};


struct Ray
{
	vec3 origin;
	vec3 direction;
};


struct HitInfo
{
	float time;
	Object *object;
};


// base class for objects that can be traced, as an excercise, why not add a plane?
// TODO: do this a bit more C++11ish and use lambdas instead for both?
class Object
{
public:
	//std::function<HitInfo(const Ray &ray)> intersect;

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


// Implementation of sphere object type
class Sphere : public Object
{
public:
	virtual HitInfo intersect(const Ray &ray) override
	{
		HitInfo hit;
		hit.time = std::numeric_limits<float>::max();
		float t = 0.0f;

		if (intersectRaySphere(ray.origin, ray.direction, position, radius, t))
		{
			hit.object = this;
			hit.time = t;
		}
		return hit;
	}

	vec3 position;
	float radius;
};



Ray generatePrimaryRay(int x, int y, const Camera &c)
{
	Ray r;

	vec2 pixelNormCoord = vec2(float(x) / float(c.width), float(y) / float(c.height)) * 2.0f - 1.0f;

	r.origin = c.position;

	r.direction = normalize(c.viewDir +
		tanf(degreesToRadians(c.fovY / 2.0f)) * c.viewUp * pixelNormCoord.y +
		tanf(degreesToRadians(c.fovY / 2.0f)) * c.aspectRatio * c.viewLeft * pixelNormCoord.x);

	return r;
}


HitInfo trace(const Ray &ray, const std::vector<Object*> &objects)
{
	HitInfo best;
	best.object = nullptr;
	best.time = std::numeric_limits<float>::max();
	for (auto o : objects)
	{
		HitInfo h = o->intersect(ray);
		if (h.time < best.time)
		{
			best = h;
		}
	}

	return best;
}


vec3 shade(const Ray &ray, const HitInfo &hit)
{
	float objectDistance = length(ray.origin - g_spherePos);
	float shading = 1.0f - (hit.time - objectDistance + g_sphereRadius) / (2.0f * g_sphereRadius);
	return hit.object->colour * shading;
}

// Callback that is called by GLUT system when a frame needs to be drawn, set up in main() using 'glutDisplayFunc'
static void onGlutDisplay()
{
	Camera camera;
	camera.height = glutGet(GLUT_WINDOW_HEIGHT);
	camera.width = glutGet(GLUT_WINDOW_WIDTH);

	camera.position = g_viewPosition;
	// 1. Form camera forwards vector
	camera.viewDir = normalize(g_viewTarget - g_viewPosition);
	// 2. Figure out image plane sideways (left?) direction
	camera.viewLeft = normalize(cross(g_viewUp, camera.viewDir));
	// 3. Figure out image up direction
	camera.viewUp = cross(camera.viewDir, camera.viewLeft);
	camera.aspectRatio = float(camera.width) / float(camera.height);
	camera.fovY = g_fov;

	// memory to use to store the pixels to
	std::vector<vec3> pixels(camera.width * camera.height, g_backGroundColour);
	for (int y = 0; y < camera.height; ++y)
	{
		for (int x = 0; x < camera.width; ++x)
		{
			Ray r = generatePrimaryRay(x, y, camera);

			HitInfo h = trace(r, g_objects);

			if (h.time < std::numeric_limits<float>::max())
			{
				pixels[y * camera.width + x] = shade(r, h);
			}
		}
	}

	// Must check for empty vector
	if (!pixels.empty())
	{
		glDrawPixels(camera.width, camera.height, GL_RGB, GL_FLOAT, &pixels[0]);
	}
	glutSwapBuffers();
}



int main(int argc, char* argv[])
{
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_RGB | GLUT_DEPTH | GLUT_DOUBLE);
	glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_GLUTMAINLOOP_RETURNS);

	glutInitWindowSize(g_startWidth, g_startHeight);
	glutCreateWindow("A somewhat more structured and extensible ray tracer");

	glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	glutSwapBuffers();

	printf("--------------------------------------\nOpenGL\n  Vendor: %s\n  Renderer: %s\n  Version: %s\n--------------------------------------\n", glGetString(GL_VENDOR), glGetString(GL_RENDERER), glGetString(GL_VERSION));


	// Set up scene:
	Sphere sphere;
	sphere.colour = g_sphereColour;
	sphere.position = g_spherePos - vec3(3.2f, 0.0f, 0.0f);;
	sphere.radius = g_sphereRadius / 2.0f;

	g_objects.push_back(&sphere);

	Sphere sphere2;
	sphere2.colour = vec3(0.2f, 0.9f, 0.3f);
	sphere2.position = g_spherePos + vec3(0.0f, 2.0f, 0.0f);
	sphere2.radius = g_sphereRadius / 2.0f;

	g_objects.push_back(&sphere2);

	Sphere sphere3;
	sphere3.colour = vec3(0.8f, 0.4f, 0.1f);
	sphere3.position = g_spherePos + vec3(3.2f, 0.0f, 0.0f);
	sphere3.radius = g_sphereRadius / 2.0f;

	g_objects.push_back(&sphere3);

	glutDisplayFunc(onGlutDisplay);

	glutMainLoop();

	return 0;
}
