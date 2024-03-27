// Termm--Fall 2020

#include <glm/ext.hpp>

#include "A4.hpp"

#include <iomanip>
#include "RayTracer.hpp"
#include "PhongMaterial.hpp"
#include "dbgPrint.hpp"
using namespace std;
using namespace glm;

#define ANTI_ALIASING 00
#define REFLECTION 01

static const float EPS = 0.000001; // correction factor
static const float MAX_RGB = 255.0f; // maximum rgb value
static const float MAX_T = numeric_limits<float>::max();// max t distance
static const int REFLECTION_HITS = 4;
static const float REFLECTION_COEFF = 0.25;


float rand_float(){
	return (float) rand()/(RAND_MAX +1.0);
}

vec3 randUnitVector(){
	vec3 randVec;
	do{
		randVec.x = rand_float();
		randVec.y = rand_float();
		randVec.z = 0.0f;

	}while(length2(randVec) >= 1.0f);
	return randVec;
}


vec3 rayTraceRGB(
	// What to render  
	SceneNode * root,
	// The ray 
	RayTracer &ray,
	const glm::vec3 & eye,
	// Lighting parameters  
	const glm::vec3 & ambient,
	const std::list<Light *> & lights,
	const int reflectionHits = REFLECTION_HITS
){
	// TODO: use reflection hits to reflect
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
			RayTracer shadeRay;
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

		if (REFLECTION > 0 and reflectionHits > 0){
			vec3 refDirVec = ray.getDirection() - 2 * record.normalVec * dot(ray.getDirection(),record.normalVec);
			RayTracer refRay;
			refRay.setOrigin(record.hitPointVec);
			refRay.setDirection(refDirVec);
			// TODO: Clarify mix use
			returnColor = glm::mix(returnColor,rayTraceRGB(root, refRay,eye,ambient,lights,reflectionHits - 1),REFLECTION_COEFF );
		}

	}else{
		// Miss happened
		// Show background color
		vec3 unitDirVec = glm::normalize(ray.getDirection());
		float tFloat = unitDirVec.x;
		// Background color is left to right gradient
		returnColor = (1 - unitDirVec.x) * vec3(161.0f/MAX_RGB,74.0f/MAX_RGB,8.0f/MAX_RGB) +
					   unitDirVec.x * vec3(20.0f/MAX_RGB,123.0f/MAX_RGB,186.0f/MAX_RGB) ;
		if(rand_float() < 0.1){
			returnColor = vec3(0.9,0.9,0.9);
		}
			   
	}
	return returnColor;

}
void A4_Render(
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

	// Deal with viewport
	// Create orthonormal basis
	vec3 wVec = normalize(view); // z-axis
	vec3 uVec = normalize(cross(up,view)); //x -axis
	vec3 vVec = cross(uVec,wVec); // y-axis
	float dFloat = h/2/glm::tan(glm::radians(fovy/2)); // focal length
	// TODO: convert to Top left if possible
	// ray direction at bottom left corner
	const vec3 initDirVec = wVec * dFloat - uVec * (float)w/2 -vVec *(float)h/2;

	float progressFloat = 0.1f;
	float ratioFloat = 0.0f;

	// loop through each pixel and peform ray tracing on each one
	for (uint y = 0; y < h; ++y) {
		for (uint x = 0; x < w; ++x) {
			// Per pixel actions here
			// Get corresponding direction for pixel
			const vec3 dirVec = initDirVec + (float)(w-x) * uVec + (float)(y) * vVec;

			// 	Create Ray
			RayTracer ray = RayTracer();
			ray.setOrigin(eye);
			ray.setDirection(dirVec);

			// Initialize Color
			vec3 pixelColorVec(0.0f,0.0f,0.0f);

			// Anti-Aliasing
			if (ANTI_ALIASING >= 1 ){
				size_t samplesPerPixel = 10;
				for (int i =0;i < samplesPerPixel;++i){
					ray.setDirection(dirVec + randUnitVector() * (uVec +vVec) * 0.5);
					pixelColorVec += rayTraceRGB(root,ray,eye,ambient,lights);
				}
				pixelColorVec = pixelColorVec/samplesPerPixel;
				
			}else{
				pixelColorVec += rayTraceRGB(root,ray,eye,ambient,lights);
			}
			
			// Red: 
			image(x, y, 0) = (double)pixelColorVec.r;
			// Green: 
			image(x, y, 1) = (double)pixelColorVec.g;
			// Blue: 
			image(x, y, 2) = (double)pixelColorVec.b;
		}
		ratioFloat = (y+1)/(float)(h);
		if ( ratioFloat >= progressFloat){
			std:cout << std::fixed<< std::setprecision(2);
			std::cout << "precentage complete: "<< 100 * ratioFloat <<"%" << std::endl;
			progressFloat = progressFloat + 0.1f;
		}
	}
	std::cout << "precentage complete: 100.0%" << std::endl;
	dbgPrint("Debug");
}
