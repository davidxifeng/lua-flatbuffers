#include <string.h>

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

static int buf_sub (lua_State *L) {
  return 0;
}

static const luaL_Reg buffer_lib[] = {
  {"sub", buf_sub},
  {NULL, NULL}
};

LUA_API int luaopen_buffer (lua_State *L) {
  luaL_newlib(L, buffer_lib);
  return 1;
}

