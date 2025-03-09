/*
** $Id: ltm.c,v 2.38 2016/12/22 13:08:50 roberto Exp $
** Tag methods
** See Copyright Notice in lua.h
*/

#define ltm_c
#define LUA_CORE

#include "lprefix.h"


#include <string.h>

#include "lua.h"

#include "ldebug.h"
#include "ldo.h"
#include "lobject.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "ltm.h"
#include "lvm.h"


static const char udatatypename[] = "userdata";

LUAI_DDEF const char *const luaT_typenames_[LUA_TOTALTAGS] = {
  "no value",
  "nil", "boolean", udatatypename, "number",
  "string", "table", "function", udatatypename, "thread",
  "proto" /* this last case is used for tests only */
};


/**
 * Initializes the tag method (TM) names in the global state of the Lua interpreter.
 * This function populates the `tmname` array in the global state with the predefined
 * tag method names, such as "__index", "__newindex", "__gc", etc. These names are
 * used to identify and handle various metamethods in Lua.
 *
 * The function creates internal string objects for each tag method name using `luaS_new`
 * and ensures that these string objects are not garbage collected by marking them as
 * fixed using `luaC_fix`.
 *
 * @param L A pointer to the Lua state in which the tag method names are initialized.
 *          This state must be valid and properly initialized.
 *
 * @note The tag method names are stored in a static array `luaT_eventname`, which is
 *       iterated over to populate the `tmname` array. The number of tag methods is
 *       determined by the constant `TM_N`.
 */
void luaT_init (lua_State *L) {
  static const char *const luaT_eventname[] = {  /* ORDER TM */
    "__index", "__newindex",
    "__gc", "__mode", "__len", "__eq",
    "__add", "__sub", "__mul", "__mod", "__pow",
    "__div", "__idiv",
    "__band", "__bor", "__bxor", "__shl", "__shr",
    "__unm", "__bnot", "__lt", "__le",
    "__concat", "__call"
  };
  int i;
  for (i=0; i<TM_N; i++) {
    G(L)->tmname[i] = luaS_new(L, luaT_eventname[i]);
    luaC_fix(L, obj2gco(G(L)->tmname[i]));  /* never collect these names */
  }
}


/*
** function to be used with macro "fasttm": optimized for absence of
** tag methods
*/
const TValue *luaT_gettm (Table *events, TMS event, TString *ename) {
  const TValue *tm = luaH_getshortstr(events, ename);
  lua_assert(event <= TM_EQ);
  if (ttisnil(tm)) {  /* no tag method? */
    events->flags |= cast_byte(1u<<event);  /* cache this fact */
    return NULL;
  }
  else return tm;
}


/**
 * Retrieves the metamethod associated with a given object and event.
 *
 * This function determines the metatable of the object `o` based on its type.
 * If the object is a table or userdata, it retrieves the metatable directly from the object.
 * For other types, it retrieves the metatable from the global metatable array.
 * 
 * Once the metatable is determined, it looks up the metamethod corresponding to the specified
 * event (e.g., addition, subtraction, etc.) in the metatable. If the metatable or the metamethod
 * does not exist, it returns a nil object.
 *
 * @param L The Lua state.
 * @param o The object whose metamethod is to be retrieved.
 * @param event The event (TMS) for which the metamethod is to be retrieved.
 * @return A pointer to the metamethod if found, otherwise a pointer to a nil object.
 */
const TValue *luaT_gettmbyobj (lua_State *L, const TValue *o, TMS event) {
  Table *mt;
  switch (ttnov(o)) {
    case LUA_TTABLE:
      mt = hvalue(o)->metatable;
      break;
    case LUA_TUSERDATA:
      mt = uvalue(o)->metatable;
      break;
    default:
      mt = G(L)->mt[ttnov(o)];
  }
  return (mt ? luaH_getshortstr(mt, G(L)->tmname[event]) : luaO_nilobject);
}


/*
** Return the name of the type of an object. For tables and userdata
** with metatable, use their '__name' metafield, if present.
*/
const char *luaT_objtypename (lua_State *L, const TValue *o) {
  Table *mt;
  if ((ttistable(o) && (mt = hvalue(o)->metatable) != NULL) ||
      (ttisfulluserdata(o) && (mt = uvalue(o)->metatable) != NULL)) {
    const TValue *name = luaH_getshortstr(mt, luaS_new(L, "__name"));
    if (ttisstring(name))  /* is '__name' a string? */
      return getstr(tsvalue(name));  /* use it as type name */
  }
  return ttypename(ttnov(o));  /* else use standard type name */
}


/**
 * Calls a metamethod function with the given arguments and handles the result.
 *
 * This function is used to invoke a metamethod (typically a __call, __index, or __newindex
 * metamethod) in the Lua state. It prepares the stack by pushing the metamethod function
 * and its arguments, then calls the function using `luaD_call` or `luaD_callnoyield`
 * depending on whether the current call context allows yielding. If a result is expected,
 * it is moved to the specified location on the stack.
 *
 * @param L The Lua state.
 * @param f The metamethod function to call (TValue).
 * @param p1 The first argument to the metamethod (TValue).
 * @param p2 The second argument to the metamethod (TValue).
 * @param p3 The third argument or the location to store the result (TValue*).
 * @param hasres Indicates whether the metamethod is expected to return a result (1 for yes, 0 for no).
 *
 * The function performs the following steps:
 * 1. Saves the position of `p3` on the stack if `hasres` is true.
 * 2. Pushes the metamethod function and its arguments onto the stack.
 * 3. Calls the metamethod using `luaD_call` (if yielding is allowed) or `luaD_callnoyield` (if not).
 * 4. If `hasres` is true, moves the result to the location specified by `p3`.
 */
void luaT_callTM (lua_State *L, const TValue *f, const TValue *p1,
                  const TValue *p2, TValue *p3, int hasres) {
  ptrdiff_t result = savestack(L, p3);
  StkId func = L->top;
  setobj2s(L, func, f);  /* push function (assume EXTRA_STACK) */
  setobj2s(L, func + 1, p1);  /* 1st argument */
  setobj2s(L, func + 2, p2);  /* 2nd argument */
  L->top += 3;
  if (!hasres)  /* no result? 'p3' is third argument */
    setobj2s(L, L->top++, p3);  /* 3rd argument */
  /* metamethod may yield only when called from Lua code */
  if (isLua(L->ci))
    luaD_call(L, func, hasres);
  else
    luaD_callnoyield(L, func, hasres);
  if (hasres) {  /* if has result, move it to its place */
    p3 = restorestack(L, result);
    setobjs2s(L, p3, --L->top);
  }
}


/**
 * @brief Invokes a binary metamethod for two Lua values.
 *
 * This function attempts to find and call a binary metamethod for the given Lua values `p1` and `p2`.
 * The metamethod is identified by the `event` parameter, which specifies the type of operation (e.g., addition, subtraction).
 * The function first tries to find the metamethod in the first operand `p1`. If no metamethod is found in `p1`, it then
 * searches in the second operand `p2`. If a metamethod is found, it is invoked with the two operands, and the result is
 * stored in the Lua stack at the position specified by `res`. If no metamethod is found, the function returns 0.
 *
 * @param L The Lua state.
 * @param p1 The first operand (Lua value).
 * @param p2 The second operand (Lua value).
 * @param res The stack index where the result of the metamethod call will be stored.
 * @param event The metamethod event type (e.g., TM_ADD, TM_SUB).
 * @return int Returns 1 if a metamethod was found and called successfully, otherwise returns 0.
 */
int luaT_callbinTM (lua_State *L, const TValue *p1, const TValue *p2,
                    StkId res, TMS event) {
  const TValue *tm = luaT_gettmbyobj(L, p1, event);  /* try first operand */
  if (ttisnil(tm))
    tm = luaT_gettmbyobj(L, p2, event);  /* try second operand */
  if (ttisnil(tm)) return 0;
  luaT_callTM(L, tm, p1, p2, res, 1);
  return 1;
}


/**
 * Attempts to invoke the binary metamethod for the given event on the two values `p1` and `p2`.
 * 
 * This function first tries to call the binary metamethod associated with the event using `luaT_callbinTM`.
 * If the metamethod is successfully invoked, the result is stored in `res`. If no metamethod is found,
 * the function handles the error based on the type of event:
 * - For `TM_CONCAT`, it raises a concatenation error.
 * - For bitwise operations (`TM_BAND`, `TM_BOR`, `TM_BXOR`, `TM_SHL`, `TM_SHR`, `TM_BNOT`), it checks if
 *   both values can be converted to numbers. If they can, it raises a bitwise operation error; otherwise,
 *   it raises a general operation error.
 * - For all other arithmetic events, it raises a general arithmetic operation error.
 * 
 * @param L The Lua state.
 * @param p1 The first value involved in the binary operation.
 * @param p2 The second value involved in the binary operation.
 * @param res The stack index where the result of the operation should be stored.
 * @param event The binary metamethod event to attempt (e.g., `TM_CONCAT`, `TM_BAND`, etc.).
 */
void luaT_trybinTM (lua_State *L, const TValue *p1, const TValue *p2,
                    StkId res, TMS event) {
  if (!luaT_callbinTM(L, p1, p2, res, event)) {
    switch (event) {
      case TM_CONCAT:
        luaG_concaterror(L, p1, p2);
      /* call never returns, but to avoid warnings: *//* FALLTHROUGH */
      case TM_BAND: case TM_BOR: case TM_BXOR:
      case TM_SHL: case TM_SHR: case TM_BNOT: {
        lua_Number dummy;
        if (tonumber(p1, &dummy) && tonumber(p2, &dummy))
          luaG_tointerror(L, p1, p2);
        else
          luaG_opinterror(L, p1, p2, "perform bitwise operation on");
      }
      /* calls never return, but to avoid warnings: *//* FALLTHROUGH */
      default:
        luaG_opinterror(L, p1, p2, "perform arithmetic on");
    }
  }
}


/**
 * Calls the appropriate metamethod for a binary order operation (e.g., `<`, `<=`, `>`, `>=`)
 * between two Lua values `p1` and `p2`. The specific metamethod to call is determined by the
 * `event` parameter, which should be one of the `TMS` enumeration values (e.g., `TM_LT`, `TM_LE`).
 *
 * @param L The Lua state.
 * @param p1 The first value to compare.
 * @param p2 The second value to compare.
 * @param event The metamethod event to trigger (e.g., `TM_LT` for less than).
 * @return Returns `1` if the comparison is true, `0` if the comparison is false, or `-1` if
 *         no metamethod is found for the operation.
 *
 * @note This function pushes the result of the metamethod call onto the Lua stack. The caller
 *       is responsible for managing the stack.
 */
int luaT_callorderTM (lua_State *L, const TValue *p1, const TValue *p2,
                      TMS event) {
  if (!luaT_callbinTM(L, p1, p2, L->top, event))
    return -1;  /* no metamethod */
  else
    return !l_isfalse(L->top);
}

