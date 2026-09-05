// Termm--Fall 2020

#include <glm/ext.hpp>

#include "render/Renderer.hpp"

#include <iomanip>
#include "core/Ray.hpp"
#include "render/Sampling.hpp"
#include "render/Camera.hpp"
#include "geometry/BVH.hpp"
#include "scene/PhongMaterial.hpp"
#include "core/dbgPrint.hpp"
#include <lodepng/lodepng.h>
#include <string>
#include <chrono>
#include <thread>

using namespace std;
using namespace glm;

#define REFLECTION 01

// Anti-aliasing && depth of field are now runtime settings, driven from
// Lua (gr.set_lens / gr.set_aa) rather than #defines, so a scene can turn
// them on without a rebuild. Both default to off, matching the old
// behaviour exactly.

static const float EPS = 0.000001f; // correction factor
static const float MAX_RGB = 255.0f; // maximum rgb value
static const float MAX_T = numeric_limits<float>::max();// max t distance
static const int REFLECTION_HITS = 3; // number of reflection bounces
static const float REFLECTION_COEFF = 0.25;

// Set from Lua before gr.render. Defaults keep the pinhole camera.
static LensConfig g_lens;
static int        g_aaSamples = 1; // 1 == no anti-aliasing

void A4_SetLens(float apertureRadius, float focusDistance, int samples) {
	g_lens.apertureRadius = apertureRadius;
	g_lens.focusDistance  = focusDistance;
	g_lens.samples        = samples;
}

void A4_SetAntiAliasing(int samples) {
	g_aaSamples = (samples < 1) ? 1 : samples;
}


//---------------------------------------------------------------------
vec3 rayTraceRGB(
	// What to render  
	SceneNode * root,
	// The ray 
	Ray &ray,
	const glm::vec3 & eye,
	// Lighting parameters  
	const glm::vec3 & ambient,
	const std::list<Light *> & lights,
	const int reflectionHits,
	const size_t y ,// y position
	const size_t x ,// x position
	const size_t h ,// output image height
	const size_t w , // output image width
	const LoadedPng & bgPng // background image details
){
	HitRecord record;
	vec3 returnColor;
	

	// EPS acts as minimum, check if valid enough
	if(root->isHit(ray, EPS, MAX_T,record)){
		// Hit happened

		record.normalVec = normalize(record.normalVec);
		record.hitPointVec += record.normalVec * EPS;

		PhongMaterial *material = static_cast<PhongMaterial *>(record.material);
		
		// Add ambience 
		returnColor += material->getDiffuse() * ambient;
		
		for (Light * light : lights){
			Ray shadeRay;
			shadeRay.setOrigin(record.hitPointVec);
			shadeRay.setDirection(light->position - record.hitPointVec);

			HitRecord shadeRecord;

			if(root->isHit(shadeRay, EPS,MAX_T,shadeRecord)){
				// Shaderay hits an object, no need for light 
				continue;
			}

			vec3 L = normalize(shadeRay.getDirection());// vector pointing towards light from hitPoint
			vec3 V = normalize(eye - record.hitPointVec); // vector pointing towards eye from hitPoint
			vec3 N = normalize(record.normalVec);// normal vector at hitPoint
			vec3 H = normalize(V + L);// half vector, bisecting vector
			
			// Add diffuse
			returnColor += std::max(0.0, (double)dot(N,L)) * material->getDiffuse() * light->colour;

			// Add specular 
			returnColor += pow(std::max(0.0, (double)dot(N,H)),material->getShininess()) *
						   material->getSpecular() * light->colour;
		}

		if (REFLECTION > 0 && reflectionHits > 0){
			vec3 refDirVec = ray.getDirection() - 2 * record.normalVec * dot(ray.getDirection(),record.normalVec);
			Ray refRay;
			refRay.setOrigin(record.hitPointVec);
			refRay.setDirection(refDirVec);
			// TODO: Clarify mix use
			returnColor = glm::mix(returnColor,rayTraceRGB(root, refRay,eye,ambient,lights,reflectionHits - 1,y,x,h,w,bgPng),REFLECTION_COEFF );
		}

	}else{
		// Miss happened
		// Remove background reflections
		if (reflectionHits < REFLECTION_HITS){
			return returnColor;
		}
		// Sample the background texture.
		//
		// This used to take a 1:1 pixel crop from the centre of the texture.
		// That made the visible framing depend on render resolution, &&
		// indexed outside the decoded image -- segfaulting -- as soon as the
		// render was larger than the texture. Map the frame onto the texture
		// in normalised coordinates instead, scaled to cover the frame
		// without distorting its aspect ratio, then clamp. Resolution
		// independent, && in bounds by construction.
		const int texW = (int) bgPng.loadedWidth;
		const int texH = (int) bgPng.loadedHeight;
		if (texW > 0 && texH > 0) {
			const float frameAspect = (float) w / (float) h;
			const float texAspect   = (float) texW / (float) texH;

			// Cover: match the axis that would otherwise letterbox, crop the other.
			float uScale = 1.0f;
			float vScale = 1.0f;
			if (frameAspect > texAspect) {
				vScale = texAspect / frameAspect;
			} else {
				uScale = frameAspect / texAspect;
			}

			const float u = 0.5f + (((x + 0.5f) / (float) w) - 0.5f) * uScale;
			const float v = 0.5f + (((y + 0.5f) / (float) h) - 0.5f) * vScale;

			const int tx = glm::clamp((int)(u * texW), 0, texW - 1);
			const int ty = glm::clamp((int)(v * texH), 0, texH - 1);

			const size_t idx = 4u * ((size_t) ty * (size_t) texW + (size_t) tx);

			returnColor = vec3(bgPng.RGBA[idx],      // R
			                   bgPng.RGBA[idx + 1],  // G
			                   bgPng.RGBA[idx + 2]); // B
			returnColor = (returnColor / MAX_RGB) * 0.3f; // reduce intensity
		}

	
	}
	return returnColor;

}
//---------------------------------------------------------------------

	// Loop for each pixel in outPutImage
// Render one horizontal band of the image.
//
// Sampling structure, which is the part the old code got tangled:
// there is ONE loop over samples, && every sample goes through the
// same two stages -- jitter the pixel position (anti-aliasing), then
// turn that pixel direction into a ray (pinhole, || thin lens for
// depth of field). The total is divided by the sample count exactly
// once, at the end.
//
// The old version ran DoF && AA as two independent blocks that each
// accumulated into the same pixel, so with DoF on && AA off you got
// the DoF average PLUS a full-weight sharp sample layered on top. That
// is what the mysterious ".1 *" fudge factor was compensating for.
void generatePixelColors(
	// Image to write to, set to a given width && height 
	Image & image,
	size_t startIdx,
	size_t endIdx,
	vec3 initDirVec,
	size_t h,
	size_t w,
	const CameraBasis & cam,
	// Lighting parameters  
	const glm::vec3 & ambient,
	const std::list<Light *> & lights,
	
	SceneNode * root,
	const LoadedPng & bgPng,
	int threadIdx
){
	float progressFloat = 0.1f;
	float ratioFloat = 0.0f;

	// One generator per thread. Seeded from the thread index so a render
	// is reproducible run to run.
	Rng rng(1u + (uint32_t)threadIdx * 9781u);

	const vec3 & eye  = cam.eye;
	const vec3 & uVec = cam.uVec;
	const vec3 & vVec = cam.vVec;

	// Anti-aliasing && depth of field both cost samples; one loop
	// serves both, so turning on each multiplies rays per pixel once.
	const int aaSamples   = g_aaSamples;
	const int lensSamples = g_lens.enabled() ? g_lens.samples : 1;
	const int totalSamples = aaSamples * lensSamples;

	for (size_t y = startIdx ; y < endIdx; ++y) {
		for (unsigned int x = 0; x < w; ++x) {
			// Direction through the centre of this pixel.
			const vec3 centreDirVec = initDirVec + (float)(w-x) * uVec + (float)(y) * vVec;

			vec3 pixelColorVec(0.0f,0.0f,0.0f);

			for (int a = 0; a < aaSamples; ++a) {
				// Stage 1: where in the pixel does this sample look?
				// With aaSamples == 1 we take the exact centre, so the
				// image is bit-identical to the old pinhole render.
				vec3 dirVec = centreDirVec;
				if (aaSamples > 1) {
					dirVec += (rng.next() - 0.5f) * uVec
					        + (rng.next() - 0.5f) * vVec;
				}

				for (int l = 0; l < lensSamples; ++l) {
					// Stage 2: pinhole ray, || a ray through a point on
					// the aperture aimed at the focal plane.
					Ray ray;
					if (g_lens.enabled()) {
						ray = thinLensRay(cam, dirVec, g_lens, rng);
					} else {
						ray.setOrigin(eye);
						ray.setDirection(dirVec);
					}

					pixelColorVec += rayTraceRGB(root,ray,eye,ambient,lights,
					                             REFLECTION_HITS,y,x,h,w,bgPng);
				}
			}

			pixelColorVec /= (float)totalSamples;

			const unsigned int py = (unsigned int) y;
			image(x, py, 0) = (double) pixelColorVec.r;
			image(x, py, 1) = (double) pixelColorVec.g;
			image(x, py, 2) = (double) pixelColorVec.b;
		}
		ratioFloat = (y+1 - startIdx)/(float)(endIdx -startIdx);
		if ( ratioFloat >= progressFloat){
			std::cout << std::fixed<< std::setprecision(2);
			std::cout << "percentage complete: "<< 100 * ratioFloat <<"% " << "for thread: "<< threadIdx <<std::endl;
			progressFloat = progressFloat + 0.4f;
		}
	}
}
//---------------------------------------------------------------------
void A4_Render(
		// What to render  
		SceneNode * root,

		// Image to write to, set to a given width && height  
		Image & image,

		// Viewing parameters  
		const glm::vec3 & eye,
		const glm::vec3 & view,
		const glm::vec3 & up,
		double fovy,

		// Lighting parameters  
		const glm::vec3 & ambient,
		const std::list<Light *> & lights
) {

  // Fill in raytracing code here...  
  auto start_time = std::chrono::high_resolution_clock::now();

  std::cout << "F20: Calling A4_Render(\n" <<
		  "\t" << *root <<
          "\t" << "Image(width:" << image.width() << ", height:" << image.height() << ")\n"
          "\t" << "eye:  " << glm::to_string(eye) << std::endl <<
		  "\t" << "view: " << glm::to_string(view) << std::endl <<
		  "\t" << "up:   " << glm::to_string(up) << std::endl <<
		  "\t" << "fovy: " << fovy << std::endl <<
          "\t" << "ambient: " << glm::to_string(ambient) << std::endl <<
		  "\t" << "lights{" << std::endl;

	for(const Light * light : lights) {
		std::cout << "\t\t" <<  *light << std::endl;
	}
	std::cout << "\t}" << std::endl;
	std:: cout <<")" << std::endl;

	size_t h = image.height();
	size_t w = image.width();
	
	// Load image section
	// Declare variables to be altered
	LoadedPng bgPng;
	// std::vector<unsigned char> loadedPNG;
	// unsigned loadedWidth, loadedHeight;
	// Decode image into loadedPNG
  	// NOTE: still a hardcoded path relative to the working directory.
  	// It belongs in the scene description -- see the backlog -- and is
  	// replaced by an environment light at staircase step 3.
  	unsigned error = lodepng::decode(bgPng.RGBA, bgPng.loadedWidth, bgPng.loadedHeight, "assets/textures/kh_stain_glass.png");

  	//if there's an error, display it
  	if(error) {
		std::cout << "decoder error " << error << ": " << lodepng_error_text(error) << std::endl;
	}else{
		cout<< "PNG loaded"<<endl;
		cout<< "width: " << bgPng.loadedWidth<<endl;
		cout<< "height: " << bgPng.loadedHeight<<endl;
	}
	
	// Deal with viewport
	// Create orthonormal basis
	vec3 wVec = normalize(view); // z-axis
	vec3 uVec = normalize(cross(up,view)); //x -axis
	vec3 vVec = cross(uVec,wVec); // y-axis
	float dFloat = (float)(h/2/glm::tan(glm::radians(fovy/2))); // focal length
	// TODO: convert to Top left if possible
	// ray direction at bottom left corner
	const vec3 initDirVec = wVec * dFloat - uVec * (float)w/2 -vVec *(float)h/2;

	// Bundle the camera up so the thin-lens code can offset the ray
	// origin within the aperture plane (uVec/vVec) rather than on world
	// axes, && measure focus distance along the view axis (wVec).
	CameraBasis cam;
	cam.eye  = eye;
	cam.uVec = uVec;
	cam.vVec = vVec;
	cam.wVec = wVec;

	if (g_lens.enabled()) {
		cout << "Lens: aperture radius " << g_lens.apertureRadius
		     << ", focus distance " << g_lens.focusDistance
		     << ", " << g_lens.samples << " samples/pixel" << endl;
	}
	if (g_aaSamples > 1) {
		cout << "Anti-aliasing: " << g_aaSamples << " samples/pixel" << endl;
	}
	BVH::resetStats();

	// loop through each pixel && peform ray tracing on each one
	// TODO: Multithreading
	const int NUM_THREADS = 16; // Number of threads to use
	int deltaH = (int)(h/NUM_THREADS);
	int extraH = (int)(h %NUM_THREADS);

	std::thread threads[NUM_THREADS];// array to store thread objects

	// Launch threads
	size_t startIdx = 0;
	size_t endIdx = 0;
	for (int i = 0;i < NUM_THREADS; i++){
		endIdx = startIdx + deltaH + (i < extraH ? 1 : 0);
		// Loop for each pixel in outPutImage
		threads[i] = std::thread(generatePixelColors,
								std::ref(image),
								startIdx,
								endIdx,
								initDirVec,
								h,
								w,
								std::cref(cam),

								ambient,
								lights,
								root,
								std::cref(bgPng),
								i);// thread index 

		startIdx = endIdx;
	}
		
	// Join threads
	for (int i = 0;i < NUM_THREADS; i++){
		threads[i].join();
	}

	std::cout << "percentage complete: 100.0%" << std::endl;
	auto end_time = std::chrono::high_resolution_clock::now();

	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time-start_time);
	std::cout << "Runtime: " << duration.count() / 1000<< "s" <<std::endl;
	BVH::reportStats("totals for this frame:");
	dbgPrint("Debug");
}
