// Termm--Fall 2020

#include <glm/ext.hpp>

#include "render/Renderer.hpp"

#include <iomanip>
#include "core/Ray.hpp"
#include "render/Sampling.hpp"
#include "render/Framebuffer.hpp"
#include "render/Camera.hpp"
#include "geometry/BVH.hpp"
#include "scene/PhongMaterial.hpp"
#include "core/dbgPrint.hpp"
#include <lodepng/lodepng.h>
#include <string>
#include <sstream>
#include <chrono>
#include <thread>
#include <vector>
#include <algorithm>

using namespace std;
using namespace glm;

#define REFLECTION 01

// Anti-aliasing and depth of field are now runtime settings, driven from
// Lua (gr.set_lens / gr.set_aa) rather than #defines, so a scene can turn
// them on without a rebuild. Both default to off, matching the old
// behaviour exactly.

static const float EPS = 0.000001f; // correction factor
static const float MAX_RGB = 255.0f; // maximum rgb value
static const float MAX_T = numeric_limits<float>::max();// max t distance
static const int REFLECTION_HITS = 3; // number of reflection bounces
static const float REFLECTION_COEFF = 0.25;

// Set from Lua before gr.render. Defaults keep the pinhole camera.
static LensConfig  g_lens;
static int         g_samplesPerPixel  = 1;
static int         g_snapshotInterval = 0;  // 0 == final image only
static std::string g_outputPath;

void SetLens(float apertureRadius, float focusDistance, int samples) {
	g_lens.apertureRadius = apertureRadius;
	g_lens.focusDistance  = focusDistance;
	g_lens.samples        = samples;
}

void SetSamplesPerPixel(int samples) {
	g_samplesPerPixel = (samples < 1) ? 1 : samples;
}

void SetSnapshotInterval(int samples) {
	g_snapshotInterval = (samples < 0) ? 0 : samples;
}

void SetOutputPath(const std::string & path) {
	g_outputPath = path;
}

// "renders/out.png" at 16 spp -> "renders/out_0016spp.png". Keeping the
// sample count in the name is what makes a convergence series legible
// afterwards; overwriting one file loses the comparison.
static std::string snapshotPath(const std::string & path, size_t samples) {
	std::string stem = path;
	std::string ext;
	const size_t dot = path.find_last_of('.');
	const size_t sep = path.find_last_of("/\\");
	if (dot != std::string::npos && (sep == std::string::npos || dot > sep)) {
		stem = path.substr(0, dot);
		ext  = path.substr(dot);
	}

	std::ostringstream oss;
	oss << stem << "_" << std::setfill('0') << std::setw(4) << samples << "spp" << ext;
	return oss.str();
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
		// independent, and in bounds by construction.
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

// Render `passes` samples per pixel for one horizontal band, accumulating
// into the shared framebuffer.
//
// Every sample is jittered inside the pixel footprint. There is no
// "centre of the pixel" special case: sampling the centre is what produces
// hard aliased edges, and a single jittered sample is an unbiased estimate
// of the same pixel while many of them converge to a smooth one.
//
// The band owns rows [startIdx, endIdx), so writes into the framebuffer
// never overlap another thread and need no synchronisation.
void renderBand(
	Framebuffer & accum,
	size_t startIdx,
	size_t endIdx,
	size_t passes,
	size_t passOffset,   // samples already accumulated, so each chunk
	                     // of passes draws a fresh random sequence
	vec3 initDirVec,
	size_t h,
	size_t w,
	const CameraBasis & cam,
	const glm::vec3 & ambient,
	const std::list<Light *> & lights,
	SceneNode * root,
	const LoadedPng & bgPng,
	int threadIdx
){
	// One generator per thread, and a different stream per chunk of passes,
	// so resuming accumulation does not replay the same jitter sequence.
	// Same scene + same thread count + same sample count => same image.
	Rng rng((uint32_t)(1u + threadIdx * 9781u + passOffset * 7919u));

	const vec3 & eye  = cam.eye;
	const vec3 & uVec = cam.uVec;
	const vec3 & vVec = cam.vVec;

	for (size_t pass = 0; pass < passes; ++pass) {
		for (size_t y = startIdx; y < endIdx; ++y) {
			for (size_t x = 0; x < w; ++x) {
				// Direction through the centre of this pixel...
				const vec3 centreDirVec = initDirVec
				                        + (float)(w - x) * uVec
				                        + (float)(y)     * vVec;

				// ...jittered uniformly within the pixel footprint.
				// TODO (step 1 refinement): stratify. Uniform jitter clumps,
				// so N stratified samples converge faster than N random ones.
				const vec3 dirVec = centreDirVec
				                  + (rng.next() - 0.5f) * uVec
				                  + (rng.next() - 0.5f) * vVec;

				// Pinhole ray, or a ray through a point on the aperture
				// aimed at the focal plane (staircase step 5).
				Ray ray;
				if (g_lens.enabled()) {
					ray = thinLensRay(cam, dirVec, g_lens, rng);
				} else {
					ray.setOrigin(eye);
					ray.setDirection(dirVec);
				}

				const vec3 radiance = rayTraceRGB(root, ray, eye, ambient, lights,
				                                  REFLECTION_HITS, y, x, h, w, bgPng);

				accum.add(x, y, radiance);
			}
		}
	}
}
//---------------------------------------------------------------------
void Render(
		// What to render  
		SceneNode * root,

		// Image to write to, set to a given width and height  
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

  std::cout << "F20: Calling Render(\n" <<
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
	// axes, and measure focus distance along the view axis (wVec).
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
	BVH::resetStats();

	// ---------------------------------------------------------------
	// Progressive accumulation.
	//
	// Rather than "loop N samples then write", each pass adds one sample
	// per pixel to a shared accumulation buffer, which can be resolved to
	// an image at any point. Snapshots therefore cost a divide and a PNG
	// write, not a re-render.
	// ---------------------------------------------------------------
	Framebuffer accum(w, h);

	const size_t totalSamples = (size_t) g_samplesPerPixel
	                          * (size_t) (g_lens.enabled() ? g_lens.samples : 1);

	// Threads are recreated once per chunk rather than once per pass, so
	// with snapshots off this is a single spawn. Step 6 replaces the whole
	// static-band scheme with a tile queue.
	const unsigned int hw = std::thread::hardware_concurrency();
	const int NUM_THREADS = (int) ((hw == 0) ? 16u : hw);

	std::cout << "Rendering " << totalSamples << " sample(s)/pixel on "
	          << NUM_THREADS << " threads";
	if (g_snapshotInterval > 0) {
		std::cout << ", snapshot every " << g_snapshotInterval;
	}
	std::cout << std::endl;

	std::vector<std::thread> threads((size_t) NUM_THREADS);

	size_t done = 0;
	while (done < totalSamples) {
		const size_t remaining = totalSamples - done;
		const size_t chunk = (g_snapshotInterval > 0)
			? std::min((size_t) g_snapshotInterval, remaining)
			: remaining;

		const size_t deltaH = h / (size_t) NUM_THREADS;
		const size_t extraH = h % (size_t) NUM_THREADS;

		size_t startIdx = 0;
		for (int i = 0; i < NUM_THREADS; i++){
			const size_t endIdx = startIdx + deltaH + ((size_t) i < extraH ? 1u : 0u);

			threads[(size_t) i] = std::thread(renderBand,
								std::ref(accum),
								startIdx,
								endIdx,
								chunk,
								done,
								initDirVec,
								h,
								w,
								std::cref(cam),
								ambient,
								lights,
								root,
								std::cref(bgPng),
								i);

			startIdx = endIdx;
		}

		for (int i = 0; i < NUM_THREADS; i++){
			threads[(size_t) i].join();
		}

		accum.addSamples(chunk);
		done += chunk;

		std::cout << "  " << done << "/" << totalSamples << " spp" << std::endl;

		// Snapshot: resolve the buffer as it stands and write it out. The
		// accumulation is untouched, so the next chunk keeps refining the
		// same image rather than starting over.
		if (g_snapshotInterval > 0 && done < totalSamples && !g_outputPath.empty()) {
			accum.resolve(image);
			image.savePng(snapshotPath(g_outputPath, done));
		}
	}

	accum.resolve(image);

	auto end_time = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time-start_time);
	std::cout << "Runtime: " << duration.count() << " ms for "
	          << accum.sampleCount() << " spp" << std::endl;
	BVH::reportStats("totals for this frame:");
	dbgPrint("Debug");
}
