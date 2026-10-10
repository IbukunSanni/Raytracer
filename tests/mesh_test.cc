// Texture coordinates on a mesh read from OBJ, tested without a render.
//
// One claim: a ray that hits a mesh comes back with the (u, v) its face's
// vt corners interpolate to, by the BVH and by the linear scan alike.
//
// The mesh is a unit square in z = 0, as two triangles fanned from one quad,
// with vt corners that map it affinely onto [0.1, 0.9] x [0.2, 0.8]. An
// affine map is exactly what barycentric interpolation reproduces, so every
// point on the square has a known (u, v): u = 0.1 + 0.8x, v = 0.2 + 0.6y,
// whichever of the two triangles it lands in.
//
// The points avoid beta == gamma: at (0.25, 0.5), for one, they are equal,
// and a UVAt with the two weights swapped passed there.

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <glm/glm.hpp>
#include <string>

#include "core/hit_record.h"
#include "core/ray.h"
#include "geometry/mesh.h"

namespace {

// Writes `body` to an OBJ under tests/out/ and returns its path.
std::string WriteObj(const std::string& name, const std::string& body) {
  std::filesystem::create_directories("tests/out");
  const std::string path = "tests/out/" + name + ".obj";
  std::ofstream(path) << body;
  return path;
}

// The square, with its four vt lines and one quad face written `corners`.
std::string Square(const std::string& corners) {
  return "v 0 0 0\nv 1 0 0\nv 0 1 0\nv 1 1 0\n"
         "vt 0.1 0.2\nvt 0.9 0.2\nvt 0.1 0.8\nvt 0.9 0.8\n"
         "vn 0 0 1\n"
         "f " +
         corners + "\n";
}

// Straight down onto the square at (x, y).
Ray DownAt(float x, float y) {
  Ray ray;
  ray.SetOrigin(glm::vec3(x, y, 1.0f));
  ray.SetDirection(glm::vec3(0.0f, 0.0f, -1.0f));
  return ray;
}

// Both of the mesh's hit paths at (x, y): IsHit, which takes the BVH when
// one is built, and LinearScan, which never does.
void CheckUV(Mesh& mesh, float x, float y, float u, float v) {
  CAPTURE(x);
  CAPTURE(y);
  for (int path = 0; path < 2; ++path) {
    CAPTURE(path);
    Ray ray = DownAt(x, y);
    HitRecord hit;
    const bool is_hit = path == 0 ? mesh.IsHit(ray, 1e-4f, 10.0f, hit)
                                  : mesh.LinearScan(ray, 1e-4f, 10.0f, hit);
    REQUIRE(is_hit);
    CHECK(hit.GetU() == doctest::Approx(u).epsilon(1e-5));
    CHECK(hit.GetV() == doctest::Approx(v).epsilon(1e-5));
  }
}

}  // namespace

TEST_SUITE("mesh/uv") {
  TEST_CASE("mesh uv: v/vt corners interpolate across both fanned triangles") {
    Mesh mesh(WriteObj("uv_square", Square("1/1 2/2 4/4 3/3")));
    REQUIRE(mesh.Faces().size() == 2);
    CheckUV(mesh, 0.20f, 0.70f, 0.1f + 0.8f * 0.20f, 0.2f + 0.6f * 0.70f);
    CheckUV(mesh, 0.70f, 0.20f, 0.1f + 0.8f * 0.70f, 0.2f + 0.6f * 0.20f);
  }

  TEST_CASE("mesh uv: v/vt/vn and negative indices read the same vt") {
    Mesh mesh(WriteObj("uv_square_vn_relative",
                       Square("-4/-4/1 -3/-3/1 -1/-1/1 -2/-2/1")));
    CheckUV(mesh, 0.20f, 0.70f, 0.1f + 0.8f * 0.20f, 0.2f + 0.6f * 0.70f);
  }

  TEST_CASE("mesh uv: a face with no vt reads (0, 0)") {
    Mesh mesh(WriteObj("uv_square_no_vt", Square("1//1 2//1 4//1 3//1")));
    REQUIRE(mesh.Faces().size() == 2);
    CheckUV(mesh, 0.20f, 0.70f, 0.0f, 0.0f);
  }

  TEST_CASE(
      "mesh uv: a vt index past the last vt costs the UVs, not the face") {
    Mesh mesh(WriteObj("uv_square_bad_vt", Square("1/1 2/2 4/9 3/3")));
    REQUIRE(mesh.Faces().size() == 2);
    CheckUV(mesh, 0.20f, 0.70f, 0.0f, 0.0f);
  }
}
