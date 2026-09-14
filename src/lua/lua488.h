// The vendored Lua 5.3.1 headers, wrapped in extern "C" once so no caller
// has to remember to. Lua is C, and scene_lua.cc is the only thing that
// needs it.

#ifndef RAYTRACER_SRC_LUA_LUA488_H_
#define RAYTRACER_SRC_LUA_LUA488_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <lua-5.3.1/src/lauxlib.h>
#include <lua-5.3.1/src/lua.h>
#include <lua-5.3.1/src/lualib.h>

#ifdef __cplusplus
}
#endif

#endif  // RAYTRACER_SRC_LUA_LUA488_H_
