/*
** $Id: lcorolib.c,v 1.10 2016/04/11 19:19:55 roberto Exp $
** Coroutine Library
** See Copyright Notice in lua.h
*/

#define lcorolib_c
#define LUA_LIB

#include "lprefix.h"


#include <stdlib.h>

#include "lua.h"

#include "lauxlib.h"
#include "lualib.h"


/**
 * Retrieves a Lua thread (coroutine) from the Lua stack.
 *
 * This function takes a Lua state `L` and retrieves the thread (coroutine) 
 * located at the first position on the stack. It performs an argument check 
 * to ensure that the value at the specified position is indeed a thread. 
 * If the value is not a thread, an error is raised with the message 
 * "thread expected".
 *
 * @param L The Lua state from which to retrieve the thread.
 * @return The Lua thread (coroutine) retrieved from the stack.
 */
static lua_State *getco (lua_State *L) {
  lua_State *co = lua_tothread(L, 1);
  luaL_argcheck(L, co, 1, "thread expected");
  return co;
}


/**
 * Resumes a coroutine with the specified number of arguments.
 *
 * This function attempts to resume the coroutine `co` using the arguments from the Lua state `L`.
 * It handles various edge cases such as insufficient stack space, attempting to resume a dead coroutine,
 * and managing the transfer of results or error messages between the coroutine and the main Lua state.
 *
 * @param L The main Lua state from which arguments are taken.
 * @param co The coroutine Lua state to be resumed.
 * @param narg The number of arguments to pass to the coroutine.
 *
 * @return If successful, returns the number of results yielded by the coroutine. If an error occurs,
 *         returns -1 and pushes an error message onto the main Lua state `L`.
 *
 * @note The function ensures that the coroutine is in a resumable state and that there is sufficient
 *       stack space for both arguments and results. If the coroutine has finished (LUA_OK) or is
 *       dead, an appropriate error message is pushed onto `L` and -1 is returned.
 */
static int auxresume (lua_State *L, lua_State *co, int narg) {
  int status;
  if (!lua_checkstack(co, narg)) {
    lua_pushliteral(L, "too many arguments to resume");
    return -1;  /* error flag */
  }
  if (lua_status(co) == LUA_OK && lua_gettop(co) == 0) {
    lua_pushliteral(L, "cannot resume dead coroutine");
    return -1;  /* error flag */
  }
  lua_xmove(L, co, narg);
  status = lua_resume(co, L, narg);
  if (status == LUA_OK || status == LUA_YIELD) {
    int nres = lua_gettop(co);
    if (!lua_checkstack(L, nres + 1)) {
      lua_pop(co, nres);  /* remove results anyway */
      lua_pushliteral(L, "too many results to resume");
      return -1;  /* error flag */
    }
    lua_xmove(co, L, nres);  /* move yielded values */
    return nres;
  }
  else {
    lua_xmove(co, L, 1);  /* move error message */
    return -1;  /* error flag */
  }
}


/**
 * Resumes a coroutine and handles its execution status.
 *
 * This function resumes the coroutine referenced by `co` and processes its execution result.
 * If the coroutine yields or encounters an error, the function returns the appropriate status
 * and error message. If the coroutine completes successfully, it returns the results of the
 * coroutine execution along with a success flag.
 *
 * @param L The Lua state from which the coroutine is being resumed.
 * @return The number of return values pushed onto the stack:
 *         - If the coroutine encounters an error, returns 2: a boolean `false` and the error message.
 *         - If the coroutine completes successfully, returns `r + 1`: a boolean `true` followed by
 *           the results returned by the coroutine.
 */
static int luaB_coresume (lua_State *L) {
  lua_State *co = getco(L);
  int r;
  r = auxresume(L, co, lua_gettop(L) - 1);
  if (r < 0) {
    lua_pushboolean(L, 0);
    lua_insert(L, -2);
    return 2;  /* return false + error message */
  }
  else {
    lua_pushboolean(L, 1);
    lua_insert(L, -(r + 1));
    return r + 1;  /* return true + 'resume' returns */
  }
}


/**
 * Resumes a coroutine and handles any errors that occur during its execution.
 * This function is typically used as a wrapper around `auxresume` to manage
 * coroutine execution and error propagation.
 *
 * @param L The main Lua state from which the coroutine is being resumed.
 * @return The number of results from the coroutine if successful. If an error
 *         occurs, it propagates the error by calling `lua_error`.
 *
 * The function retrieves the coroutine state from the upvalue at index 1 using
 * `lua_tothread`. It then resumes the coroutine using `auxresume`, passing the
 * main Lua state, the coroutine state, and the number of arguments on the stack.
 * If `auxresume` returns a negative value, indicating an error, the function
 * checks if the error object is a string. If so, it adds additional context
 * using `luaL_where` and concatenates it with the error message. Finally, the
 * error is propagated by calling `lua_error`.
 */
static int luaB_auxwrap (lua_State *L) {
  lua_State *co = lua_tothread(L, lua_upvalueindex(1));
  int r = auxresume(L, co, lua_gettop(L));
  if (r < 0) {
    if (lua_type(L, -1) == LUA_TSTRING) {  /* error object is a string? */
      luaL_where(L, 1);  /* add extra info */
      lua_insert(L, -2);
      lua_concat(L, 2);
    }
    return lua_error(L);  /* propagate error */
  }
  return r;
}


/**
 * Creates a new coroutine from a given function and pushes it onto the stack.
 *
 * This function takes a Lua function as its first argument and creates a new coroutine (thread)
 * from it. The new coroutine is represented by a new Lua state (`NL`). The function is then moved
 * from the original Lua state (`L`) to the new coroutine's state (`NL`). The new coroutine is
 * pushed onto the stack of the original Lua state (`L`).
 *
 * @param L The Lua state from which the function is taken and where the new coroutine is pushed.
 * @return Returns 1, indicating that the new coroutine has been pushed onto the stack.
 *
 * @note The function expects the first argument on the stack to be of type `LUA_TFUNCTION`.
 *       If this is not the case, a Lua error will be raised.
 */
static int luaB_cocreate (lua_State *L) {
  lua_State *NL;
  luaL_checktype(L, 1, LUA_TFUNCTION);
  NL = lua_newthread(L);
  lua_pushvalue(L, 1);  /* move function to top */
  lua_xmove(L, NL, 1);  /* move function from L to NL */
  return 1;
}


/**
 * Wraps a coroutine into a closure for use as a Lua function.
 *
 * This function first creates a new coroutine using `luaB_cocreate` and then wraps
 * it into a closure using `lua_pushcclosure` with `luaB_auxwrap` as the function
 * to be called when the closure is invoked. The resulting closure can be used as
 * a regular Lua function, which, when called, will resume the coroutine.
 *
 * @param L The Lua state.
 * @return Returns 1, pushing the newly created closure onto the Lua stack.
 */
static int luaB_cowrap (lua_State *L) {
  luaB_cocreate(L);
  lua_pushcclosure(L, luaB_auxwrap, 1);
  return 1;
}


/**
 * Yields the current coroutine, suspending its execution and returning control to the caller.
 * 
 * This function is a wrapper around `lua_yield`, which is used to yield a coroutine. It retrieves 
 * the number of arguments on the Lua stack using `lua_gettop` and passes them to `lua_yield`, 
 * allowing the coroutine to return these values to the caller when it resumes.
 * 
 * @param L The Lua state associated with the coroutine.
 * @return The result of `lua_yield`, which is the number of values returned to the caller.
 */
static int luaB_yield (lua_State *L) {
  return lua_yield(L, lua_gettop(L));
}


/**
 * Retrieves the status of a coroutine and pushes the corresponding status string onto the Lua stack.
 *
 * This function determines the status of the coroutine referenced by `co` and pushes a string
 * describing its current state onto the Lua stack. The possible status strings are:
 * - "running": The coroutine is currently executing.
 * - "suspended": The coroutine is suspended, either because it yielded or is in its initial state.
 * - "normal": The coroutine is active but not running (e.g., it called another coroutine).
 * - "dead": The coroutine has finished execution or encountered an error.
 *
 * @param L The Lua state from which the coroutine is accessed.
 * @return Returns 1, as the function pushes exactly one value (the status string) onto the stack.
 */
static int luaB_costatus (lua_State *L) {
  lua_State *co = getco(L);
  if (L == co) lua_pushliteral(L, "running");
  else {
    switch (lua_status(co)) {
      case LUA_YIELD:
        lua_pushliteral(L, "suspended");
        break;
      case LUA_OK: {
        lua_Debug ar;
        if (lua_getstack(co, 0, &ar) > 0)  /* does it have frames? */
          lua_pushliteral(L, "normal");  /* it is running */
        else if (lua_gettop(co) == 0)
            lua_pushliteral(L, "dead");
        else
          lua_pushliteral(L, "suspended");  /* initial state */
        break;
      }
      default:  /* some error occurred */
        lua_pushliteral(L, "dead");
        break;
    }
  }
  return 1;
}


/**
 * Pushes a boolean value onto the Lua stack indicating whether the current
 * coroutine is yieldable. A coroutine is yieldable if it can be suspended
 * (yielded) at the current point in its execution. This is typically true
 * unless the coroutine is running in a context where yielding is not allowed,
 * such as during certain critical sections or in C functions that do not
 * support yielding.
 *
 * @param L The Lua state.
 * @return Returns 1, indicating that one value (the boolean) has been pushed
 *         onto the stack.
 */
static int luaB_yieldable (lua_State *L) {
  lua_pushboolean(L, lua_isyieldable(L));
  return 1;
}


/**
 * Pushes the currently running coroutine onto the stack and indicates whether it is the main coroutine.
 *
 * This function retrieves the currently running coroutine and pushes it onto the Lua stack. 
 * Additionally, it pushes a boolean value onto the stack indicating whether the coroutine is the 
 * main coroutine (i.e., the initial thread of the Lua state). 
 *
 * @param L The Lua state.
 * @return Returns 2, indicating that two values (the coroutine and the boolean) have been pushed onto the stack.
 */
static int luaB_corunning (lua_State *L) {
  int ismain = lua_pushthread(L);
  lua_pushboolean(L, ismain);
  return 2;
}


static const luaL_Reg co_funcs[] = {
  {"create", luaB_cocreate},
  {"resume", luaB_coresume},
  {"running", luaB_corunning},
  {"status", luaB_costatus},
  {"wrap", luaB_cowrap},
  {"yield", luaB_yield},
  {"isyieldable", luaB_yieldable},
  {NULL, NULL}
};



/**
 * Initializes the coroutine library and registers its functions in the Lua state.
 *
 * This function is the entry point for loading the coroutine library in Lua. It creates a new
 * table in the Lua state `L` and populates it with the functions defined in the `co_funcs` array.
 * The table is then returned to the caller, making the coroutine library available for use in Lua.
 *
 * @param L The Lua state in which the coroutine library is being initialized.
 * @return Returns 1, indicating that the library table is pushed onto the Lua stack.
 */
LUAMOD_API int luaopen_coroutine (lua_State *L) {
  luaL_newlib(L, co_funcs);
  return 1;
}

