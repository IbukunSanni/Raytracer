// Termm--Fall 2020

#include "core/Image.hpp"

#include "core/Log.hpp"
#include "core/ToneMap.hpp"
#include <iostream>
#include <cstring>
#include <filesystem>
#include <lodepng/lodepng.h>
#include <vector>

const unsigned int Image::m_colorComponents = 3; // Red, blue, green

//---------------------------------------------------------------------------------------
Image::Image()
  : m_width(0),
    m_height(0),
    m_data(0)
{
}

//---------------------------------------------------------------------------------------
Image::Image(
		unsigned int width,
		unsigned int height
)
  : m_width(width),
    m_height(height)
{
	size_t numElements = m_width * m_height * m_colorComponents;
	m_data = new double[numElements];
	memset(m_data, 0, numElements*sizeof(double));
}

//---------------------------------------------------------------------------------------
Image::Image(const Image & other)
  : m_width(other.m_width),
    m_height(other.m_height),
    m_data(other.m_data ? new double[m_width * m_height * m_colorComponents] : 0)
{
  if (m_data) {
    std::memcpy(m_data, other.m_data,
                m_width * m_height * m_colorComponents * sizeof(double));
  }
}

//---------------------------------------------------------------------------------------
Image::~Image()
{
  delete [] m_data;
}

//---------------------------------------------------------------------------------------
Image & Image::operator=(const Image& other)
{
  delete [] m_data;
  
  m_width = other.m_width;
  m_height = other.m_height;
  m_data = (other.m_data ? new double[m_width * m_height * m_colorComponents] : 0);

  if (m_data) {
    std::memcpy(m_data,
                other.m_data,
                m_width * m_height * m_colorComponents * sizeof(double)
    );
  }
  
  return *this;
}

//---------------------------------------------------------------------------------------
unsigned int Image::width() const
{
  return m_width;
}

//---------------------------------------------------------------------------------------
unsigned int Image::height() const
{
  return m_height;
}

//---------------------------------------------------------------------------------------
double Image::operator()(unsigned int x, unsigned int y, unsigned int i) const
{
  return m_data[m_colorComponents * (m_width * y + x) + i];
}

//---------------------------------------------------------------------------------------
double & Image::operator()(unsigned int x, unsigned int y, unsigned int i)
{
  return m_data[m_colorComponents * (m_width * y + x) + i];
}

//---------------------------------------------------------------------------------------
bool Image::savePng(const std::string & filename) const
{
	return savePng(filename, tonemap::Config{});
}

//---------------------------------------------------------------------------------------
bool Image::savePng(const std::string & filename, const tonemap::Config & cfg) const
{
	// Linear radiance -> tone map -> sRGB transfer -> bytes. Both stages
	// live here, after linear averaging in Framebuffer::resolve. Clamping
	// is the tone map's job -- clamping first would flatten the curve's
	// input. See core/ToneMap.hpp.
	std::vector<unsigned char> image(m_width * m_height * m_colorComponents);

	for (unsigned int y = 0; y < m_height; ++y) {
		for (unsigned int x = 0; x < m_width; ++x) {
			const size_t pixelIndex = m_colorComponents * (m_width * y + x);

			// All three channels at once: luminance-based operators need
			// the whole colour.
			const glm::vec3 linear((float) m_data[pixelIndex + 0],
			                       (float) m_data[pixelIndex + 1],
			                       (float) m_data[pixelIndex + 2]);
			const glm::vec3 mapped = tonemap::apply(linear, cfg);

			for (unsigned int c = 0; c < m_colorComponents; ++c) {
				const double v = cfg.encodeSRGB ? tonemap::encodeSRGB(mapped[c])
				                                : (double) mapped[c];
				// +0.5 so the cast rounds instead of truncating.
				image[pixelIndex + c] = static_cast<unsigned char>(255.0 * v + 0.5);
			}
		}
	}

	// lodepng won't create missing directories; it would just fail below.
	const std::filesystem::path parent =
	    std::filesystem::path(filename).parent_path();
	if (!parent.empty()) {
		std::error_code ec;
		std::filesystem::create_directories(parent, ec);
		if (ec) {
			LOG_ERROR(IMAGE) << "could not create output directory "
			                 << parent.string() << ": " << ec.message();
			return false;
		}
	}

	// Encode the image
	unsigned error = lodepng::encode(filename, image, m_width, m_height, LCT_RGB);

	if(error) {
		LOG_ERROR(IMAGE) << "png encode failed for " << filename << ": "
		                 << lodepng_error_text(error);
		return false;
	}

	return true;
}

//---------------------------------------------------------------------------------------
const double * Image::data() const
{
  return m_data;
}

//---------------------------------------------------------------------------------------
double * Image::data()
{
  return m_data;
}
