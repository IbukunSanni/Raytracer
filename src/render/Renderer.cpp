
#include <glm/ext.hpp>

#include "render/Renderer.hpp"

#include <iomanip>
#include "core/Ray.hpp"
#include "render/Sampling.hpp"
#include "render/Framebuffer.hpp"
#include "render/Camera.hpp"
#include "geometry/BVH.hpp"
#include "scene/PhongMaterial.hpp"
#include "core/Log.hpp"
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

// AA and depth of field are runtime settings (gr.set_samples / gr.set_lens
// from Lua), not compile-time #defines. Both default to off.

static const float EPS = 0.000001f;                      // self-intersection offset
static const float MAX_RGB = 255.0f;                     // 8-bit channel max
static const float MAX_T = numeric_limits<float>::max(); // unbounded ray length
static const int REFLECTION_HITS = 3;                    // max reflection bounces
static const float REFLECTION_COEFF = 0.25;              // reflected-ray weight per bounce

// Set from Lua before gr.render. Defaults: one sample, pinhole camera.
static LensConfig  g_lens;
static int            g_samplesPerPixel  = 1;
static int            g_snapshotInterval = 0;  // 0 == final image only
static std::string    g_outputPath;
static tonemap::Config g_tonemap;              // defaults: no tone map, sRGB on

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

void SetToneMap(const tonemap::Config & cfg) {
	g_tonemap = cfg;
}

const tonemap::Config & GetToneMap() {
	return g_tonemap;
}

// "renders/out.png" at 16 spp -> "renders/out_0016spp.png", so a
// convergence series doesn't overwrite itself.
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
// Shade one ray: nearest hit, Phong lighting with hard shadows, and a
// mirror-reflection bounce. A miss returns the background texture.
vec3 rayTraceRGB(
	SceneNode * root,
	Ray &ray,
	const glm::vec3 & eye,
	const glm::vec3 & ambient,
	const std::list<Light *> & lights,
	const int reflectionHits,          // bounces left
	const size_t y ,                   // pixel row
	const size_t x ,                   // pixel column
	const size_t h ,                   // image height
	const size_t w ,                   // image width
	const LoadedPng & bgPng            // background texture
){
	HitRecord record;
	vec3 returnColor;


	// EPS as tMin: ignore hits right at the ray origin.
	if(root->isHit(ray, EPS, MAX_T,record)){
		// Hit.
		record.normalVec = normalize(record.normalVec);
		// Nudge off the surface so shadow rays don't self-hit.
		record.hitPointVec += record.normalVec * EPS;

		PhongMaterial *material = static_cast<PhongMaterial *>(record.material);

		// Ambient term.
		returnColor += material->getDiffuse() * ambient;

		for (Light * light : lights){
			Ray shadeRay;
			shadeRay.setOrigin(record.hitPointVec);
			shadeRay.setDirection(light->position - record.hitPointVec);

			HitRecord shadeRecord;

			// Anything in the way: this light is occluded, skip it.
			if(root->isHit(shadeRay, EPS,MAX_T,shadeRecord)){
				continue;
			}

			vec3 L = normalize(shadeRay.getDirection()); // toward light
			vec3 V = normalize(eye - record.hitPointVec); // toward eye
			vec3 N = normalize(record.normalVec);         // surface normal
			vec3 H = normalize(V + L);                    // half-vector

			// Diffuse.
			returnColor += std::max(0.0, (double)dot(N,L)) * material->getDiffuse() * light->colour;

			// Specular.
			returnColor += pow(std::max(0.0, (double)dot(N,H)),material->getShininess()) *
						   material->getSpecular() * light->colour;
		}

		// Recurse along the mirror direction and blend the result in.
		if (REFLECTION > 0 && reflectionHits > 0){
			vec3 refDirVec = ray.getDirection() - 2 * record.normalVec * dot(ray.getDirection(),record.normalVec);
			Ray refRay;
			refRay.setOrigin(record.hitPointVec);
			refRay.setDirection(refDirVec);
			// Mostly local shading, REFLECTION_COEFF from the reflected ray.
			returnColor = glm::mix(returnColor,rayTraceRGB(root, refRay,eye,ambient,lights,reflectionHits - 1,y,x,h,w,bgPng),REFLECTION_COEFF );
		}

	}else{
		// Miss. Only the primary ray shows the background; reflected rays
		// that escape return black so the scene isn't wrapped in it.
		if (reflectionHits < REFLECTION_HITS){
			return returnColor;
		}
		// Map the frame onto the background texture in normalised [0,1]
		// coords with a "cover" fit (fill the frame, crop the overflow),
		// then clamp. Resolution-independent and always in bounds -- the
		// old centre-crop indexed past the texture and segfaulted once the
		// render was larger than it.
		const int texW = (int) bgPng.loadedWidth;
		const int texH = (int) bgPng.loadedHeight;
		if (texW > 0 && texH > 0) {
			const float frameAspect = (float) w / (float) h;
			const float texAspect   = (float) texW / (float) texH;

			// Shrink the axis that would otherwise letterbox.
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

			// The PNG holds sRGB bytes; linearise them so they enter
			// shading as radiance. Image::savePng re-encodes on the way out.
			returnColor = vec3(
				(float) tonemap::decodeSRGB(bgPng.RGBA[idx]     / (double) MAX_RGB),
				(float) tonemap::decodeSRGB(bgPng.RGBA[idx + 1] / (double) MAX_RGB),
				(float) tonemap::decodeSRGB(bgPng.RGBA[idx + 2] / (double) MAX_RGB));
		}


	}
	return returnColor;

}
//---------------------------------------------------------------------

// Trace `passes` jittered samples per pixel for rows [startIdx, endIdx)
// and accumulate them into the shared framebuffer. Bands own disjoint
// rows, so no locking. Jittering every sample (no unjittered centre
// sample) is what anti-aliases the edges.
void renderBand(
	Framebuffer & accum,
	size_t startIdx,
	size_t endIdx,
	size_t passes,
	size_t passOffset,   // samples already done; picks a fresh RNG stream
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
	// Per-thread RNG, seeded from thread index and passOffset so chunks
	// don't replay jitter. Deterministic: same scene + thread count => same image.
	Rng rng((uint32_t)(1u + threadIdx * 9781u + passOffset * 7919u));

	const vec3 & eye  = cam.eye;
	const vec3 & uVec = cam.uVec;
	const vec3 & vVec = cam.vVec;

	for (size_t pass = 0; pass < passes; ++pass) {
		for (size_t y = startIdx; y < endIdx; ++y) {
			for (size_t x = 0; x < w; ++x) {
				// Direction through the pixel centre...
				const vec3 centreDirVec = initDirVec
				                        + (float)(w - x) * uVec
				                        + (float)(y)     * vVec;

				// ...offset by up to half a pixel in u and v.
				// TODO: stratify the offsets for faster convergence.
				const vec3 dirVec = centreDirVec
				                  + (rng.next() - 0.5f) * uVec
				                  + (rng.next() - 0.5f) * vVec;

				// Pinhole ray, or a lens ray for depth of field.
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
		SceneNode * root,                   // scene graph
		Image & image,                      // output, already sized w x h

		const glm::vec3 & eye,               // camera position
		const glm::vec3 & view,              // look direction (not a target point)
		const glm::vec3 & up,
		double fovy,                         // vertical field of view, degrees

		const glm::vec3 & ambient,
		const std::list<Light *> & lights
) {

  auto start_time = std::chrono::high_resolution_clock::now();

	// The scene header is one statement per line, at debug. At 13 lines a
	// frame it would otherwise dominate an 85-frame animation log.
	if (rt::log::enabled(rt::log::Level::Debug, rt::log::Cat::RENDER)) {
		LOG_DEBUG(RENDER) << "render " << image.width() << "x" << image.height();
		LOG_DEBUG(RENDER) << "  root    " << *root;
		LOG_DEBUG(RENDER) << "  eye     " << glm::to_string(eye);
		LOG_DEBUG(RENDER) << "  view    " << glm::to_string(view);
		LOG_DEBUG(RENDER) << "  up      " << glm::to_string(up);
		LOG_DEBUG(RENDER) << "  fovy    " << fovy;
		LOG_DEBUG(RENDER) << "  ambient " << glm::to_string(ambient);
		for (const Light * light : lights) {
			LOG_DEBUG(RENDER) << "  light   " << *light;
		}
	}

	size_t h = image.height();
	size_t w = image.width();

	// Background texture. NOTE: hardcoded path, relative to the working
	// directory -- belongs in the scene description (see backlog), and is
	// replaced by an environment light at staircase step 3.
	LoadedPng bgPng;
  	unsigned error = lodepng::decode(bgPng.RGBA, bgPng.loadedWidth, bgPng.loadedHeight, "assets/textures/kh_stain_glass.png");

  	if(error) {
		LOG_ERROR(RENDER) << "background texture: " << lodepng_error_text(error);
	}else{
		LOG_DEBUG(RENDER) << "background texture " << bgPng.loadedWidth
		                  << "x" << bgPng.loadedHeight;
	}

	// Camera basis: w = forward, u = right, v = true up.
	vec3 wVec = normalize(view);
	vec3 uVec = normalize(cross(up,view));
	vec3 vVec = cross(uVec,wVec);
	// Distance to the image plane that makes it fovy tall.
	float dFloat = (float)(h/2/glm::tan(glm::radians(fovy/2)));
	// Direction to the bottom-left corner; renderBand steps u/v from here.
	// TODO: origin the grid at the top-left instead.
	const vec3 initDirVec = wVec * dFloat - uVec * (float)w/2 -vVec *(float)h/2;

	// Pack the basis for the thin-lens code (aperture in u/v, focus along w).
	CameraBasis cam;
	cam.eye  = eye;
	cam.uVec = uVec;
	cam.vVec = vVec;
	cam.wVec = wVec;

	if (g_lens.enabled()) {
		LOG_INFO(RENDER) << "lens: aperture " << g_lens.apertureRadius
		                 << ", focus " << g_lens.focusDistance
		                 << ", " << g_lens.samples << " samples/pixel";
	}
	BVH::resetStats();

	// Progressive accumulation: samples add into a shared buffer that can
	// be resolved to an image at any time, so a snapshot is just a divide
	// and a PNG write, not a re-render.
	Framebuffer accum(w, h);

	// AA samples x lens samples.
	const size_t totalSamples = (size_t) g_samplesPerPixel
	                          * (size_t) (g_lens.enabled() ? g_lens.samples : 1);

	// One thread per hardware core, respawned per snapshot chunk (a single
	// spawn when snapshots are off).
	// TODO: replace the static bands with a tile queue.
	const unsigned int hw = std::thread::hardware_concurrency();
	const int NUM_THREADS = (int) ((hw == 0) ? 16u : hw);

	{
		// One Line object so the whole sentence is a single log record,
		// rather than two that another thread could split.
		rt::log::Line ln(rt::log::Level::Info, rt::log::Cat::RENDER);
		ln.stream() << "rendering " << totalSamples << " sample(s)/pixel on "
		            << NUM_THREADS << " threads";
		if (g_snapshotInterval > 0) {
			ln.stream() << ", snapshot every " << g_snapshotInterval;
		}
	}

	std::vector<std::thread> threads((size_t) NUM_THREADS);

	// Each iteration renders `chunk` more samples, then optionally snapshots.
	size_t done = 0;
	while (done < totalSamples) {
		const size_t remaining = totalSamples - done;
		const size_t chunk = (g_snapshotInterval > 0)
			? std::min((size_t) g_snapshotInterval, remaining)  // up to the interval
			: remaining;                                        // everything left

		// Split rows across threads; spread the remainder one per thread.
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

		LOG_INFO(RENDER) << done << "/" << totalSamples << " spp";

		// Intermediate image; accumulation carries on untouched.
		if (g_snapshotInterval > 0 && done < totalSamples && !g_outputPath.empty()) {
			accum.resolve(image);
			const std::string snap = snapshotPath(g_outputPath, done);
			if (!image.savePng(snap, g_tonemap)) {
				LOG_ERROR(RENDER) << "snapshot write failed: " << snap;
			}
		}
	}

	accum.resolve(image);

	auto end_time = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time-start_time);
	LOG_INFO(RENDER) << "done in " << duration.count() << " ms, "
	                 << accum.sampleCount() << " spp";
	BVH::reportStats("frame totals");
}
