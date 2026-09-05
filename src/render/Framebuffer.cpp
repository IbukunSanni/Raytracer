#include "render/Framebuffer.hpp"

Framebuffer::Framebuffer(size_t width, size_t height)
	: m_width(width)
	, m_height(height)
	, m_samples(0)
	, m_sum(width * height, glm::dvec3(0.0))
{
}

void Framebuffer::add(size_t x, size_t y, const glm::vec3 & radiance)
{
	m_sum[y * m_width + x] += glm::dvec3(radiance);
}

void Framebuffer::addSamples(size_t n)
{
	m_samples += n;
}

void Framebuffer::resolve(Image & out) const
{
	if (m_samples == 0) {
		for (size_t y = 0; y < m_height; ++y) {
			for (size_t x = 0; x < m_width; ++x) {
				out((unsigned int) x, (unsigned int) y, 0) = 0.0;
				out((unsigned int) x, (unsigned int) y, 1) = 0.0;
				out((unsigned int) x, (unsigned int) y, 2) = 0.0;
			}
		}
		return;
	}

	const double inv = 1.0 / (double) m_samples;

	for (size_t y = 0; y < m_height; ++y) {
		for (size_t x = 0; x < m_width; ++x) {
			const glm::dvec3 & s = m_sum[y * m_width + x];
			out((unsigned int) x, (unsigned int) y, 0) = s.r * inv;
			out((unsigned int) x, (unsigned int) y, 1) = s.g * inv;
			out((unsigned int) x, (unsigned int) y, 2) = s.b * inv;
		}
	}
}
