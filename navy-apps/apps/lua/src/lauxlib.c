/*
** $Id: lauxlib.c,v 1.289 2016/12/20 18:37:00 roberto Exp $
** Auxiliary functions for building Lua libraries
** See Copyright Notice in lua.h
*/

#define lauxlib_c
#define LUA_LIB

#include "lprefix.h"


#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/*
** This file uses only the official API of Lua.
** Any function declared here could be written as an application function.
*/

#include "lua.h"

#include "lauxlib.h"


/*
** {======================================================
** Traceback
** =======================================================
*/


#define LEVELS1	10	/* size of the first part of the stack */
#define LEVELS2	11	/* size of the second part of the stack */



/*
** search for 'objidx' in table at index -1.
** return 1 + string at top if find a good name.
*/
static int findfield (lua_State *L, int objidx, int level) {
  if (level == 0 || !lua_istable(L, -1))
    return 0;  /* not found */
  lua_pushnil(L);  /* start 'next' loop */
  while (lua_next(L, -2)) {  /* for each pair in table */
    if (lua_type(L, -2) == LUA_TSTRING) {  /* ignore non-string keys */
      if (lua_rawequal(L, objidx, -1)) {  /* found object? */
        lua_pop(L, 1);  /* remove value (but keep name) */
        return 1;
      }
      else if (findfield(L, objidx, level - 1)) {  /* try recursively */
        lua_remove(L, -2);  /* remove table (but keep name) */
        lua_pushliteral(L, ".");
        lua_insert(L, -2);  /* place '.' between the two names */
        lua_concat(L, 3);
        return 1;
      }
    }
    lua_pop(L, 1);  /* remove value */
  }
  return 0;  /* not found */
}


/*
** Search for a name for a function in all loaded modules
*/
static int pushglobalfuncname (lua_State *L, lua_Debug *ar) {
  int top = lua_gettop(L);
  lua_getinfo(L, "f", ar);  /* push function */
  lua_getfield(L, LUA_REGISTRYINDEX, LUA_LOADED_TABLE);
  if (findfield(L, top + 1, 2)) {
    const char *name = lua_tostring(L, -1);
    if (strncmp(name, "_G.", 3) == 0) {  /* name start with '_G.'? */
      lua_pushstring(L, name + 3);  /* push name without prefix */
      lua_remove(L, -2);  /* remove original name */
    }
    lua_copy(L, -1, top + 1);  /* move name to proper place */
    lua_pop(L, 2);  /* remove pushed values */
    return 1;
  }
  else {
    lua_settop(L, top);  /* remove function and global table */
    return 0;
  }
}


/**
 * Pushes a string representation of the function name or identifier onto the Lua stack.
 * The function attempts to determine the name of the function in the following order:
 * 1. If the function has a global name, it formats and pushes the string "function 'global_name'".
 * 2. If the function has a name from the code (e.g., a local function), it formats and pushes the string "namewhat 'name'".
 * 3. If the function is the main chunk, it pushes the string "main chunk".
 * 4. If the function is a Lua function (not a C function), it formats and pushes the string "function <file:line>".
 * 5. If none of the above applies, it pushes the string "?".
 *
 * @param L The Lua state.
 * @param ar Pointer to a lua_Debug structure containing information about the function.
 */
static void pushfuncname (lua_State *L, lua_Debug *ar) {
  if (pushglobalfuncname(L, ar)) {  /* try first a global name */
    lua_pushfstring(L, "function '%s'", lua_tostring(L, -1));
    lua_remove(L, -2);  /* remove name */
  }
  else if (*ar->namewhat != '\0')  /* is there a name from code? */
    lua_pushfstring(L, "%s '%s'", ar->namewhat, ar->name);  /* use it */
  else if (*ar->what == 'm')  /* main? */
      lua_pushliteral(L, "main chunk");
  else if (*ar->what != 'C')  /* for Lua functions, use <file:line> */
    lua_pushfstring(L, "function <%s:%d>", ar->short_src, ar->linedefined);
  else  /* nothing left... */
    lua_pushliteral(L, "?");
}


/**
 * Determines the last active stack level in the Lua call stack.
 *
 * This function calculates the highest stack level that is currently active in the Lua call stack.
 * It first finds an upper bound by exponentially increasing the level until `lua_getstack` fails.
 * Then, it performs a binary search between the last successful level and the upper bound to pinpoint
 * the exact last active stack level.
 *
 * @param L A pointer to the Lua state.
 * @return The last active stack level, or -1 if no stack levels are active.
 */
static int lastlevel (lua_State *L) {
  lua_Debug ar;
  int li = 1, le = 1;
  /* find an upper bound */
  while (lua_getstack(L, le, &ar)) { li = le; le *= 2; }
  /* do a binary search */
  while (li < le) {
    int m = (li + le)/2;
    if (lua_getstack(L, m, &ar)) li = m + 1;
    else le = m;
  }
  return le - 1;
}


/**
 * Generates a stack traceback for the Lua state `L1` and appends it to the Lua state `L`.
 * The traceback includes the source file, line number, and function name for each stack level.
 * If `msg` is provided, it is prepended to the traceback as a header.
 * The traceback is limited to a maximum number of levels (`LEVELS1 + LEVELS2`) to avoid excessive output.
 * If the number of levels exceeds this limit, an ellipsis (`...`) is inserted to indicate skipped levels.
 * The traceback is formatted as a multi-line string, with each level indented by a tab character (`\t`).
 *
 * @param L The Lua state to which the traceback will be appended.
 * @param L1 The Lua state for which the traceback is generated.
 * @param msg Optional message to be prepended to the traceback. If `NULL`, no message is added.
 * @param level The starting stack level for the traceback. Levels are counted from the bottom of the stack.
 */
LUALIB_API void luaL_traceback (lua_State *L, lua_State *L1,
                                const char *msg, int level) {
  lua_Debug ar;
  int top = lua_gettop(L);
  int last = lastlevel(L1);
  int n1 = (last - level > LEVELS1 + LEVELS2) ? LEVELS1 : -1;
  if (msg)
    lua_pushfstring(L, "%s\n", msg);
  luaL_checkstack(L, 10, NULL);
  lua_pushliteral(L, "stack traceback:");
  while (lua_getstack(L1, level++, &ar)) {
    if (n1-- == 0) {  /* too many levels? */
      lua_pushliteral(L, "\n\t...");  /* add a '...' */
      level = last - LEVELS2 + 1;  /* and skip to last ones */
    }
    else {
      lua_getinfo(L1, "Slnt", &ar);
      lua_pushfstring(L, "\n\t%s:", ar.short_src);
      if (ar.currentline > 0)
        lua_pushfstring(L, "%d:", ar.currentline);
      lua_pushliteral(L, " in ");
      pushfuncname(L, &ar);
      if (ar.istailcall)
        lua_pushliteral(L, "\n\t(...tail calls...)");
      lua_concat(L, lua_gettop(L) - top);
    }
  }
  lua_concat(L, lua_gettop(L) - top);
}

/* }====================================================== */


/*
** {======================================================
** Error-report functions
** =======================================================
*/

LUALIB_API int luaL_argerror (lua_State *L, int arg, const char *extramsg) {
  lua_Debug ar;
  if (!lua_getstack(L, 0, &ar))  /* no stack frame? */
    return luaL_error(L, "bad argument #%d (%s)", arg, extramsg);
  lua_getinfo(L, "n", &ar);
  if (strcmp(ar.namewhat, "method") == 0) {
    arg--;  /* do not count 'self' */
    if (arg == 0)  /* error is in the self argument itself? */
      return luaL_error(L, "calling '%s' on bad self (%s)",
                           ar.name, extramsg);
  }
  if (ar.name == NULL)
    ar.name = (pushglobalfuncname(L, &ar)) ? lua_tostring(L, -1) : "?";
  return luaL_error(L, "bad argument #%d to '%s' (%s)",
                        arg, ar.name, extramsg);
}


/**
 * Generates and returns a type error message for a Lua function argument.
 *
 * This function is used to produce a descriptive error message when a Lua function
 * receives an argument of an unexpected type. It retrieves the actual type of the
 * argument and compares it with the expected type (`tname`). If the argument has a
 * metatable with a `__name` field, that name is used as the type name. Otherwise,
 * the standard type name or a special name (e.g., "light userdata") is used.
 *
 * @param L The Lua state.
 * @param arg The index of the argument that caused the type error.
 * @param tname The expected type name as a string.
 * @return An integer representing the error code, typically used to propagate the
 *         error in Lua.
 */
static int typeerror (lua_State *L, int arg, const char *tname) {
  const char *msg;
  const char *typearg;  /* name for the type of the actual argument */
  if (luaL_getmetafield(L, arg, "__name") == LUA_TSTRING)
    typearg = lua_tostring(L, -1);  /* use the given type name */
  else if (lua_type(L, arg) == LUA_TLIGHTUSERDATA)
    typearg = "light userdata";  /* special name for messages */
  else
    typearg = luaL_typename(L, arg);  /* standard name */
  msg = lua_pushfstring(L, "%s expected, got %s", tname, typearg);
  return luaL_argerror(L, arg, msg);
}


/**
 * Raises a type error for a given argument in the Lua state.
 *
 * This function is used to generate a type error message when an argument
 * passed to a Lua function does not match the expected type. The error message
 * includes the name of the expected type, which is retrieved using `lua_typename`.
 *
 * @param L The Lua state in which the error is raised.
 * @param arg The index of the argument that caused the type error.
 * @param tag The type tag of the expected Lua type.
 */
static void tag_error (lua_State *L, int arg, int tag) {
  typeerror(L, arg, lua_typename(L, tag));
}


/*
** The use of 'lua_pushfstring' ensures this function does not
** need reserved stack space when called.
*/
LUALIB_API void luaL_where (lua_State *L, int level) {
  lua_Debug ar;
  if (lua_getstack(L, level, &ar)) {  /* check function at level */
    lua_getinfo(L, "Sl", &ar);  /* get info about it */
    if (ar.currentline > 0) {  /* is there info? */
      lua_pushfstring(L, "%s:%d: ", ar.short_src, ar.currentline);
      return;
    }
  }
  lua_pushfstring(L, "");  /* else, no information available... */
}


/*
** Again, the use of 'lua_pushvfstring' ensures this function does
** not need reserved stack space when called. (At worst, it generates
** an error with "stack overflow" instead of the given message.)
*/
LUALIB_API int luaL_error (lua_State *L, const char *fmt, ...) {
  va_list argp;
  va_start(argp, fmt);
  luaL_where(L, 1);
  lua_pushvfstring(L, fmt, argp);
  va_end(argp);
  lua_concat(L, 2);
  return lua_error(L);
}


/**
 * @brief Handles the result of a file operation and pushes the appropriate values onto the Lua stack.
 *
 * This function is used to report the success or failure of a file operation to Lua. If the operation
 * was successful (`stat` is non-zero), it pushes `true` onto the Lua stack. If the operation failed
 * (`stat` is zero), it pushes `nil`, an error message, and the error code onto the Lua stack.
 *
 * The error message is constructed using the provided filename (`fname`) and the system error message
 * corresponding to the `errno` value. If `fname` is `NULL`, only the system error message is used.
 *
 * @param L The Lua state.
 * @param stat The result of the file operation (non-zero for success, zero for failure).
 * @param fname The name of the file involved in the operation, or `NULL` if not applicable.
 * @return The number of values pushed onto the Lua stack (1 for success, 3 for failure).
 */
LUALIB_API int luaL_fileresult (lua_State *L, int stat, const char *fname) {
  int en = errno;  /* calls to Lua API may change this value */
  if (stat) {
    lua_pushboolean(L, 1);
    return 1;
  }
  else {
    lua_pushnil(L);
    if (fname)
      lua_pushfstring(L, "%s: %s", fname, strerror(en));
    else
      lua_pushstring(L, strerror(en));
    lua_pushinteger(L, en);
    return 3;
  }
}


#if !defined(l_inspectstat)	/* { */

#if defined(LUA_USE_POSIX)

#include <sys/wait.h>

/*
** use appropriate macros to interpret 'pclose' return status
*/
#define l_inspectstat(stat,what)  \
   if (WIFEXITED(stat)) { stat = WEXITSTATUS(stat); } \
   else if (WIFSIGNALED(stat)) { stat = WTERMSIG(stat); what = "signal"; }

#else

#define l_inspectstat(stat,what)  /* no op */

#endif

#endif				/* } */


LUALIB_API int luaL_execresult (lua_State *L, int stat) {
  const char *what = "exit";  /* type of termination */
  if (stat == -1)  /* error? */
    return luaL_fileresult(L, 0, NULL);
  else {
    l_inspectstat(stat, what);  /* interpret result */
    if (*what == 'e' && stat == 0)  /* successful termination? */
      lua_pushboolean(L, 1);
    else
      lua_pushnil(L);
    lua_pushstring(L, what);
    lua_pushinteger(L, stat);
    return 3;  /* return true/nil,what,code */
  }
}

/* }====================================================== */


/*
** {======================================================
** Userdata's metatable manipulation
** =======================================================
*/

LUALIB_API int luaL_newmetatable (lua_State *L, const char *tname) {
  if (luaL_getmetatable(L, tname) != LUA_TNIL)  /* name already in use? */
    return 0;  /* leave previous value on top, but return 0 */
  lua_pop(L, 1);
  lua_createtable(L, 0, 2);  /* create metatable */
  lua_pushstring(L, tname);
  lua_setfield(L, -2, "__name");  /* metatable.__name = tname */
  lua_pushvalue(L, -1);
  lua_setfield(L, LUA_REGISTRYINDEX, tname);  /* registry.name = metatable */
  return 1;
}


/**
 * Sets the metatable for the object at the top of the Lua stack.
 *
 * This function retrieves the metatable associated with the name `tname` from the Lua registry
 * and sets it as the metatable for the object at the top of the stack. The object at the top of
 * the stack is typically a userdata or table that needs to have its metatable assigned.
 *
 * @param L Pointer to the Lua state.
 * @param tname The name of the metatable to be set, as registered in the Lua registry.
 *
 * @note This function does not remove the object from the stack; it only sets its metatable.
 * The metatable is retrieved using `luaL_getmetatable` and then assigned using `lua_setmetatable`.
 */
LUALIB_API void luaL_setmetatable (lua_State *L, const char *tname) {
  luaL_getmetatable(L, tname);
  lua_setmetatable(L, -2);
}


/**
 * @brief Checks if the value at the given stack index `ud` is a userdata with the specified metatable.
 *
 * This function retrieves the userdata at the specified stack index `ud` and checks if it has a metatable.
 * If the userdata has a metatable, it compares the metatable with the one associated with the type name `tname`.
 * If the metatables match, the function returns the userdata pointer. Otherwise, it returns `NULL`.
 *
 * @param L The Lua state.
 * @param ud The stack index of the value to check.
 * @param tname The name of the expected metatable type.
 * @return Returns the userdata pointer if the value is a userdata with the correct metatable; otherwise, returns `NULL`.
 */
LUALIB_API void *luaL_testudata (lua_State *L, int ud, const char *tname) {
  void *p = lua_touserdata(L, ud);
  if (p != NULL) {  /* value is a userdata? */
    if (lua_getmetatable(L, ud)) {  /* does it have a metatable? */
      luaL_getmetatable(L, tname);  /* get correct metatable */
      if (!lua_rawequal(L, -1, -2))  /* not the same? */
        p = NULL;  /* value is a userdata with wrong metatable */
      lua_pop(L, 2);  /* remove both metatables */
      return p;
    }
  }
  return NULL;  /* value is not a userdata with a metatable */
}


/**
 * @brief Checks and retrieves a userdata object of a specific type from the Lua stack.
 *
 * This function retrieves the userdata object at the given stack index `ud` and verifies
 * that it matches the metatable name `tname`. If the userdata object is not of the expected
 * type, a type error is raised using `typeerror`. If the userdata is valid, it is returned.
 *
 * @param L A pointer to the Lua state.
 * @param ud The stack index of the userdata object to check.
 * @param tname The name of the expected metatable type.
 * @return A pointer to the userdata object if it is of the expected type.
 * @throws Raises a Lua error if the userdata is not of the expected type.
 */
LUALIB_API void *luaL_checkudata (lua_State *L, int ud, const char *tname) {
  void *p = luaL_testudata(L, ud, tname);
  if (p == NULL) typeerror(L, ud, tname);
  return p;
}

/* }====================================================== */


/*
** {======================================================
** Argument check functions
** =======================================================
*/

LUALIB_API int luaL_checkoption (lua_State *L, int arg, const char *def,
                                 const char *const lst[]) {
  const char *name = (def) ? luaL_optstring(L, arg, def) :
                             luaL_checkstring(L, arg);
  int i;
  for (i=0; lst[i]; i++)
    if (strcmp(lst[i], name) == 0)
      return i;
  return luaL_argerror(L, arg,
                       lua_pushfstring(L, "invalid option '%s'", name));
}


/*
** Ensures the stack has at least 'space' extra slots, raising an error
** if it cannot fulfill the request. (The error handling needs a few
** extra slots to format the error message. In case of an error without
** this extra space, Lua will generate the same 'stack overflow' error,
** but without 'msg'.)
*/
LUALIB_API void luaL_checkstack (lua_State *L, int space, const char *msg) {
  if (!lua_checkstack(L, space)) {
    if (msg)
      luaL_error(L, "stack overflow (%s)", msg);
    else
      luaL_error(L, "stack overflow");
  }
}


/**
 * Ensures that the value at the specified index in the Lua stack is of the expected type.
 *
 * This function checks the type of the Lua value at the given index `arg` in the Lua state `L`.
 * If the type does not match the expected type `t`, it raises an error using `tag_error`.
 *
 * @param L Pointer to the Lua state.
 * @param arg Index of the value in the Lua stack to check.
 * @param t Expected type of the value (e.g., LUA_TNUMBER, LUA_TSTRING, etc.).
 *
 * @note This function does not return if the type check fails; instead, it triggers an error.
 */
LUALIB_API void luaL_checktype (lua_State *L, int arg, int t) {
  if (lua_type(L, arg) != t)
    tag_error(L, arg, t);
}


/**
 * Ensures that the argument at the specified index in the Lua stack is not `nil` or missing.
 * If the argument is `nil` or missing, an error is raised with a message indicating that a value was expected.
 *
 * @param L The Lua state.
 * @param arg The index of the argument to check.
 *
 * @throws Raises a Lua error if the argument is `nil` or missing.
 */
LUALIB_API void luaL_checkany (lua_State *L, int arg) {
  if (lua_type(L, arg) == LUA_TNONE)
    luaL_argerror(L, arg, "value expected");
}


/**
 * @brief Retrieves and validates a string from the Lua stack.
 *
 * This function retrieves the string at the specified index `arg` on the Lua stack.
 * If the value at the given index is not a string or cannot be converted to a string,
 * it raises an error using `tag_error`. The length of the string is stored in the
 * variable pointed to by `len`.
 *
 * @param L Pointer to the Lua state.
 * @param arg Index of the argument on the Lua stack.
 * @param len Pointer to a variable where the length of the string will be stored.
 * @return const char* Pointer to the retrieved string. If the value is not a string,
 *         this function does not return (it raises an error).
 */
LUALIB_API const char *luaL_checklstring (lua_State *L, int arg, size_t *len) {
  const char *s = lua_tolstring(L, arg, len);
  if (!s) tag_error(L, arg, LUA_TSTRING);
  return s;
}


/**
 * Retrieves an optional string argument from the Lua stack.
 *
 * This function checks if the argument at position `arg` on the Lua stack is either `nil` or not provided.
 * If so, it returns the default string `def` and, if `len` is not `NULL`, sets `*len` to the length of `def`.
 * If the argument is not `nil` or missing, it treats the argument as a string and returns it, also setting `*len` to the string's length if `len` is not `NULL`.
 *
 * @param L The Lua state.
 * @param arg The index of the argument on the Lua stack.
 * @param def The default string to return if the argument is `nil` or missing.
 * @param len A pointer to store the length of the returned string. Can be `NULL` if the length is not needed.
 * @return The string at the given argument index or the default string if the argument is `nil` or missing.
 */
LUALIB_API const char *luaL_optlstring (lua_State *L, int arg,
                                        const char *def, size_t *len) {
  if (lua_isnoneornil(L, arg)) {
    if (len)
      *len = (def ? strlen(def) : 0);
    return def;
  }
  else return luaL_checklstring(L, arg, len);
}


/**
 * @brief Checks if the value at the specified stack index is a number and returns it.
 *
 * This function retrieves the value at the given stack index `arg` and attempts to convert it to a `lua_Number`.
 * If the conversion is successful, the function returns the number. If the value is not a number, the function
 * raises an error using `tag_error` with the expected type `LUA_TNUMBER`.
 *
 * @param L A pointer to the Lua state.
 * @param arg The stack index of the value to check.
 * @return The number value at the specified stack index.
 * @throws Raises an error if the value at the specified stack index is not a number.
 */
LUALIB_API lua_Number luaL_checknumber (lua_State *L, int arg) {
  int isnum;
  lua_Number d = lua_tonumberx(L, arg, &isnum);
  if (!isnum)
    tag_error(L, arg, LUA_TNUMBER);
  return d;
}


/**
 * @brief Retrieves a number from the Lua stack or returns a default value if the argument is missing or nil.
 *
 * This function is used to safely retrieve a numeric value from the Lua stack at the specified index.
 * If the argument at the given index is missing or nil, the function returns the provided default value.
 * If the argument is not a number, it raises a Lua error.
 *
 * @param L The Lua state.
 * @param arg The index of the argument in the Lua stack.
 * @param def The default value to return if the argument is missing or nil.
 * @return The numeric value from the Lua stack or the default value if the argument is missing or nil.
 */
LUALIB_API lua_Number luaL_optnumber (lua_State *L, int arg, lua_Number def) {
  return luaL_opt(L, luaL_checknumber, arg, def);
}


/**
 * Checks if the argument at position `arg` in the Lua stack is a valid integer.
 * If the argument is a number but cannot be represented as an integer, it raises
 * an error with the message "number has no integer representation". If the argument
 * is not a number at all, it raises a type error indicating that a number was expected.
 *
 * @param L The Lua state.
 * @param arg The index of the argument in the Lua stack to check.
 */
static void interror (lua_State *L, int arg) {
  if (lua_isnumber(L, arg))
    luaL_argerror(L, arg, "number has no integer representation");
  else
    tag_error(L, arg, LUA_TNUMBER);
}


/**
 * @brief Retrieves and validates an integer from the Lua stack at the specified index.
 *
 * This function attempts to convert the Lua value at the given stack index `arg` to a Lua integer (`lua_Integer`).
 * If the conversion is successful, the integer value is returned. If the value at the specified index cannot be
 * converted to an integer, an error is raised using `interror`, which typically results in a Lua error being thrown.
 *
 * @param L Pointer to the Lua state.
 * @param arg Index of the value on the Lua stack to be checked and converted.
 * @return The integer value retrieved from the Lua stack.
 * @throws Raises a Lua error if the value at the specified index is not a valid integer.
 */
LUALIB_API lua_Integer luaL_checkinteger (lua_State *L, int arg) {
  int isnum;
  lua_Integer d = lua_tointegerx(L, arg, &isnum);
  if (!isnum) {
    interror(L, arg);
  }
  return d;
}


/**
 * @brief Retrieves an optional integer argument from the Lua stack.
 *
 * This function is used to retrieve an integer argument from the Lua stack at the specified index.
 * If the argument is not present or is `nil`, the function returns the provided default value.
 *
 * @param L A pointer to the Lua state.
 * @param arg The index of the argument on the Lua stack.
 * @param def The default value to return if the argument is `nil` or not present.
 * @return The integer value of the argument at the specified index, or the default value if the argument is `nil` or not present.
 */
LUALIB_API lua_Integer luaL_optinteger (lua_State *L, int arg,
                                                      lua_Integer def) {
  return luaL_opt(L, luaL_checkinteger, arg, def);
}

/* }====================================================== */


/*
** {======================================================
** Generic Buffer manipulation
** =======================================================
*/

/* userdata to box arbitrary data */
typedef struct UBox {
  void *box;
  size_t bsize;
} UBox;


/**
 * Resizes the memory block associated with a userdata object (UBox) in the Lua state.
 * This function retrieves the current allocator function and userdata from the Lua state,
 * then attempts to resize the memory block using the allocator. If the allocation fails
 * (i.e., `temp` is `NULL` and `newsize` is greater than 0), it frees the existing buffer
 * and raises a Lua error indicating insufficient memory. On successful resizing, it
 * updates the UBox structure with the new memory block and size.
 *
 * @param L The Lua state.
 * @param idx The stack index of the userdata object (UBox) to resize.
 * @param newsize The new size of the memory block.
 * @return A pointer to the resized memory block, or `NULL` if `newsize` is 0.
 * @throws Raises a Lua error if memory allocation fails.
 */
static void *resizebox (lua_State *L, int idx, size_t newsize) {
  void *ud;
  lua_Alloc allocf = lua_getallocf(L, &ud);
  UBox *box = (UBox *)lua_touserdata(L, idx);
  void *temp = allocf(ud, box->box, box->bsize, newsize);
  if (temp == NULL && newsize > 0) {  /* allocation error? */
    resizebox(L, idx, 0);  /* free buffer */
    luaL_error(L, "not enough memory for buffer allocation");
  }
  box->box = temp;
  box->bsize = newsize;
  return temp;
}


/**
 * @brief Frees the memory associated with a box object in Lua.
 *
 * This function is a Lua C API method that is typically used as a garbage collection
 * finalizer for a box object. It resizes the box to zero, effectively releasing any
 * allocated memory or resources associated with the box. The function assumes that
 * the box object is located at position 1 on the Lua stack.
 *
 * @param L A pointer to the Lua state.
 * @return Always returns 0, indicating no values are pushed onto the Lua stack.
 */
static int boxgc (lua_State *L) {
  resizebox(L, 1, 0);
  return 0;
}


/**
 * Allocates and initializes a new UBox structure on the Lua stack.
 * 
 * This function creates a new userdata object of type UBox and associates it with a metatable named "LUABOX".
 * If the metatable does not already exist, it is created and the `__gc` metamethod is set to the `boxgc` function,
 * which is responsible for garbage collection of the UBox object. The UBox structure is initialized with `box` set
 * to NULL and `bsize` set to 0. Finally, the function calls `resizebox` to allocate memory of the specified `newsize`
 * for the UBox object and returns a pointer to the allocated memory.
 *
 * @param L The Lua state.
 * @param newsize The size of the memory block to allocate for the UBox object.
 * @return A pointer to the allocated memory block, or NULL if allocation fails.
 */
static void *newbox (lua_State *L, size_t newsize) {
  UBox *box = (UBox *)lua_newuserdata(L, sizeof(UBox));
  box->box = NULL;
  box->bsize = 0;
  if (luaL_newmetatable(L, "LUABOX")) {  /* creating metatable? */
    lua_pushcfunction(L, boxgc);
    lua_setfield(L, -2, "__gc");  /* metatable.__gc = boxgc */
  }
  lua_setmetatable(L, -2);
  return resizebox(L, -1, newsize);
}


/*
** check whether buffer is using a userdata on the stack as a temporary
** buffer
*/
#define buffonstack(B)	((B)->b != (B)->initb)


/*
** returns a pointer to a free area with at least 'sz' bytes
*/
LUALIB_API char *luaL_prepbuffsize (luaL_Buffer *B, size_t sz) {
  lua_State *L = B->L;
  if (B->size - B->n < sz) {  /* not enough space? */
    char *newbuff;
    size_t newsize = B->size * 2;  /* double buffer size */
    if (newsize - B->n < sz)  /* not big enough? */
      newsize = B->n + sz;
    if (newsize < B->n || newsize - B->n < sz)
      luaL_error(L, "buffer too large");
    /* create larger buffer */
    if (buffonstack(B))
      newbuff = (char *)resizebox(L, -1, newsize);
    else {  /* no buffer yet */
      newbuff = (char *)newbox(L, newsize);
      memcpy(newbuff, B->b, B->n * sizeof(char));  /* copy original content */
    }
    B->b = newbuff;
    B->size = newsize;
  }
  return &B->b[B->n];
}


/**
 * Appends a string of a specified length to the buffer managed by `luaL_Buffer`.
 *
 * This function appends the first `l` characters of the string `s` to the buffer `B`. 
 * If `l` is greater than 0, the function ensures that the buffer has enough space to 
 * accommodate the new string by calling `luaL_prepbuffsize`. It then copies the string 
 * into the buffer using `memcpy` and updates the buffer's size by calling `luaL_addsize`.
 *
 * @param B Pointer to the `luaL_Buffer` structure where the string will be appended.
 * @param s Pointer to the string to be appended. This can be NULL if `l` is 0.
 * @param l The number of characters to append from the string `s`. If `l` is 0, 
 *          the function does nothing.
 */
LUALIB_API void luaL_addlstring (luaL_Buffer *B, const char *s, size_t l) {
  if (l > 0) {  /* avoid 'memcpy' when 's' can be NULL */
    char *b = luaL_prepbuffsize(B, l);
    memcpy(b, s, l * sizeof(char));
    luaL_addsize(B, l);
  }
}


/**
 * Appends a null-terminated string to the buffer `B`.
 *
 * This function takes a string `s` and appends its contents to the buffer `B`. 
 * The string `s` must be null-terminated. Internally, it calculates the length 
 * of the string using `strlen` and then calls `luaL_addlstring` to append the 
 * string to the buffer.
 *
 * @param B Pointer to the `luaL_Buffer` structure where the string will be appended.
 * @param s The null-terminated string to be appended to the buffer.
 *
 * @note This function is a convenience wrapper around `luaL_addlstring` for 
 *       null-terminated strings.
 */
LUALIB_API void luaL_addstring (luaL_Buffer *B, const char *s) {
  luaL_addlstring(B, s, strlen(s));
}


/**
 * Pushes the content of the buffer `B` onto the Lua stack as a string and cleans up the buffer.
 * 
 * This function takes a buffer `B` and pushes its content as a string onto the Lua stack using `lua_pushlstring`.
 * If the buffer is on the stack (i.e., it was previously initialized with `luaL_buffinit`), the function
 * resizes the buffer to zero length (effectively deleting the old buffer) and removes its header from the stack.
 * 
 * @param B Pointer to the `luaL_Buffer` structure containing the buffer to be pushed.
 * 
 * @note This function assumes that the buffer `B` has been properly initialized and that the Lua state `B->L` is valid.
 * 
 * @see luaL_buffinit, lua_pushlstring, buffonstack, resizebox, lua_remove
 */
LUALIB_API void luaL_pushresult (luaL_Buffer *B) {
  lua_State *L = B->L;
  lua_pushlstring(L, B->b, B->n);
  if (buffonstack(B)) {
    resizebox(L, -2, 0);  /* delete old buffer */
    lua_remove(L, -2);  /* remove its header from the stack */
  }
}


/**
 * Pushes the contents of the buffer to the Lua stack and finalizes the buffer.
 * This function adjusts the buffer's size by the specified amount (`sz`) and
 * then pushes the accumulated contents of the buffer onto the Lua stack as a
 * string or userdata, depending on the buffer's configuration. After this
 * operation, the buffer is considered finalized and should not be used further
 * without reinitialization.
 *
 * @param B Pointer to the `luaL_Buffer` structure that holds the buffer to be
 *          finalized and pushed.
 * @param sz The size to add to the buffer before finalizing it. This is typically
 *           the length of the data that was added to the buffer since the last
 *           call to `luaL_addsize`.
 */
LUALIB_API void luaL_pushresultsize (luaL_Buffer *B, size_t sz) {
  luaL_addsize(B, sz);
  luaL_pushresult(B);
}


/**
 * Adds the value at the top of the Lua stack to the buffer `B`.
 *
 * This function retrieves the string representation of the value at the top of the Lua stack
 * and appends it to the buffer `B`. If the buffer is currently using the Lua stack for storage,
 * the value is inserted below the buffer on the stack before being added to the buffer. After
 * appending the value, it is removed from the stack.
 *
 * @param B Pointer to the `luaL_Buffer` structure where the value will be appended.
 *
 * @note The function assumes that the value at the top of the stack is convertible to a string.
 *       If the buffer is on the stack, the function ensures that the value is placed below the
 *       buffer to maintain the correct stack order.
 */
LUALIB_API void luaL_addvalue (luaL_Buffer *B) {
  lua_State *L = B->L;
  size_t l;
  const char *s = lua_tolstring(L, -1, &l);
  if (buffonstack(B))
    lua_insert(L, -2);  /* put value below buffer */
  luaL_addlstring(B, s, l);
  lua_remove(L, (buffonstack(B)) ? -2 : -1);  /* remove value */
}


/**
 * Initializes a buffer for use with Lua.
 *
 * This function sets up a `luaL_Buffer` structure for subsequent use with other
 * buffer-related functions in the Lua auxiliary library. It initializes the buffer
 * to use the internal storage provided by the `luaL_Buffer` structure and sets the
 * buffer size to the default value defined by `LUAL_BUFFERSIZE`.
 *
 * @param L Pointer to the Lua state.
 * @param B Pointer to the `luaL_Buffer` structure to be initialized.
 *
 * @note After calling this function, the buffer is ready to be used with functions
 * like `luaL_addvalue`, `luaL_addlstring`, and `luaL_pushresult`.
 */
LUALIB_API void luaL_buffinit (lua_State *L, luaL_Buffer *B) {
  B->L = L;
  B->b = B->initb;
  B->n = 0;
  B->size = LUAL_BUFFERSIZE;
}


/**
 * Initializes a Lua buffer and prepares it to accommodate a specified size.
 * 
 * This function initializes the buffer `B` using `luaL_buffinit` and then prepares
 * the buffer to hold at least `sz` bytes by calling `luaL_prepbuffsize`. It returns
 * a pointer to the buffer's internal storage, which can be used to directly write
 * data into the buffer.
 *
 * @param L The Lua state.
 * @param B The buffer to initialize and prepare.
 * @param sz The minimum size (in bytes) to prepare the buffer for.
 * @return A pointer to the buffer's internal storage, ready for writing.
 */
LUALIB_API char *luaL_buffinitsize (lua_State *L, luaL_Buffer *B, size_t sz) {
  luaL_buffinit(L, B);
  return luaL_prepbuffsize(B, sz);
}

/* }====================================================== */


/*
** {======================================================
** Reference system
** =======================================================
*/

/* index of free-list header */
#define freelist	0


/**
 * Creates a reference to the object at the top of the Lua stack and stores it in the table at index `t`.
 * The referenced object is removed from the stack. If the object is `nil`, a unique fixed reference
 * (`LUA_REFNIL`) is returned. The reference can be used later to retrieve the object using `lua_rawgeti`.
 *
 * The function maintains a freelist within the table to reuse previously freed references, optimizing
 * memory usage. If no free references are available, a new reference is created.
 *
 * @param L The Lua state.
 * @param t The index of the table where the reference will be stored.
 * @return An integer reference to the object, or `LUA_REFNIL` if the object is `nil`.
 */
LUALIB_API int luaL_ref (lua_State *L, int t) {
  int ref;
  if (lua_isnil(L, -1)) {
    lua_pop(L, 1);  /* remove from stack */
    return LUA_REFNIL;  /* 'nil' has a unique fixed reference */
  }
  t = lua_absindex(L, t);
  lua_rawgeti(L, t, freelist);  /* get first free element */
  ref = (int)lua_tointeger(L, -1);  /* ref = t[freelist] */
  lua_pop(L, 1);  /* remove it from stack */
  if (ref != 0) {  /* any free element? */
    lua_rawgeti(L, t, ref);  /* remove it from list */
    lua_rawseti(L, t, freelist);  /* (t[freelist] = t[ref]) */
  }
  else  /* no free elements */
    ref = (int)lua_rawlen(L, t) + 1;  /* get a new reference */
  lua_rawseti(L, t, ref);
  return ref;
}


/**
 * Releases a reference in a Lua table, making the associated value available for reuse.
 * This function is typically used to manage references in a registry or a table that
 * acts as a reference store. When a reference is released, it is added to a freelist
 * within the table, allowing it to be reused for future references.
 *
 * @param L The Lua state.
 * @param t The index of the table in the Lua stack that holds the references.
 * @param ref The reference to be released. If the reference is negative, the function
 *            does nothing. Otherwise, the reference is freed and added to the freelist.
 *
 * The function performs the following steps:
 * 1. If `ref` is non-negative, it converts the table index `t` to an absolute index.
 * 2. It retrieves the current value of the freelist (a special key in the table) and
 *    assigns it to the slot of the released reference (`t[ref] = t[freelist]`).
 * 3. It then updates the freelist to point to the newly released reference
 *    (`t[freelist] = ref`), making it available for future reuse.
 *
 * This mechanism allows efficient management of references, avoiding fragmentation
 * and enabling reuse of previously allocated references.
 */
LUALIB_API void luaL_unref (lua_State *L, int t, int ref) {
  if (ref >= 0) {
    t = lua_absindex(L, t);
    lua_rawgeti(L, t, freelist);
    lua_rawseti(L, t, ref);  /* t[ref] = t[freelist] */
    lua_pushinteger(L, ref);
    lua_rawseti(L, t, freelist);  /* t[freelist] = ref */
  }
}

/* }====================================================== */


/*
** {======================================================
** Load functions
** =======================================================
*/

typedef struct LoadF {
  int n;  /* number of pre-read characters */
  FILE *f;  /* file being read */
  char buff[BUFSIZ];  /* area for reading file */
} LoadF;


/**
 * Reads data from a file or a pre-read buffer and returns it.
 *
 * This function is used to read data from a file or a buffer that has been pre-read. 
 * It is typically used in conjunction with Lua's loading mechanism to read chunks of 
 * data from a file or buffer.
 *
 * @param L The Lua state (unused in this function).
 * @param ud A pointer to user data, expected to be of type `LoadF*`.
 * @param size A pointer to a variable where the size of the read data will be stored.
 *
 * @return A pointer to the buffer containing the read data. If the end of the file is 
 *         reached and no more data is available, it returns `NULL`.
 *
 * The function first checks if there are any pre-read characters in the buffer. If so, 
 * it returns those characters and resets the pre-read count. If no pre-read characters 
 * are available, it reads a block of data from the file using `fread`. If the end of 
 * the file is reached, it returns `NULL`. The size of the read data is stored in the 
 * variable pointed to by `size`.
 */
static const char *getF (lua_State *L, void *ud, size_t *size) {
  LoadF *lf = (LoadF *)ud;
  (void)L;  /* not used */
  if (lf->n > 0) {  /* are there pre-read characters to be read? */
    *size = lf->n;  /* return them (chars already in buffer) */
    lf->n = 0;  /* no more pre-read characters */
  }
  else {  /* read a block from file */
    /* 'fread' can return > 0 *and* set the EOF flag. If next call to
       'getF' called 'fread', it might still wait for user input.
       The next check avoids this problem. */
    if (feof(lf->f)) return NULL;
    *size = fread(lf->buff, 1, sizeof(lf->buff), lf->f);  /* read block */
  }
  return lf->buff;
}


/**
 * Generates and pushes an error message to the Lua stack when a file operation fails.
 *
 * This function constructs an error message using the provided operation description (`what`),
 * the filename obtained from the Lua stack at the specified index (`fnameindex`), and the system
 * error message corresponding to the current value of `errno`. The error message is formatted as:
 * "cannot <what> <filename>: <system error message>".
 *
 * After pushing the error message onto the Lua stack, the function removes the filename from the
 * stack at the specified index to clean up the stack state.
 *
 * @param L The Lua state pointer.
 * @param what A string describing the operation that failed (e.g., "open", "read").
 * @param fnameindex The stack index of the filename string in the Lua stack.
 * @return Returns `LUA_ERRFILE` to indicate a file-related error.
 */
static int errfile (lua_State *L, const char *what, int fnameindex) {
  const char *serr = strerror(errno);
  const char *filename = lua_tostring(L, fnameindex) + 1;
  lua_pushfstring(L, "cannot %s %s: %s", what, filename, serr);
  lua_remove(L, fnameindex);
  return LUA_ERRFILE;
}


/**
 * Skips the Byte Order Mark (BOM) at the beginning of a file if it exists.
 * The BOM is a special marker at the start of a text stream that indicates
 * the encoding is UTF-8. This method reads the first few bytes of the file
 * and checks if they match the UTF-8 BOM sequence (0xEF, 0xBB, 0xBF).
 * If the BOM is found, it is skipped, and the next character after the BOM
 * is returned. If the BOM is not found, the first character of the file is
 * returned.
 *
 * @param lf A pointer to a LoadF structure containing the file to be read.
 * @return The first character after the BOM if it exists, or the first
 *         character of the file if the BOM is not present. Returns EOF if
 *         the file is empty or an error occurs.
 */
static int skipBOM (LoadF *lf) {
  const char *p = "\xEF\xBB\xBF";  /* UTF-8 BOM mark */
  int c;
  lf->n = 0;
  do {
    c = getc(lf->f);
    if (c == EOF || c != *(const unsigned char *)p++) return c;
    lf->buff[lf->n++] = c;  /* to be read by the parser */
  } while (*p != '\0');
  lf->n = 0;  /* prefix matched; discard it */
  return getc(lf->f);  /* return next character */
}


/*
** reads the first character of file 'f' and skips an optional BOM mark
** in its beginning plus its first line if it starts with '#'. Returns
** true if it skipped the first line.  In any case, '*cp' has the
** first "valid" character of the file (after the optional BOM and
** a first-line comment).
*/
static int skipcomment (LoadF *lf, int *cp) {
  int c = *cp = skipBOM(lf);
  if (c == '#') {  /* first line is a comment (Unix exec. file)? */
    do {  /* skip first line */
      c = getc(lf->f);
    } while (c != EOF && c != '\n');
    *cp = getc(lf->f);  /* skip end-of-line, if present */
    return 1;  /* there was a comment */
  }
  else return 0;  /* no comment */
}


/**
 * Loads and compiles a Lua chunk from a file.
 *
 * This function opens the specified file, reads its contents, and compiles it into a Lua chunk.
 * The chunk is then pushed onto the Lua stack as a function. If the file is a binary Lua file (indicated
 * by the Lua signature), it is reopened in binary mode. The function handles both text and binary Lua files.
 *
 * @param L The Lua state.
 * @param filename The name of the file to load. If NULL, the function reads from standard input (stdin).
 * @param mode A string that controls whether the file is loaded as text or binary. It can be "t" for text,
 *             "b" for binary, or NULL for the default behavior.
 *
 * @return Returns LUA_OK if the file is successfully loaded and compiled. If an error occurs, it returns
 *         one of the following error codes:
 *         - LUA_ERRFILE: If the file cannot be opened or read.
 *         - LUA_ERRSYNTAX: If there is a syntax error in the Lua code.
 *         - LUA_ERRMEM: If there is a memory allocation error.
 *
 * @note The function pushes the filename onto the Lua stack before loading the file. After loading, the
 *       filename is removed from the stack. If the file is read from stdin, the string "=stdin" is pushed
 *       onto the stack instead of a filename.
 */
LUALIB_API int luaL_loadfilex (lua_State *L, const char *filename,
                                             const char *mode) {
  LoadF lf;
  int status, readstatus;
  int c;
  int fnameindex = lua_gettop(L) + 1;  /* index of filename on the stack */
  if (filename == NULL) {
    lua_pushliteral(L, "=stdin");
    lf.f = stdin;
  }
  else {
    lua_pushfstring(L, "@%s", filename);
    lf.f = fopen(filename, "r");
    if (lf.f == NULL) return errfile(L, "open", fnameindex);
  }
  if (skipcomment(&lf, &c))  /* read initial portion */
    lf.buff[lf.n++] = '\n';  /* add line to correct line numbers */
  if (c == LUA_SIGNATURE[0] && filename) {  /* binary file? */
    lf.f = freopen(filename, "rb", lf.f);  /* reopen in binary mode */
    if (lf.f == NULL) return errfile(L, "reopen", fnameindex);
    skipcomment(&lf, &c);  /* re-read initial portion */
  }
  if (c != EOF)
    lf.buff[lf.n++] = c;  /* 'c' is the first character of the stream */
  status = lua_load(L, getF, &lf, lua_tostring(L, -1), mode);
  readstatus = ferror(lf.f);
  if (filename) fclose(lf.f);  /* close file (even in case of errors) */
  if (readstatus) {
    lua_settop(L, fnameindex);  /* ignore results from 'lua_load' */
    return errfile(L, "read", fnameindex);
  }
  lua_remove(L, fnameindex);
  return status;
}


typedef struct LoadS {
  const char *s;
  size_t size;
} LoadS;


/**
 * Retrieves a string from a LoadS structure and updates its size.
 *
 * This function is typically used in conjunction with Lua's loading mechanism to 
 * provide a string from a custom data source. It takes a Lua state (which is unused 
 * in this implementation), a pointer to user data (expected to be of type LoadS), 
 * and a pointer to a size_t variable to store the size of the string.
 *
 * @param L The Lua state (unused in this function).
 * @param ud A pointer to user data, expected to be of type LoadS.
 * @param size A pointer to a size_t variable where the size of the string will be stored.
 * @return A pointer to the string if available, or NULL if the string size is zero.
 *
 * The function checks if the size of the string in the LoadS structure is zero. If it is, 
 * the function returns NULL. Otherwise, it sets the size of the string in the provided 
 * size_t variable, resets the size in the LoadS structure to zero, and returns a pointer 
 * to the string.
 */
static const char *getS (lua_State *L, void *ud, size_t *size) {
  LoadS *ls = (LoadS *)ud;
  (void)L;  /* not used */
  if (ls->size == 0) return NULL;
  *size = ls->size;
  ls->size = 0;
  return ls->s;
}


/**
 * Loads a Lua chunk from a buffer into the Lua state.
 *
 * This function loads a Lua chunk from the specified buffer `buff` of size `size`
 * into the Lua state `L`. The chunk is associated with the given `name`, which is
 * used for error messages and debugging information. The `mode` parameter controls
 * how the chunk is loaded, allowing for text, binary, or both formats.
 *
 * @param L The Lua state in which to load the chunk.
 * @param buff The buffer containing the Lua chunk to load.
 * @param size The size of the buffer in bytes.
 * @param name The name associated with the chunk, used for error messages and debugging.
 * @param mode A string specifying the loading mode ("t" for text, "b" for binary, or "bt" for both).
 * @return Returns 0 on success, or a non-zero error code if the chunk could not be loaded.
 *         The error code can be used with `lua_error` to generate an error message.
 */
LUALIB_API int luaL_loadbufferx (lua_State *L, const char *buff, size_t size,
                                 const char *name, const char *mode) {
  LoadS ls;
  ls.s = buff;
  ls.size = size;
  return lua_load(L, getS, &ls, name, mode);
}


/**
 * Loads a Lua chunk from a string.
 *
 * This function compiles the Lua code contained in the string `s` and pushes the resulting
 * chunk as a Lua function onto the stack. If there are no errors, the function returns 0.
 * If there is an error (e.g., a syntax error in the string), the function pushes an error
 * message onto the stack and returns a non-zero error code.
 *
 * @param L The Lua state.
 * @param s The string containing the Lua code to be compiled.
 * @return 0 on success, or a non-zero error code on failure.
 */
LUALIB_API int luaL_loadstring (lua_State *L, const char *s) {
  return luaL_loadbuffer(L, s, strlen(s), s);
}

/* }====================================================== */



LUALIB_API int luaL_getmetafield (lua_State *L, int obj, const char *event) {
  if (!lua_getmetatable(L, obj))  /* no metatable? */
    return LUA_TNIL;
  else {
    int tt;
    lua_pushstring(L, event);
    tt = lua_rawget(L, -2);
    if (tt == LUA_TNIL)  /* is metafield nil? */
      lua_pop(L, 2);  /* remove metatable and metafield */
    else
      lua_remove(L, -2);  /* remove only metatable */
    return tt;  /* return metafield type */
  }
}


/**
 * Invokes a metamethod for a given object and event.
 *
 * This function attempts to call a metamethod associated with the specified object and event.
 * If the object has a metatable and the metatable contains a field corresponding to the event,
 * the function pushes the object onto the stack and calls the metamethod with the object as its
 * argument. The result of the metamethod call is left on the stack.
 *
 * @param L The Lua state.
 * @param obj The index of the object in the Lua stack.
 * @param event The name of the metamethod to call (e.g., "__index", "__add", etc.).
 * @return Returns 1 if the metamethod was successfully called, otherwise returns 0 if the
 *         metafield does not exist or is nil.
 */
LUALIB_API int luaL_callmeta (lua_State *L, int obj, const char *event) {
  obj = lua_absindex(L, obj);
  if (luaL_getmetafield(L, obj, event) == LUA_TNIL)  /* no metafield? */
    return 0;
  lua_pushvalue(L, obj);
  lua_call(L, 1, 1);
  return 1;
}


/**
 * Retrieves the length of the object at the given index on the Lua stack.
 * 
 * This function first calls `lua_len` to compute the length of the object at index `idx` 
 * and pushes the result onto the stack. It then attempts to convert the result to an 
 * integer using `lua_tointegerx`. If the conversion fails (i.e., the length is not an integer), 
 * it raises a Lua error with the message "object length is not an integer". Finally, it removes 
 * the length value from the stack and returns the integer length.
 *
 * @param L A pointer to the Lua state.
 * @param idx The stack index of the object whose length is to be retrieved.
 * @return The length of the object as a Lua integer.
 * @throws Raises a Lua error if the object's length is not an integer.
 */
LUALIB_API lua_Integer luaL_len (lua_State *L, int idx) {
  lua_Integer l;
  int isnum;
  lua_len(L, idx);
  l = lua_tointegerx(L, -1, &isnum);
  if (!isnum)
    luaL_error(L, "object length is not an integer");
  lua_pop(L, 1);  /* remove object */
  return l;
}


/**
 * Converts the Lua value at the given index `idx` on the stack to a string representation.
 * 
 * This function first checks if the value has a metatable with a `__tostring` metamethod. If so,
 * it calls the metamethod and ensures it returns a string. If no `__tostring` metamethod is found,
 * the function converts the value to a string based on its type:
 * - Numbers are formatted as integers or floating-point numbers.
 * - Strings are pushed directly.
 * - Booleans are converted to "true" or "false".
 * - `nil` is converted to "nil".
 * - For other types (e.g., tables, functions), the function attempts to use the `__name` metamethod
 *   to get a descriptive name, or falls back to the type name, and appends the object's address.
 * 
 * The resulting string is placed on the top of the stack, and a pointer to this string is returned.
 * If `len` is not `NULL`, the length of the string is stored in `*len`.
 * 
 * @param L The Lua state.
 * @param idx The index of the value on the stack to convert to a string.
 * @param len A pointer to store the length of the resulting string (optional, can be `NULL`).
 * @return A pointer to the string representation of the value.
 */
LUALIB_API const char *luaL_tolstring (lua_State *L, int idx, size_t *len) {
  if (luaL_callmeta(L, idx, "__tostring")) {  /* metafield? */
    if (!lua_isstring(L, -1))
      luaL_error(L, "'__tostring' must return a string");
  }
  else {
    switch (lua_type(L, idx)) {
      case LUA_TNUMBER: {
        if (lua_isinteger(L, idx))
          lua_pushfstring(L, "%I", (LUAI_UACINT)lua_tointeger(L, idx));
        else
          lua_pushfstring(L, "%f", (LUAI_UACNUMBER)lua_tonumber(L, idx));
        break;
      }
      case LUA_TSTRING:
        lua_pushvalue(L, idx);
        break;
      case LUA_TBOOLEAN:
        lua_pushstring(L, (lua_toboolean(L, idx) ? "true" : "false"));
        break;
      case LUA_TNIL:
        lua_pushliteral(L, "nil");
        break;
      default: {
        int tt = luaL_getmetafield(L, idx, "__name");  /* try name */
        const char *kind = (tt == LUA_TSTRING) ? lua_tostring(L, -1) :
                                                 luaL_typename(L, idx);
        lua_pushfstring(L, "%s: %p", kind, lua_topointer(L, idx));
        if (tt != LUA_TNIL)
          lua_remove(L, -2);  /* remove '__name' */
        break;
      }
    }
  }
  return lua_tolstring(L, -1, len);
}


/*
** {======================================================
** Compatibility with 5.1 module functions
** =======================================================
*/
#if defined(LUA_COMPAT_MODULE)

/**
 * Searches for or creates a nested table within a given table in the Lua state.
 *
 * This function traverses a table hierarchy in the Lua state `L`, starting from the table at index `idx`.
 * It processes the fully qualified name `fname`, which is a dot-separated string representing the path
 * to the desired nested table. If any intermediate table in the path does not exist, it is created.
 *
 * @param L The Lua state in which to operate.
 * @param idx The stack index of the starting table. If `idx` is 0, the function starts from the global table.
 * @param fname The fully qualified name of the nested table, using dots (`.`) as separators.
 * @param szhint A hint for the size of newly created tables. This is used to optimize table creation.
 *
 * @return Returns `NULL` if the nested table is successfully found or created. If a non-table value is
 *         encountered during traversal, the function returns the problematic part of the name (the substring
 *         starting from the point of failure).
 *
 * Example:
 * If `fname` is "a.b.c" and `idx` refers to the global table, this function will ensure that the tables
 * `_G.a`, `_G.a.b`, and `_G.a.b.c` exist, creating them if necessary.
 */
static const char *luaL_findtable (lua_State *L, int idx,
                                   const char *fname, int szhint) {
  const char *e;
  if (idx) lua_pushvalue(L, idx);
  do {
    e = strchr(fname, '.');
    if (e == NULL) e = fname + strlen(fname);
    lua_pushlstring(L, fname, e - fname);
    if (lua_rawget(L, -2) == LUA_TNIL) {  /* no such field? */
      lua_pop(L, 1);  /* remove this nil */
      lua_createtable(L, 0, (*e == '.' ? 1 : szhint)); /* new table for field */
      lua_pushlstring(L, fname, e - fname);
      lua_pushvalue(L, -2);
      lua_settable(L, -4);  /* set new table into field */
    }
    else if (!lua_istable(L, -1)) {  /* field has a non-table value? */
      lua_pop(L, 2);  /* remove table and value */
      return fname;  /* return problematic part of the name */
    }
    lua_remove(L, -2);  /* remove previous table */
    fname = e + 1;
  } while (*e == '.');
  return NULL;
}


/*
** Count number of elements in a luaL_Reg list.
*/
static int libsize (const luaL_Reg *l) {
  int size = 0;
  for (; l && l->name; l++) size++;
  return size;
}


/*
** Find or create a module table with a given name. The function
** first looks at the LOADED table and, if that fails, try a
** global variable with that name. In any case, leaves on the stack
** the module table.
*/
LUALIB_API void luaL_pushmodule (lua_State *L, const char *modname,
                                 int sizehint) {
  luaL_findtable(L, LUA_REGISTRYINDEX, LUA_LOADED_TABLE, 1);
  if (lua_getfield(L, -1, modname) != LUA_TTABLE) {  /* no LOADED[modname]? */
    lua_pop(L, 1);  /* remove previous result */
    /* try global variable (and create one if it does not exist) */
    lua_pushglobaltable(L);
    if (luaL_findtable(L, 0, modname, sizehint) != NULL)
      luaL_error(L, "name conflict for module '%s'", modname);
    lua_pushvalue(L, -1);
    lua_setfield(L, -3, modname);  /* LOADED[modname] = new table */
  }
  lua_remove(L, -2);  /* remove LOADED table */
}


/**
 * Opens a Lua library by registering its functions into a new or existing table.
 *
 * This function is used to create or open a library in the Lua state `L`. The library's
 * functions are defined by the `luaL_Reg` array `l`, which contains function names and
 * their corresponding C function pointers. The library table is created or retrieved
 * using the name `libname`. If `libname` is `NULL`, the functions are registered directly
 * into the global table.
 *
 * The `nup` parameter specifies the number of upvalues to be passed to each function in
 * the library. These upvalues are expected to be on the stack when this function is called.
 * If `nup` is greater than 0, the library table is moved below the upvalues on the stack
 * before registering the functions.
 *
 * If `l` is `NULL`, the function removes the `nup` upvalues from the stack without
 * registering any functions.
 *
 * @param L The Lua state in which to open the library.
 * @param libname The name of the library. If `NULL`, functions are registered globally.
 * @param l An array of `luaL_Reg` structures defining the library's functions.
 * @param nup The number of upvalues to pass to each function in the library.
 */
LUALIB_API void luaL_openlib (lua_State *L, const char *libname,
                               const luaL_Reg *l, int nup) {
  luaL_checkversion(L);
  if (libname) {
    luaL_pushmodule(L, libname, libsize(l));  /* get/create library table */
    lua_insert(L, -(nup + 1));  /* move library table to below upvalues */
  }
  if (l)
    luaL_setfuncs(L, l, nup);
  else
    lua_pop(L, nup);  /* remove upvalues */
}

#endif
/* }====================================================== */

/*
** set functions from list 'l' into table at top - 'nup'; each
** function gets the 'nup' elements at the top as upvalues.
** Returns with only the table at the stack.
*/
LUALIB_API void luaL_setfuncs (lua_State *L, const luaL_Reg *l, int nup) {
  luaL_checkstack(L, nup, "too many upvalues");
  for (; l->name != NULL; l++) {  /* fill the table with given functions */
    int i;
    for (i = 0; i < nup; i++)  /* copy upvalues to the top */
      lua_pushvalue(L, -nup);
    lua_pushcclosure(L, l->func, nup);  /* closure with those upvalues */
    lua_setfield(L, -(nup + 2), l->name);
  }
  lua_pop(L, nup);  /* remove upvalues */
}


/*
** ensure that stack[idx][fname] has a table and push that table
** into the stack
*/
LUALIB_API int luaL_getsubtable (lua_State *L, int idx, const char *fname) {
  if (lua_getfield(L, idx, fname) == LUA_TTABLE)
    return 1;  /* table already there */
  else {
    lua_pop(L, 1);  /* remove previous result */
    idx = lua_absindex(L, idx);
    lua_newtable(L);
    lua_pushvalue(L, -1);  /* copy to be left at top */
    lua_setfield(L, idx, fname);  /* assign new table to field */
    return 0;  /* false, because did not find table there */
  }
}


/*
** Stripped-down 'require': After checking "loaded" table, calls 'openf'
** to open a module, registers the result in 'package.loaded' table and,
** if 'glb' is true, also registers the result in the global table.
** Leaves resulting module on the top.
*/
LUALIB_API void luaL_requiref (lua_State *L, const char *modname,
                               lua_CFunction openf, int glb) {
  luaL_getsubtable(L, LUA_REGISTRYINDEX, LUA_LOADED_TABLE);
  lua_getfield(L, -1, modname);  /* LOADED[modname] */
  if (!lua_toboolean(L, -1)) {  /* package not already loaded? */
    lua_pop(L, 1);  /* remove field */
    lua_pushcfunction(L, openf);
    lua_pushstring(L, modname);  /* argument to open function */
    lua_call(L, 1, 1);  /* call 'openf' to open module */
    lua_pushvalue(L, -1);  /* make copy of module (call result) */
    lua_setfield(L, -3, modname);  /* LOADED[modname] = module */
  }
  lua_remove(L, -2);  /* remove LOADED table */
  if (glb) {
    lua_pushvalue(L, -1);  /* copy of module */
    lua_setglobal(L, modname);  /* _G[modname] = module */
  }
}


/**
 * Replaces all occurrences of the pattern `p` in the string `s` with the replacement string `r`.
 * 
 * This function iterates over the input string `s`, searching for the pattern `p`. Each time `p` is found,
 * it replaces it with `r` and continues processing the remaining part of the string. The result is a new string
 * with all occurrences of `p` replaced by `r`.
 * 
 * The function uses a Lua buffer to efficiently build the resulting string. The buffer is initialized, and
 * the string is constructed by appending the parts of `s` before and after each occurrence of `p`, along with
 * the replacement string `r`. Finally, the buffer is converted to a Lua string and pushed onto the Lua stack.
 * 
 * @param L The Lua state.
 * @param s The input string in which to search for the pattern.
 * @param p The pattern to search for in the string `s`.
 * @param r The replacement string to substitute for each occurrence of `p`.
 * @return Returns a pointer to the resulting string after all replacements have been made. The string is
 *         also pushed onto the Lua stack.
 */
LUALIB_API const char *luaL_gsub (lua_State *L, const char *s, const char *p,
                                                               const char *r) {
  const char *wild;
  size_t l = strlen(p);
  luaL_Buffer b;
  luaL_buffinit(L, &b);
  while ((wild = strstr(s, p)) != NULL) {
    luaL_addlstring(&b, s, wild - s);  /* push prefix */
    luaL_addstring(&b, r);  /* push replacement in place of pattern */
    s = wild + l;  /* continue after 'p' */
  }
  luaL_addstring(&b, s);  /* push last suffix */
  luaL_pushresult(&b);
  return lua_tostring(L, -1);
}


/**
 * Allocates, reallocates, or deallocates memory based on the provided parameters.
 *
 * This function serves as a custom memory allocator. It can be used to allocate new memory,
 * resize existing memory, or free memory. The function ignores the `ud` and `osize` parameters,
 * as they are not used in the implementation.
 *
 * @param ud   A user-defined pointer (unused in this implementation).
 * @param ptr  A pointer to the memory block to be reallocated or freed. If `nsize` is 0,
 *             this pointer is freed, and NULL is returned.
 * @param osize The old size of the memory block (unused in this implementation).
 * @param nsize The new size of the memory block. If `nsize` is 0, the memory block is freed.
 *              Otherwise, the memory block is reallocated to the new size.
 *
 * @return If `nsize` is 0, the function returns NULL after freeing the memory block.
 *         Otherwise, it returns a pointer to the reallocated memory block. If reallocation
 *         fails, NULL is returned, and the original memory block remains unchanged.
 */
static void *l_alloc (void *ud, void *ptr, size_t osize, size_t nsize) {
  (void)ud; (void)osize;  /* not used */
  if (nsize == 0) {
    free(ptr);
    return NULL;
  }
  else
    return realloc(ptr, nsize);
}


/**
 * Handles a panic situation in the Lua API by printing an error message and 
 * aborting the execution. This function is typically invoked when an 
 * unprotected error occurs during a Lua API call. The error message is 
 * retrieved from the top of the Lua stack and printed to the standard error 
 * stream. After printing the message, the function returns control to Lua, 
 * which will abort the execution.
 *
 * @param L The Lua state in which the panic occurred.
 * @return Always returns 0, indicating to Lua that it should abort execution.
 */
static int panic (lua_State *L) {
  lua_writestringerror("PANIC: unprotected error in call to Lua API (%s)\n",
                        lua_tostring(L, -1));
  return 0;  /* return to Lua to abort */
}


/**
 * Creates and initializes a new Lua state.
 *
 * This function creates a new Lua state using the default allocator (`l_alloc`) 
 * and sets a panic function (`panic`) to handle unrecoverable errors. The panic 
 * function is invoked when Lua encounters a critical error, such as a memory 
 * allocation failure.
 *
 * @return A pointer to the newly created Lua state (`lua_State*`). If the state 
 *         cannot be created (e.g., due to memory allocation failure), the function 
 *         returns `NULL`.
 */
LUALIB_API lua_State *luaL_newstate (void) {
  lua_State *L = lua_newstate(l_alloc, NULL);
  if (L) lua_atpanic(L, &panic);
  return L;
}


/**
 * @brief Checks the compatibility between the Lua core and the library.
 *
 * This function verifies that the Lua core and the library are compatible in terms of numeric types and version numbers.
 * It ensures that the numeric types used by the core and the library are the same size, and that the version of the Lua
 * core matches the version expected by the application. If any incompatibility is detected, an error is raised.
 *
 * @param L Pointer to the Lua state.
 * @param ver The expected version number of the Lua core.
 * @param sz The size of the numeric types used by the library.
 *
 * @throws Raises a Lua error if:
 *         - The numeric types used by the core and the library are incompatible.
 *         - Multiple Lua VMs are detected.
 *         - The version of the Lua core does not match the expected version.
 */
LUALIB_API void luaL_checkversion_ (lua_State *L, lua_Number ver, size_t sz) {
  const lua_Number *v = lua_version(L);
  if (sz != LUAL_NUMSIZES)  /* check numeric types */
    luaL_error(L, "core and library have incompatible numeric types");
  if (v != lua_version(NULL))
    luaL_error(L, "multiple Lua VMs detected");
  else if (*v != ver)
    luaL_error(L, "version mismatch: app. needs %f, Lua core provides %f",
                  (LUAI_UACNUMBER)ver, (LUAI_UACNUMBER)*v);
}

