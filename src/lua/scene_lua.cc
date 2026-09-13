// Lua bindings for the scene description language.
//
// A scene file is a Lua script. This file defines the `gr` table it calls
// into: constructors for nodes, materials and lights, methods on a node for
// transforms and parenting, and the render entry point. Two luaL_Reg tables
// near the bottom register them, and run_lua drives the interpreter.
//
// The C API talks through a stack, so every lua_/luaL_ call pushes, reads or
// pops it, and each binding returns how many values it left there for Lua.
// The luaL_ functions come from lauxlib, a convenience layer over the core
// API. Those that "check" an argument raise a Lua error and do NOT return on
// failure -- there is no error branch to write after one.
//
// Reference: the Lua 5.3 manual, https://www.lua.org/manual/5.3/
//
// Derived from the University of Waterloo CS488 course framework (2005).

#include "lua/scene_lua.h"

#include <cctype>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <map>
#include <vector>

#include "lua/lua488.h"

#include "core/log.h"
#include "core/tone_map.h"
#include "geometry/mesh.h"
#include "geometry/primitive.h"
#include "render/renderer.h"
#include "scene/geometry_node.h"
#include "scene/joint_node.h"
#include "scene/light.h"
#include "scene/material.h"

typedef std::map<std::string, Mesh*> MeshMap;
static MeshMap mesh_map;

// Uncomment the following line to enable debugging messages
// #define GRLUA_ENABLE_DEBUG

#ifdef GRLUA_ENABLE_DEBUG
#define GRLUA_DEBUG(x)   \
  do {                   \
    LOG_TRACE(LUA) << x; \
  } while (0)
#define GRLUA_DEBUG_CALL            \
  do {                              \
    LOG_TRACE(LUA) << __FUNCTION__; \
  } while (0)
#else
#define GRLUA_DEBUG(x) \
  do {                 \
  } while (0)
#define GRLUA_DEBUG_CALL \
  do {                   \
  } while (0)
#endif

// Lua allocates the userdata; C++ owns whatever it points at. Lua runs no
// constructors or destructors, and the scene must outlive the interpreter so
// it can still be rendered after parsing -- so each userdata holds nothing but
// a pointer, and closing Lua loses only that pointer.
struct GrNodeUd {
  SceneNode* node;
};

struct GrMaterialUd {
  Material* material;
};

struct GrLightUd {
  Light* light;
};

// Useful function to retrieve and check an n-tuple of numbers.
template <typename T>
void GetTuple(lua_State* state, int arg, T* data, int n) {
  luaL_checktype(state, arg, LUA_TTABLE);
  luaL_argcheck(state, lua_rawlen(state, arg) == static_cast<size_t>(n), arg,
                "N-tuple expected");
  for (int i = 1; i <= n; i++) {
    lua_rawgeti(state, arg, i);
    data[i - 1] = static_cast<T>(luaL_checknumber(state, -1));
    lua_pop(state, 1);
  }
}

// Read three consecutive numeric arguments as a vector. Kept separate from
// get_tuple because these arrive as loose arguments, not inside a table.
static glm::vec3 GetVec3Args(lua_State* state, int arg) {
  const double x = luaL_checknumber(state, arg);
  const double y = luaL_checknumber(state, arg + 1);
  const double z = luaL_checknumber(state, arg + 2);

  return glm::vec3(x, y, z);
}

// Wrap a node in the userdata Lua holds and stamp it with the gr.node
// metatable, which is what luaL_checkudata later matches against. Shared tail
// of every node constructor, the way push_material is for materials.
static int PushNode(lua_State* state, SceneNode* node) {
  GrNodeUd* data =
      static_cast<GrNodeUd*>(lua_newuserdata(state, sizeof(GrNodeUd)));
  data->node = node;

  luaL_getmetatable(state, "gr.node");
  lua_setmetatable(state, -2);

  return 1;
}

// gr.node(name) -- a bare transform node, the scene graph's interior
extern "C" int GrNodeCmd(lua_State* state) {
  GRLUA_DEBUG_CALL;

  const char* name = luaL_checkstring(state, 1);

  return PushNode(state, new SceneNode(name));
}

// gr.joint(name, {min, init, max}, {min, init, max})
extern "C" int GrJointCmd(lua_State* state) {
  GRLUA_DEBUG_CALL;

  const char* name = luaL_checkstring(state, 1);

  double x[3], y[3];
  GetTuple(state, 2, x, 3);
  GetTuple(state, 3, y, 3);

  JointNode* node = new JointNode(name);
  node->SetJointX(x[0], x[1], x[2]);
  node->SetJointY(y[0], y[1], y[2]);

  return PushNode(state, node);
}

// gr.sphere(name) -- unit sphere at the origin, placed by node transforms
extern "C" int GrSphereCmd(lua_State* state) {
  GRLUA_DEBUG_CALL;

  const char* name = luaL_checkstring(state, 1);

  return PushNode(state, new GeometryNode(name, new Sphere()));
}

// gr.cube(name) -- unit cube at the origin, placed by node transforms
extern "C" int GrCubeCmd(lua_State* state) {
  GRLUA_DEBUG_CALL;

  const char* name = luaL_checkstring(state, 1);

  return PushNode(state, new GeometryNode(name, new Cube()));
}

// gr.nh_sphere(name, {x, y, z}, radius) -- position baked into the primitive
extern "C" int GrNhSphereCmd(lua_State* state) {
  GRLUA_DEBUG_CALL;

  const char* name = luaL_checkstring(state, 1);

  glm::vec3 pos;
  GetTuple(state, 2, &pos[0], 3);

  double radius = luaL_checknumber(state, 3);

  return PushNode(state,
                  new GeometryNode(name, new NonhierSphere(pos, radius)));
}

// gr.nh_box(name, {x, y, z}, size) -- position baked into the primitive
extern "C" int GrNhBoxCmd(lua_State* state) {
  GRLUA_DEBUG_CALL;

  const char* name = luaL_checkstring(state, 1);

  glm::vec3 pos;
  GetTuple(state, 2, &pos[0], 3);

  double size = luaL_checknumber(state, 3);

  return PushNode(state, new GeometryNode(name, new NonhierBox(pos, size)));
}

// gr.mesh(name, 'path/to/model.obj')
extern "C" int GrMeshCmd(lua_State* state) {
  GRLUA_DEBUG_CALL;

  const char* name = luaL_checkstring(state, 1);
  const char* obj_fname = luaL_checkstring(state, 2);

  std::string sfname(obj_fname);

  // Keyed by filename so a model shared by several nodes is parsed once.
  auto i = mesh_map.find(sfname);
  Mesh* mesh = nullptr;

  if (i == mesh_map.end()) {
    mesh = new Mesh(obj_fname);
    mesh_map[sfname] = mesh;
  } else {
    mesh = i->second;
  }

  return PushNode(state, new GeometryNode(name, mesh));
}

// gr.light({x,y,z}, {r,g,b} [, {const, linear, quadratic}])
// Falloff defaults to {1, 0, 0} -- no attenuation -- when omitted.
extern "C" int GrLightCmd(lua_State* state) {
  GRLUA_DEBUG_CALL;

  // Checked before lua_newuserdata below, which pushes a value onto the
  // stack and would otherwise land on top of, and hide, a missing arg 3.
  const bool has_falloff = !lua_isnoneornil(state, 3);

  GrLightUd* data = (GrLightUd*)lua_newuserdata(state, sizeof(GrLightUd));
  data->light = 0;

  Light l;

  double col[3];
  GetTuple(state, 1, &l.position[0], 3);
  GetTuple(state, 2, col, 3);
  l.colour = glm::vec3(col[0], col[1], col[2]);

  if (has_falloff) {
    GetTuple(state, 3, l.falloff, 3);
  } else {
    l.falloff[0] = 1.0;
    l.falloff[1] = 0.0;
    l.falloff[2] = 0.0;
  }

  data->light = new Light(l);

  luaL_newmetatable(state, "gr.light");
  lua_setmetatable(state, -2);

  return 1;
}

// Render a scene. `lights` may be empty -- ambient/background is enough to
// light a scene on its own.
extern "C" int GrRenderCmd(lua_State* state) {
  GRLUA_DEBUG_CALL;

  GrNodeUd* root = static_cast<GrNodeUd*>(luaL_checkudata(state, 1, "gr.node"));

  const char* filename = luaL_checkstring(state, 2);

  int width = static_cast<int>(luaL_checknumber(state, 3));
  int height = static_cast<int>(luaL_checknumber(state, 4));

  glm::vec3 eye;
  glm::vec3 view, up;

  GetTuple(state, 5, &eye[0], 3);
  GetTuple(state, 6, &view[0], 3);
  GetTuple(state, 7, &up[0], 3);

  double fov = luaL_checknumber(state, 8);

  double ambient_data[3];
  GetTuple(state, 9, ambient_data, 3);
  glm::vec3 ambient(ambient_data[0], ambient_data[1], ambient_data[2]);

  luaL_checktype(state, 10, LUA_TTABLE);
  int light_count = int(lua_rawlen(state, 10));

  std::list<Light*> lights;
  for (int i = 1; i <= light_count; i++) {
    lua_rawgeti(state, 10, i);
    GrLightUd* ldata = (GrLightUd*)luaL_checkudata(state, -1, "gr.light");

    lights.push_back(ldata->light);
    lua_pop(state, 1);
  }

  Image im(width, height);
  SetOutputPath(filename);
  Render(root->node, im, eye, view, up, fov, ambient, lights);
  if (!im.SavePng(filename, GetToneMap())) {
    return luaL_error(state,
                      "gr.render: could not write output '%s' "
                      "(see log for details)",
                      filename);
  }

  return 0;
}

// Configure the thin-lens camera (depth of field).
//   gr.set_lens(aperture_radius, focus_distance, samples)
// aperture_radius 0 restores the pinhole camera.
extern "C" int GrSetLensCmd(lua_State* state) {
  GRLUA_DEBUG_CALL;

  float aperture = static_cast<float>(luaL_checknumber(state, 1));
  float focus = static_cast<float>(luaL_checknumber(state, 2));
  int samples = static_cast<int>(luaL_optnumber(state, 3, 16));

  luaL_argcheck(state, aperture >= 0.0f, 1, "aperture radius must be >= 0");
  luaL_argcheck(state, focus > 0.0f, 2, "focus distance must be > 0");
  luaL_argcheck(state, samples >= 1, 3, "samples must be >= 1");

  SetLens(aperture, focus, samples);
  return 0;
}

// Total samples per pixel. Every sample is jittered inside the pixel
// footprint, so this controls both edge quality and, later, convergence.
//   gr.set_samples(n)
extern "C" int GrSetSamplesCmd(lua_State* state) {
  GRLUA_DEBUG_CALL;

  int samples = static_cast<int>(luaL_checknumber(state, 1));
  luaL_argcheck(state, samples >= 1, 1, "samples must be >= 1");

  SetSamplesPerPixel(samples);
  return 0;
}

// Deprecated alias for gr.set_samples, kept so older scenes still load.
extern "C" int GrSetAaCmd(lua_State* state) { return GrSetSamplesCmd(state); }

// Environment texture: a lat-long PNG sampled by ray direction, so it
// lights the scene as well as filling the background. Omit it, or pass
// an empty string, and `ambient` becomes a uniform environment instead.
//   gr.set_background('assets/textures/kh_stain_glass.png')
//   gr.set_background('')   -- uniform, for the furnace test
extern "C" int GrSetBackgroundCmd(lua_State* state) {
  GRLUA_DEBUG_CALL;

  SetBackground(luaL_checkstring(state, 1));
  return 0;
}

// Write a progressive snapshot every N samples, named <out>_NNNNspp.png,
// so a convergence series can be produced in a single render.
//   gr.set_snapshot_interval(n);  0 disables
extern "C" int GrSetSnapshotIntervalCmd(lua_State* state) {
  GRLUA_DEBUG_CALL;

  int n = static_cast<int>(luaL_checknumber(state, 1));
  luaL_argcheck(state, n >= 0, 1, "interval must be >= 0");

  SetSnapshotInterval(n);
  return 0;
}

// Reject any table key not in `allowed` (nullptr-terminated), so a typo'd
// field name is a load error instead of a silently-ignored default.
static void CheckKnownFields(lua_State* state, int arg, const char* what,
                             const char* const allowed[]) {
  lua_pushnil(state);
  while (lua_next(state, arg) != 0) {
    const char* k = (lua_type(state, -2) == LUA_TSTRING)
                        ? lua_tostring(state, -2)
                        : nullptr;
    bool known = false;
    for (int i = 0; allowed[i]; i++) {
      if (k && strcmp(k, allowed[i]) == 0) {
        known = true;
        break;
      }
    }
    if (!known)
      luaL_error(state, "%s: unknown field '%s'", what, k ? k : "(non-string)");
    lua_pop(state, 1);  // pop value, keep key
  }
}

// gr.set_tonemap{ operator = 'reinhard', exposure = 1.0,
//                 white_point = 4.0, srgb = true }
// operator: 'none' | 'reinhard' | 'reinhard-extended' | 'aces'. All fields
// optional; srgb = false writes a raw linear dump.
extern "C" int GrSetTonemapCmd(lua_State* state) {
  GRLUA_DEBUG_CALL;
  luaL_checktype(state, 1, LUA_TTABLE);

  tonemap::Config cfg;

  static const char* const kFields[] = {"operator", "exposure", "white_point",
                                        "srgb", nullptr};
  CheckKnownFields(state, 1, "gr.set_tonemap", kFields);

  lua_getfield(state, 1, "operator");
  if (!lua_isnil(state, -1)) {
    const char* op = luaL_checkstring(state, -1);
    if (strcmp(op, "none") == 0)
      cfg.op = tonemap::Operator::kNone;
    else if (strcmp(op, "reinhard") == 0)
      cfg.op = tonemap::Operator::kReinhard;
    else if (strcmp(op, "reinhard-extended") == 0)
      cfg.op = tonemap::Operator::kReinhardExtended;
    else if (strcmp(op, "aces") == 0)
      cfg.op = tonemap::Operator::kAces;
    else
      return luaL_error(state, "gr.set_tonemap: unknown operator '%s'", op);
  }
  lua_pop(state, 1);

  lua_getfield(state, 1, "exposure");
  if (!lua_isnil(state, -1))
    cfg.exposure = static_cast<float>(luaL_checknumber(state, -1));
  lua_pop(state, 1);

  lua_getfield(state, 1, "white_point");
  if (!lua_isnil(state, -1))
    cfg.white_point = static_cast<float>(luaL_checknumber(state, -1));
  lua_pop(state, 1);

  lua_getfield(state, 1, "srgb");
  if (!lua_isnil(state, -1)) cfg.encode_srgb = lua_toboolean(state, -1) != 0;
  lua_pop(state, 1);

  luaL_argcheck(state, cfg.exposure > 0.0f, 1, "exposure must be > 0");
  luaL_argcheck(state, cfg.white_point > 0.0f, 1, "white_point must be > 0");

  SetToneMap(cfg);
  return 0;
}

// Materials
//
// Every constructor returns the same userdata: the scene only ever handles
// a Material*, so set_material does not care which concrete class it was
// given. Adding a material is a constructor plus a row in grlib_functions.
//
//   gr.lambertian{ kd = {0.7, 0.3, 0.3} }
//   gr.blinn_phong{ kd = {...}, ks = {...}, shininess = 25 }
//   gr.material(kd, ks, shininess)        -- deprecated, positional
static int PushMaterial(lua_State* state, Material* material) {
  GrMaterialUd* data =
      static_cast<GrMaterialUd*>(lua_newuserdata(state, sizeof(GrMaterialUd)));
  data->material = material;

  luaL_newmetatable(state, "gr.material");
  lua_setmetatable(state, -2);

  return 1;
}

// Reads a 3-tuple out of field `name`. lua_gettop gives get_tuple an
// absolute index -- it indexes the stack after pushing, so a relative one
// would drift.
static void GetFieldTuple(lua_State* state, int arg, const char* name,
                          double* out) {
  lua_getfield(state, arg, name);
  luaL_argcheck(state, lua_istable(state, -1), arg, name);
  GetTuple(state, lua_gettop(state), out, 3);
  lua_pop(state, 1);
}

// Reflectance must be positive, and no more than all of the light that
// arrived. kd + ks <= 1 is what makes BlinnPhongMaterial energy-conserving
// -- it is the precondition every energy check in tests/bsdf_test.cc runs
// under. It is sufficient, not necessary, so exceeding it is a warning
// rather than an error: the material may still conserve energy, but
// nothing guarantees it any more and bounces can gain light.
static void CheckReflectance(lua_State* state, int arg, const char* what,
                             const double* kd, const double* ks) {
  static const char* k_channel[3] = {"red", "green", "blue"};

  for (int i = 0; i < 3; i++) {
    const double sum = kd[i] + (ks ? ks[i] : 0.0);
    luaL_argcheck(state, kd[i] >= 0.0 && (!ks || ks[i] >= 0.0), arg,
                  "reflectance must be >= 0");
    if (sum > 1.0) {
      LOG_WARN(kLua) << what << ": " << k_channel[i] << " reflectance " << sum
                     << " > 1 -- not energy-conserving";
    }
  }
}

// gr.lambertian{ kd = {0.7, 0.3, 0.3} }
extern "C" int GrLambertianCmd(lua_State* state) {
  GRLUA_DEBUG_CALL;
  luaL_checktype(state, 1, LUA_TTABLE);

  static const char* const kFields[] = {"kd", nullptr};
  CheckKnownFields(state, 1, "gr.lambertian", kFields);

  double kd[3];
  GetFieldTuple(state, 1, "kd", kd);
  CheckReflectance(state, 1, "gr.lambertian", kd, 0);

  return PushMaterial(state,
                      new LambertianMaterial(glm::vec3(kd[0], kd[1], kd[2])));
}

// gr.blinn_phong{ kd = {...}, ks = {...}, shininess = 25 }
extern "C" int GrBlinnPhongCmd(lua_State* state) {
  GRLUA_DEBUG_CALL;
  luaL_checktype(state, 1, LUA_TTABLE);

  static const char* const kFields[] = {"kd", "ks", "shininess", nullptr};
  CheckKnownFields(state, 1, "gr.blinn_phong", kFields);

  double kd[3], ks[3];
  GetFieldTuple(state, 1, "kd", kd);
  GetFieldTuple(state, 1, "ks", ks);
  CheckReflectance(state, 1, "gr.blinn_phong", kd, ks);

  lua_getfield(state, 1, "shininess");
  const double shininess = luaL_checknumber(state, -1);
  lua_pop(state, 1);
  luaL_argcheck(state, shininess >= 0.0, 1, "shininess must be >= 0");

  return PushMaterial(
      state, new BlinnPhongMaterial(glm::vec3(kd[0], kd[1], kd[2]),
                                    glm::vec3(ks[0], ks[1], ks[2]), shininess));
}

// gr.mirror{ albedo = {1.0, 1.0, 1.0} }
//
// A perfect specular reflector -- a delta lobe, so unlike gr.lambertian and
// gr.blinn_phong there is no ks/shininess: every photon leaves in exactly
// one direction. gr.metal is the same lobe with a fuzz radius.
extern "C" int GrMirrorCmd(lua_State* state) {
  GRLUA_DEBUG_CALL;
  luaL_checktype(state, 1, LUA_TTABLE);

  static const char* const kFields[] = {"albedo", nullptr};
  CheckKnownFields(state, 1, "gr.mirror", kFields);

  double albedo[3];
  GetFieldTuple(state, 1, "albedo", albedo);
  CheckReflectance(state, 1, "gr.mirror", albedo, 0);

  return PushMaterial(
      state, new MirrorMaterial(glm::vec3(albedo[0], albedo[1], albedo[2])));
}

// gr.metal{ albedo = {0.8, 0.8, 0.8}, fuzz = 0.3 }
//
// A reflector whose scattered direction is randomly perturbed around the
// mirror direction. fuzz 0 is a sharp reflection; fuzz 1 is the widest
// perturbation allowed, and anything outside [0, 1] is a scene error
// rather than something to clamp silently.
extern "C" int GrMetalCmd(lua_State* state) {
  GRLUA_DEBUG_CALL;
  luaL_checktype(state, 1, LUA_TTABLE);

  static const char* const kFields[] = {"albedo", "fuzz", nullptr};
  CheckKnownFields(state, 1, "gr.metal", kFields);

  double albedo[3];
  GetFieldTuple(state, 1, "albedo", albedo);
  CheckReflectance(state, 1, "gr.metal", albedo, 0);

  lua_getfield(state, 1, "fuzz");
  const double fuzz = luaL_checknumber(state, -1);
  lua_pop(state, 1);
  luaL_argcheck(state, fuzz >= 0.0 && fuzz <= 1.0, 1,
                "gr.metal: fuzz must be in [0, 1]");

  return PushMaterial(
      state, new MetalMaterial(glm::vec3(albedo[0], albedo[1], albedo[2]),
                               static_cast<float>(fuzz)));
}

// gr.dielectric{ ior = 1.5}
//
extern "C" int GrDielectricCmd(lua_State* state) {
  GRLUA_DEBUG_CALL;
  luaL_checktype(state, 1, LUA_TTABLE);

  static const char* const kFields[] = {"ior", nullptr};
  CheckKnownFields(state, 1, "gr.dielectric", kFields);

  lua_getfield(state, 1, "ior");
  const double ior = luaL_checknumber(state, -1);
  lua_pop(state, 1);

  return PushMaterial(state, new DielectricMaterial(static_cast<float>(ior)));
}

// Deprecated positional alias for gr.blinn_phong, kept so older scenes
// still load -- the same shape as gr.set_aa -> gr.set_samples.
extern "C" int GrMaterialCmd(lua_State* state) {
  GRLUA_DEBUG_CALL;

  double kd[3], ks[3];
  GetTuple(state, 1, kd, 3);
  GetTuple(state, 2, ks, 3);
  CheckReflectance(state, 1, "gr.material", kd, ks);

  const double shininess = luaL_checknumber(state, 3);
  luaL_argcheck(state, shininess >= 0.0, 3, "shininess must be >= 0");

  return PushMaterial(
      state, new BlinnPhongMaterial(glm::vec3(kd[0], kd[1], kd[2]),
                                    glm::vec3(ks[0], ks[1], ks[2]), shininess));
}

// Add a Child to a node
extern "C" int GrNodeAddChildCmd(lua_State* state) {
  GRLUA_DEBUG_CALL;

  GrNodeUd* selfdata =
      static_cast<GrNodeUd*>(luaL_checkudata(state, 1, "gr.node"));
  GrNodeUd* childdata =
      static_cast<GrNodeUd*>(luaL_checkudata(state, 2, "gr.node"));

  selfdata->node->AddChild(childdata->node);

  return 0;
}

// Set a node's Material
extern "C" int GrNodeSetMaterialCmd(lua_State* state) {
  GRLUA_DEBUG_CALL;

  GrNodeUd* selfdata =
      static_cast<GrNodeUd*>(luaL_checkudata(state, 1, "gr.node"));

  // Every node shares the gr.node type, so "is this one geometry?" is a
  // question the metatable cannot answer and this cast has to.
  GeometryNode* self = dynamic_cast<GeometryNode*>(selfdata->node);
  luaL_argcheck(state, self != 0, 1, "Geometry node expected");

  GrMaterialUd* matdata =
      static_cast<GrMaterialUd*>(luaL_checkudata(state, 2, "gr.material"));

  self->SetMaterial(matdata->material);

  return 0;
}

// node:Scale(x, y, z)
extern "C" int GrNodeScaleCmd(lua_State* state) {
  GRLUA_DEBUG_CALL;

  GrNodeUd* selfdata =
      static_cast<GrNodeUd*>(luaL_checkudata(state, 1, "gr.node"));

  selfdata->node->Scale(GetVec3Args(state, 2));

  return 0;
}

// node:Translate(x, y, z)
extern "C" int GrNodeTranslateCmd(lua_State* state) {
  GRLUA_DEBUG_CALL;

  GrNodeUd* selfdata =
      static_cast<GrNodeUd*>(luaL_checkudata(state, 1, "gr.node"));

  selfdata->node->Translate(GetVec3Args(state, 2));

  return 0;
}

// node:Rotate('x'|'y'|'z', degrees)
extern "C" int GrNodeRotateCmd(lua_State* state) {
  GRLUA_DEBUG_CALL;

  GrNodeUd* selfdata =
      static_cast<GrNodeUd*>(luaL_checkudata(state, 1, "gr.node"));

  const char* axis_string = luaL_checkstring(state, 2);
  luaL_argcheck(state, std::strlen(axis_string) == 1, 2,
                "Single character expected");

  char axis = std::tolower(axis_string[0]);
  luaL_argcheck(state, axis >= 'x' && axis <= 'z', 2, "Axis must be x, y or z");

  double angle = luaL_checknumber(state, 3);

  selfdata->node->Rotate(axis, static_cast<float>(angle));

  return 0;
}

// __gc for gr.node. Clears the pointer without deleting the node: the scene
// has to outlive the interpreter, so C++ keeps ownership and Lua only drops
// its handle.
extern "C" int GrNodeGcCmd(lua_State* state) {
  GRLUA_DEBUG_CALL;

  GrNodeUd* data = static_cast<GrNodeUd*>(luaL_checkudata(state, 1, "gr.node"));

  data->node = 0;

  return 0;
}

// The gr table: everything a scene calls as gr.<name>.
static const luaL_Reg kGrlibFunctions[] = {
    {"node", GrNodeCmd},
    {"sphere", GrSphereCmd},
    {"joint", GrJointCmd},
    {"material", GrMaterialCmd},  // deprecated alias for blinn_phong
    {"lambertian", GrLambertianCmd},
    {"blinn_phong", GrBlinnPhongCmd},
    {"mirror", GrMirrorCmd},
    {"metal", GrMetalCmd},
    {"dielectric", GrDielectricCmd},
    {"cube", GrCubeCmd},
    {"nh_sphere", GrNhSphereCmd},
    {"nh_box", GrNhBoxCmd},
    {"mesh", GrMeshCmd},
    {"light", GrLightCmd},
    // The raw positional binding. gr.render itself is defined by the Lua
    // shim below, which accepts a named-parameter table and forwards here.
    {"_render", GrRenderCmd},
    {"set_lens", GrSetLensCmd},
    {"set_samples", GrSetSamplesCmd},
    {"set_snapshot_interval", GrSetSnapshotIntervalCmd},
    {"set_background", GrSetBackgroundCmd},
    {"set_tonemap", GrSetTonemapCmd},
    {"set_aa", GrSetAaCmd},  // deprecated alias
    {0, 0}};

// Methods on a gr.node userdata, reached as node:<name>(). Materials and
// lights are inert handles and need none.
//
// Every node type shares one Lua type rather than mirroring the C++
// hierarchy, so a method that needs a specific subclass downcasts and
// reports the mismatch itself.
static const luaL_Reg kGrlibNodeMethods[] = {
    {"__gc", GrNodeGcCmd},
    {"add_child", GrNodeAddChildCmd},
    {"set_material", GrNodeSetMaterialCmd},
    {"scale", GrNodeScaleCmd},
    {"rotate", GrNodeRotateCmd},
    {"translate", GrNodeTranslateCmd},
    {"render", GrRenderCmd},
    {0, 0}};

// Lua-side prelude, run after the gr table is registered and before the
// scene file is loaded.
//
// gr.render originally took ten positional arguments, which meant every call
// site was a row of unlabelled numbers and tuples -- impossible to read and
// easy to transpose. This wraps it so a scene can pass a named table instead.
// The positional form still works, so scenes convert one at a time rather
// than all at once.
//
// Embedded as a string rather than shipped as a .lua file so there is no
// extra path to resolve at runtime.
static const char* gr_prelude = R"PRELUDE(
local _render = gr._render

local known = {
  root = true, output = true, width = true, height = true,
  eye = true, view = true, up = true, fov = true,
  ambient = true, lights = true,
}

local order = {
  "root", "output", "width", "height",
  "eye", "view", "up", "fov", "ambient", "lights",
}

function gr.render(a, ...)
  -- The positional form always starts with a gr.node userdata, so a table
  -- here unambiguously means the named form.
  if type(a) ~= "table" then
    return _render(a, ...)
  end

  if select("#", ...) > 0 then
    error("gr.render: pass a single table of named fields, or the positional "
          .. "arguments -- not both", 2)
  end

  -- Catch typos loudly. Without this, `outut = ...` would silently surface
  -- as "missing output" and send you looking in the wrong place.
  for k in pairs(a) do
    if not known[k] then
      error("gr.render: unknown field '" .. tostring(k) .. "'", 2)
    end
  end

  local args = {}
  for i, name in ipairs(order) do
    local v = a[name]
    if v == nil then
      error("gr.render: missing '" .. name .. "'", 2)
    end
    args[i] = v
  end

  return _render(table.unpack(args))
end
)PRELUDE";

// Stand up an interpreter, register gr, run the scene file. Rendering happens
// as a side effect of the scene calling gr.render.
bool RunLua(const std::string& filename) {
  GRLUA_DEBUG("Importing scene from " << filename);

  // Start a lua interpreter
  lua_State* state = luaL_newstate();

  GRLUA_DEBUG("Loading base libraries");

  // Load some base library
  luaL_openlibs(state);

  GRLUA_DEBUG("Setting up our functions");

  // Set up the metatable for gr.node
  luaL_newmetatable(state, "gr.node");
  lua_pushstring(state, "__index");
  lua_pushvalue(state, -2);
  lua_settable(state, -3);

  // Load the gr.node methods
  luaL_setfuncs(state, kGrlibNodeMethods, 0);

  // Load the gr functions
  luaL_setfuncs(state, kGrlibFunctions, 0);
  lua_setglobal(state, "gr");

  GRLUA_DEBUG("Installing the Lua prelude");
  if (luaL_dostring(state, gr_prelude)) {
    LOG_ERROR(kLua) << "gr prelude: " << lua_tostring(state, -1);
    lua_close(state);
    return false;
  }

  GRLUA_DEBUG("Parsing the scene...");
  // Now parse the actual scene
  if (luaL_loadfile(state, filename.c_str()) || lua_pcall(state, 0, 0, 0)) {
    LOG_ERROR(kLua) << filename << ": " << lua_tostring(state, -1);
    return false;
  }
  GRLUA_DEBUG("Closing the interpreter");

  // Close the interpreter, free up any resources not needed
  lua_close(state);

  return true;
}
