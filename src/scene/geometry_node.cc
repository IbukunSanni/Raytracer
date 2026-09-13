// Termm--Fall 2020

#include "scene/geometry_node.h"

//---------------------------------------------------------------------
GeometryNode::GeometryNode(const std::string& name, Primitive* prim,
                           Material* mat)
    : SceneNode(name), material(mat), primitive(prim) {
  node_type = NodeType::kGeometryNode;
}

void GeometryNode::SetMaterial(Material* mat) {
  // Obviously, there's a potential memory leak here.  A good solution
  // would be to use some kind of reference counting, as in the
  // C++ shared_ptr.  But I'm going to punt on that problem here.
  // Why?  Two reasons:
  // (a) In practice we expect the scene to be constructed exactly
  //     once.  There's no reason to believe that materials will be
  //     repeatedly overwritten in a GeometryNode.
  // (b) A ray tracer is a program in which you compute once, and
  //     throw away all your data.  A memory leak won't build up and
  //     crash the program.

  material = mat;
}

bool GeometryNode::IsHit(Ray& ray, float t0_float, float t1_float,
                         HitRecord& record) {
  // Transform into this node's space exactly once.
  Ray local_ray = ToLocal(ray);

  bool hit = false;

  // This node's own primitive.
  HitRecord prim_record;
  if (primitive->IsHit(local_ray, t0_float, t1_float, prim_record)) {
    prim_record.SetMaterial(material);
    hit = true;
    t1_float = prim_record.GetT();  // narrow the search
    record = prim_record;
  }

  // Any children. HitChildren does NOT re-transform -- local_ray is already
  // in this node's space, and each child applies its own transform.
  HitRecord child_record;
  if (HitChildren(local_ray, t0_float, t1_float, child_record)) {
    hit = true;
    t1_float = child_record.GetT();
    record = child_record;
  }

  if (hit) {
    ToWorld(record);
  }

  return hit;
}
