/* Licensed under the terms of the MIT License; see full copyright information
 * in the "LICENSE" file or at <http://www.lua.org/license.html> */

#define linit_c
#define LUA_LIB

#include "lua.h"

#include "lauxlib.h"
#include "lualib.h"

static const luaL_Reg lualibs[] = {
    { LUA_BASELIBNAME, luaopen_base },
    { LUA_BITLIBNAME, luaopen_bit },
    { LUA_DBLIBNAME, luaopen_debug },
    { LUA_DBLIBNAME ".security", luaopen_security },
    { LUA_DBLIBNAME ".stats", luaopen_stats },
    { LUA_IOLIBNAME, luaopen_io },
    { LUA_LOADLIBNAME, luaopen_package },
    { LUA_MATHLIBNAME, luaopen_math },
    { LUA_OSLIBNAME, luaopen_os },
    { LUA_STRLIBNAME, luaopen_string },
    { LUA_TABLIBNAME, luaopen_table },
    { LUA_COMPATLIBNAME, luaopen_compat },
    /* clang-format off */
    { NULL, NULL },
    /* clang-format on */
};

static const luaL_Reg reflibs[] = {
    { LUA_BASELIBNAME, luaopen_wow_base },
    { LUA_BITLIBNAME, luaopen_wow_bit },
    { LUA_DBLIBNAME, luaopen_wow_debug },
    { LUA_COLIBNAME, luaopen_wow_coroutine },
    { LUA_MATHLIBNAME, luaopen_wow_math },
    { LUA_OSLIBNAME, luaopen_wow_os },
    { LUA_STRLIBNAME, luaopen_wow_string },
    { LUA_TABLIBNAME, luaopen_wow_table },
    { LUA_COMPATLIBNAME, luaopen_compat },
    /* clang-format off */
    { NULL, NULL },
    /* clang-format on */
};

static void openlibs (lua_State *L, const luaL_Reg *lib) {
    for (; lib->func; lib++) {
        lua_pushcclosure(L, lib->func, 0);
        lua_pushstring(L, lib->name);
        lua_call(L, 1, 0);
    }
}

LUALIB_API void luaL_openlibs (lua_State *L) {
    luaL_openlibsx(L, LUALIB_STANDARD);
}

LUALIB_API void luaL_openlibsx (lua_State *L, int type) {
    switch (type) {
        case LUALIB_STANDARD:
            openlibs(L, lualibs);
            break;
        case LUALIB_REFERENCE:
            openlibs(L, reflibs);
            break;
    }
}

LUALIB_API int luaopen_wow (lua_State *L) {
    lua_createtable(L, 0, 2);
    lua_pushvalue(L, -1);
    lua_replace(L, LUA_ENVIRONINDEX);
    luaL_openlibsx(L, LUALIB_REFERENCE);
    return 1;
}
