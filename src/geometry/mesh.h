// Termm--Fall 2020

#ifndef RAYTRACER_SRC_GEOMETRY_MESH_H_
#define RAYTRACER_SRC_GEOMETRY_MESH_H_

#include <glm/glm.hpp>
#include <iosfwd>
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

// A polygonal mesh.
class Mesh : public Primitive {
 public:
  explicit Mesh(const std::string& fname);
  Mesh(std::vector<glm::vec3>& complete_verts,
       const std::vector<glm::vec3>& faces);
  // static so the BVH can test triangles without holding a Mesh.
  static bool IsTriangleIntersection(Ray& ray, glm::vec3 vert0, glm::vec3 vert1,
                                     glm::vec3 vert2, float& pot_t1_float,
                                     float t0_float, float t1_float);
  // Exhaustive scan over every face. Kept as the fallback while the
  // BVH is unfinished, and as the reference the BVH is checked
  // against when BVH_VERIFY=1 is set in the environment.
  bool LinearScan(Ray& ray, float t0_float, float t1_float,
                  HitRecord& record) const;
  const BVH& Bvh() const { return bvh_; }
  bool IsHit(Ray& ray, float t0_float, float t1_float,
             HitRecord& record) override;

 private:
  std::vector<glm::vec3> vertices_;
  std::vector<Triangle> faces_;
  BVH bvh_;

  friend std::ostream& operator<<(std::ostream& out, const Mesh& mesh);
};

#endif  // RAYTRACER_SRC_GEOMETRY_MESH_H_
