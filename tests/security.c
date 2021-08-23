#include "common.h"

#include <acutest.h>

/**
 * Basic State Taint Tests
 */

static void test_startup_taint(void) {
  TEST_CHECK(lua_getthreadtaint(LT) == NULL);
  TEST_CHECK(luaL_issecurethread(LT));
}

static void test_tainted(void) {
  lua_setthreadtaint(LT, &luaT_taint);
  TEST_CHECK(lua_getthreadtaint(LT) == &luaT_taint);
  TEST_CHECK(!luaL_issecurethread(LT));
}

static void test_forced_taint(void) {
  luaL_forcetaintthread(LT);
  TEST_CHECK(lua_getthreadtaint(LT) != NULL);
}

static void test_forced_taint_override(void) {
  lua_setthreadtaint(LT, &luaT_taint);
  luaL_forcetaintthread(LT);
  TEST_CHECK(lua_getthreadtaint(LT) == &luaT_taint);
}

/**
 * State => Value and Object Taint Propagation Tests
 */

static void test_push_values_with_taint(void) {
  for (struct LuaValueVector *vec = luaT_value_vectors; vec->name; ++vec) {
    TEST_CASE(vec->name);
    luaT_test_reinit();
    lua_setthreadtaint(LT, &luaT_taint);
    vec->func(LT);
    TEST_CHECK(lua_getvaluetaint(LT, 1) != NULL);
    TEST_CHECK(!luaL_issecurevalue(LT, 1));
  }
}

static void test_push_values_without_taint(void) {
  for (struct LuaValueVector *vec = luaT_value_vectors; vec->name; ++vec) {
    TEST_CASE(vec->name);
    luaT_test_reinit();
    vec->func(LT);
    TEST_CHECK(lua_getvaluetaint(LT, 1) == NULL);
    TEST_CHECK(luaL_issecurevalue(LT, 1));
  }
}

static void test_push_objects_with_taint(void) {
  for (struct LuaValueVector *vec = luaT_object_vectors; vec->name; ++vec) {
    TEST_CASE(vec->name);
    luaT_test_reinit();
    lua_setthreadtaint(LT, &luaT_taint);
    vec->func(LT);
    TEST_CHECK(lua_getobjecttaint(LT, 1) != NULL);
    TEST_CHECK(!luaL_issecureobject(LT, 1));
  }
}

static void test_push_objects_without_taint(void) {
  for (struct LuaValueVector *vec = luaT_object_vectors; vec->name; ++vec) {
    TEST_CASE(vec->name);
    luaT_test_reinit();
    vec->func(LT);
    TEST_CHECK(lua_getobjecttaint(LT, 1) == NULL);
    TEST_CHECK(luaL_issecureobject(LT, 1));
  }
}

static void test_tainted_table_structures(void) {
  lua_setthreadtaint(LT, &luaT_taint);

  // Create a color-like table on the stack.
  lua_createtable(LT, 0, 3);
  lua_pushnumber(LT, 0.3);
  lua_setfield(LT, 1, "r");
  lua_pushnumber(LT, 0.6);
  lua_setfield(LT, 1, "g");
  lua_pushnumber(LT, 0.9);
  lua_setfield(LT, 1, "b");

  // Expect all fields to be tainted. This is to mimic behaviors like calling
  // C_ClassColor.GetClassColor ingame which returns a table with tainted
  // keys.

  lua_setthreadtaint(LT, NULL);
  lua_getfield(LT, 1, "r");
  TEST_CHECK(!luaL_issecurethread(LT));
  TEST_CHECK(!luaL_issecurevalue(LT, -1));
  lua_pop(LT, 1);

  lua_setthreadtaint(LT, NULL);
  lua_getfield(LT, 1, "g");
  TEST_CHECK(!luaL_issecurethread(LT));
  TEST_CHECK(!luaL_issecurevalue(LT, -1));
  lua_pop(LT, 1);

  lua_setthreadtaint(LT, NULL);
  lua_getfield(LT, 1, "b");
  TEST_CHECK(!luaL_issecurethread(LT));
  TEST_CHECK(!luaL_issecurevalue(LT, -1));
  lua_pop(LT, 1);
}

/**
 * Value => State Taint Propagation Tests
 */

static void test_secure_value_reads(void) {
  lua_pushboolean(LT, 1);
  lua_setfield(LT, LUA_GLOBALSINDEX, "SecureGlobal");

  luaL_forcetaintthread(LT);
  lua_getfield(LT, LUA_GLOBALSINDEX, "SecureGlobal");

  TEST_CHECK(!luaL_issecurethread(LT));
  TEST_CHECK(!luaL_issecurevalue(LT, 1));
}

/**
 * Lua Security API tests
 */

static void test_getfenv_metatable(void) {
  /**
   * Given a function with an environment that has a metatable applied, the
   * return of lua_getfenv should be that of the '__environment' key if
   * present on the metatable.
   */

  static const int ENVIRONMENT_INDEX = 1;
  static const int METATABLE_INDEX = 2;
  static const int FUNCTION_INDEX = 3;

  lua_createtable(LT, 0, 0);                         /* environment */
  lua_createtable(LT, 0, 1);                         /* metatable */
  TEST_ASSERT(luaL_loadstring(LT, "return 1") == 0); /* function */

  /* Querying environment should return the global environment. */
  lua_getfenv(LT, FUNCTION_INDEX);
  TEST_CHECK(lua_rawequal(LT, -1, LUA_GLOBALSINDEX));
  lua_pop(LT, 1);

  /* Apply metatable to environment. */
  lua_pushvalue(LT, METATABLE_INDEX);
  TEST_ASSERT(lua_setmetatable(LT, ENVIRONMENT_INDEX) == 1);

  /* Apply environment to function. */
  lua_pushvalue(LT, ENVIRONMENT_INDEX);
  TEST_ASSERT(lua_setfenv(LT, FUNCTION_INDEX) == 1);
  TEST_ASSERT(lua_gettop(LT) == 3);

  /* Querying environment should return the expected table. */
  lua_getfenv(LT, FUNCTION_INDEX);
  TEST_CHECK(lua_rawequal(LT, -1, ENVIRONMENT_INDEX));
  lua_pop(LT, 1);

  /* Set '__environment' key to false. */
  lua_pushboolean(LT, 0);
  lua_setfield(LT, METATABLE_INDEX, "__environment");

  /* Querying environment should now return false. */
  lua_getfenv(LT, FUNCTION_INDEX);
  lua_pushboolean(LT, 0);
  TEST_CHECK(lua_rawequal(LT, -2, -1));
  lua_pop(LT, 2);
}

static void test_securecall_initiallysecure(void) {
  luaT_loadfixture(LT, luac_securecallaux);

  lua_pushliteral(LT, "securecall_identity");
  luaT_pushstring(LT);
  luaT_pushnumber(LT);
  luaT_pushboolean(LT);
  luaT_pushtable(LT);
  TEST_CHECK(lua_securecall(LT, 4, LUA_MULTRET, 0) == 0);

  TEST_CHECK(luaL_issecurethread(LT));      /* Should still be secure. */
  TEST_CHECK(lua_gettop(LT) == 4);          /* Should have four return values. */
  TEST_CHECK(luaL_issecurevalue(LT, -1));   /* All function returns should be secure. */
  TEST_CHECK(luaL_issecurevalue(LT, -1));
  TEST_CHECK(luaL_issecurevalue(LT, -1));
  TEST_CHECK(luaL_issecurevalue(LT, -1));
}

static void test_securecall_initiallyinsecure(void) {
  luaT_loadfixture(LT, luac_securecallaux);

  lua_setthreadtaint(LT, &luaT_taint);      /* Taint execution */
  lua_pushliteral(LT, "securecall_identity");
  luaT_pushstring(LT);
  luaT_pushnumber(LT);
  luaT_pushboolean(LT);
  luaT_pushtable(LT);
  TEST_CHECK(lua_securecall(LT, 4, LUA_MULTRET, 0) == 0);

  TEST_CHECK(!luaL_issecurethread(LT));     /* Should not be secure. */
  TEST_CHECK(lua_gettop(LT) == 4);          /* Should have four return values. */
  TEST_CHECK(!luaL_issecurevalue(LT, -1));  /* All function returns should be insecure. */
  TEST_CHECK(!luaL_issecurevalue(LT, -1));
  TEST_CHECK(!luaL_issecurevalue(LT, -1));
  TEST_CHECK(!luaL_issecurevalue(LT, -1));
}

static void test_securecall_insecurefunction(void) {
  lua_setthreadtaint(LT, &luaT_taint);            /* Load fixture insecurely. */
  luaT_loadfixture(LT, luac_securecallaux);
  lua_setthreadtaint(LT, NULL);

  TEST_CHECK(luaL_issecurethread(LT));      /* Should be secure at this point. */

  lua_pushliteral(LT, "securecall_identity");
  luaT_pushstring(LT);
  luaT_pushnumber(LT);
  luaT_pushboolean(LT);
  luaT_pushtable(LT);
  TEST_CHECK(lua_securecall(LT, 4, LUA_MULTRET, 0) == 0);

  TEST_CHECK(luaL_issecurethread(LT));      /* Should still be secure. */
  TEST_CHECK(lua_gettop(LT) == 4);          /* Should have four return values. */
  TEST_CHECK(luaL_issecurevalue(LT, -1));   /* All function returns should be secure. */
  TEST_CHECK(luaL_issecurevalue(LT, -1));
  TEST_CHECK(luaL_issecurevalue(LT, -1));
  TEST_CHECK(luaL_issecurevalue(LT, -1));
}

static void test_securecall_forceinsecure(void) {
  luaT_loadfixture(LT, luac_securecallaux);

  lua_pushliteral(LT, "securecall_forceinsecure");
  luaT_pushstring(LT);
  luaT_pushnumber(LT);
  luaT_pushboolean(LT);
  luaT_pushtable(LT);
  TEST_CHECK(lua_securecall(LT, 4, LUA_MULTRET, 0) == 0);

  TEST_CHECK(luaL_issecurethread(LT));      /* Should still be secure. */
  TEST_CHECK(lua_gettop(LT) == 4);          /* Should have four return values. */
  TEST_CHECK(luaL_issecurevalue(LT, -1));   /* All function returns should be secure. */
  TEST_CHECK(luaL_issecurevalue(LT, -1));
  TEST_CHECK(luaL_issecurevalue(LT, -1));
  TEST_CHECK(luaL_issecurevalue(LT, -1));
}

static void test_securecall_directsecurefunction(void) {
  luaT_loadfixture(LT, luac_securecallaux);

  TEST_CHECK(luaL_issecurethread(LT));      /* Should be secure at this point. */

  lua_pushvalue(LT, LUA_GLOBALSINDEX);
  lua_pushliteral(LT, "securecall_identity");
  lua_gettable(LT, -2);
  lua_remove(LT, 1);                        /* Leave only the function on the stack. */

  luaT_pushstring(LT);
  luaT_pushnumber(LT);
  luaT_pushboolean(LT);
  luaT_pushtable(LT);
  TEST_CHECK(lua_securecall(LT, 4, LUA_MULTRET, 0) == 0);

  TEST_CHECK(luaL_issecurethread(LT));      /* Should still be secure. */
  TEST_CHECK(lua_gettop(LT) == 4);          /* Should have four return values. */
  TEST_CHECK(luaL_issecurevalue(LT, -1));   /* All function returns should be secure. */
  TEST_CHECK(luaL_issecurevalue(LT, -1));
  TEST_CHECK(luaL_issecurevalue(LT, -1));
  TEST_CHECK(luaL_issecurevalue(LT, -1));
}

static void test_securecall_directinsecurefunction(void) {
  lua_setthreadtaint(LT, &luaT_taint);            /* Load fixture insecurely. */
  luaT_loadfixture(LT, luac_securecallaux);

  lua_pushvalue(LT, LUA_GLOBALSINDEX);
  lua_pushliteral(LT, "securecall_identity");
  lua_gettable(LT, -2);
  lua_remove(LT, 1);                        /* Leave only the function on the stack. */
  lua_setthreadtaint(LT, NULL);
  TEST_CHECK(luaL_issecurethread(LT));      /* Should be secure at this point. */
  TEST_CHECK(!luaL_issecurevalue(LT, -1));  /* Function value should be insecure. */

  luaT_pushstring(LT);
  luaT_pushnumber(LT);
  luaT_pushboolean(LT);
  luaT_pushtable(LT);
  TEST_CHECK(lua_securecall(LT, 4, LUA_MULTRET, 0) == 0);

  TEST_CHECK(luaL_issecurethread(LT));      /* Should still be secure. */
  TEST_CHECK(lua_gettop(LT) == 4);          /* Should have four return values. */
  TEST_CHECK(luaL_issecurevalue(LT, -1));   /* All function returns should be secure. */
  TEST_CHECK(luaL_issecurevalue(LT, -1));
  TEST_CHECK(luaL_issecurevalue(LT, -1));
  TEST_CHECK(luaL_issecurevalue(LT, -1));
}

static int secureerrorhandler(lua_State *L) {
  TEST_CHECK(lua_gettop(L) == 1);          /* Expect only error information on the stack */
  TEST_CHECK(luaL_issecurethread(L));      /* Expect to have been called securely... */
  TEST_CHECK(luaL_issecurevalue(L, 1));    /* ...And that our one argument is secure. */
  return 1;
}

static void test_securecall_secureerror(void) {
  luaT_loadfixture(LT, luac_securecallaux);

  lua_pushcclosure(LT, secureerrorhandler, 0);    /* See securerrrorhandler for additional checks. */
  lua_pushliteral(LT, "securecall_error");
  lua_pushliteral(LT, "test error message");
  TEST_CHECK(lua_securecall(LT, 1, 0, 1) != 0);   /* Expect call to fail. */
  TEST_CHECK(luaL_issecurethread(LT));                  /* Expect to remain secure. */
  TEST_CHECK(lua_gettop(LT) == 2);                /* Expect two values on stack; errfunc and err */
  TEST_CHECK(luaL_issecurevalue(LT, -1));         /* Error object should be secure */
}

static int insecureerrorhandler(lua_State *L) {
  TEST_CHECK(lua_gettop(L) == 1);          /* Expect only error information on the stack */
  TEST_CHECK(!luaL_issecurethread(L));     /* Expect to have been called insecurely... */
  TEST_CHECK(!luaL_issecurevalue(L, 1));   /* ...And that our one argument is insecure. */
  return 1;
}

static void test_securecall_insecureerror(void) {
  luaT_loadfixture(LT, luac_securecallaux);

  lua_setthreadtaint(LT, &luaT_taint);
  lua_pushcclosure(LT, insecureerrorhandler, 0);  /* See insecurerrorhandler for additional checks. */
  lua_pushliteral(LT, "securecall_error");
  lua_pushliteral(LT, "test error message");
  TEST_CHECK(lua_securecall(LT, 1, 0, 1) != 0);   /* Expect call to fail. */
  TEST_CHECK(!luaL_issecurethread(LT));            /* Expect to remain insecure. */
  TEST_CHECK(lua_gettop(LT) == 2);                /* Expect two values on stack; errfunc and err */
  TEST_CHECK(!luaL_issecurevalue(LT, -1));        /* Error object should be insecure */
}

static void test_securecall_forceinsecureerror(void) {
  luaT_loadfixture(LT, luac_securecallaux);

  lua_pushcclosure(LT, insecureerrorhandler, 0);  /* See insecurerrorhandler for additional checks. */
  lua_pushliteral(LT, "securecall_insecureerror");
  lua_pushliteral(LT, "test error message");
  TEST_CHECK(lua_securecall(LT, 1, 0, 1) != 0);   /* Expect call to fail. */
  TEST_CHECK(luaL_issecurethread(LT));            /* Expect to remain secure. */
  TEST_CHECK(lua_gettop(LT) == 2);                /* Expect two values on stack; errfunc and err */
  TEST_CHECK(luaL_issecurevalue(LT, -1));         /* Error object should be secure */
}

/**
 * VM Script Tests
 */

static void test_vm_arith(void) {
  luaT_loadfixture(LT, luac_lvmutil);
  luaT_loadfixture(LT, luac_lvmarith);
}

static void test_vm_fields(void) {
  luaT_loadfixture(LT, luac_lvmutil);
  luaT_loadfixture(LT, luac_lvmfields);
}

static void test_vm_globals(void) {
  luaT_loadfixture(LT, luac_lvmutil);
  luaT_loadfixture(LT, luac_lvmglobals);
}

static void test_vm_locals(void) {
  luaT_loadfixture(LT, luac_lvmutil);
  luaT_loadfixture(LT, luac_lvmlocals);
}

static void test_vm_misc(void) {
  luaT_loadfixture(LT, luac_lvmutil);
  luaT_loadfixture(LT, luac_lvmmisc);
}

static void test_vm_upvalues(void) {
  luaT_loadfixture(LT, luac_lvmutil);
  luaT_loadfixture(LT, luac_lvmupvalues);
}

/**
 * Script Tests
 */

static void test_script_constantassignments(void) {
  luaT_loadfixture(LT, luac_lvmutil);
  luaT_loadfixture(LT, luac_mixin);
  luaT_loadfixture(LT, luac_rectanglemixin);
  luaT_loadfixture(LT, luac_lconstantassignments);
}

static void test_script_dblib(void) {
  luaT_loadfixture(LT, luac_lvmutil);
  luaT_loadfixture(LT, luac_ldblib);
}

static void test_script_issecurevariable(void) {
  luaT_loadfixture(LT, luac_lvmutil);
  luaT_loadfixture(LT, luac_lissecurevariable);
}

/**
 * Test Listing
 */

TEST_LIST = {
  { "initially secure on state creation", test_startup_taint },
  { "taint can be applied to state", test_tainted },
  { "taint can be forced", test_forced_taint },
  { "forced taint doesn't override existing taint", test_forced_taint_override },
  { "pushed values inherit state taint", test_push_values_with_taint },
  { "pushed values aren't tainted by default", test_push_values_without_taint },
  { "pushed objects inherit state taint", test_push_objects_with_taint },
  { "pushed objects aren't tainted by default", test_push_objects_without_taint },
  { "tainted table structures", test_tainted_table_structures },
  { "read secure values while tainted", test_secure_value_reads },
  { "lua_getfenv: __environment metatable field", &test_getfenv_metatable },
  { "lua_securecall: named call while secure", &test_securecall_initiallysecure },
  { "lua_securecall: named call while insecure", &test_securecall_initiallyinsecure },
  { "lua_securecall: named call insecure function", &test_securecall_insecurefunction },
  { "lua_securecall: named call forceinsecure", &test_securecall_forceinsecure },
  { "lua_securecall: direct call secure function value", &test_securecall_directsecurefunction },
  { "lua_securecall: direct call insecure function value", &test_securecall_directinsecurefunction },
  { "lua_securecall: secure error handling", &test_securecall_secureerror },
  { "lua_securecall: insecure error handling", &test_securecall_insecureerror },
  { "lua_securecall: forceinsecure error handling", &test_securecall_forceinsecureerror },
  { "vm: arithmetic ops", &test_vm_arith },
  { "vm: table fields", &test_vm_fields },
  { "vm: global variables", &test_vm_globals },
  { "vm: local values", &test_vm_locals },
  { "vm: misc opcodes", &test_vm_misc },
  { "vm: upvalues", &test_vm_upvalues },
  { "script: constant assignments", &test_script_constantassignments },
  { "script: debug library extensions", &test_script_dblib },
  { "script: issecurevariable", &test_script_issecurevariable },
  { NULL, NULL }
};
