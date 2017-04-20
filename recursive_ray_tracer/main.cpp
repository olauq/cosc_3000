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

// 2. Definition of spheres is now located in the main function

// 3. Definition of virtual camera
static vec3 g_viewPosition = { 0.0f, 0.0f, -10.0f };
static vec3 g_viewTarget = { 0.0f, 0.0f, 0.0f };
static vec3 g_viewUp = { 0.0f, 1.0f, 0.0f };

const int g_maxDepth = 8;

static float g_fov = 45.0f;

static vec3 g_ambientLight = { 0.2f, 0.2f, 0.2f };
static vec3 g_lightPosition = { -100.0f, 100.0f, 20.0f };
static vec3 g_lightColour = { 0.9f, 0.9f, 0.9f };

// Tiny offset used to get reflection and shadow rays a starting point outside of the object that was just hit 
// since floating point arithmetic is of limited precision, this kind of thing is usually needed.
// Experiment by setting it to 0 and see what happens!
const float g_rayEpsilon = 0.001f;
// 4. Misc...
const vec3 g_backGroundColour = { 0.0f, 0.0f, 0.0f};
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
 * Helper to make a ray.
 */
Ray makeRay(vec3 origin, vec3 direction)
{
	Ray r;
	r.origin = origin;
	r.direction = direction;
	return r;
}

/**
 * Structure information about a hit point. By default initialized to represent not having hit anything.
 * We chose to repreesnt this using the maximum number floats can representation.
 * This struct could be extended with more information about the hit point, for example the normal is usuall very useful, and instead of the object
 * maybe representing the material (an object might have several materials) is more useful, or maybe just a function object that returns the shading?
 */
struct HitInfo
{
	static constexpr float s_missTime = std::numeric_limits<float>::max();

	HitInfo() : time(s_missTime), object(nullptr) { }

	/**
	 * Returns true if the info represents a valid hit.
	 */
	inline bool valid() const
	{
		return object != nullptr && time < s_missTime;
	}

	vec3 position;
	vec3 normal;

	float time;
	Object *object;
};


/**
 * Base class for objects that can be traced.
*  As an excercise, why not add a plane?
 */
class Object
{
public:

	/**
	 * intersect is a pure virtual function, it must be overridden in all derived classes. Derive to implent different object types
	 * (see Sphere below). The ray direction should not be expected to be normalized, and the time value calculated should be scaled
	 * by the length (this follows naturally from most intersection routines).
	 */
	virtual HitInfo intersect(const Ray &ray) = 0;

	// This is sensibly replaced with a more comprehensive material class, or some such that takes care of the shading calculation!
	vec3 diffuseColour; 
	float reflectivity;
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

			// This implementation must provide the position and normal. This data is useful to compute the shading.
			// However, since this data may be costly and complex to compute (e.g., procedural surface) a more fully
			// featured ray-tracer might be better off calculating this as part of the shading.
			hit.position = ray.origin + ray.direction * t;
			hit.normal = normalize(hit.position - position);
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
Sphere *makeSphere(const vec3 &position, float radius, const vec3 &colour, const float reflectivity = 0.5f)
{
	Sphere *sphere = new Sphere;

	sphere->position = position;
	sphere->radius = radius;
	sphere->diffuseColour = colour;
	sphere->reflectivity = reflectivity;

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

// Forward declaration, needed in C/C++
vec3 shade(const Ray &ray, const HitInfo &hit, int depth);

/**
 * Finds the closest (smallest time value) intersection with the objects. 
 * The hit info is initially invalid, and this is returned if no hit was found.
 * Note that the ray does not have to be normalized (unit length), the time value is simply scaled by the length.
 */
HitInfo findClosestIntersection(const Ray &ray, const std::vector<Object*> &objects)
{
	// A hit info is intialized to float max time.
	HitInfo best;

	// This uses a linear search which is fine for a small handful of objects, to support a large scene
	// an acceleration structure such as a Bounding Volume Hierarchy (BVH) should be used.
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
 * Traces a ray through the scene (here represented by a list of objects), returns information about the intersection point.
 * In a recursive ray tracer this information would include the shading at the intersection point.
 */
vec3 trace(const Ray &ray, const std::vector<Object*> &objects, int depth = 0)
{
	HitInfo hit = findClosestIntersection(ray, objects);

	// If a hit point was found...
	if (hit.valid())
	{
		//...call 'shade' to calculte the colour.
		return shade(ray, hit, depth);
	}
	// Otherwise we just return the background colour.
	return g_backGroundColour;
}



/**
 * Returns true if anything is in the way of the ray over the distance specified ('maxDistance'), and false otherwise.
 * Having a 'maxDistance' lets us make sure that we don't get intersections for things behind the light source. Since 
 * this form of query does not care about the nearest hit, it can be more efficient since we can return true as soon as
 * any hit is found. It is also different from 'trace' in that there is no recursive tracing.
 */
bool isRayOccluded(const Ray &ray, const std::vector<Object*> &objects, float maxDistance)
{
	for (auto o : objects)
	{
		HitInfo h = o->intersect(ray);

		if (h.time < maxDistance)
		{
			return true;
		}
	}

	return false;
}

/**
 * This funciton is called to calculate shading for the hit point.
 */
vec3 shade(const Ray &ray, const HitInfo &hit, int depth)
{
	// Things missing in this simple light model (experiment with adding them!): 
	//   1. Fresnel reflection (angle based reflectivity) 
	//   2. Physical light model, e.g., proper units for light intensity, fall-off for distance.
	//      - this pretty much requires a tone mapping step too to get to [0-1] RGB colour range.
	//        tone mapping is typically done as a post processing pass over the frame buffer.
	//   3. Specular reflection.
	//   4. Transparency & refraction.

	// 1. construct direction to the light (unit length vector)
	vec3 lightDir = normalize(g_lightPosition - hit.position);
	
	// 2. test for back facinng, cos(angle) is equivalent to the length of the projection of the light direction on the normal,
	//    or vice versa, so positive values means they point in the same direction, or in other words, the light is in front of the
	//    hit point. If it is not, no light will hit the surface from this light source.
	float cosAngle = dot(lightDir, hit.normal);

	// Construct a shadow ray, this ray will be used to check for any obstruction between the light and point we are shading
	// if there is anything in the way, light is blocked and should not be added.
	// We offset the starting point slightly in the normal direction to avoid self-intersection. I.e., to not hit the object 
	// that the currently shaded point belongs to. Note that we don't offset in the light direction since it may be nearly 
	// tangential, which would then fail to move the starting point outside of the hit object.
	Ray shadowRay = makeRay(hit.position + hit.normal * g_rayEpsilon, lightDir);
	
	// Ambient light is a huge hack and is there to replace all the global illumination effects of indirect light bouncing around the scene.
	// If we did not use this term, any surface not facing the light would be pitch black.
	vec3 light = g_ambientLight;

	// check backfacing and if it passes, check for occlusion. Note: C++ has lazy evaluation for logical expressions which means the 
	// shadow ray will not be tested unless the angle test passes.
	if (cosAngle > 0.0f && !isRayOccluded(shadowRay, g_objects, length(g_lightPosition - hit.position)))
	{
		// Light is arriving at the surface from the light, add contribution.
		// Here a trivial lambertian light model, which just depends on the cos(angle) which we happily already calculated.
		light += g_lightColour * cosAngle;
	}

	// The light (both ambient and possible diffuse) is modulated by the material diffuse colour to produce the final 
	// reflected diffuse light.
	vec3 resultColour = hit.object->diffuseColour * light;

	// If we're not too deep (application specified constant, could be replaced with weight based limit
	// since as we get deeper the contribution to the pixel colour diminishes, unless pure mirrors).
	if (depth < g_maxDepth && hit.object->reflectivity > 0.0f)
	{
		// Construct reflection ray.
		Ray reflectionRay;
		// reflect the ray direction around the normal
		reflectionRay.direction = glm::reflect(ray.direction, hit.normal);
		// starting point of reflection ray. We offset the starting point slightly in the normal direction
		// to avoid self-intersection. Note that we don't offset in the reflection direction since it may be nearly tangential.
		// Which would then fail to move the starting point outside of the hit object.
		reflectionRay.origin = hit.position + hit.normal * g_rayEpsilon;
		resultColour += trace(reflectionRay, g_objects, depth + 1) * hit.object->reflectivity;
	}

	return resultColour;
}


// Callback that is called by GLUT system when a frame needs to be drawn, set up in main() using 'glutDisplayFunc'
static void onGlutDisplay()
{
	Camera camera = makeCamera(glutGet(GLUT_WINDOW_WIDTH), glutGet(GLUT_WINDOW_HEIGHT), g_viewPosition, g_viewTarget, g_viewUp, g_fov);

	// memory to use to store the pixels to
	std::vector<vec3> pixels(camera.width * camera.height, g_backGroundColour);
	
	// loop over all the pixel locations.
	
	// It is trivial to use all the processor cores since rays are independent: 
	// For a trivial, compute bound, scene like four spheres, this should get more or less linear speed up.
	//     #pragma omp parallel for
	for (int y = 0; y < camera.height; ++y)
	{
		for (int x = 0; x < camera.width; ++x)
		{
			Ray r = generatePinHolePrimaryRay(x, y, camera);

			pixels[y * camera.width + x] = trace(r, g_objects); 
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
	g_objects.push_back(makeSphere(vec3(-3.2f, 0.0f, 0.0f), 1.5f, vec3(0.2f, 0.3f, 1.0f))); // blue sphere to the left
	g_objects.push_back(makeSphere(vec3(0.0f, 2.0f, 0.0f), 1.5f, vec3(0.2f, 0.9f, 0.3f))); // green sphere in the middle and up a bit
	g_objects.push_back(makeSphere(vec3(3.2f, 0.0f, 0.0f), 1.5f, vec3(0.8f, 0.4f, 0.1f))); // red sphere to the right.
	g_objects.push_back(makeSphere(vec3(0.0f, -1.0f, 0.0f), 1.0f, vec3(0.1f), 0.9f)); // smaller dark gray with high reflectivity
	g_objects.push_back(makeSphere(vec3(0.0f, -1003.0f, 0.0f), 1000.0f, vec3(0.8f), 0.0f)); // huge light gray sphere underneath, no refleciton

	glutDisplayFunc(onGlutDisplay);

	glutMainLoop();

	return 0;
}
