
#include <glm/ext.hpp>

#include "render/Renderer.hpp"

#include <iomanip>
#include "core/Ray.hpp"
#include "render/Sampling.hpp"
#include "render/Framebuffer.hpp"
#include "render/Camera.hpp"
#include "geometry/BVH.hpp"
#include "scene/Material.hpp"
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

// AA and depth of field are runtime settings (gr.set_samples / gr.set_lens
// from Lua), not compile-time #defines. Both default to off.

static const float MAX_RGB = 255.0f;					 // 8-bit channel max
static const float MAX_T = numeric_limits<float>::max(); // unbounded ray length
// Safety valve for pathological geometry -- a hall of mirrors, or light
// trapped in a box. Russian roulette is the real termination and is
// unbiased; this cut is not, so it should almost never fire.
static const int MAX_DEPTH = 8;
static const int RR_START_DEPTH = 3; // bounces taken before roulette begins

// Set from Lua before gr.render. Defaults: one sample, pinhole camera.
static LensConfig g_lens;
static int g_samplesPerPixel = 1;
static int g_snapshotInterval = 0; // 0 == final image only
static std::string g_outputPath;
static std::string g_backgroundPath; // empty => uniform `ambient` environment
static tonemap::Config g_tonemap;    // defaults: no tone map, sRGB on

void SetLens(float apertureRadius, float focusDistance, int samples)
{
	g_lens.apertureRadius = apertureRadius;
	g_lens.focusDistance = focusDistance;
	g_lens.samples = samples;
}

void SetSamplesPerPixel(int samples)
{
	g_samplesPerPixel = (samples < 1) ? 1 : samples;
}

void SetSnapshotInterval(int samples)
{
	g_snapshotInterval = (samples < 0) ? 0 : samples;
}

void SetOutputPath(const std::string &path)
{
	g_outputPath = path;
}

void SetBackground(const std::string &path)
{
	g_backgroundPath = path;
}

void SetToneMap(const tonemap::Config &cfg)
{
	g_tonemap = cfg;
}

const tonemap::Config &GetToneMap()
{
	return g_tonemap;
}

// "renders/out.png" at 16 spp -> "renders/out_0016spp.png", so a
// convergence series doesn't overwrite itself.
static std::string snapshotPath(const std::string &path, size_t samples)
{
	std::string stem = path;
	std::string ext;
	const size_t dot = path.find_last_of('.');
	const size_t sep = path.find_last_of("/\\");
	if (dot != std::string::npos && (sep == std::string::npos || dot > sep))
	{
		stem = path.substr(0, dot);
		ext = path.substr(dot);
	}

	std::ostringstream oss;
	oss << stem << "_" << std::setfill('0') << std::setw(4) << samples << "spp" << ext;
	return oss.str();
}

//---------------------------------------------------------------------
// Radiance arriving from outside the scene, for a ray that hit nothing.
//
// A function of direction alone, so the camera ray and every bounce ray
// see the same environment. The old lookup was screen-space, which is
// meaningless for a bounce ray -- it has no pixel -- and meant a sphere
// could never match the background the furnace test compares it to.
//
// With no texture loaded this returns `ambient`: a uniform emissive
// environment, which is exactly the furnace condition.
static vec3 environment(const vec3 &dirVec, const vec3 &ambient,
						const LoadedPng &bgPng)
{
	const int texW = (int)bgPng.loadedWidth;
	const int texH = (int)bgPng.loadedHeight;
	if (texW <= 0 || texH <= 0)
	{
		return ambient;
	}

	// Lat-long (equirectangular): azimuth about +y to u, polar angle to v.
	const vec3 dVec = normalize(dirVec);
	const float u = 0.5f + std::atan2(dVec.x, -dVec.z) / (2.0f * kPI);
	const float v = std::acos(glm::clamp(dVec.y, -1.0f, 1.0f)) / kPI;

	const int tx = glm::clamp((int)(u * texW), 0, texW - 1);
	const int ty = glm::clamp((int)(v * texH), 0, texH - 1);

	const size_t idx = 4u * ((size_t)ty * (size_t)texW + (size_t)tx);

	// The PNG holds sRGB bytes; linearise them so they enter shading as
	// radiance. Image::savePng re-encodes on the way out.
	return vec3(
		(float)tonemap::decodeSRGB(bgPng.RGBA[idx] / (double)MAX_RGB),
		(float)tonemap::decodeSRGB(bgPng.RGBA[idx + 1] / (double)MAX_RGB),
		(float)tonemap::decodeSRGB(bgPng.RGBA[idx + 2] / (double)MAX_RGB));
}

//---------------------------------------------------------------------
// Trace one path: bounce until it escapes, dies to roulette, or hits the
// depth cap, accumulating radiance weighted by the throughput carried so
// far. One loop iteration is one ray cast.
vec3 rayTraceRGB(
	SceneNode *root,
	Ray ray,				  // by value: the loop advances it
	Rng &rng,
	const glm::vec3 &ambient, // uniform environment radiance
	const std::list<Light *> &lights,
	const LoadedPng &bgPng	  // environment texture, may be empty
)
{
	vec3 radiance(0.0f);
	vec3 throughput(1.0f);

	for (int bounces = 0;; ++bounces)
	{
		// kEpsilon as tMin: ignore hits right at the ray origin.
		HitRecord record;
		if (!root->isHit(ray, kEpsilon, MAX_T, record))
		{
			radiance += throughput * environment(ray.getDirection(), ambient, bgPng);
			break;
		}

		// Read into locals: the record is geometry output, not scratch
		// space. N is normalised here because primitives return an
		// unnormalised normal; P is nudged off the surface by kEpsilon so
		// shadow and bounce rays do not self-hit.
		const vec3 N = normalize(record.getNormal());
		const vec3 P = record.getHitPoint() + N * kEpsilon;
		const vec3 in = -normalize(ray.getDirection()); // AWAY from surface
		Material *material = record.getMaterial();

		// Crude next event estimation. A point light is a Dirac delta with
		// zero solid angle, so BSDF sampling can never draw a direction
		// that lands on one -- without this loop every scene is black.
		for (Light *light : lights)
		{
			Ray shadeRay;
			shadeRay.setOrigin(P);
			shadeRay.setDirection(light->position - P);

			// Anything in the way: this light is occluded, skip it.
			HitRecord occlusion;
			if (root->isHit(shadeRay, kEpsilon, MAX_T, occlusion))
				continue;

			const vec3 L = normalize(shadeRay.getDirection());
			radiance += throughput * material->eval(in, N, L) *
						std::max(0.0f, dot(N, L)) * light->colour;
		}

		float pdf;
		vec3 brdf;
		const vec3 out = material->sample(rng, in, N, &pdf, &brdf);
		if (pdf <= 0.0f)
			break; // scattered below the surface

		// For cosine-weighted Lambertian this reduces to throughput *= albedo.
		//
		// A delta lobe skips the estimator entirely: its brdf is already the
		// weight. Running it through the general form would divide by a
		// cosine only to multiply the same cosine back, and that round trip
		// is not exact in float -- the drift is what left a mirror one code
		// darker than its environment in the furnace test.
		if (material->isSpecular())
			throughput *= brdf;
		else
			throughput *= brdf * std::fabs(dot(out, N)) / pdf;

		// The usual exit. Unbiased: a path survives with probability q and
		// its weight is divided by q, so the estimator is unchanged.
		if (bounces >= RR_START_DEPTH)
		{
			const float q = std::min(0.95f, std::max(throughput.x,
													 std::max(throughput.y, throughput.z)));
			if (rng.next() >= q)
				break;
			throughput /= q;
		}

		if (bounces + 1 >= MAX_DEPTH)
			break; // safety valve, biased -- see MAX_DEPTH

		ray.setOrigin(P);
		ray.setDirection(out);
	}
	return radiance;
}
//---------------------------------------------------------------------

// Trace `passes` jittered samples per pixel for rows [startIdx, endIdx)
// and accumulate them into the shared framebuffer. Bands own disjoint
// rows, so no locking. Jittering every sample (no unjittered centre
// sample) is what anti-aliases the edges.
void renderBand(
	Framebuffer &accum,
	size_t startIdx,
	size_t endIdx,
	size_t passes,
	size_t passOffset, // samples already done; picks a fresh RNG stream
	vec3 initDirVec,
	size_t h,
	size_t w,
	const CameraBasis &cam,
	const glm::vec3 &ambient,
	const std::list<Light *> &lights,
	SceneNode *root,
	const LoadedPng &bgPng,
	int threadIdx)
{
	// Per-thread RNG, seeded from thread index and passOffset so chunks
	// don't replay jitter. Deterministic: same scene + thread count => same image.
	Rng rng((uint32_t)(1u + threadIdx * 9781u + passOffset * 7919u));

	const vec3 &eye = cam.eye;
	const vec3 &uVec = cam.uVec;
	const vec3 &vVec = cam.vVec;

	for (size_t pass = 0; pass < passes; ++pass)
	{
		for (size_t y = startIdx; y < endIdx; ++y)
		{
			for (size_t x = 0; x < w; ++x)
			{
				// Direction through the pixel centre...
				const vec3 centreDirVec = initDirVec + (float)(w - x) * uVec + (float)(y)*vVec;

				// ...offset by up to half a pixel in u and v.
				// TODO: stratify the offsets for faster convergence.
				const vec3 dirVec = centreDirVec + (rng.next() - 0.5f) * uVec + (rng.next() - 0.5f) * vVec;

				// Pinhole ray, or a lens ray for depth of field.
				Ray ray;
				if (g_lens.enabled())
				{
					ray = thinLensRay(cam, dirVec, g_lens, rng);
				}
				else
				{
					ray.setOrigin(eye);
					ray.setDirection(dirVec);
				}

				const vec3 radiance = rayTraceRGB(root, ray, rng, ambient, lights, bgPng);

				accum.add(x, y, radiance);
			}
		}
	}
}
//---------------------------------------------------------------------
void Render(
	SceneNode *root, // scene graph
	Image &image,	 // output, already sized w x h

	const glm::vec3 &eye,  // camera position
	const glm::vec3 &view, // look direction (not a target point)
	const glm::vec3 &up,
	double fovy, // vertical field of view, degrees

	const glm::vec3 &ambient,
	const std::list<Light *> &lights)
{

	auto start_time = std::chrono::high_resolution_clock::now();

	// The scene header is one statement per line, at debug. At 13 lines a
	// frame it would otherwise dominate an 85-frame animation log.
	if (rt::log::enabled(rt::log::Level::Debug, rt::log::Cat::RENDER))
	{
		LOG_DEBUG(RENDER) << "render " << image.width() << "x" << image.height();
		LOG_DEBUG(RENDER) << "  root    " << *root;
		LOG_DEBUG(RENDER) << "  eye     " << glm::to_string(eye);
		LOG_DEBUG(RENDER) << "  view    " << glm::to_string(view);
		LOG_DEBUG(RENDER) << "  up      " << glm::to_string(up);
		LOG_DEBUG(RENDER) << "  fovy    " << fovy;
		LOG_DEBUG(RENDER) << "  ambient " << glm::to_string(ambient);
		for (const Light *light : lights)
		{
			LOG_DEBUG(RENDER) << "  light   " << *light;
		}
	}

	size_t h = image.height();
	size_t w = image.width();

	// Environment texture, from gr.set_background. Left empty the scene
	// gets a uniform environment of radiance `ambient` instead.
	LoadedPng bgPng;
	bgPng.loadedWidth = 0;
	bgPng.loadedHeight = 0;
	if (!g_backgroundPath.empty())
	{
		const unsigned error = lodepng::decode(bgPng.RGBA, bgPng.loadedWidth,
											   bgPng.loadedHeight, g_backgroundPath);
		if (error)
		{
			// Fall back to the uniform environment rather than rendering
			// against whatever half-decoded bytes are in the buffer.
			bgPng.loadedWidth = 0;
			bgPng.loadedHeight = 0;
			LOG_ERROR(RENDER) << "environment texture: " << lodepng_error_text(error);
		}
		else
		{
			LOG_DEBUG(RENDER) << "environment texture " << bgPng.loadedWidth
							  << "x" << bgPng.loadedHeight;
		}
	}
	else
	{
		LOG_DEBUG(RENDER) << "uniform environment " << glm::to_string(ambient);
	}

	// Camera basis: w = forward, u = right, v = true up.
	vec3 wVec = normalize(view);
	vec3 uVec = normalize(cross(up, view));
	vec3 vVec = cross(uVec, wVec);
	// Distance to the image plane that makes it fovy tall.
	float dFloat = (float)(h / 2 / glm::tan(glm::radians(fovy / 2)));
	// Direction to the bottom-left corner; renderBand steps u/v from here.
	// TODO: origin the grid at the top-left instead.
	const vec3 initDirVec = wVec * dFloat - uVec * (float)w / 2 - vVec * (float)h / 2;

	// Pack the basis for the thin-lens code (aperture in u/v, focus along w).
	CameraBasis cam;
	cam.eye = eye;
	cam.uVec = uVec;
	cam.vVec = vVec;
	cam.wVec = wVec;

	if (g_lens.enabled())
	{
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
	const size_t totalSamples = (size_t)g_samplesPerPixel * (size_t)(g_lens.enabled() ? g_lens.samples : 1);

	// One thread per hardware core, respawned per snapshot chunk (a single
	// spawn when snapshots are off).
	// TODO: replace the static bands with a tile queue.
	const unsigned int hw = std::thread::hardware_concurrency();
	const int NUM_THREADS = (int)((hw == 0) ? 16u : hw);

	{
		// One Line object so the whole sentence is a single log record,
		// rather than two that another thread could split.
		rt::log::Line ln(rt::log::Level::Info, rt::log::Cat::RENDER);
		ln.stream() << "rendering " << totalSamples << " sample(s)/pixel on "
					<< NUM_THREADS << " threads";
		if (g_snapshotInterval > 0)
		{
			ln.stream() << ", snapshot every " << g_snapshotInterval;
		}
	}

	std::vector<std::thread> threads((size_t)NUM_THREADS);

	// Each iteration renders `chunk` more samples, then optionally snapshots.
	size_t done = 0;
	while (done < totalSamples)
	{
		const size_t remaining = totalSamples - done;
		const size_t chunk = (g_snapshotInterval > 0)
								 ? std::min((size_t)g_snapshotInterval, remaining) // up to the interval
								 : remaining;									   // everything left

		// Split rows across threads; spread the remainder one per thread.
		const size_t deltaH = h / (size_t)NUM_THREADS;
		const size_t extraH = h % (size_t)NUM_THREADS;

		size_t startIdx = 0;
		for (int i = 0; i < NUM_THREADS; i++)
		{
			const size_t endIdx = startIdx + deltaH + ((size_t)i < extraH ? 1u : 0u);

			threads[(size_t)i] = std::thread(renderBand,
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

		for (int i = 0; i < NUM_THREADS; i++)
		{
			threads[(size_t)i].join();
		}

		accum.addSamples(chunk);
		done += chunk;

		LOG_INFO(RENDER) << done << "/" << totalSamples << " spp";

		// Intermediate image; accumulation carries on untouched.
		if (g_snapshotInterval > 0 && done < totalSamples && !g_outputPath.empty())
		{
			accum.resolve(image);
			const std::string snap = snapshotPath(g_outputPath, done);
			if (!image.savePng(snap, g_tonemap))
			{
				LOG_ERROR(RENDER) << "snapshot write failed: " << snap;
			}
		}
	}

	accum.resolve(image);

	auto end_time = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
	LOG_INFO(RENDER) << "done in " << duration.count() << " ms, "
					 << accum.sampleCount() << " spp";
	BVH::reportStats("frame totals");
}
