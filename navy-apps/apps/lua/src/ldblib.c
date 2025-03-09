/*
** $Id: ldblib.c,v 1.151 2015/11/23 11:29:43 roberto Exp $
** Interface from Lua to its debug API
** See Copyright Notice in lua.h
*/

#define ldblib_c
#define LUA_LIB

#include "lprefix.h"


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lua.h"

#include "lauxlib.h"
#include "lualib.h"


/*
** The hook table at registry[&HOOKKEY] maps threads to their current
** hook function. (We only need the unique address of 'HOOKKEY'.)
*/
static const int HOOKKEY = 0;


/*
** If L1 != L, L1 can be in any state, and therefore there are no
** guarantees about its stack space; any push in L1 must be
** checked.
*/
static void checkstack (lua_State *L, lua_State *L1, int n) {
  if (L != L1 && !lua_checkstack(L1, n))
    luaL_error(L, "stack overflow");
}


/**
 * Pushes the Lua registry onto the stack and returns it to the caller.
 *
 * The Lua registry is a predefined table that can be used to store values
 * that need to be shared across different parts of a Lua program or between
 * Lua and C code. This function retrieves the registry by pushing it onto
 * the Lua stack using the `LUA_REGISTRYINDEX` pseudo-index.
 *
 * @param L A pointer to the Lua state.
 * @return Returns 1, indicating that one value (the registry table) has been
 *         pushed onto the stack.
 */
static int db_getregistry (lua_State *L) {
  lua_pushvalue(L, LUA_REGISTRYINDEX);
  return 1;
}


/**
 * Retrieves the metatable of the object at the top of the Lua stack.
 * 
 * This function checks if the object at the top of the Lua stack has a metatable.
 * If the object has a metatable, it is pushed onto the stack. If the object does
 * not have a metatable, `nil` is pushed onto the stack instead. The function
 * always returns 1, indicating that one value (either the metatable or `nil`)
 * has been pushed onto the stack.
 *
 * @param L The Lua state.
 * @return Always returns 1, indicating one value has been pushed onto the stack.
 */
static int db_getmetatable (lua_State *L) {
  luaL_checkany(L, 1);
  if (!lua_getmetatable(L, 1)) {
    lua_pushnil(L);  /* no metatable */
  }
  return 1;
}


/**
 * Sets the metatable for the object at the top of the Lua stack.
 *
 * This function expects two arguments on the Lua stack:
 * 1. The object for which the metatable is to be set.
 * 2. The metatable to be assigned, which must be either `nil` or a table.
 *
 * If the second argument is `nil`, the metatable of the object is removed.
 * If the second argument is a table, it is set as the metatable for the object.
 *
 * @param L The Lua state.
 * @return int Always returns 1, indicating that the first argument (the object) is returned.
 */
static int db_setmetatable (lua_State *L) {
  int t = lua_type(L, 2);
  luaL_argcheck(L, t == LUA_TNIL || t == LUA_TTABLE, 2,
                    "nil or table expected");
  lua_settop(L, 2);
  lua_setmetatable(L, 1);
  return 1;  /* return 1st argument */
}


/**
 * Retrieves the user value associated with the userdata at the top of the Lua stack.
 * 
 * This function checks if the first argument on the Lua stack is of type `LUA_TUSERDATA`.
 * If it is not, the function pushes `nil` onto the stack. If it is, the function retrieves
 * the user value associated with the userdata and pushes it onto the stack.
 * 
 * @param L The Lua state.
 * @return Always returns 1, indicating that one value has been pushed onto the stack.
 */
static int db_getuservalue (lua_State *L) {
  if (lua_type(L, 1) != LUA_TUSERDATA)
    lua_pushnil(L);
  else
    lua_getuservalue(L, 1);
  return 1;
}


/**
 * Sets the user value associated with a userdata object in the Lua state.
 *
 * This function expects two arguments on the Lua stack:
 * 1. A userdata object (checked using `luaL_checktype`).
 * 2. Any Lua value to be set as the user value of the userdata object.
 *
 * The function ensures the stack contains exactly these two arguments by calling `lua_settop`.
 * It then sets the provided value as the user value of the userdata object using `lua_setuservalue`.
 *
 * @param L The Lua state in which the operation is performed.
 * @return Always returns 1, indicating that one value (the userdata object) is left on the stack.
 */
static int db_setuservalue (lua_State *L) {
  luaL_checktype(L, 1, LUA_TUSERDATA);
  luaL_checkany(L, 2);
  lua_settop(L, 2);
  lua_setuservalue(L, 1);
  return 1;
}


/*
** Auxiliary function used by several library functions: check for
** an optional thread as function's first argument and set 'arg' with
** 1 if this argument is present (so that functions can skip it to
** access their other arguments)
*/
static lua_State *getthread (lua_State *L, int *arg) {
  if (lua_isthread(L, 1)) {
    *arg = 1;
    return lua_tothread(L, 1);
  }
  else {
    *arg = 0;
    return L;  /* function will operate over current thread */
  }
}


/*
** Variations of 'lua_settable', used by 'db_getinfo' to put results
** from 'lua_getinfo' into result table. Key is always a string;
** value can be a string, an int, or a boolean.
*/
static void settabss (lua_State *L, const char *k, const char *v) {
  lua_pushstring(L, v);
  lua_setfield(L, -2, k);
}

/**
 * Sets an integer value in a table on the Lua stack.
 *
 * This function pushes the integer value `v` onto the Lua stack and then assigns it to the key `k`
 * in the table located at the top of the stack. The table remains on the stack after the operation.
 *
 * @param L The Lua state.
 * @param k The key in the table where the integer value will be stored. Must be a valid string.
 * @param v The integer value to be stored in the table.
 */
static void settabsi (lua_State *L, const char *k, int v) {
  lua_pushinteger(L, v);
  lua_setfield(L, -2, k);
}

/**
 * Sets a boolean value in a Lua table at the top of the Lua stack.
 *
 * This function pushes a boolean value `v` onto the Lua stack and then assigns it to the field `k`
 * in the table located at the top of the stack. The table remains at the top of the stack after
 * the operation.
 *
 * @param L The Lua state.
 * @param k The key (field name) in the table where the boolean value will be stored.
 * @param v The boolean value to be stored in the table.
 */
static void settabsb (lua_State *L, const char *k, int v) {
  lua_pushboolean(L, v);
  lua_setfield(L, -2, k);
}


/*
** In function 'db_getinfo', the call to 'lua_getinfo' may push
** results on the stack; later it creates the result table to put
** these objects. Function 'treatstackoption' puts the result from
** 'lua_getinfo' on top of the result table so that it can call
** 'lua_setfield'.
*/
static void treatstackoption (lua_State *L, lua_State *L1, const char *fname) {
  if (L == L1)
    lua_rotate(L, -2, 1);  /* exchange object and table */
  else
    lua_xmove(L1, L, 1);  /* move object to the "main" stack */
  lua_setfield(L, -2, fname);  /* put object into table */
}


/*
** Calls 'lua_getinfo' and collects all results in a new table.
** L1 needs stack space for an optional input (function) plus
** two optional outputs (function and line table) from function
** 'lua_getinfo'.
*/
static int db_getinfo (lua_State *L) {
  lua_Debug ar;
  int arg;
  lua_State *L1 = getthread(L, &arg);
  const char *options = luaL_optstring(L, arg+2, "flnStu");
  checkstack(L, L1, 3);
  if (lua_isfunction(L, arg + 1)) {  /* info about a function? */
    options = lua_pushfstring(L, ">%s", options);  /* add '>' to 'options' */
    lua_pushvalue(L, arg + 1);  /* move function to 'L1' stack */
    lua_xmove(L, L1, 1);
  }
  else {  /* stack level */
    if (!lua_getstack(L1, (int)luaL_checkinteger(L, arg + 1), &ar)) {
      lua_pushnil(L);  /* level out of range */
      return 1;
    }
  }
  if (!lua_getinfo(L1, options, &ar))
    return luaL_argerror(L, arg+2, "invalid option");
  lua_newtable(L);  /* table to collect results */
  if (strchr(options, 'S')) {
    settabss(L, "source", ar.source);
    settabss(L, "short_src", ar.short_src);
    settabsi(L, "linedefined", ar.linedefined);
    settabsi(L, "lastlinedefined", ar.lastlinedefined);
    settabss(L, "what", ar.what);
  }
  if (strchr(options, 'l'))
    settabsi(L, "currentline", ar.currentline);
  if (strchr(options, 'u')) {
    settabsi(L, "nups", ar.nups);
    settabsi(L, "nparams", ar.nparams);
    settabsb(L, "isvararg", ar.isvararg);
  }
  if (strchr(options, 'n')) {
    settabss(L, "name", ar.name);
    settabss(L, "namewhat", ar.namewhat);
  }
  if (strchr(options, 't'))
    settabsb(L, "istailcall", ar.istailcall);
  if (strchr(options, 'L'))
    treatstackoption(L, L1, "activelines");
  if (strchr(options, 'f'))
    treatstackoption(L, L1, "func");
  return 1;  /* return table */
}


/**
 * Retrieves the name and value of a local variable from a Lua function or stack level.
 *
 * This function can be used in two ways:
 * 1. If the first argument is a function, it retrieves the name of the local variable
 *    at the specified index within that function. Only the name is returned, not the value.
 * 2. If the first argument is a stack level, it retrieves both the name and value of the
 *    local variable at the specified index within the specified stack level. Both the name
 *    and value are returned.
 *
 * @param L The Lua state.
 * @return The number of values returned to Lua:
 *         - 1 if only the name is returned (when the first argument is a function).
 *         - 2 if both the name and value are returned (when the first argument is a stack level).
 *         - 1 if no local variable is found (nil is returned).
 */
static int db_getlocal (lua_State *L) {
  int arg;
  lua_State *L1 = getthread(L, &arg);
  lua_Debug ar;
  const char *name;
  int nvar = (int)luaL_checkinteger(L, arg + 2);  /* local-variable index */
  if (lua_isfunction(L, arg + 1)) {  /* function argument? */
    lua_pushvalue(L, arg + 1);  /* push function */
    lua_pushstring(L, lua_getlocal(L, NULL, nvar));  /* push local name */
    return 1;  /* return only name (there is no value) */
  }
  else {  /* stack-level argument */
    int level = (int)luaL_checkinteger(L, arg + 1);
    if (!lua_getstack(L1, level, &ar))  /* out of range? */
      return luaL_argerror(L, arg+1, "level out of range");
    checkstack(L, L1, 1);
    name = lua_getlocal(L1, &ar, nvar);
    if (name) {
      lua_xmove(L1, L, 1);  /* move local value */
      lua_pushstring(L, name);  /* push name */
      lua_rotate(L, -2, 1);  /* re-order */
      return 2;
    }
    else {
      lua_pushnil(L);  /* no name (nor value) */
      return 1;
    }
  }
}


/**
 * Sets a local variable in a specified stack frame of a Lua thread.
 *
 * This function retrieves a Lua thread (`L1`) from the Lua state (`L`) and a stack level (`level`) 
 * within that thread. It then sets a local variable (`nvar`) in the specified stack frame to a value 
 * provided on the Lua stack. If the stack level or local variable index is out of range, an error is raised.
 *
 * @param L The Lua state from which the thread and arguments are retrieved.
 * @return Returns 1 on success, pushing the name of the local variable onto the stack. If the local 
 *         variable could not be set (e.g., due to an invalid index), the function returns 0.
 *
 * @details The function performs the following steps:
 * 1. Retrieves the Lua thread (`L1`) and the argument index (`arg`) using `getthread`.
 * 2. Validates the stack level (`level`) and local variable index (`nvar`) using `luaL_checkinteger`.
 * 3. Retrieves the stack frame information (`ar`) for the specified level using `lua_getstack`.
 * 4. Checks if the provided value (at `arg+3`) is valid using `luaL_checkany`.
 * 5. Moves the value from the current Lua state (`L`) to the target thread (`L1`) using `lua_xmove`.
 * 6. Sets the local variable in the specified stack frame using `lua_setlocal`.
 * 7. If the local variable was successfully set, the function pushes its name onto the stack and returns 1.
 *    If the local variable could not be set, the function pops the value from the stack and returns 0.
 */
static int db_setlocal (lua_State *L) {
  int arg;
  const char *name;
  lua_State *L1 = getthread(L, &arg);
  lua_Debug ar;
  int level = (int)luaL_checkinteger(L, arg + 1);
  int nvar = (int)luaL_checkinteger(L, arg + 2);
  if (!lua_getstack(L1, level, &ar))  /* out of range? */
    return luaL_argerror(L, arg+1, "level out of range");
  luaL_checkany(L, arg+3);
  lua_settop(L, arg+3);
  checkstack(L, L1, 1);
  lua_xmove(L, L1, 1);
  name = lua_setlocal(L1, &ar, nvar);
  if (name == NULL)
    lua_pop(L1, 1);  /* pop value (if not popped by 'lua_setlocal') */
  lua_pushstring(L, name);
  return 1;
}


/*
** get (if 'get' is true) or set an upvalue from a closure
*/
static int auxupvalue (lua_State *L, int get) {
  const char *name;
  int n = (int)luaL_checkinteger(L, 2);  /* upvalue index */
  luaL_checktype(L, 1, LUA_TFUNCTION);  /* closure */
  name = get ? lua_getupvalue(L, 1, n) : lua_setupvalue(L, 1, n);
  if (name == NULL) return 0;
  lua_pushstring(L, name);
  lua_insert(L, -(get+1));  /* no-op if get is false */
  return get + 1;
}


/**
 * Retrieves the upvalue at the specified index from the Lua state.
 *
 * This function is a wrapper around `auxupvalue` and is used to fetch an upvalue
 * from the Lua state `L`. The upvalue is identified by the index provided, which
 * is passed as the second argument to `auxupvalue`. The function returns the result
 * of `auxupvalue`, which typically includes the upvalue or an error code if the
 * operation fails.
 *
 * @param L The Lua state from which to retrieve the upvalue.
 * @return The result of the `auxupvalue` function, which may be the upvalue or an
 *         error code.
 */
static int db_getupvalue (lua_State *L) {
  return auxupvalue(L, 1);
}


/**
 * Sets up a value in the Lua state for a debug hook.
 *
 * This function is used to set up a value in the Lua state `L` for a debug hook. 
 * It ensures that the third argument on the Lua stack is valid by calling `luaL_checkany`.
 * The function then delegates the actual setup to `auxupvalue` with the `L` state and a flag 
 * of `0` to indicate the specific setup operation.
 *
 * @param L The Lua state in which the value is to be set up.
 * @return The result of the `auxupvalue` function, typically the number of values returned to Lua.
 */
static int db_setupvalue (lua_State *L) {
  luaL_checkany(L, 3);
  return auxupvalue(L, 0);
}


/*
** Check whether a given upvalue from a given closure exists and
** returns its index
*/
static int checkupval (lua_State *L, int argf, int argnup) {
  int nup = (int)luaL_checkinteger(L, argnup);  /* upvalue index */
  luaL_checktype(L, argf, LUA_TFUNCTION);  /* closure */
  luaL_argcheck(L, (lua_getupvalue(L, argf, nup) != NULL), argnup,
                   "invalid upvalue index");
  return nup;
}


/**
 * Retrieves the unique identifier of an upvalue associated with a function.
 * 
 * This function takes a Lua state `L` and retrieves the unique identifier of the `n`th upvalue
 * of the function at index `1` on the Lua stack. The upvalue index `n` is specified by the
 * second argument. The identifier is pushed onto the Lua stack as a light userdata.
 * 
 * @param L The Lua state.
 * @return Returns 1, indicating that one value (the upvalue identifier) has been pushed onto the stack.
 */
static int db_upvalueid (lua_State *L) {
  int n = checkupval(L, 1, 2);
  lua_pushlightuserdata(L, lua_upvalueid(L, 1, n));
  return 1;
}


/**
 * Joins two upvalues from different Lua functions.
 *
 * This function takes two Lua functions and two upvalue indices, and joins the
 * specified upvalues from the two functions. The first function and its upvalue
 * are identified by the first and second arguments, respectively. The second
 * function and its upvalue are identified by the third and fourth arguments.
 *
 * The function performs the following steps:
 * 1. Validates that the first and third arguments are Lua functions (not C functions).
 * 2. Retrieves the upvalue indices from the second and fourth arguments.
 * 3. Joins the specified upvalues using `lua_upvaluejoin`.
 *
 * @param L The Lua state.
 * @return Always returns 0, indicating no values are pushed onto the Lua stack.
 */
static int db_upvaluejoin (lua_State *L) {
  int n1 = checkupval(L, 1, 2);
  int n2 = checkupval(L, 3, 4);
  luaL_argcheck(L, !lua_iscfunction(L, 1), 1, "Lua function expected");
  luaL_argcheck(L, !lua_iscfunction(L, 3), 3, "Lua function expected");
  lua_upvaluejoin(L, 1, n1, 3, n2);
  return 0;
}


/*
** Call hook function registered at hook table for the current
** thread (if there is one)
*/
static void hookf (lua_State *L, lua_Debug *ar) {
  static const char *const hooknames[] =
    {"call", "return", "line", "count", "tail call"};
  lua_rawgetp(L, LUA_REGISTRYINDEX, &HOOKKEY);
  lua_pushthread(L);
  if (lua_rawget(L, -2) == LUA_TFUNCTION) {  /* is there a hook function? */
    lua_pushstring(L, hooknames[(int)ar->event]);  /* push event name */
    if (ar->currentline >= 0)
      lua_pushinteger(L, ar->currentline);  /* push current line */
    else lua_pushnil(L);
    lua_assert(lua_getinfo(L, "lS", ar));
    lua_call(L, 2, 0);  /* call hook function */
  }
}


/*
** Convert a string mask (for 'sethook') into a bit mask
*/
static int makemask (const char *smask, int count) {
  int mask = 0;
  if (strchr(smask, 'c')) mask |= LUA_MASKCALL;
  if (strchr(smask, 'r')) mask |= LUA_MASKRET;
  if (strchr(smask, 'l')) mask |= LUA_MASKLINE;
  if (count > 0) mask |= LUA_MASKCOUNT;
  return mask;
}


/*
** Convert a bit mask (for 'gethook') into a string mask
*/
static char *unmakemask (int mask, char *smask) {
  int i = 0;
  if (mask & LUA_MASKCALL) smask[i++] = 'c';
  if (mask & LUA_MASKRET) smask[i++] = 'r';
  if (mask & LUA_MASKLINE) smask[i++] = 'l';
  smask[i] = '\0';
  return smask;
}


/**
 * Sets a hook function for a Lua thread, allowing for the interception of
 * specific events during the execution of Lua code. The hook function can
 * be triggered on events such as function calls, returns, line execution,
 * or instruction execution, depending on the provided mask and count.
 *
 * @param L The Lua state in which the hook is being set.
 * @return Always returns 0, indicating no return values are pushed onto the stack.
 *
 * The function expects the following arguments on the Lua stack:
 * - arg+1: The hook function to be set. If `nil` or not provided, the hook is removed.
 * - arg+2: A string specifying the event mask (e.g., "call", "return", "line", "count").
 * - arg+3: An optional integer specifying the count for the "count" event.
 *
 * The hook function is stored in a registry table associated with the Lua thread.
 * If the hook function is `nil`, the hook is disabled by setting the hook function
 * to `NULL`, the mask to 0, and the count to 0. Otherwise, the hook function is
 * registered with the specified mask and count.
 *
 * The registry table is created if it does not already exist, and it is marked
 * with a weak key mode to allow garbage collection of threads that are no longer
 * referenced.
 *
 * The hook is applied to the Lua thread `L1`, which is either the current thread
 * or a thread specified by the `getthread` function.
 */
static int db_sethook (lua_State *L) {
  int arg, mask, count;
  lua_Hook func;
  lua_State *L1 = getthread(L, &arg);
  if (lua_isnoneornil(L, arg+1)) {  /* no hook? */
    lua_settop(L, arg+1);
    func = NULL; mask = 0; count = 0;  /* turn off hooks */
  }
  else {
    const char *smask = luaL_checkstring(L, arg+2);
    luaL_checktype(L, arg+1, LUA_TFUNCTION);
    count = (int)luaL_optinteger(L, arg + 3, 0);
    func = hookf; mask = makemask(smask, count);
  }
  if (lua_rawgetp(L, LUA_REGISTRYINDEX, &HOOKKEY) == LUA_TNIL) {
    lua_createtable(L, 0, 2);  /* create a hook table */
    lua_pushvalue(L, -1);
    lua_rawsetp(L, LUA_REGISTRYINDEX, &HOOKKEY);  /* set it in position */
    lua_pushstring(L, "k");
    lua_setfield(L, -2, "__mode");  /** hooktable.__mode = "k" */
    lua_pushvalue(L, -1);
    lua_setmetatable(L, -2);  /* setmetatable(hooktable) = hooktable */
  }
  checkstack(L, L1, 1);
  lua_pushthread(L1); lua_xmove(L1, L, 1);  /* key (thread) */
  lua_pushvalue(L, arg + 1);  /* value (hook function) */
  lua_rawset(L, -3);  /* hooktable[L1] = new Lua hook */
  lua_sethook(L1, func, mask, count);
  return 0;
}


/**
 * Retrieves the hook information for a given Lua thread.
 *
 * This function retrieves the hook mask, hook function, and hook count for the specified Lua thread.
 * If the thread has no hook set, it pushes `nil` onto the stack. If the hook is an external hook (not
 * the default hook function), it pushes the string "external hook" onto the stack. Otherwise, it
 * retrieves the hook table from the Lua registry and pushes the corresponding hook information onto
 * the stack.
 *
 * @param L The Lua state from which the thread and hook information are retrieved.
 * @return Returns 3 values on the Lua stack:
 *         1. The hook function (or `nil`/ "external hook" if applicable).
 *         2. A string representation of the hook mask.
 *         3. The hook count as an integer.
 */
static int db_gethook (lua_State *L) {
  int arg;
  lua_State *L1 = getthread(L, &arg);
  char buff[5];
  int mask = lua_gethookmask(L1);
  lua_Hook hook = lua_gethook(L1);
  if (hook == NULL)  /* no hook? */
    lua_pushnil(L);
  else if (hook != hookf)  /* external hook? */
    lua_pushliteral(L, "external hook");
  else {  /* hook table must exist */
    lua_rawgetp(L, LUA_REGISTRYINDEX, &HOOKKEY);
    checkstack(L, L1, 1);
    lua_pushthread(L1); lua_xmove(L1, L, 1);
    lua_rawget(L, -2);   /* 1st result = hooktable[L1] */
    lua_remove(L, -2);  /* remove hook table */
  }
  lua_pushstring(L, unmakemask(mask, buff));  /* 2nd result = mask */
  lua_pushinteger(L, lua_gethookcount(L1));  /* 3rd result = count */
  return 3;
}


/**
 * The `db_debug` function is a debugging utility for Lua scripts. It enters an infinite loop
 * where it continuously prompts the user for input in the format of Lua commands. The function
 * reads input from the standard input (stdin) and processes it as a Lua command.
 *
 * The function performs the following steps in each iteration of the loop:
 * 1. Prompts the user with "lua_debug> " to indicate that it is ready to accept a command.
 * 2. Reads a line of input from stdin into a buffer of size 250.
 * 3. If the input is empty (e.g., EOF) or the command "cont\n" is entered, the function exits
 *    and returns 0, effectively terminating the debug loop.
 * 4. If a valid Lua command is entered, it attempts to load and execute the command using
 *    `luaL_loadbuffer` and `lua_pcall`. If an error occurs during execution, the error message
 *    is printed to the standard error output (stderr) using `lua_writestringerror`.
 * 5. After processing the command, the Lua stack is cleared using `lua_settop(L, 0)` to remove
 *    any return values or intermediate results.
 *
 * This function is useful for interactive debugging of Lua scripts, allowing users to execute
 * arbitrary Lua commands in the context of the current Lua state.
 *
 * @param L A pointer to the Lua state (lua_State) in which the debug commands are executed.
 * @return Returns 0 when the debug loop is terminated by the "cont" command or EOF.
 */
static int db_debug (lua_State *L) {
  for (;;) {
    char buffer[250];
    lua_writestringerror("%s", "lua_debug> ");
    if (fgets(buffer, sizeof(buffer), stdin) == 0 ||
        strcmp(buffer, "cont\n") == 0)
      return 0;
    if (luaL_loadbuffer(L, buffer, strlen(buffer), "=(debug command)") ||
        lua_pcall(L, 0, 0, 0))
      lua_writestringerror("%s\n", lua_tostring(L, -1));
    lua_settop(L, 0);  /* remove eventual returns */
  }
}


/**
 * Generates a traceback for the specified Lua thread or the current thread.
 * 
 * This function retrieves a message and an optional level from the Lua stack,
 * and then generates a traceback for the specified Lua thread (or the current
 * thread if no thread is specified). The traceback is pushed onto the Lua stack.
 * 
 * @param L The Lua state from which the function is called.
 * 
 * @return Always returns 1, indicating that one value (the traceback) is pushed
 *         onto the Lua stack.
 * 
 * @details The function first retrieves the Lua thread (L1) and the argument
 *          index (arg) using `getthread`. If the message (msg) is not a string
 *          and is not nil or none, it is pushed back onto the stack untouched.
 *          Otherwise, a traceback is generated using `luaL_traceback`. The
 *          level parameter specifies the depth of the traceback, defaulting to
 *          1 if the current thread is used, or 0 if a different thread is used.
 */
static int db_traceback (lua_State *L) {
  int arg;
  lua_State *L1 = getthread(L, &arg);
  const char *msg = lua_tostring(L, arg + 1);
  if (msg == NULL && !lua_isnoneornil(L, arg + 1))  /* non-string 'msg'? */
    lua_pushvalue(L, arg + 1);  /* return it untouched */
  else {
    int level = (int)luaL_optinteger(L, arg + 2, (L == L1) ? 1 : 0);
    luaL_traceback(L, L1, msg, level);
  }
  return 1;
}


static const luaL_Reg dblib[] = {
  {"debug", db_debug},
  {"getuservalue", db_getuservalue},
  {"gethook", db_gethook},
  {"getinfo", db_getinfo},
  {"getlocal", db_getlocal},
  {"getregistry", db_getregistry},
  {"getmetatable", db_getmetatable},
  {"getupvalue", db_getupvalue},
  {"upvaluejoin", db_upvaluejoin},
  {"upvalueid", db_upvalueid},
  {"setuservalue", db_setuservalue},
  {"sethook", db_sethook},
  {"setlocal", db_setlocal},
  {"setmetatable", db_setmetatable},
  {"setupvalue", db_setupvalue},
  {"traceback", db_traceback},
  {NULL, NULL}
};


/**
 * @brief Opens the Lua debug library and registers its functions.
 *
 * This function is the entry point for the Lua debug module. It creates a new
 * library table on the Lua stack and registers the functions defined in the
 * `dblib` array. The library table is then returned to the caller.
 *
 * @param L A pointer to the Lua state.
 * @return Returns 1, indicating that the library table is left on the stack.
 */
LUAMOD_API int luaopen_debug (lua_State *L) {
  luaL_newlib(L, dblib);
  return 1;
}

