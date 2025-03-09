/*
** $Id: lbaselib.c,v 1.314 2016/09/05 19:06:34 roberto Exp $
** Basic library
** See Copyright Notice in lua.h
*/

#define lbaselib_c
#define LUA_LIB

#include "lprefix.h"


#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lua.h"

#include "lauxlib.h"
#include "lualib.h"


/**
 * Prints the values passed as arguments to the Lua stack, separated by tabs.
 * This function mimics the behavior of Lua's built-in `print` function.
 * It retrieves the global `tostring` function to convert each argument to a string,
 * then writes the resulting strings to the output, followed by a newline.
 *
 * @param L The Lua state, which contains the stack with the values to be printed.
 * @return Always returns 0, indicating no errors occurred during execution.
 *
 * @details The function performs the following steps:
 * 1. Determines the number of arguments on the stack using `lua_gettop`.
 * 2. Retrieves the global `tostring` function to convert arguments to strings.
 * 3. Iterates over each argument, converts it to a string using `tostring`, and writes it to the output.
 * 4. Separates multiple arguments with a tab character.
 * 5. Writes a newline character after all arguments have been printed.
 * 6. Returns 0 to indicate successful execution.
 *
 * @note If the `tostring` function fails to return a string for any argument,
 * the function raises a Lua error with the message "'tostring' must return a string to 'print'".
 */
static int luaB_print (lua_State *L) {
  int n = lua_gettop(L);  /* number of arguments */
  int i;
  lua_getglobal(L, "tostring");
  for (i=1; i<=n; i++) {
    const char *s;
    size_t l;
    lua_pushvalue(L, -1);  /* function to be called */
    lua_pushvalue(L, i);   /* value to print */
    lua_call(L, 1, 1);
    s = lua_tolstring(L, -1, &l);  /* get result */
    if (s == NULL)
      return luaL_error(L, "'tostring' must return a string to 'print'");
    if (i>1) lua_writestring("\t", 1);
    lua_writestring(s, l);
    lua_pop(L, 1);  /* pop result */
  }
  lua_writeline();
  return 0;
}


#define SPACECHARS	" \f\n\r\t\v"

/**
 * Converts a string representation of a number to a Lua integer.
 *
 * This function parses a string `s` that represents a number in a given `base` and
 * converts it to a Lua integer, storing the result in `pn`. The function handles
 * optional leading spaces, an optional sign ('+' or '-'), and skips trailing spaces
 * after the number. The number can be represented in any base from 2 to 36, where
 * digits greater than 9 are represented by letters ('A' to 'Z' or 'a' to 'z').
 *
 * @param s     The string to be converted. It can contain leading and trailing spaces.
 * @param base  The base of the number system (2 to 36) in which the string is represented.
 * @param pn    A pointer to a `lua_Integer` where the result of the conversion will be stored.
 *
 * @return      A pointer to the first character in the string after the parsed number.
 *              If the string does not contain a valid number in the specified base,
 *              the function returns `NULL` and does not modify `pn`.
 */
static const char *b_str2int (const char *s, int base, lua_Integer *pn) {
  lua_Unsigned n = 0;
  int neg = 0;
  s += strspn(s, SPACECHARS);  /* skip initial spaces */
  if (*s == '-') { s++; neg = 1; }  /* handle signal */
  else if (*s == '+') s++;
  if (!isalnum((unsigned char)*s))  /* no digit? */
    return NULL;
  do {
    int digit = (isdigit((unsigned char)*s)) ? *s - '0'
                   : (toupper((unsigned char)*s) - 'A') + 10;
    if (digit >= base) return NULL;  /* invalid numeral */
    n = n * base + digit;
    s++;
  } while (isalnum((unsigned char)*s));
  s += strspn(s, SPACECHARS);  /* skip trailing spaces */
  *pn = (lua_Integer)((neg) ? (0u - n) : n);
  return s;
}


/**
 * Converts the Lua value at the given index to a number.
 *
 * This function attempts to convert the Lua value at index 1 to a number. If the value is already
 * a number, it is returned directly. If the value is a string, it is converted to a number. If the
 * conversion fails, `nil` is returned.
 *
 * If a second argument is provided, it is treated as the base for the conversion (must be between
 * 2 and 36). The function then attempts to convert the string at index 1 to an integer using the
 * specified base. If the conversion fails, `nil` is returned.
 *
 * @param L The Lua state.
 * @return Returns 1, pushing the converted number or `nil` onto the stack.
 */
static int luaB_tonumber (lua_State *L) {
  if (lua_isnoneornil(L, 2)) {  /* standard conversion? */
    luaL_checkany(L, 1);
    if (lua_type(L, 1) == LUA_TNUMBER) {  /* already a number? */
      lua_settop(L, 1);  /* yes; return it */
      return 1;
    }
    else {
      size_t l;
      const char *s = lua_tolstring(L, 1, &l);
      if (s != NULL && lua_stringtonumber(L, s) == l + 1)
        return 1;  /* successful conversion to number */
      /* else not a number */
    }
  }
  else {
    size_t l;
    const char *s;
    lua_Integer n = 0;  /* to avoid warnings */
    lua_Integer base = luaL_checkinteger(L, 2);
    luaL_checktype(L, 1, LUA_TSTRING);  /* no numbers as strings */
    s = lua_tolstring(L, 1, &l);
    luaL_argcheck(L, 2 <= base && base <= 36, 2, "base out of range");
    if (b_str2int(s, (int)base, &n) == s + l) {
      lua_pushinteger(L, n);
      return 1;
    }  /* else not a number */
  }  /* else not a number */
  lua_pushnil(L);  /* not a number */
  return 1;
}


/**
 * Raises an error in the Lua environment. This function is designed to be called from Lua code
 * and is typically used to handle errors in a controlled manner. It allows specifying an error
 * message and an optional stack level to include additional context in the error message.
 *
 * @param L The Lua state in which the error is raised.
 * @return Always returns 0, as `lua_error` does not return to the caller.
 *
 * The function first retrieves an optional integer argument `level` (defaulting to 1) which
 * specifies the stack level to include in the error message. It then ensures that the stack
 * contains only the error message (the first argument). If the error message is a string and
 * the level is greater than 0, the function appends additional context (such as the file and
 * line number) to the error message by calling `luaL_where`. Finally, the function raises the
 * error using `lua_error`.
 *
 * Example usage in Lua:
 *   error("Something went wrong", 2)  -- Raises an error with context from stack level 2
 */
static int luaB_error (lua_State *L) {
  int level = (int)luaL_optinteger(L, 2, 1);
  lua_settop(L, 1);
  if (lua_type(L, 1) == LUA_TSTRING && level > 0) {
    luaL_where(L, level);   /* add extra information */
    lua_pushvalue(L, 1);
    lua_concat(L, 2);
  }
  return lua_error(L);
}


/**
 * Retrieves the metatable of the object at the top of the Lua stack.
 * 
 * This function first checks if the object at the top of the Lua stack has a metatable.
 * If the object does not have a metatable, it pushes `nil` onto the stack and returns.
 * If the object has a metatable, it then checks if the metatable has a field named "__metatable".
 * If the "__metatable" field exists, it pushes the value of this field onto the stack.
 * If the "__metatable" field does not exist, it pushes the metatable itself onto the stack.
 * 
 * @param L The Lua state.
 * @return Always returns 1, indicating that one value (either the metatable, the "__metatable" field, or `nil`) has been pushed onto the stack.
 */
static int luaB_getmetatable (lua_State *L) {
  luaL_checkany(L, 1);
  if (!lua_getmetatable(L, 1)) {
    lua_pushnil(L);
    return 1;  /* no metatable */
  }
  luaL_getmetafield(L, 1, "__metatable");
  return 1;  /* returns either __metatable field (if present) or metatable */
}


/**
 * Sets the metatable for a table.
 *
 * This function sets the metatable for the table at index 1 in the Lua stack. 
 * The metatable to be set is expected to be at index 2 in the stack. 
 * The function first checks that the object at index 1 is a table. 
 * It then verifies that the object at index 2 is either `nil` or a table. 
 * If the table at index 1 has a protected metatable (i.e., it has a `__metatable` field), 
 * the function raises an error and does not allow the metatable to be changed. 
 * If all checks pass, the metatable is set for the table at index 1, 
 * and the function returns 1 to indicate success.
 *
 * @param L The Lua state.
 * @return Returns 1 on success. Raises an error if the metatable cannot be set.
 */
static int luaB_setmetatable (lua_State *L) {
  int t = lua_type(L, 2);
  luaL_checktype(L, 1, LUA_TTABLE);
  luaL_argcheck(L, t == LUA_TNIL || t == LUA_TTABLE, 2,
                    "nil or table expected");
  if (luaL_getmetafield(L, 1, "__metatable") != LUA_TNIL)
    return luaL_error(L, "cannot change a protected metatable");
  lua_settop(L, 2);
  lua_setmetatable(L, 1);
  return 1;
}


/**
 * Compares two values in the Lua stack for raw equality, without invoking any
 * metamethods. This function checks if the values at index 1 and index 2 in the
 * Lua stack are of the same type and have the same value.
 *
 * @param L Pointer to the Lua state.
 * @return 1, pushing a boolean result onto the stack: `true` if the values are
 *         equal, `false` otherwise.
 *
 * @note This function uses `lua_rawequal` internally, which performs a direct
 *       comparison without calling any metamethods (e.g., `__eq`).
 * @see lua_rawequal
 */
static int luaB_rawequal (lua_State *L) {
  luaL_checkany(L, 1);
  luaL_checkany(L, 2);
  lua_pushboolean(L, lua_rawequal(L, 1, 2));
  return 1;
}


/**
 * Pushes the raw length of a table or string onto the Lua stack.
 *
 * This function checks if the first argument on the Lua stack is either a table or a string.
 * If the argument is of the correct type, it retrieves the raw length of the table or string
 * using `lua_rawlen` and pushes the result as an integer onto the stack.
 *
 * @param L The Lua state.
 * @return Returns 1, indicating that one value (the length) has been pushed onto the stack.
 * @throws Lua error if the first argument is not a table or string.
 */
static int luaB_rawlen (lua_State *L) {
  int t = lua_type(L, 1);
  luaL_argcheck(L, t == LUA_TTABLE || t == LUA_TSTRING, 1,
                   "table or string expected");
  lua_pushinteger(L, lua_rawlen(L, 1));
  return 1;
}


/**
 * Retrieves the value associated with a key from a table without invoking any metamethods.
 *
 * This function expects two arguments on the Lua stack:
 * 1. A table (must be of type LUA_TTABLE).
 * 2. A key (can be of any type).
 *
 * The function performs a raw get operation on the table, meaning it directly accesses the table's
 * value for the given key without triggering any metamethods (e.g., __index). The result is pushed
 * onto the Lua stack, and the function returns 1 to indicate that one value has been returned.
 *
 * @param L The Lua state.
 * @return int Always returns 1, indicating that one value (the result of the raw get operation)
 *             has been pushed onto the Lua stack.
 */
static int luaB_rawget (lua_State *L) {
  luaL_checktype(L, 1, LUA_TTABLE);
  luaL_checkany(L, 2);
  lua_settop(L, 2);
  lua_rawget(L, 1);
  return 1;
}

/**
 * Sets a raw value in a Lua table without invoking any metamethods.
 *
 * This function expects three arguments on the Lua stack:
 * 1. A table (must be of type LUA_TTABLE).
 * 2. A key (any Lua value).
 * 3. A value (any Lua value).
 *
 * The function performs a raw assignment of the value to the key in the table,
 * bypassing any metamethods that might be defined for the table. This is equivalent
 * to the `rawset` function in Lua.
 *
 * @param L The Lua state.
 * @return Always returns 1, as the function pushes a single result (the table) onto the stack.
 */
static int luaB_rawset (lua_State *L) {
  luaL_checktype(L, 1, LUA_TTABLE);
  luaL_checkany(L, 2);
  luaL_checkany(L, 3);
  lua_settop(L, 3);
  lua_rawset(L, 1);
  return 1;
}


/**
 * Performs garbage collection operations based on the specified option.
 *
 * This function is a Lua binding for controlling the garbage collector (GC) in Lua.
 * It allows the user to perform various GC operations such as stopping, restarting,
 * collecting, counting memory usage, stepping the GC, setting pause and step multipliers,
 * and checking if the GC is running.
 *
 * @param L The Lua state.
 * @return Returns different values depending on the GC operation performed:
 *         - For `LUA_GCCOUNT`: Returns the total memory in use (in KB) as a Lua number.
 *         - For `LUA_GCSTEP` and `LUA_GCISRUNNING`: Returns a boolean indicating the success
 *           of the step operation or the running state of the GC.
 *         - For all other operations: Returns the result of the GC operation as an integer.
 *
 * The function expects the following arguments:
 * - `arg1` (string): The GC operation to perform. Valid options are:
 *   - "stop": Stops the GC.
 *   - "restart": Restarts the GC.
 *   - "collect": Performs a full garbage collection cycle.
 *   - "count": Returns the total memory in use (in KB).
 *   - "step": Performs an incremental step of garbage collection.
 *   - "setpause": Sets the GC pause parameter.
 *   - "setstepmul": Sets the GC step multiplier.
 *   - "isrunning": Checks if the GC is running.
 * - `arg2` (integer, optional): An additional argument for certain operations (e.g., step size).
 *
 * @note The default operation is "collect" if no option is specified.
 */
static int luaB_collectgarbage (lua_State *L) {
  static const char *const opts[] = {"stop", "restart", "collect",
    "count", "step", "setpause", "setstepmul",
    "isrunning", NULL};
  static const int optsnum[] = {LUA_GCSTOP, LUA_GCRESTART, LUA_GCCOLLECT,
    LUA_GCCOUNT, LUA_GCSTEP, LUA_GCSETPAUSE, LUA_GCSETSTEPMUL,
    LUA_GCISRUNNING};
  int o = optsnum[luaL_checkoption(L, 1, "collect", opts)];
  int ex = (int)luaL_optinteger(L, 2, 0);
  int res = lua_gc(L, o, ex);
  switch (o) {
    case LUA_GCCOUNT: {
      int b = lua_gc(L, LUA_GCCOUNTB, 0);
      lua_pushnumber(L, (lua_Number)res + ((lua_Number)b/1024));
      return 1;
    }
    case LUA_GCSTEP: case LUA_GCISRUNNING: {
      lua_pushboolean(L, res);
      return 1;
    }
    default: {
      lua_pushinteger(L, res);
      return 1;
    }
  }
}


/**
 * Determines the type of the value at the top of the Lua stack and pushes its corresponding type name as a string.
 *
 * This function retrieves the type of the value at index 1 on the Lua stack using `lua_type`. It ensures that
 * the value is not `LUA_TNONE` (indicating an invalid or missing value) by calling `luaL_argcheck`. If the value
 * is valid, the function pushes the name of the type (as returned by `lua_typename`) onto the stack as a string.
 *
 * @param L A pointer to the Lua state.
 * @return Returns 1, indicating that one value (the type name as a string) has been pushed onto the stack.
 */
static int luaB_type (lua_State *L) {
  int t = lua_type(L, 1);
  luaL_argcheck(L, t != LUA_TNONE, 1, "value expected");
  lua_pushstring(L, lua_typename(L, t));
  return 1;
}


/**
 * Pairsmeta is a helper function used to handle the iteration over Lua objects
 * with metamethods. It checks if the given object has a specified metamethod
 * and sets up the iteration accordingly.
 *
 * @param L The Lua state.
 * @param method The name of the metamethod to check for (e.g., "__pairs").
 * @param iszero If non-zero, the initial value for iteration is set to 0;
 *               otherwise, it is set to nil.
 * @param iter The default iterator function to use if the metamethod is not found.
 *
 * @return Returns 3 values on the Lua stack:
 *         1. The iterator function (either the metamethod or the provided `iter`).
 *         2. The object being iterated over (the first argument).
 *         3. The initial value for iteration (0 or nil, depending on `iszero`).
 *
 * This function is typically used to implement custom iteration behavior for
 * Lua objects by leveraging metamethods. If the specified metamethod is not
 * found, it falls back to the provided iterator function.
 */
static int pairsmeta (lua_State *L, const char *method, int iszero,
                      lua_CFunction iter) {
  luaL_checkany(L, 1);
  if (luaL_getmetafield(L, 1, method) == LUA_TNIL) {  /* no metamethod? */
    lua_pushcfunction(L, iter);  /* will return generator, */
    lua_pushvalue(L, 1);  /* state, */
    if (iszero) lua_pushinteger(L, 0);  /* and initial value */
    else lua_pushnil(L);
  }
  else {
    lua_pushvalue(L, 1);  /* argument 'self' to metamethod */
    lua_call(L, 1, 3);  /* get 3 values from metamethod */
  }
  return 3;
}


/**
 * Iterates over a Lua table, returning the next key-value pair.
 *
 * This function is designed to be used in conjunction with `lua_next` to traverse
 * all elements of a Lua table. It expects a table as its first argument and an
 * optional key as its second argument. If no key is provided, the function starts
 * iteration from the beginning of the table.
 *
 * @param L The Lua state.
 * @return Returns 2 if a key-value pair is found, pushing both the key and value
 *         onto the stack. If no more pairs are found, it returns 1, pushing `nil`
 *         onto the stack to indicate the end of iteration.
 *
 * @note The function ensures that the stack has exactly two arguments by setting
 *       the top of the stack to 2. This means that if a second argument (key) is
 *       not provided, it will be created as `nil`.
 */
static int luaB_next (lua_State *L) {
  luaL_checktype(L, 1, LUA_TTABLE);
  lua_settop(L, 2);  /* create a 2nd argument if there isn't one */
  if (lua_next(L, 1))
    return 2;
  else {
    lua_pushnil(L);
    return 1;
  }
}


/**
 * Invokes the `__pairs` metamethod of the table at the top of the Lua stack.
 * This function is used to iterate over key-value pairs in a table using the
 * `pairs` function in Lua. If the table has a metatable with a `__pairs` metamethod,
 * that metamethod is called to provide the iterator, initial state, and initial value.
 * Otherwise, the default `luaB_next` function is used as the iterator.
 *
 * @param L The Lua state.
 * @return The number of results pushed onto the stack (the iterator function,
 *         the table, and the initial state).
 */
static int luaB_pairs (lua_State *L) {
  return pairsmeta(L, "__pairs", 0, luaB_next);
}


/*
** Traversal function for 'ipairs'
*/
static int ipairsaux (lua_State *L) {
  lua_Integer i = luaL_checkinteger(L, 2) + 1;
  lua_pushinteger(L, i);
  return (lua_geti(L, 1, i) == LUA_TNIL) ? 1 : 2;
}


/*
** 'ipairs' function. Returns 'ipairsaux', given "table", 0.
** (The given "table" may not be a table.)
*/
static int luaB_ipairs (lua_State *L) {
#if defined(LUA_COMPAT_IPAIRS)
  return pairsmeta(L, "__ipairs", 1, ipairsaux);
#else
  luaL_checkany(L, 1);
  lua_pushcfunction(L, ipairsaux);  /* iteration function */
  lua_pushvalue(L, 1);  /* state */
  lua_pushinteger(L, 0);  /* initial value */
  return 3;
#endif
}


/**
 * Loads a Lua function and sets its environment if provided.
 *
 * This function is used to load a Lua function and optionally set its environment.
 * If the loading is successful (status == LUA_OK), the function checks if an environment
 * index (envidx) is provided. If so, it sets the environment as the first upvalue of the
 * loaded function. If the environment is not used by the function, it is removed from the stack.
 *
 * In case of an error during loading, the function pushes `nil` onto the stack, followed by
 * the error message, and returns 2. This allows the caller to handle the error appropriately.
 *
 * @param L The Lua state.
 * @param status The status of the Lua function loading operation.
 * @param envidx The index of the environment to be set as the first upvalue, or 0 if no environment is provided.
 * @return Returns 1 if the function is successfully loaded, or 2 if an error occurs (nil followed by the error message).
 */
static int load_aux (lua_State *L, int status, int envidx) {
  if (status == LUA_OK) {
    if (envidx != 0) {  /* 'env' parameter? */
      lua_pushvalue(L, envidx);  /* environment for loaded function */
      if (!lua_setupvalue(L, -2, 1))  /* set it as 1st upvalue */
        lua_pop(L, 1);  /* remove 'env' if not used by previous call */
    }
    return 1;
  }
  else {  /* error (message is on top of the stack) */
    lua_pushnil(L);
    lua_insert(L, -2);  /* put before error message */
    return 2;  /* return nil plus error message */
  }
}


/**
 * Loads and compiles a Lua file into a Lua function.
 *
 * This function attempts to load and compile the Lua file specified by `fname`.
 * If `fname` is `NULL`, it reads from the standard input. The `mode` parameter
 * controls whether the file is loaded as text or binary. An optional `env`
 * parameter can be provided to set the environment for the loaded function.
 *
 * @param L The Lua state.
 * @return Returns 1 on success, pushing the compiled function onto the stack.
 *         On failure, returns an error code and pushes an error message onto
 *         the stack.
 *
 * @param fname The name of the file to load. If `NULL`, reads from standard input.
 * @param mode The mode for loading the file ("t" for text, "b" for binary, or `NULL` for default).
 * @param env The environment table to set for the loaded function (optional).
 *
 * @see luaL_loadfilex
 * @see load_aux
 */
static int luaB_loadfile (lua_State *L) {
  const char *fname = luaL_optstring(L, 1, NULL);
  const char *mode = luaL_optstring(L, 2, NULL);
  int env = (!lua_isnone(L, 3) ? 3 : 0);  /* 'env' index or 0 if no 'env' */
  int status = luaL_loadfilex(L, fname, mode);
  return load_aux(L, status, env);
}


/*
** {======================================================
** Generic Read function
** =======================================================
*/


/*
** reserved slot, above all arguments, to hold a copy of the returned
** string to avoid it being collected while parsed. 'load' has four
** optional arguments (chunk, source name, mode, and environment).
*/
#define RESERVEDSLOT	5


/*
** Reader for generic 'load' function: 'lua_load' uses the
** stack for internal stuff, so the reader cannot change the
** stack top. Instead, it keeps its resulting string in a
** reserved slot inside the stack.
*/
static const char *generic_reader (lua_State *L, void *ud, size_t *size) {
  (void)(ud);  /* not used */
  luaL_checkstack(L, 2, "too many nested functions");
  lua_pushvalue(L, 1);  /* get function */
  lua_call(L, 0, 1);  /* call it */
  if (lua_isnil(L, -1)) {
    lua_pop(L, 1);  /* pop result */
    *size = 0;
    return NULL;
  }
  else if (!lua_isstring(L, -1))
    luaL_error(L, "reader function must return a string");
  lua_replace(L, RESERVEDSLOT);  /* save string in reserved slot */
  return lua_tolstring(L, RESERVEDSLOT, size);
}


/**
 * Loads a Lua chunk from a string or a reader function and pushes the compiled
 * chunk onto the stack as a Lua function.
 *
 * This function supports two modes of loading:
 * 1. From a string: The Lua chunk is provided as a string (first argument).
 * 2. From a reader function: The Lua chunk is read incrementally using a
 *    reader function (first argument).
 *
 * The function accepts the following arguments:
 * 1. The Lua chunk as a string or a reader function.
 * 2. An optional chunk name (used in error messages and debug information).
 * 3. An optional mode string ("b", "t", or "bt") to control binary/text loading.
 * 4. An optional environment table to set as the function's environment.
 *
 * The function returns 1 on success, pushing the compiled function onto the stack.
 * On error, it pushes an error message onto the stack and returns 0.
 *
 * @param L The Lua state.
 * @return The number of results pushed onto the stack (1 on success, 0 on error).
 */
static int luaB_load (lua_State *L) {
  int status;
  size_t l;
  const char *s = lua_tolstring(L, 1, &l);
  const char *mode = luaL_optstring(L, 3, "bt");
  int env = (!lua_isnone(L, 4) ? 4 : 0);  /* 'env' index or 0 if no 'env' */
  if (s != NULL) {  /* loading a string? */
    const char *chunkname = luaL_optstring(L, 2, s);
    status = luaL_loadbufferx(L, s, l, chunkname, mode);
  }
  else {  /* loading from a reader function */
    const char *chunkname = luaL_optstring(L, 2, "=(load)");
    luaL_checktype(L, 1, LUA_TFUNCTION);
    lua_settop(L, RESERVEDSLOT);  /* create reserved slot */
    status = lua_load(L, generic_reader, NULL, chunkname, mode);
  }
  return load_aux(L, status, env);
}

/* }====================================================== */


static int dofilecont (lua_State *L, int d1, lua_KContext d2) {
  (void)d1;  (void)d2;  /* only to match 'lua_Kfunction' prototype */
  return lua_gettop(L) - 1;
}


/**
 * Executes a Lua file and returns its results.
 *
 * This function loads and executes the Lua file specified by `fname`. If `fname` is `NULL`,
 * it uses the value on the top of the Lua stack as the file name. The file is loaded using
 * `luaL_loadfile`, and if successful, it is executed using `lua_callk`. The results of the
 * execution are returned to the caller.
 *
 * @param L The Lua state.
 * @return The number of results returned by the executed Lua file, or an error if the file
 *         cannot be loaded or executed.
 */
static int luaB_dofile (lua_State *L) {
  const char *fname = luaL_optstring(L, 1, NULL);
  lua_settop(L, 1);
  if (luaL_loadfile(L, fname) != LUA_OK)
    return lua_error(L);
  lua_callk(L, 0, LUA_MULTRET, 0, dofilecont);
  return dofilecont(L, 0, 0);
}


/**
 * @brief Asserts that a given condition is true. If the condition is true, all arguments are returned. 
 *        If the condition is false, an error is raised with a default or provided error message.
 *
 * This function checks the boolean value of the first argument on the Lua stack. If the value is true,
 * the function returns all arguments on the stack. If the value is false, the function removes the
 * condition from the stack, pushes a default error message ("assertion failed!"), and calls the
 * `luaB_error` function to raise an error. If additional arguments are provided, they are used as
 * the error message instead of the default.
 *
 * @param L A pointer to the Lua state.
 * @return The number of arguments on the stack if the condition is true, or the result of `luaB_error`
 *         if the condition is false.
 */
static int luaB_assert (lua_State *L) {
  if (lua_toboolean(L, 1))  /* condition is true? */
    return lua_gettop(L);  /* return all arguments */
  else {  /* error */
    luaL_checkany(L, 1);  /* there must be a condition */
    lua_remove(L, 1);  /* remove it */
    lua_pushliteral(L, "assertion failed!");  /* default message */
    lua_settop(L, 1);  /* leave only message (default if no other one) */
    return luaB_error(L);  /* call 'error' */
  }
}


/**
 * Selects elements from the Lua stack based on the provided arguments.
 *
 * This function is typically used in Lua to emulate the behavior of the `select` function.
 * It operates in two modes depending on the first argument:
 *
 * 1. If the first argument is the string "#", the function returns the number of arguments
 *    passed to it (excluding the "#" string itself). This is equivalent to `select("#", ...)` in Lua.
 *
 * 2. If the first argument is a number, the function returns all arguments starting from
 *    the specified index. The index can be positive or negative:
 *    - A positive index counts from the start of the argument list.
 *    - A negative index counts from the end of the argument list.
 *    If the index is out of range, the function raises an error.
 *
 * @param L The Lua state.
 * @return In mode 1, returns 1 (the number of arguments). In mode 2, returns the number of
 *         arguments starting from the specified index.
 */
static int luaB_select (lua_State *L) {
  int n = lua_gettop(L);
  if (lua_type(L, 1) == LUA_TSTRING && *lua_tostring(L, 1) == '#') {
    lua_pushinteger(L, n-1);
    return 1;
  }
  else {
    lua_Integer i = luaL_checkinteger(L, 1);
    if (i < 0) i = n + i;
    else if (i > n) i = n;
    luaL_argcheck(L, 1 <= i, 1, "index out of range");
    return n - (int)i;
  }
}


/*
** Continuation function for 'pcall' and 'xpcall'. Both functions
** already pushed a 'true' before doing the call, so in case of success
** 'finishpcall' only has to return everything in the stack minus
** 'extra' values (where 'extra' is exactly the number of items to be
** ignored).
*/
static int finishpcall (lua_State *L, int status, lua_KContext extra) {
  if (status != LUA_OK && status != LUA_YIELD) {  /* error? */
    lua_pushboolean(L, 0);  /* first result (false) */
    lua_pushvalue(L, -2);  /* error message */
    return 2;  /* return false, msg */
  }
  else
    return lua_gettop(L) - (int)extra;  /* return all results */
}


/**
 * Calls a Lua function in protected mode, handling errors gracefully.
 * 
 * This function is a wrapper around `lua_pcallk` that ensures the Lua function
 * at the top of the stack is executed in a protected environment. If an error
 * occurs during the execution of the function, it is caught and handled without
 * crashing the program. The function expects at least one argument on the Lua
 * stack (the function to be called) and pushes a boolean `true` as the first
 * result if no errors occur. The function then calls `lua_pcallk` with the
 * appropriate arguments and delegates the final result handling to `finishpcall`.
 *
 * @param L The Lua state.
 * @return The number of results returned by the Lua function or the error handler.
 */
static int luaB_pcall (lua_State *L) {
  int status;
  luaL_checkany(L, 1);
  lua_pushboolean(L, 1);  /* first result if no errors */
  lua_insert(L, 1);  /* put it in place */
  status = lua_pcallk(L, lua_gettop(L) - 2, LUA_MULTRET, 0, 0, finishpcall);
  return finishpcall(L, status, 0);
}


/*
** Do a protected call with error handling. After 'lua_rotate', the
** stack will have <f, err, true, f, [args...]>; so, the function passes
** 2 to 'finishpcall' to skip the 2 first values when returning results.
*/
static int luaB_xpcall (lua_State *L) {
  int status;
  int n = lua_gettop(L);
  luaL_checktype(L, 2, LUA_TFUNCTION);  /* check error function */
  lua_pushboolean(L, 1);  /* first result */
  lua_pushvalue(L, 1);  /* function */
  lua_rotate(L, 3, 2);  /* move them below function's arguments */
  status = lua_pcallk(L, n - 2, LUA_MULTRET, 2, 2, finishpcall);
  return finishpcall(L, status, 2);
}


/**
 * Converts the Lua value at the given stack index to a string and pushes the result onto the stack.
 *
 * This function first ensures that there is a value at the specified stack index by calling
 * `luaL_checkany`. It then converts the value to a string representation using `luaL_tolstring`,
 * which handles all Lua types appropriately. The resulting string is pushed onto the stack,
 * and the function returns 1 to indicate that one value has been added to the stack.
 *
 * @param L The Lua state.
 * @return Returns 1, indicating that one value (the string representation) has been pushed onto the stack.
 */
static int luaB_tostring (lua_State *L) {
  luaL_checkany(L, 1);
  luaL_tolstring(L, 1, NULL);
  return 1;
}


static const luaL_Reg base_funcs[] = {
  {"assert", luaB_assert},
  {"collectgarbage", luaB_collectgarbage},
  {"dofile", luaB_dofile},
  {"error", luaB_error},
  {"getmetatable", luaB_getmetatable},
  {"ipairs", luaB_ipairs},
  {"loadfile", luaB_loadfile},
  {"load", luaB_load},
#if defined(LUA_COMPAT_LOADSTRING)
  {"loadstring", luaB_load},
#endif
  {"next", luaB_next},
  {"pairs", luaB_pairs},
  {"pcall", luaB_pcall},
  {"print", luaB_print},
  {"rawequal", luaB_rawequal},
  {"rawlen", luaB_rawlen},
  {"rawget", luaB_rawget},
  {"rawset", luaB_rawset},
  {"select", luaB_select},
  {"setmetatable", luaB_setmetatable},
  {"tonumber", luaB_tonumber},
  {"tostring", luaB_tostring},
  {"type", luaB_type},
  {"xpcall", luaB_xpcall},
  /* placeholders */
  {"_G", NULL},
  {"_VERSION", NULL},
  {NULL, NULL}
};


/**
 * Opens the Lua base library and registers its functions into the global table.
 * This function is the entry point for loading the base Lua library into a Lua state.
 * It performs the following operations:
 * 1. Pushes the global table onto the stack.
 * 2. Registers the base library functions (`base_funcs`) into the global table.
 * 3. Sets the global `_G` variable to reference the global table itself.
 * 4. Sets the global `_VERSION` variable to the current Lua version string.
 * 
 * @param L Pointer to the Lua state.
 * @return Always returns 1, indicating that the global table is left on the stack.
 */
LUAMOD_API int luaopen_base (lua_State *L) {
  /* open lib into global table */
  lua_pushglobaltable(L);
  luaL_setfuncs(L, base_funcs, 0);
  /* set global _G */
  lua_pushvalue(L, -1);
  lua_setfield(L, -2, "_G");
  /* set global _VERSION */
  lua_pushliteral(L, LUA_VERSION);
  lua_setfield(L, -2, "_VERSION");
  return 1;
}

