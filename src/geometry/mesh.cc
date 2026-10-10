#include <cmath>
#include <cstdlib>
#include <fstream>
#include <glm/ext.hpp>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "core/log.h"
#include "core/stats.h"

// (OBJ parsing is done inline in Mesh(const std::string&) below)
#include "geometry/mesh.h"

// Barycentric tolerance for the triangle test. TODO: it trims the edges at
// corner a but not the third, so a ray within kEps of a fan diagonal, an edge
// at a in both its triangles, misses both. Test all three against -kEps.
static const float kEps = 0.00001f;

// The vt index in a face corner ("v/vt" or "v/vt/vn"), 0-based. kNone when
// the corner has none ("v", "v//vn") or it names no vt line read so far, so
// a bad vt costs the face its UVs but keeps its geometry.
static size_t ParseUVIndex(const std::string& token, size_t uv_count) {
  const size_t first = token.find('/');
  if (first == std::string::npos) return TriangleUV::kNone;
  const size_t second = token.find('/', first + 1);
  const size_t end = second == std::string::npos ? token.size() : second;

  const std::string field = token.substr(first + 1, end - first - 1);

  if (field.empty()) return TriangleUV::kNone;

  char* endp = nullptr;
  const long raw = std::strtol(field.c_str(), &endp, 10);
  if (endp == field.c_str() || *endp != '\0') return TriangleUV::kNone;

  // 1-based, and negative counts back from the last vt line, as for "v".
  const long idx1 = (raw < 0) ? static_cast<long>(uv_count) + raw + 1 : raw;
  if (idx1 < 1 || static_cast<size_t>(idx1) > uv_count) {
    return TriangleUV::kNone;
  }
  return static_cast<size_t>(idx1 - 1);
}

Mesh::Mesh(const std::string& fname) : vertices_(), faces_() {
  // Reads an OBJ one line at a time, keeping `v`, `vt` and `f` and skipping
  // the rest. A face corner may be written "v", "v/vt", "v//vn" or
  // "v/vt/vn"; indices are 1-based, and negative ones count back from the
  // last line read. Faces with more than three corners are split into a fan
  // of triangles. A face with a bad vertex index is skipped and counted; a
  // bad vt index loses only that face's UVs.

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

    if (code == "vt") {
      // A third coordinate, for 3D textures, is ignored.
      double u, v;
      if (ls >> u >> v) {
        uvs_.push_back(glm::vec2(u, v));
      }
      continue;
    }

    if (code != "f") continue;  // vn, g, usemtl, comments...

    // Collect every corner of this face, however many there are, with its
    // texture coordinate index where it has one.
    std::vector<size_t> corners;
    std::vector<size_t> corner_uvs;
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
      corner_uvs.push_back(ParseUVIndex(token, uvs_.size()));
    }

    if (!face_ok || corners.size() < 3) {
      ++skipped_faces;
      continue;
    }

    // Fan a polygon into triangles. Correct for convex faces, which is
    // what OBJ exporters emit.
    for (size_t k = 2; k < corners.size(); ++k) {
      faces_.push_back(Triangle(corners[0], corners[k - 1], corners[k]));
      face_uvs_.push_back(
          TriangleUV{corner_uvs[0], corner_uvs[k - 1], corner_uvs[k]});
    }
  }

  if (skipped_faces > 0) {
    LOG_WARN(kGeom) << fname << ": skipped " << skipped_faces
                    << " malformed face(s)";
  }

  bvh_.Build(vertices_, faces_, BVH::Split());
  if (bvh_.IsBuilt() && !rt::stats::kEnabled) {
    LOG_DEBUG(kGeom) << "mesh " << fname << ": " << vertices_.size()
                     << " verts, " << faces_.size()
                     << " faces, bvh built (stats off, set RT_STATS=1)";
    return;
  }
  if (!bvh_.IsBuilt()) {
    LOG_DEBUG(kGeom) << "mesh " << fname << ": " << vertices_.size()
                     << " verts, " << faces_.size()
                     << " faces, bvh not built (linear scan)";
    return;
  }
  LOG_DEBUG(kGeom) << "mesh " << fname << ": " << vertices_.size() << " verts, "
                   << faces_.size() << " faces, bvh ("
                   << BVH::SplitName(BVH::Split()) << ") built in "
                   << std::fixed << std::setprecision(2) << bvh_.BuildMs()
                   << " ms: " << bvh_.NodeCount() << " nodes, "
                   << bvh_.LeafCount() << " leaves, depth " << bvh_.MaxDepth()
                   << ", "
                   << static_cast<double>(faces_.size()) / bvh_.LeafCount()
                   << " tris/leaf (max " << bvh_.MaxLeafSize() << ")";
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

/**
 * Tests a ray against one triangle by solving e + t*d = a + beta*(b - a) +
 * gamma*(c - a) for (beta, gamma, t) with Cramer's rule.
 *
 * @param ray          the ray; its direction need not be normalised.
 * @param vert0        triangle corner a.
 * @param vert1        triangle corner b.
 * @param vert2        triangle corner c.
 * @param pot_t1_float out: the ray parameter t of the plane crossing. It is
 *                     written even when the function returns false, so read
 *                     it only on a true return.
 * @param t0_float     nearest accepted t, inclusive.
 * @param t1_float     farthest accepted t, inclusive. Pass the closest hit
 *                     so far to accept only nearer triangles.
 * @param beta_out     out, optional: beta at the crossing, on a true return.
 * @param gamma_out    out, optional: gamma at the crossing, on a true return.
 * @return true if t lies in [t0_float, t1_float] and the crossing is inside
 *         the triangle: beta >= kEps, gamma >= kEps, beta + gamma <= 1.
 *         Hits within kEps of the edges at a are rejected.
 */
bool Mesh::IsTriangleIntersection(Ray& ray, glm::vec3 vert0, glm::vec3 vert1,
                                  glm::vec3 vert2, float& pot_t1_float,
                                  float t0_float, float t1_float,
                                  float* beta_out, float* gamma_out) {
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
  // Each test below accepts only what lies inside its range. A ray
  // parallel to the triangle's plane makes m zero and these values NaN,
  // and "outside the range" phrased as two comparisons lets NaN through.
  if (!(pot_t1_float >= t0_float && pot_t1_float <= t1_float)) {
    // cout << "Mesh::IsTriangleIntersection() left t false" << endl;
    return false;
  }

  // compute gamma
  float gamma = (1 / m) * (i * (a * k - j * b) + h * (j * c - a * l) +
                           g * (b * l - k * c));
  if (!(gamma >= kEps && gamma <= 1)) {
    // cout << "Mesh::IsTriangleIntersection() left gamma false" << endl;
    return false;
  }

  // compute beta
  float beta = (1 / m) * (j * (e * i - h * f) + k * (g * f - d * i) +
                          l * (d * h - e * g));
  if (!(beta >= kEps && beta <= 1 - gamma)) {
    // cout << "Mesh::IsTriangleIntersection() left beta false" << endl;
    return false;
  }

  if (beta_out) *beta_out = beta;
  if (gamma_out) *gamma_out = gamma;

  // cout << "Mesh::IsTriangleIntersection() left true" << endl;
  return true;
}

bool Mesh::LinearScan(Ray& ray, float t0_float, float t1_float,
                      HitRecord& hit) const {
  bool is_hit = false;
  glm::vec3 normal_vec = glm::vec3();
  float new_t1float = t1_float;
  size_t best_face = 0;
  float best_beta = 0.0f, best_gamma = 0.0f;
  // One atomic add for the whole scan. The loop below tests every face
  // unconditionally, so the total matches a per-triangle count exactly.
  BVH::CountTrianglesTested(static_cast<long long>(faces_.size()));
  // Traverse every face looking for the closest hit.
  for (size_t face_index = 0; face_index < faces_.size(); ++face_index) {
    const Triangle& face = faces_[face_index];
    float pot_t1_float = 0.0f, beta = 0.0f, gamma = 0.0f;
    if (IsTriangleIntersection(ray, vertices_[face.v1], vertices_[face.v2],
                               vertices_[face.v3], pot_t1_float, t0_float,
                               new_t1float, &beta, &gamma)) {
      is_hit = true;
      new_t1float = pot_t1_float;
      best_face = face_index;
      best_beta = beta;
      best_gamma = gamma;
      glm::vec3 face_vec1 = vertices_[face.v1] - vertices_[face.v2];
      glm::vec3 face_vec2 = vertices_[face.v2] - vertices_[face.v3];
      normal_vec = cross(face_vec1, face_vec2);
    }
  }
  if (!is_hit) {
    return false;
  }
  // Flipping the normals
  if (dot(ray.GetDirection(), normal_vec) > 0) {
    normal_vec = -normal_vec;
  }

  hit.SetHit(new_t1float, ray.GetPointAtT(new_t1float), normal_vec);
  const glm::vec2 uv = UVAt(best_face, best_beta, best_gamma);
  hit.SetUV(uv.x, uv.y);
  hit.SetMaterial(nullptr);
  return is_hit;
}

// Set BVH_VERIFY=1 in the environment to run BOTH paths on every ray
// and report any disagreement. Slow, but it is the fastest way to find a
// BVH bug: a tree that is merely inefficient still renders correctly,
// while one that drops triangles produces holes you may not notice.
static bool BvhVerifyEnabled() {
  static const bool kOn = (std::getenv("BVH_VERIFY") != nullptr);
  return kOn;
}

bool Mesh::IsHit(Ray& ray, float t0_float, float t1_float, HitRecord& hit) {
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
    hit.SetHit(t_float, p_vec, p_vec - center);
    return true;
  }

  // No usable tree yet, or the scan was asked for -> exhaustive scan.
  if (!bvh_.IsBuilt() || BVH::Traversal() == BVHTraversal::kLinear) {
    return LinearScan(ray, t0_float, t1_float, hit);
  }

  BVHHit bvh_hit;
  bool is_hit = BVH::Traversal() == BVHTraversal::kRecursive
                    ? bvh_.TraverseRecursive(ray, t0_float, t1_float, vertices_,
                                             faces_, bvh_hit)
                    : bvh_.TraverseIterative(ray, t0_float, t1_float, vertices_,
                                             faces_, bvh_hit);

  if (BvhVerifyEnabled()) {
    HitRecord ref_hit;
    bool ref_is_hit = LinearScan(ray, t0_float, t1_float, ref_hit);
    if (ref_is_hit != is_hit ||
        (ref_is_hit && std::abs(ref_hit.GetT() - bvh_hit.t) > 1e-4f)) {
      LOG_ERROR(kGeom) << "bvh mismatch: linear hit=" << ref_is_hit
                       << " t=" << (ref_is_hit ? ref_hit.GetT() : -1.0f)
                       << " | bvh hit=" << is_hit
                       << " t=" << (is_hit ? bvh_hit.t : -1.0f) << " | "
                       << faces_.size() << " faces, ray from " << std::hexfloat
                       << ray.GetOrigin().x << " " << ray.GetOrigin().y << " "
                       << ray.GetOrigin().z << " along " << ray.GetDirection().x
                       << " " << ray.GetDirection().y << " "
                       << ray.GetDirection().z << ", t in [" << t0_float << ", "
                       << t1_float << "]";
    }
  }

  if (!is_hit) return false;

  const Triangle& face = faces_[bvh_hit.face_index];
  glm::vec3 face_vec1 = vertices_[face.v1] - vertices_[face.v2];
  glm::vec3 face_vec2 = vertices_[face.v2] - vertices_[face.v3];
  glm::vec3 normal_vec = cross(face_vec1, face_vec2);
  if (dot(ray.GetDirection(), normal_vec) > 0) {
    normal_vec = -normal_vec;
  }

  hit.SetHit(bvh_hit.t, ray.GetPointAtT(bvh_hit.t), normal_vec);
  const glm::vec2 uv = UVAt(bvh_hit.face_index, bvh_hit.beta, bvh_hit.gamma);
  hit.SetUV(uv.x, uv.y);
  hit.SetMaterial(nullptr);
  return true;
}

glm::vec2 Mesh::UVAt(size_t face_index, float beta, float gamma) const {
  if (face_index >= face_uvs_.size()) return glm::vec2(0.0f);
  const TriangleUV& corners = face_uvs_[face_index];
  if (corners.uv1 == TriangleUV::kNone || corners.uv2 == TriangleUV::kNone ||
      corners.uv3 == TriangleUV::kNone) {
    return glm::vec2(0.0f);
  }
  // The same weights that place the point: a gets what b and c do not.
  return (1.0f - beta - gamma) * uvs_[corners.uv1] + beta * uvs_[corners.uv2] +
         gamma * uvs_[corners.uv3];
}

// Builds a mesh from vertices and face indices that are already computed.
// NonhierBox uses it to turn its eight corners into twelve triangles once,
// instead of re-deriving them on every intersection test.

Mesh::Mesh(std::vector<glm::vec3>& complete_verts,
           const std::vector<glm::vec3>& faces)
    : vertices_(complete_verts), faces_() {
  for (size_t i = 0; i < faces.size(); i++) {
    faces_.push_back(Triangle(static_cast<size_t>(faces[i].x),
                              static_cast<size_t>(faces[i].y),
                              static_cast<size_t>(faces[i].z)));
  }

  bvh_.Build(vertices_, faces_, BVH::Split());
}
