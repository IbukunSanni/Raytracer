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

#include "lua/scene_lua.hpp"

#include <iostream>
#include <cctype>
#include <cstring>
#include <cstdio>
#include <vector>
#include <map>

#include "lua/lua488.hpp"
#include "core/Log.hpp"

#include "scene/Light.hpp"
#include "geometry/Mesh.hpp"
#include "scene/GeometryNode.hpp"
#include "scene/JointNode.hpp"
#include "geometry/Primitive.hpp"
#include "scene/Material.hpp"
#include "render/Renderer.hpp"
#include "core/ToneMap.hpp"

typedef std::map<std::string,Mesh*> MeshMap;
static MeshMap mesh_map;

// Uncomment the following line to enable debugging messages
// #define GRLUA_ENABLE_DEBUG

#ifdef GRLUA_ENABLE_DEBUG
#  define GRLUA_DEBUG(x) do { LOG_TRACE(LUA) << x; } while (0)
#  define GRLUA_DEBUG_CALL do { LOG_TRACE(LUA) << __FUNCTION__; } while (0)
#else
#  define GRLUA_DEBUG(x) do { } while (0)
#  define GRLUA_DEBUG_CALL do { } while (0)
#endif

// Lua allocates the userdata; C++ owns whatever it points at. Lua runs no
// constructors or destructors, and the scene must outlive the interpreter so
// it can still be rendered after parsing -- so each userdata holds nothing but
// a pointer, and closing Lua loses only that pointer.
struct gr_node_ud {
  SceneNode* node;
};

struct gr_material_ud {
  Material* material;
};

struct gr_light_ud {
  Light* light;
};

// Useful function to retrieve and check an n-tuple of numbers.
template<typename T>
void get_tuple(lua_State* L, int arg, T* data, int n)
{
  luaL_checktype(L, arg, LUA_TTABLE);
  luaL_argcheck(L, lua_rawlen(L, arg) == (size_t) n, arg, "N-tuple expected");
  for (int i = 1; i <= n; i++) {
    lua_rawgeti(L, arg, i);
    data[i - 1] = (T) luaL_checknumber(L, -1);
    lua_pop(L, 1);
  }
}

// Read three consecutive numeric arguments as a vector. Kept separate from
// get_tuple because these arrive as loose arguments, not inside a table.
static glm::vec3 get_vec3_args(lua_State* L, int arg)
{
  const double x = luaL_checknumber(L, arg);
  const double y = luaL_checknumber(L, arg + 1);
  const double z = luaL_checknumber(L, arg + 2);

  return glm::vec3(x, y, z);
}

// Wrap a node in the userdata Lua holds and stamp it with the gr.node
// metatable, which is what luaL_checkudata later matches against. Shared tail
// of every node constructor, the way push_material is for materials.
static int push_node(lua_State* L, SceneNode* node)
{
  gr_node_ud* data = (gr_node_ud*)lua_newuserdata(L, sizeof(gr_node_ud));
  data->node = node;

  luaL_getmetatable(L, "gr.node");
  lua_setmetatable(L, -2);

  return 1;
}

// gr.node(name) -- a bare transform node, the scene graph's interior
extern "C"
int gr_node_cmd(lua_State* L)
{
  GRLUA_DEBUG_CALL;

  const char* name = luaL_checkstring(L, 1);

  return push_node(L, new SceneNode(name));
}

// gr.joint(name, {min, init, max}, {min, init, max})
extern "C"
int gr_joint_cmd(lua_State* L)
{
  GRLUA_DEBUG_CALL;

  const char* name = luaL_checkstring(L, 1);

  double x[3], y[3];
  get_tuple(L, 2, x, 3);
  get_tuple(L, 3, y, 3);

  JointNode* node = new JointNode(name);
  node->set_joint_x(x[0], x[1], x[2]);
  node->set_joint_y(y[0], y[1], y[2]);

  return push_node(L, node);
}

// gr.sphere(name) -- unit sphere at the origin, placed by node transforms
extern "C"
int gr_sphere_cmd(lua_State* L)
{
  GRLUA_DEBUG_CALL;

  const char* name = luaL_checkstring(L, 1);

  return push_node(L, new GeometryNode(name, new Sphere()));
}

// gr.cube(name) -- unit cube at the origin, placed by node transforms
extern "C"
int gr_cube_cmd(lua_State* L)
{
  GRLUA_DEBUG_CALL;

  const char* name = luaL_checkstring(L, 1);

  return push_node(L, new GeometryNode(name, new Cube()));
}

// gr.nh_sphere(name, {x, y, z}, radius) -- position baked into the primitive
extern "C"
int gr_nh_sphere_cmd(lua_State* L)
{
  GRLUA_DEBUG_CALL;

  const char* name = luaL_checkstring(L, 1);

  glm::vec3 pos;
  get_tuple(L, 2, &pos[0], 3);

  double radius = luaL_checknumber(L, 3);

  return push_node(L, new GeometryNode(name, new NonhierSphere(pos, radius)));
}

// gr.nh_box(name, {x, y, z}, size) -- position baked into the primitive
extern "C"
int gr_nh_box_cmd(lua_State* L)
{
  GRLUA_DEBUG_CALL;

  const char* name = luaL_checkstring(L, 1);

  glm::vec3 pos;
  get_tuple(L, 2, &pos[0], 3);

  double size = luaL_checknumber(L, 3);

  return push_node(L, new GeometryNode(name, new NonhierBox(pos, size)));
}

// gr.mesh(name, 'path/to/model.obj')
extern "C"
int gr_mesh_cmd(lua_State* L)
{
  GRLUA_DEBUG_CALL;

  const char* name = luaL_checkstring(L, 1);
  const char* obj_fname = luaL_checkstring(L, 2);

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

  return push_node(L, new GeometryNode(name, mesh));
}

// Make a Point light
extern "C"
int gr_light_cmd(lua_State* L)
{
  GRLUA_DEBUG_CALL;

  gr_light_ud* data = (gr_light_ud*)lua_newuserdata(L, sizeof(gr_light_ud));
  data->light = 0;

  
  Light l;

  double col[3];
  get_tuple(L, 1, &l.position[0], 3);
  get_tuple(L, 2, col, 3);
  get_tuple(L, 3, l.falloff, 3);

  l.colour = glm::vec3(col[0], col[1], col[2]);
  
  data->light = new Light(l);

  luaL_newmetatable(L, "gr.light");
  lua_setmetatable(L, -2);

  return 1;
}
// TODO: can take anempty light or no light in the scene instead of having to pass an empty one.
// Render a scene
extern "C"
int gr_render_cmd(lua_State* L)
{
  GRLUA_DEBUG_CALL;
  
  gr_node_ud* root = (gr_node_ud*)luaL_checkudata(L, 1, "gr.node");

  const char* filename = luaL_checkstring(L, 2);

  int width  = (int) luaL_checknumber(L, 3);
  int height = (int) luaL_checknumber(L, 4);

  glm::vec3 eye;
  glm::vec3 view, up;
  
  get_tuple(L, 5, &eye[0], 3);
  get_tuple(L, 6, &view[0], 3);
  get_tuple(L, 7, &up[0], 3);

  double fov = luaL_checknumber(L, 8);

  double ambient_data[3];
  get_tuple(L, 9, ambient_data, 3);
  glm::vec3 ambient(ambient_data[0], ambient_data[1], ambient_data[2]);

  luaL_checktype(L, 10, LUA_TTABLE);
  int light_count = int(lua_rawlen(L, 10));
  
  luaL_argcheck(L, light_count >= 1, 10, "Tuple of lights expected");
  std::list<Light*> lights;
  for (int i = 1; i <= light_count; i++) {
    lua_rawgeti(L, 10, i);
    gr_light_ud* ldata = (gr_light_ud*)luaL_checkudata(L, -1, "gr.light");

    lights.push_back(ldata->light);
    lua_pop(L, 1);
  }

  Image im(width, height);
  SetOutputPath(filename);
  Render(root->node, im, eye, view, up, fov, ambient, lights);
  if (!im.savePng(filename, GetToneMap())) {
    return luaL_error(L, "gr.render: could not write output '%s' "
                         "(see log for details)", filename);
  }

  return 0;
}

// Configure the thin-lens camera (depth of field).
//   gr.set_lens(aperture_radius, focus_distance, samples)
// aperture_radius 0 restores the pinhole camera.
extern "C"
int gr_set_lens_cmd(lua_State* L)
{
  GRLUA_DEBUG_CALL;

  float aperture = (float)luaL_checknumber(L, 1);
  float focus    = (float)luaL_checknumber(L, 2);
  int   samples  = (int)luaL_optnumber(L, 3, 16);

  luaL_argcheck(L, aperture >= 0.0f, 1, "aperture radius must be >= 0");
  luaL_argcheck(L, focus > 0.0f, 2, "focus distance must be > 0");
  luaL_argcheck(L, samples >= 1, 3, "samples must be >= 1");

  SetLens(aperture, focus, samples);
  return 0;
}

// Total samples per pixel. Every sample is jittered inside the pixel
// footprint, so this controls both edge quality and, later, convergence.
//   gr.set_samples(n)
extern "C"
int gr_set_samples_cmd(lua_State* L)
{
  GRLUA_DEBUG_CALL;

  int samples = (int)luaL_checknumber(L, 1);
  luaL_argcheck(L, samples >= 1, 1, "samples must be >= 1");

  SetSamplesPerPixel(samples);
  return 0;
}

// Deprecated alias for gr.set_samples, kept so older scenes still load.
extern "C"
int gr_set_aa_cmd(lua_State* L)
{
  return gr_set_samples_cmd(L);
}

// Environment texture: a lat-long PNG sampled by ray direction, so it
// lights the scene as well as filling the background. Omit it, or pass
// an empty string, and `ambient` becomes a uniform environment instead.
//   gr.set_background('assets/textures/kh_stain_glass.png')
//   gr.set_background('')   -- uniform, for the furnace test
extern "C"
int gr_set_background_cmd(lua_State* L)
{
  GRLUA_DEBUG_CALL;

  SetBackground(luaL_checkstring(L, 1));
  return 0;
}

// Write a progressive snapshot every N samples, named <out>_NNNNspp.png,
// so a convergence series can be produced in a single render.
//   gr.set_snapshot_interval(n);  0 disables
extern "C"
int gr_set_snapshot_interval_cmd(lua_State* L)
{
  GRLUA_DEBUG_CALL;

  int n = (int)luaL_checknumber(L, 1);
  luaL_argcheck(L, n >= 0, 1, "interval must be >= 0");

  SetSnapshotInterval(n);
  return 0;
}

// gr.set_tonemap{ operator = 'reinhard', exposure = 1.0,
//                 white_point = 4.0, srgb = true }
// operator: 'none' | 'reinhard' | 'reinhard-extended' | 'aces'. All fields
// optional; srgb = false writes a raw linear dump.
extern "C"
int gr_set_tonemap_cmd(lua_State* L)
{
  GRLUA_DEBUG_CALL;
  luaL_checktype(L, 1, LUA_TTABLE);

  tonemap::Config cfg;

  // Reject unknown keys so a typo is loud.
  lua_pushnil(L);
  while (lua_next(L, 1) != 0) {
    const char* k = (lua_type(L, -2) == LUA_TSTRING) ? lua_tostring(L, -2) : nullptr;
    if (!k || (strcmp(k, "operator") != 0 && strcmp(k, "exposure") != 0 &&
               strcmp(k, "white_point") != 0 && strcmp(k, "srgb") != 0)) {
      return luaL_error(L, "gr.set_tonemap: unknown field '%s'", k ? k : "(non-string)");
    }
    lua_pop(L, 1);   // pop value, keep key
  }

  lua_getfield(L, 1, "operator");
  if (!lua_isnil(L, -1)) {
    const char* op = luaL_checkstring(L, -1);
    if      (strcmp(op, "none") == 0)              cfg.op = tonemap::Operator::None;
    else if (strcmp(op, "reinhard") == 0)          cfg.op = tonemap::Operator::Reinhard;
    else if (strcmp(op, "reinhard-extended") == 0) cfg.op = tonemap::Operator::ReinhardExtended;
    else if (strcmp(op, "aces") == 0)              cfg.op = tonemap::Operator::ACES;
    else return luaL_error(L, "gr.set_tonemap: unknown operator '%s'", op);
  }
  lua_pop(L, 1);

  lua_getfield(L, 1, "exposure");
  if (!lua_isnil(L, -1)) cfg.exposure = (float) luaL_checknumber(L, -1);
  lua_pop(L, 1);

  lua_getfield(L, 1, "white_point");
  if (!lua_isnil(L, -1)) cfg.whitePoint = (float) luaL_checknumber(L, -1);
  lua_pop(L, 1);

  lua_getfield(L, 1, "srgb");
  if (!lua_isnil(L, -1)) cfg.encodeSRGB = lua_toboolean(L, -1) != 0;
  lua_pop(L, 1);

  luaL_argcheck(L, cfg.exposure > 0.0f, 1, "exposure must be > 0");
  luaL_argcheck(L, cfg.whitePoint > 0.0f, 1, "white_point must be > 0");

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
static int push_material(lua_State* L, Material* material)
{
  gr_material_ud* data = (gr_material_ud*)lua_newuserdata(L, sizeof(gr_material_ud));
  data->material = material;

  luaL_newmetatable(L, "gr.material");
  lua_setmetatable(L, -2);

  return 1;
}

// Reads a 3-tuple out of field `name`. lua_gettop gives get_tuple an
// absolute index -- it indexes the stack after pushing, so a relative one
// would drift.
static void get_field_tuple(lua_State* L, int arg, const char* name, double* out)
{
  lua_getfield(L, arg, name);
  luaL_argcheck(L, lua_istable(L, -1), arg, name);
  get_tuple(L, lua_gettop(L), out, 3);
  lua_pop(L, 1);
}

// Reflectance must be positive, and no more than all of the light that
// arrived. kd + ks <= 1 is what makes BlinnPhongMaterial energy-conserving
// -- it is the precondition every energy check in tests/furnace.cpp runs
// under. It is sufficient, not necessary, so exceeding it is a warning
// rather than an error: the material may still conserve energy, but
// nothing guarantees it any more and bounces can gain light.
static void check_reflectance(lua_State* L, int arg, const char* what,
                              const double* kd, const double* ks)
{
  static const char* kChannel[3] = {"red", "green", "blue"};

  for (int i = 0; i < 3; i++) {
    const double sum = kd[i] + (ks ? ks[i] : 0.0);
    luaL_argcheck(L, kd[i] >= 0.0 && (!ks || ks[i] >= 0.0), arg,
                  "reflectance must be >= 0");
    if (sum > 1.0) {
      LOG_WARN(LUA) << what << ": " << kChannel[i] << " reflectance "
                    << sum << " > 1 -- not energy-conserving";
    }
  }
}

// gr.lambertian{ kd = {0.7, 0.3, 0.3} }
extern "C"
int gr_lambertian_cmd(lua_State* L)
{
  GRLUA_DEBUG_CALL;
  luaL_checktype(L, 1, LUA_TTABLE);

  double kd[3];
  get_field_tuple(L, 1, "kd", kd);
  check_reflectance(L, 1, "gr.lambertian", kd, 0);

  return push_material(L, new LambertianMaterial(glm::vec3(kd[0], kd[1], kd[2])));
}

// gr.blinn_phong{ kd = {...}, ks = {...}, shininess = 25 }
extern "C"
int gr_blinn_phong_cmd(lua_State* L)
{
  GRLUA_DEBUG_CALL;
  luaL_checktype(L, 1, LUA_TTABLE);

  double kd[3], ks[3];
  get_field_tuple(L, 1, "kd", kd);
  get_field_tuple(L, 1, "ks", ks);
  check_reflectance(L, 1, "gr.blinn_phong", kd, ks);

  lua_getfield(L, 1, "shininess");
  const double shininess = luaL_checknumber(L, -1);
  lua_pop(L, 1);
  luaL_argcheck(L, shininess >= 0.0, 1, "shininess must be >= 0");

  return push_material(L, new BlinnPhongMaterial(glm::vec3(kd[0], kd[1], kd[2]),
                                                 glm::vec3(ks[0], ks[1], ks[2]),
                                                 shininess));
}

// gr.mirror{ albedo = {1.0, 1.0, 1.0} }
//
// A perfect specular reflector -- a delta lobe, so unlike gr.lambertian and
// gr.blinn_phong there is no ks/shininess: every photon leaves in exactly
// one direction. gr.metal is the same lobe with a fuzz radius.
extern "C"
int gr_mirror_cmd(lua_State* L)
{
  GRLUA_DEBUG_CALL;
  luaL_checktype(L, 1, LUA_TTABLE);

  double albedo[3];
  get_field_tuple(L, 1, "albedo", albedo);
  check_reflectance(L, 1, "gr.mirror", albedo, 0);

  return push_material(L, new MirrorMaterial(glm::vec3(albedo[0], albedo[1], albedo[2])));
}

// gr.metal{ albedo = {0.8, 0.8, 0.8}, fuzz = 0.3 }
//
// A reflector whose scattered direction is randomly perturbed around the
// mirror direction. fuzz 0 is a sharp reflection; fuzz 1 is the widest
// perturbation allowed, and anything outside [0, 1] is a scene error
// rather than something to clamp silently.
extern "C"
int gr_metal_cmd(lua_State* L)
{
  GRLUA_DEBUG_CALL;
  luaL_checktype(L, 1, LUA_TTABLE);

  double albedo[3];
  get_field_tuple(L, 1, "albedo", albedo);
  check_reflectance(L, 1, "gr.metal", albedo, 0);

  lua_getfield(L, 1, "fuzz");
  const double fuzz = luaL_checknumber(L, -1);
  lua_pop(L, 1);
  luaL_argcheck(L, fuzz >= 0.0 && fuzz <= 1.0, 1,
                "gr.metal: fuzz must be in [0, 1]");

  return push_material(L, new MetalMaterial(glm::vec3(albedo[0], albedo[1], albedo[2]),
                                            (float) fuzz));
}

// Deprecated positional alias for gr.blinn_phong, kept so older scenes
// still load -- the same shape as gr.set_aa -> gr.set_samples.
extern "C"
int gr_material_cmd(lua_State* L)
{
  GRLUA_DEBUG_CALL;

  double kd[3], ks[3];
  get_tuple(L, 1, kd, 3);
  get_tuple(L, 2, ks, 3);
  check_reflectance(L, 1, "gr.material", kd, ks);

  const double shininess = luaL_checknumber(L, 3);
  luaL_argcheck(L, shininess >= 0.0, 3, "shininess must be >= 0");

  return push_material(L, new BlinnPhongMaterial(glm::vec3(kd[0], kd[1], kd[2]),
                                                 glm::vec3(ks[0], ks[1], ks[2]),
                                                 shininess));
}

// Add a Child to a node
extern "C"
int gr_node_add_child_cmd(lua_State* L)
{
  GRLUA_DEBUG_CALL;
  
  gr_node_ud* selfdata = (gr_node_ud*)luaL_checkudata(L, 1, "gr.node");
  gr_node_ud* childdata = (gr_node_ud*)luaL_checkudata(L, 2, "gr.node");

  selfdata->node->add_child(childdata->node);

  return 0;
}

// Set a node's Material
extern "C"
int gr_node_set_material_cmd(lua_State* L)
{
  GRLUA_DEBUG_CALL;
  
  gr_node_ud* selfdata = (gr_node_ud*)luaL_checkudata(L, 1, "gr.node");

  // Every node shares the gr.node type, so "is this one geometry?" is a
  // question the metatable cannot answer and this cast has to.
  GeometryNode* self = dynamic_cast<GeometryNode*>(selfdata->node);
  luaL_argcheck(L, self != 0, 1, "Geometry node expected");

  gr_material_ud* matdata = (gr_material_ud*)luaL_checkudata(L, 2, "gr.material");

  self->setMaterial(matdata->material);

  return 0;
}

// node:scale(x, y, z)
extern "C"
int gr_node_scale_cmd(lua_State* L)
{
  GRLUA_DEBUG_CALL;

  gr_node_ud* selfdata = (gr_node_ud*)luaL_checkudata(L, 1, "gr.node");

  selfdata->node->scale(get_vec3_args(L, 2));

  return 0;
}

// node:translate(x, y, z)
extern "C"
int gr_node_translate_cmd(lua_State* L)
{
  GRLUA_DEBUG_CALL;

  gr_node_ud* selfdata = (gr_node_ud*)luaL_checkudata(L, 1, "gr.node");

  selfdata->node->translate(get_vec3_args(L, 2));

  return 0;
}

// node:rotate('x'|'y'|'z', degrees)
extern "C"
int gr_node_rotate_cmd(lua_State* L)
{
  GRLUA_DEBUG_CALL;

  gr_node_ud* selfdata = (gr_node_ud*)luaL_checkudata(L, 1, "gr.node");

  const char* axis_string = luaL_checkstring(L, 2);
  luaL_argcheck(L, std::strlen(axis_string) == 1, 2, "Single character expected");

  char axis = std::tolower(axis_string[0]);
  luaL_argcheck(L, axis >= 'x' && axis <= 'z', 2, "Axis must be x, y or z");

  double angle = luaL_checknumber(L, 3);

  selfdata->node->rotate(axis, (float) angle);

  return 0;
}

// __gc for gr.node. Clears the pointer without deleting the node: the scene
// has to outlive the interpreter, so C++ keeps ownership and Lua only drops
// its handle.
extern "C"
int gr_node_gc_cmd(lua_State* L)
{
  GRLUA_DEBUG_CALL;

  gr_node_ud* data = (gr_node_ud*)luaL_checkudata(L, 1, "gr.node");

  data->node = 0;

  return 0;
}

// The gr table: everything a scene calls as gr.<name>.
static const luaL_Reg grlib_functions[] = {
  {"node", gr_node_cmd},
  {"sphere", gr_sphere_cmd},
  {"joint", gr_joint_cmd},
  {"material", gr_material_cmd},   // deprecated alias for blinn_phong
  {"lambertian", gr_lambertian_cmd},
  {"blinn_phong", gr_blinn_phong_cmd},
  {"mirror", gr_mirror_cmd},
  {"metal", gr_metal_cmd},
  {"cube", gr_cube_cmd},
  {"nh_sphere", gr_nh_sphere_cmd},
  {"nh_box", gr_nh_box_cmd},
  {"mesh", gr_mesh_cmd},
  {"light", gr_light_cmd},
  // The raw positional binding. gr.render itself is defined by the Lua
  // shim below, which accepts a named-parameter table and forwards here.
  {"_render", gr_render_cmd},
  {"set_lens", gr_set_lens_cmd},
  {"set_samples", gr_set_samples_cmd},
  {"set_snapshot_interval", gr_set_snapshot_interval_cmd},
  {"set_background", gr_set_background_cmd},
  {"set_tonemap", gr_set_tonemap_cmd},
  {"set_aa", gr_set_aa_cmd},   // deprecated alias
  {0, 0}
};

// Methods on a gr.node userdata, reached as node:<name>(). Materials and
// lights are inert handles and need none.
//
// Every node type shares one Lua type rather than mirroring the C++
// hierarchy, so a method that needs a specific subclass downcasts and
// reports the mismatch itself.
static const luaL_Reg grlib_node_methods[] = {
  {"__gc", gr_node_gc_cmd},
  {"add_child", gr_node_add_child_cmd},
  {"set_material", gr_node_set_material_cmd},
  {"scale", gr_node_scale_cmd},
  {"rotate", gr_node_rotate_cmd},
  {"translate", gr_node_translate_cmd},
  {"render", gr_render_cmd},
  {0, 0}
};

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
static const char * GR_PRELUDE = R"PRELUDE(
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
bool run_lua(const std::string& filename)
{
  GRLUA_DEBUG("Importing scene from " << filename);
  
  // Start a lua interpreter
  lua_State* L = luaL_newstate();

  GRLUA_DEBUG("Loading base libraries");
  
  // Load some base library
  luaL_openlibs(L);

  GRLUA_DEBUG("Setting up our functions");

  // Set up the metatable for gr.node
  luaL_newmetatable(L, "gr.node");
  lua_pushstring(L, "__index");
  lua_pushvalue(L, -2);
  lua_settable(L, -3);

  // Load the gr.node methods
  luaL_setfuncs( L, grlib_node_methods, 0 );

  // Load the gr functions
  luaL_setfuncs(L, grlib_functions, 0);
  lua_setglobal(L, "gr");

  GRLUA_DEBUG("Installing the Lua prelude");
  if (luaL_dostring(L, GR_PRELUDE)) {
    LOG_ERROR(LUA) << "gr prelude: " << lua_tostring(L, -1);
    lua_close(L);
    return false;
  }

  GRLUA_DEBUG("Parsing the scene...");
  // Now parse the actual scene
  if (luaL_loadfile(L, filename.c_str()) || lua_pcall(L, 0, 0, 0)) {
    LOG_ERROR(LUA) << filename << ": " << lua_tostring(L, -1);
    return false;
  }
  GRLUA_DEBUG("Closing the interpreter");
  
  // Close the interpreter, free up any resources not needed
  lua_close(L);

  return true;
}
