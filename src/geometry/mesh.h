#ifndef RAYTRACER_SRC_GEOMETRY_MESH_H_
#define RAYTRACER_SRC_GEOMETRY_MESH_H_

#include <glm/glm.hpp>
#include <iosfwd>
#include <limits>
#include <string>
#include <vector>

#include "geometry/bvh.h"
#include "geometry/primitive.h"
#include "math/polyroots.h"

// Use this #define to selectively compile your code to render the
// bounding boxes around your mesh objects. Uncomment this option
// to turn it on.
#define RENDER_BOUNDING_VOLUMES 0

struct Triangle {
  size_t v1;
  size_t v2;
  size_t v3;

  Triangle(size_t pv1, size_t pv2, size_t pv3) : v1(pv1), v2(pv2), v3(pv3) {}
};

// A face's three indices into the mesh's texture coordinates, one per corner
// of the matching Triangle. Kept apart from Triangle on purpose: the BVH
// reads Triangle in its inner loop, and only the nearest hit needs UVs.
struct TriangleUV {
  static constexpr size_t kNone = std::numeric_limits<size_t>::max();

  size_t uv1 = kNone;
  size_t uv2 = kNone;
  size_t uv3 = kNone;
};

// A polygonal mesh.
class Mesh : public Primitive {
 public:
  explicit Mesh(const std::string& fname);
  Mesh(std::vector<glm::vec3>& complete_verts,
       const std::vector<glm::vec3>& faces);
  // static so the BVH can test triangles without holding a Mesh. Defined out
  // of line, so a caller in another file pays a call per triangle; defining
  // it inline here, or building with -flto, removes that cost.
  static bool IsTriangleIntersection(Ray& ray, glm::vec3 vert0, glm::vec3 vert1,
                                     glm::vec3 vert2, float& pot_t1_float,
                                     float t0_float, float t1_float,
                                     float* beta_out = nullptr,
                                     float* gamma_out = nullptr);
  // Exhaustive scan over every face. Kept as the fallback while the
  // BVH is unfinished, and as the reference the BVH is checked
  // against when BVH_VERIFY=1 is set in the environment.
  bool LinearScan(Ray& ray, float t0_float, float t1_float,
                  HitRecord& hit) const;
  const BVH& Bvh() const { return bvh_; }
  const std::vector<glm::vec3>& Vertices() const { return vertices_; }
  const std::vector<Triangle>& Faces() const { return faces_; }
  bool IsHit(Ray& ray, float t0_float, float t1_float, HitRecord& hit) override;

  // The (u, v) at barycentric (beta, gamma) on face `face_index`, interpolated
  // from its corners' texture coordinates. (0, 0) when the face has none,
  // as for any surface that sets no (u, v).
  glm::vec2 UVAt(size_t face_index, float beta, float gamma) const;

 private:
  std::vector<glm::vec3> vertices_;
  std::vector<Triangle> faces_;
  std::vector<glm::vec2> uvs_;  // the OBJ's vt lines, in file order
  std::vector<TriangleUV> face_uvs_;
  BVH bvh_;

  friend std::ostream& operator<<(std::ostream& out, const Mesh& mesh);
};

#endif  // RAYTRACER_SRC_GEOMETRY_MESH_H_
