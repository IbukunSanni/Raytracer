#include <cmath>
#include <cstdlib>
#include <fstream>
#include <glm/ext.hpp>
#include <iostream>
#include <sstream>

#include "core/log.h"

// (OBJ parsing is done inline in Mesh(const std::string&) below)
#include "geometry/mesh.h"

// TODO: confirm const is good
static const float kEps = 0.00001f;

Mesh::Mesh(const std::string& fname) : vertices_(), faces_() {
  // OBJ face parsing.
  //
  // This used to read three bare integers per face:
  //
  //     size_t s1, s2, s3;
  //     ifs >> s1 >> s2 >> s3;
  //     faces_.push_back( Triangle( s1 - 1, s2 - 1, s3 - 1 ) );
  //
  // which silently corrupted any file using the "v/vt/vn" form. On
  // "f 1/1/1 2/2/1 3/3/1" the first extraction reads 1 and stops at the
  // slash; the next fails and (C++11) sets its target to 0. Then 0 - 1 on
  // an unsigned size_t wraps to 18446744073709551615, one garbage triangle
  // is pushed, and failbit ends the loop -- so the rest of the mesh is
  // dropped too. Rendering then indexed vertices_ far out of bounds, which
  // faulted or not depending on heap layout.
  //
  // Read a line at a time and parse properly instead: any of "v", "v/vt",
  // "v//vn", "v/vt/vn"; negative (relative) indices; polygons larger than a
  // triangle, fanned; and every index range-checked before it is stored.

  std::ifstream ifs(fname.c_str());
  if (!ifs) {
    LOG_ERROR(kGeom) << "could not open mesh " << fname;
    return;
  }

  std::string line;
  size_t skipped_faces = 0;

  while (std::getline(ifs, line)) {
    std::istringstream ls(line);
    std::string code;
    if (!(ls >> code)) continue;

    if (code == "v") {
      double vx, vy, vz;
      if (ls >> vx >> vy >> vz) {
        vertices_.push_back(glm::vec3(vx, vy, vz));
      }
      continue;
    }

    if (code != "f") continue;  // vt, vn, g, usemtl, comments...

    // Collect every corner of this face, however many there are.
    std::vector<size_t> corners;
    std::string token;
    bool face_ok = true;

    while (ls >> token) {
      // Keep only the position index: everything up to the first slash.
      const std::string head = token.substr(0, token.find('/'));
      if (head.empty()) {
        face_ok = false;
        break;
      }

      char* endp = nullptr;
      const long raw = std::strtol(head.c_str(), &endp, 10);
      if (endp == head.c_str() || *endp != '\0') {
        face_ok = false;
        break;
      }

      // OBJ indices are 1-based; negative means relative to the end of
      // the vertices seen so far.
      long idx1 =
          (raw < 0) ? static_cast<long>(vertices_.size()) + raw + 1 : raw;
      if (idx1 < 1 || static_cast<size_t>(idx1) > vertices_.size()) {
        face_ok = false;
        break;
      }
      corners.push_back(static_cast<size_t>(idx1 - 1));
    }

    if (!face_ok || corners.size() < 3) {
      ++skipped_faces;
      continue;
    }

    // Fan a polygon into triangles. Correct for convex faces, which is
    // what OBJ exporters emit.
    for (size_t k = 2; k < corners.size(); ++k) {
      faces_.push_back(Triangle(corners[0], corners[k - 1], corners[k]));
    }
  }

  if (skipped_faces > 0) {
    LOG_WARN(kGeom) << fname << ": skipped " << skipped_faces
                    << " malformed face(s)";
  }

  bvh_.Build(vertices_, faces_);
  LOG_DEBUG(kGeom) << "mesh " << fname << ": " << vertices_.size() << " verts, "
                   << faces_.size() << " faces, bvh "
                   << (bvh_.IsBuilt() ? "built" : "not built (linear scan)");
}

std::ostream& operator<<(std::ostream& out, const Mesh& mesh) {
  out << "mesh {";
  /*

  for( size_t idx = 0; idx < mesh.verts_.size(); ++idx ) {
        const MeshVertex& v = mesh.verts_[idx];
        out << glm::to_string( v.position_ );
        if( mesh.have_norm_ ) {
          out << " / " << glm::to_string( v.normal_ );
        }
        if( mesh.have_uv_ ) {
          out << " / " << glm::to_string( v.uv_ );
        }
  }

*/
  out << "}";
  return out;
}

bool Mesh::IsTriangleIntersection(Ray& ray, glm::vec3 vert0, glm::vec3 vert1,
                                  glm::vec3 vert2, float& pot_t1_float,
                                  float t0_float, float t1_float) {
  // cout << "Mesh::IsTriangleIntersection() called" << endl;
  // Define all vectors, (e_vec,d_vec) for ray and (a_vec,b_vec,c_vec) for
  // triangle
  glm::vec3 e_vec = ray.GetOrigin();
  glm::vec3 d_vec = ray.GetDirection();

  glm::vec3 a_vec = vert0;
  glm::vec3 b_vec = vert1;
  glm::vec3 c_vec = vert2;

  // Ax = b, where x is unknown
  // Declare elements for A
  float a = (a_vec.x - b_vec.x);
  float b = (a_vec.y - b_vec.y);
  float c = (a_vec.z - b_vec.z);
  float d = (a_vec.x - c_vec.x);
  float e = (a_vec.y - c_vec.y);
  float f = (a_vec.z - c_vec.z);
  float g = (d_vec.x);
  float h = (d_vec.y);
  float i = (d_vec.z);

  // Declare elements for b
  float j = (a_vec.x - e_vec.x);
  float k = (a_vec.y - e_vec.y);
  float l = (a_vec.z - e_vec.z);

  // Denominator for cramer's rule
  float m = a * (e * i - h * f) + b * (g * f - d * i) + c * (d * h - e * g);

  // compute t
  pot_t1_float =
      (-1) * (1 / m) *
      (f * (a * k - j * b) + e * (j * c - a * l) + d * (b * l - k * c));
  if (pot_t1_float < t0_float || pot_t1_float > t1_float) {
    // cout << "Mesh::IsTriangleIntersection() left t false" << endl;
    return false;
  }

  // compute gamma
  float gamma = (1 / m) * (i * (a * k - j * b) + h * (j * c - a * l) +
                           g * (b * l - k * c));
  if (gamma < kEps || gamma > 1) {
    // cout << "Mesh::IsTriangleIntersection() left gamma false" << endl;
    return false;
  }

  // compute beta
  float beta = (1 / m) * (j * (e * i - h * f) + k * (g * f - d * i) +
                          l * (d * h - e * g));
  if (beta < kEps || (beta > 1 - gamma)) {
    // cout << "Mesh::IsTriangleIntersection() left beta false" << endl;
    return false;
  }

  // cout << "Mesh::IsTriangleIntersection() left true" << endl;
  return true;
}

bool Mesh::LinearScan(Ray& ray, float t0_float, float t1_float,
                      HitRecord& record) const {
  bool hit = false;
  glm::vec3 normal_vec = glm::vec3();
  float new_t1float = t1_float;
  // Traverse every face looking for the closest hit.
  for (auto face : faces_) {
    float pot_t1_float = 0.0f;
    if (IsTriangleIntersection(ray, vertices_[face.v1], vertices_[face.v2],
                               vertices_[face.v3], pot_t1_float, t0_float,
                               new_t1float)) {
      hit = true;
      new_t1float = pot_t1_float;
      glm::vec3 face_vec1 = vertices_[face.v1] - vertices_[face.v2];
      glm::vec3 face_vec2 = vertices_[face.v2] - vertices_[face.v3];
      normal_vec = cross(face_vec1, face_vec2);
    }
  }
  if (!hit) {
    return false;
  }
  // Flipping the normals
  if (dot(ray.GetDirection(), normal_vec) > 0) {
    normal_vec = -normal_vec;
  }

  record.SetHit(new_t1float, ray.GetPointAtT(new_t1float), normal_vec);
  record.SetMaterial(nullptr);
  return hit;
}

// Set BVH_VERIFY=1 in the environment to run BOTH paths on every ray
// and report any disagreement. Slow, but it is the fastest way to find a
// BVH bug: a tree that is merely inefficient still renders correctly,
// while one that drops triangles produces holes you may not notice.
static bool BvhVerifyEnabled() {
  static const bool kOn = (std::getenv("BVH_VERIFY") != nullptr);
  return kOn;
}

bool Mesh::IsHit(Ray& ray, float t0_float, float t1_float, HitRecord& record) {
  if (RENDER_BOUNDING_VOLUMES >= 1) {
    // Debug view: draw the mesh as its bounding sphere instead of its
    // geometry. This is a visualisation, not an acceleration test.
    AABB box;
    for (auto vert : vertices_) box.Expand(vert);
    glm::vec3 center = box.Centroid();
    float r = glm::length(box.max_vec - center);

    glm::vec3 e_minus_c_vec = ray.GetOrigin() - center;
    glm::vec3 d_vec = ray.GetDirection();
    double a = static_cast<double>(dot(d_vec, d_vec));
    double b = static_cast<double>(2 * dot(d_vec, e_minus_c_vec));
    double c = static_cast<double>(dot(e_minus_c_vec, e_minus_c_vec) - (r * r));
    double roots[2];
    size_t num_roots = QuadraticRoots(a, b, c, roots);
    float t_float = 0;
    switch (num_roots) {
      case 0:
        return false;
      case 1:
        t_float = static_cast<float>(roots[0]);
        break;
      default:
        t_float = static_cast<float>(glm::min(roots[0], roots[1]));
        break;
    }
    if (t_float <= t0_float || t1_float <= t_float) return false;
    const glm::vec3 p_vec = ray.GetPointAtT(t_float);
    record.SetHit(t_float, p_vec, p_vec - center);
    return true;
  }

  // No usable tree yet -> exhaustive scan. Correct, just slow.
  if (!bvh_.IsBuilt()) {
    return LinearScan(ray, t0_float, t1_float, record);
  }

  BVHHit bvh_hit;
  bool hit = bvh_.Traverse(ray, t0_float, t1_float, vertices_, faces_, bvh_hit);

  if (BvhVerifyEnabled()) {
    HitRecord ref_record;
    bool ref_hit = LinearScan(ray, t0_float, t1_float, ref_record);
    if (ref_hit != hit ||
        (ref_hit && std::abs(ref_record.GetT() - bvh_hit.t) > 1e-4f)) {
      LOG_ERROR(kGeom) << "bvh mismatch: linear hit=" << ref_hit
                       << " t=" << (ref_hit ? ref_record.GetT() : -1.0f)
                       << " | bvh hit=" << hit
                       << " t=" << (hit ? bvh_hit.t : -1.0f);
    }
  }

  if (!hit) return false;

  const Triangle& face = faces_[bvh_hit.face_index];
  glm::vec3 face_vec1 = vertices_[face.v1] - vertices_[face.v2];
  glm::vec3 face_vec2 = vertices_[face.v2] - vertices_[face.v3];
  glm::vec3 normal_vec = cross(face_vec1, face_vec2);
  if (dot(ray.GetDirection(), normal_vec) > 0) {
    normal_vec = -normal_vec;
  }

  record.SetHit(bvh_hit.t, ray.GetPointAtT(bvh_hit.t), normal_vec);
  record.SetMaterial(nullptr);
  return true;
}

// New Mesh Construction
// Used to create boxes to avoid triangle recalcultaion

Mesh::Mesh(std::vector<glm::vec3>& complete_verts,
           const std::vector<glm::vec3>& faces)
    : vertices_(complete_verts), faces_() {
  for (size_t i = 0; i < faces.size(); i++) {
    faces_.push_back(Triangle(static_cast<size_t>(faces[i].x),
                              static_cast<size_t>(faces[i].y),
                              static_cast<size_t>(faces[i].z)));
  }

  bvh_.Build(vertices_, faces_);
}
