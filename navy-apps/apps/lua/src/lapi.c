/*
** $Id: lapi.c,v 2.259 2016/02/29 14:27:14 roberto Exp $
** Lua API
** See Copyright Notice in lua.h
*/

#define lapi_c
#define LUA_CORE

#include "lprefix.h"


#include <stdarg.h>
#include <string.h>

#include "lua.h"

#include "lapi.h"
#include "ldebug.h"
#include "ldo.h"
#include "lfunc.h"
#include "lgc.h"
#include "lmem.h"
#include "lobject.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "ltm.h"
#include "lundump.h"
#include "lvm.h"



const char lua_ident[] =
  "$LuaVersion: " LUA_COPYRIGHT " $"
  "$LuaAuthors: " LUA_AUTHORS " $";


/* value at a non-valid index */
#define NONVALIDVALUE		cast(TValue *, luaO_nilobject)

/* corresponding test */
#define isvalid(o)	((o) != luaO_nilobject)

/* test for pseudo index */
#define ispseudo(i)		((i) <= LUA_REGISTRYINDEX)

/* test for upvalue */
#define isupvalue(i)		((i) < LUA_REGISTRYINDEX)

/* test for valid but not pseudo index */
#define isstackindex(i, o)	(isvalid(o) && !ispseudo(i))

#define api_checkvalidindex(l,o)  api_check(l, isvalid(o), "invalid index")

#define api_checkstackindex(l, i, o)  \
	api_check(l, isstackindex(i, o), "index not in the stack")


/**
 * Converts a Lua stack index to a pointer to the corresponding TValue.
 * 
 * This function handles both positive and negative indices, as well as pseudo-indices.
 * Positive indices refer to the stack starting from the current function's base (1 being the first argument).
 * Negative indices refer to the stack relative to the top (-1 being the topmost element).
 * Pseudo-indices are special indices used to access Lua's registry or upvalues.
 * 
 * @param L The Lua state.
 * @param idx The index to convert. Positive, negative, or pseudo-index.
 * @return A pointer to the TValue corresponding to the index, or NONVALIDVALUE if the index is invalid.
 * 
 * The function performs the following steps:
 * 1. For positive indices, it calculates the offset from the current function's base and checks if the index is within bounds.
 * 2. For negative indices, it calculates the offset from the top of the stack and checks if the index is within bounds.
 * 3. For pseudo-indices, it handles special cases like LUA_REGISTRYINDEX or upvalues, ensuring the index is valid.
 * 
 * @note This function is essential for accessing Lua stack elements safely and efficiently.
 */
static TValue *index2addr (lua_State *L, int idx) {
  CallInfo *ci = L->ci;
  if (idx > 0) {
    TValue *o = ci->func + idx;
    api_check(L, idx <= ci->top - (ci->func + 1), "unacceptable index");
    if (o >= L->top) return NONVALIDVALUE;
    else return o;
  }
  else if (!ispseudo(idx)) {  /* negative index */
    api_check(L, idx != 0 && -idx <= L->top - (ci->func + 1), "invalid index");
    return L->top + idx;
  }
  else if (idx == LUA_REGISTRYINDEX)
    return &G(L)->l_registry;
  else {  /* upvalues */
    idx = LUA_REGISTRYINDEX - idx;
    api_check(L, idx <= MAXUPVAL + 1, "upvalue index too large");
    if (ttislcf(ci->func))  /* light C function? */
      return NONVALIDVALUE;  /* it has no upvalues */
    else {
      CClosure *func = clCvalue(ci->func);
      return (idx <= func->nupvalues) ? &func->upvalue[idx-1] : NONVALIDVALUE;
    }
  }
}


/*
** to be called by 'lua_checkstack' in protected mode, to grow stack
** capturing memory errors
*/
static void growstack (lua_State *L, void *ud) {
  int size = *(int *)ud;
  luaD_growstack(L, size);
}


/**
 * Ensures that the Lua stack has enough space to accommodate at least `n` additional elements.
 * If the current stack size is insufficient, the stack is grown to accommodate the required space.
 *
 * @param L Pointer to the Lua state.
 * @param n The number of additional stack elements to ensure space for.
 * @return 1 if the stack has enough space or if it was successfully grown, 0 if the stack cannot
 *         be grown to the required size (e.g., due to memory constraints or exceeding the maximum
 *         stack size).
 *
 * @note This function is thread-safe as it locks the Lua state during execution.
 * @note If the stack is grown, the top of the current call frame (`ci->top`) is adjusted to reflect
 *       the new stack size.
 * @see LUAI_MAXSTACK for the maximum allowed stack size.
 */
LUA_API int lua_checkstack (lua_State *L, int n) {
  int res;
  CallInfo *ci = L->ci;
  lua_lock(L);
  api_check(L, n >= 0, "negative 'n'");
  if (L->stack_last - L->top > n)  /* stack large enough? */
    res = 1;  /* yes; check is OK */
  else {  /* no; need to grow stack */
    int inuse = cast_int(L->top - L->stack) + EXTRA_STACK;
    if (inuse > LUAI_MAXSTACK - n)  /* can grow without overflow? */
      res = 0;  /* no */
    else  /* try to grow stack */
      res = (luaD_rawrunprotected(L, &growstack, &n) == LUA_OK);
  }
  if (res && ci->top < L->top + n)
    ci->top = L->top + n;  /* adjust frame top */
  lua_unlock(L);
  return res;
}


/**
 * Moves `n` elements from the stack of the `from` Lua state to the stack of the `to` Lua state.
 * 
 * This function is used to transfer elements between two Lua states that share the same global state.
 * The elements are removed from the `from` state's stack and pushed onto the `to` state's stack.
 * 
 * @param from The source Lua state from which elements are moved.
 * @param to The destination Lua state to which elements are moved.
 * @param n The number of elements to move from the `from` state to the `to` state.
 * 
 * @note The function does nothing if `from` and `to` are the same Lua state.
 * @note The function performs checks to ensure that the `to` state's stack has enough space to accommodate the moved elements.
 * @note The function assumes that both Lua states share the same global state; otherwise, an error is raised.
 * 
 * @see lua_lock
 * @see lua_unlock
 * @see api_checknelems
 * @see api_check
 */
LUA_API void lua_xmove (lua_State *from, lua_State *to, int n) {
  int i;
  if (from == to) return;
  lua_lock(to);
  api_checknelems(from, n);
  api_check(from, G(from) == G(to), "moving among independent states");
  api_check(from, to->ci->top - to->top >= n, "stack overflow");
  from->top -= n;
  for (i = 0; i < n; i++) {
    setobj2s(to, to->top, from->top + i);
    to->top++;  /* stack already checked by previous 'api_check' */
  }
  lua_unlock(to);
}


/**
 * Sets a new panic function for the Lua state and returns the previous one.
 *
 * The panic function is called by Lua when an unprotected error occurs (i.e., 
 * an error outside of a `pcall` or `xpcall`). The panic function should not 
 * return, as it is typically used to handle unrecoverable errors. Instead, 
 * it should terminate the application or perform some other form of cleanup.
 *
 * @param L         The Lua state.
 * @param panicf    The new panic function to set. This function should have 
 *                  the signature `int panicf(lua_State *L)`.
 * @return          The previous panic function, or `NULL` if no panic function 
 *                  was previously set.
 */
LUA_API lua_CFunction lua_atpanic (lua_State *L, lua_CFunction panicf) {
  lua_CFunction old;
  lua_lock(L);
  old = G(L)->panic;
  G(L)->panic = panicf;
  lua_unlock(L);
  return old;
}


/**
 * @brief Returns the version number of the Lua library.
 *
 * This function returns a pointer to a constant `lua_Number` representing the version
 * of the Lua library. If the provided `lua_State` pointer `L` is `NULL`, the function
 * returns a pointer to a static internal variable containing the version number.
 * Otherwise, it returns the version number associated with the given Lua state `L`.
 *
 * The version number is typically defined by the macro `LUA_VERSION_NUM`, which encodes
 * the Lua version in a numeric format (e.g., 504 for Lua 5.4).
 *
 * @param L A pointer to the Lua state. If `NULL`, the function returns the global version.
 * @return A pointer to a constant `lua_Number` representing the Lua version.
 */
LUA_API const lua_Number *lua_version (lua_State *L) {
  static const lua_Number version = LUA_VERSION_NUM;
  if (L == NULL) return &version;
  else return G(L)->version;
}



/*
** basic stack manipulation
*/


/*
** convert an acceptable stack index into an absolute index
*/
LUA_API int lua_absindex (lua_State *L, int idx) {
  return (idx > 0 || ispseudo(idx))
         ? idx
         : cast_int(L->top - L->ci->func) + idx;
}


/**
 * @brief Returns the index of the top element in the Lua stack.
 *
 * This function calculates the number of elements in the Lua stack by subtracting
 * the base of the current function's stack frame (i.e., `L->ci->func + 1`) from the
 * current top of the stack (`L->top`). The result is cast to an integer and returned.
 *
 * @param L A pointer to the Lua state.
 * @return The index of the top element in the Lua stack.
 */
LUA_API int lua_gettop (lua_State *L) {
  return cast_int(L->top - (L->ci->func + 1));
}


/**
 * Sets the top of the Lua stack to the specified index.
 *
 * This function adjusts the top of the stack in the Lua state `L` to the index `idx`.
 * If `idx` is non-negative, it sets the top of the stack to `idx` positions above the
 * current function's base. If the new top exceeds the current stack size, the stack
 * is extended by setting new slots to `nil`. If `idx` is negative, it sets the top of
 * the stack to `idx` positions below the current top, effectively removing elements
 * from the stack.
 *
 * @param L The Lua state.
 * @param idx The index to set as the new top of the stack. If non-negative, it is
 *            relative to the current function's base. If negative, it is relative
 *            to the current top of the stack.
 * @throws If `idx` is positive and exceeds the stack's capacity, or if `idx` is
 *         negative and would result in an invalid stack top, an error is raised.
 */
LUA_API void lua_settop (lua_State *L, int idx) {
  StkId func = L->ci->func;
  lua_lock(L);
  if (idx >= 0) {
    api_check(L, idx <= L->stack_last - (func + 1), "new top too large");
    while (L->top < (func + 1) + idx)
      setnilvalue(L->top++);
    L->top = (func + 1) + idx;
  }
  else {
    api_check(L, -(idx+1) <= (L->top - (func + 1)), "invalid new top");
    L->top += idx+1;  /* 'subtract' index (index is negative) */
  }
  lua_unlock(L);
}


/*
** Reverse the stack segment from 'from' to 'to'
** (auxiliary to 'lua_rotate')
*/
static void reverse (lua_State *L, StkId from, StkId to) {
  for (; from < to; from++, to--) {
    TValue temp;
    setobj(L, &temp, from);
    setobjs2s(L, from, to);
    setobj2s(L, to, &temp);
  }
}


/*
** Let x = AB, where A is a prefix of length 'n'. Then,
** rotate x n == BA. But BA == (A^r . B^r)^r.
*/
LUA_API void lua_rotate (lua_State *L, int idx, int n) {
  StkId p, t, m;
  lua_lock(L);
  t = L->top - 1;  /* end of stack segment being rotated */
  p = index2addr(L, idx);  /* start of segment */
  api_checkstackindex(L, idx, p);
  api_check(L, (n >= 0 ? n : -n) <= (t - p + 1), "invalid 'n'");
  m = (n >= 0 ? t - n : p - n - 1);  /* end of prefix */
  reverse(L, p, m);  /* reverse the prefix with length 'n' */
  reverse(L, m + 1, t);  /* reverse the suffix */
  reverse(L, p, t);  /* reverse the entire segment */
  lua_unlock(L);
}


/**
 * Copies the value at the specified index `fromidx` to the index `toidx` in the Lua stack.
 * This function ensures that the destination index is valid and performs necessary garbage
 * collection barriers if the destination is an upvalue of a function.
 *
 * @param L The Lua state.
 * @param fromidx The index of the value to copy from. This can be a stack index or a pseudo-index.
 * @param toidx The index of the value to copy to. This can be a stack index or a pseudo-index.
 *
 * @note If `toidx` is an upvalue, a garbage collection barrier is triggered to ensure the
 *       copied value is properly tracked by the garbage collector. However, if `toidx` is
 *       `LUA_REGISTRYINDEX`, no barrier is needed as the registry is revisited by the
 *       collector during garbage collection.
 *
 * @see lua_lock, lua_unlock, index2addr, api_checkvalidindex, setobj, isupvalue, luaC_barrier
 */
LUA_API void lua_copy (lua_State *L, int fromidx, int toidx) {
  TValue *fr, *to;
  lua_lock(L);
  fr = index2addr(L, fromidx);
  to = index2addr(L, toidx);
  api_checkvalidindex(L, to);
  setobj(L, to, fr);
  if (isupvalue(toidx))  /* function upvalue? */
    luaC_barrier(L, clCvalue(L->ci->func), fr);
  /* LUA_REGISTRYINDEX does not need gc barrier
     (collector revisits it before finishing collection) */
  lua_unlock(L);
}


/**
 * Pushes a copy of the element at the given index onto the stack.
 *
 * This function retrieves the value located at the specified index `idx` in the 
 * Lua stack and pushes a copy of it onto the top of the stack. The original 
 * value remains unchanged. The index can be any valid stack position, including 
 * negative indices which count from the top of the stack.
 *
 * @param L Pointer to the Lua state.
 * @param idx Index of the element to be copied and pushed onto the stack.
 *
 * @note This function is thread-safe as it locks the Lua state during execution.
 */
LUA_API void lua_pushvalue (lua_State *L, int idx) {
  lua_lock(L);
  setobj2s(L, L->top, index2addr(L, idx));
  api_incr_top(L);
  lua_unlock(L);
}



/*
** access functions (stack -> C)
*/


LUA_API int lua_type (lua_State *L, int idx) {
  StkId o = index2addr(L, idx);
  return (isvalid(o) ? ttnov(o) : LUA_TNONE);
}


/**
 * Returns the name of the type associated with the given tag `t`.
 *
 * This function maps a Lua type tag (an integer) to its corresponding type name as a string.
 * The tag `t` must be a valid Lua type tag, i.e., it must satisfy `LUA_TNONE <= t < LUA_NUMTAGS`.
 * If the tag is invalid, an error is raised.
 *
 * @param L The Lua state (unused in this function but required for API consistency).
 * @param t The type tag to look up.
 * @return A string representing the name of the type associated with the tag `t`.
 */
LUA_API const char *lua_typename (lua_State *L, int t) {
  UNUSED(L);
  api_check(L, LUA_TNONE <= t && t < LUA_NUMTAGS, "invalid tag");
  return ttypename(t);
}


/**
 * Checks if the value at the given index in the Lua stack is a C function or a C closure.
 *
 * @param L Pointer to the Lua state.
 * @param idx Index of the value in the Lua stack to check.
 * @return Returns 1 if the value is a C function or a C closure, otherwise returns 0.
 *
 * This function first retrieves the value at the specified index using `index2addr`. It then checks
 * if the value is either a light C function (`ttislcf`) or a C closure (`ttisCclosure`). If either
 * condition is true, the function returns 1, indicating that the value is a C function or C closure.
 * Otherwise, it returns 0.
 */
LUA_API int lua_iscfunction (lua_State *L, int idx) {
  StkId o = index2addr(L, idx);
  return (ttislcf(o) || (ttisCclosure(o)));
}


/**
 * Checks whether the value at the specified index in the Lua stack is an integer.
 *
 * This function retrieves the value at the given stack index and determines if it
 * is of type integer. It does not perform any type conversion or coercion.
 *
 * @param L Pointer to the Lua state.
 * @param idx Index of the value in the stack to check.
 * @return Returns 1 if the value is an integer, otherwise returns 0.
 */
LUA_API int lua_isinteger (lua_State *L, int idx) {
  StkId o = index2addr(L, idx);
  return ttisinteger(o);
}


/**
 * Checks if the value at the specified index in the Lua stack is a number or a string that can be converted to a number.
 *
 * This function retrieves the value at the given index `idx` in the Lua stack and checks if it is either a number
 * or a string that can be converted to a number. If the value is a number or a convertible string, the function
 * returns 1 (true). Otherwise, it returns 0 (false).
 *
 * @param L Pointer to the Lua state.
 * @param idx Index of the value in the Lua stack to check.
 * @return 1 if the value is a number or a convertible string, 0 otherwise.
 */
LUA_API int lua_isnumber (lua_State *L, int idx) {
  lua_Number n;
  const TValue *o = index2addr(L, idx);
  return tonumber(o, &n);
}


/**
 * Checks whether the value at the given index in the Lua stack is a string or can be converted to a string.
 * 
 * This function first retrieves the value at the specified index `idx` in the Lua stack `L`. 
 * It then checks if the value is of type string (`ttisstring`) or if it can be converted to a string (`cvt2str`).
 * 
 * @param L Pointer to the Lua state.
 * @param idx Index of the value in the Lua stack to check.
 * @return Returns 1 if the value is a string or can be converted to a string, otherwise returns 0.
 */
LUA_API int lua_isstring (lua_State *L, int idx) {
  const TValue *o = index2addr(L, idx);
  return (ttisstring(o) || cvt2str(o));
}


/**
 * Checks whether the value at the given index in the Lua stack is a userdata.
 * Userdata in Lua can be either "full userdata" or "light userdata".
 * 
 * - Full userdata is a block of memory managed by Lua, typically used to represent
 *   complex data structures or objects from C.
 * - Light userdata is a raw C pointer that is not managed by Lua.
 *
 * @param L Pointer to the Lua state.
 * @param idx Index of the value in the Lua stack to check.
 * @return Returns 1 if the value at the given index is a userdata (either full or light),
 *         otherwise returns 0.
 */
LUA_API int lua_isuserdata (lua_State *L, int idx) {
  const TValue *o = index2addr(L, idx);
  return (ttisfulluserdata(o) || ttislightuserdata(o));
}


/**
 * Compares two values in the Lua stack without invoking any metamethods.
 *
 * This function retrieves the values located at `index1` and `index2` in the Lua stack
 * and performs a raw equality comparison between them. The comparison does not trigger
 * any metamethods (e.g., `__eq`), making it a direct comparison of the values' types
 * and contents.
 *
 * @param L Pointer to the Lua state.
 * @param index1 Stack index of the first value to compare.
 * @param index2 Stack index of the second value to compare.
 * @return Returns 1 if the values are equal, 0 otherwise. If either index is invalid,
 *         the function returns 0.
 */
LUA_API int lua_rawequal (lua_State *L, int index1, int index2) {
  StkId o1 = index2addr(L, index1);
  StkId o2 = index2addr(L, index2);
  return (isvalid(o1) && isvalid(o2)) ? luaV_rawequalobj(o1, o2) : 0;
}


/**
 * Performs an arithmetic or bitwise operation on the top elements of the Lua stack.
 *
 * This function executes an arithmetic or bitwise operation specified by the `op` parameter
 * on the top elements of the Lua stack. The operation can be either binary (e.g., addition,
 * subtraction, multiplication) or unary (e.g., negation, bitwise NOT). For binary operations,
 * the function expects two operands on the stack, with the first operand at `top - 2` and
 * the second at `top - 1`. For unary operations, only one operand is required, and a fake
 * second operand is added to the stack to maintain consistency.
 *
 * The result of the operation is stored at the position of the first operand (`top - 2`),
 * and the second operand is removed from the stack. The function ensures thread safety by
 * locking the Lua state during the operation.
 *
 * @param L The Lua state.
 * @param op The operation to perform, which can be one of the following:
 *           - `LUA_OPADD`: Addition
 *           - `LUA_OPSUB`: Subtraction
 *           - `LUA_OPMUL`: Multiplication
 *           - `LUA_OPDIV`: Division
 *           - `LUA_OPMOD`: Modulo
 *           - `LUA_OPPOW`: Exponentiation
 *           - `LUA_OPUNM`: Unary minus (negation)
 *           - `LUA_OPBNOT`: Bitwise NOT
 *           - `LUA_OPBAND`: Bitwise AND
 *           - `LUA_OPBOR`: Bitwise OR
 *           - `LUA_OPBXOR`: Bitwise XOR
 *           - `LUA_OPSHL`: Bitwise left shift
 *           - `LUA_OPSHR`: Bitwise right shift
 *
 * @note This function assumes that the necessary operands are already on the stack.
 *       It modifies the stack by removing the second operand and replacing the first
 *       operand with the result of the operation.
 */
LUA_API void lua_arith (lua_State *L, int op) {
  lua_lock(L);
  if (op != LUA_OPUNM && op != LUA_OPBNOT)
    api_checknelems(L, 2);  /* all other operations expect two operands */
  else {  /* for unary operations, add fake 2nd operand */
    api_checknelems(L, 1);
    setobjs2s(L, L->top, L->top - 1);
    api_incr_top(L);
  }
  /* first operand at top - 2, second at top - 1; result go to top - 2 */
  luaO_arith(L, op, L->top - 2, L->top - 1, L->top - 2);
  L->top--;  /* remove second operand */
  lua_unlock(L);
}


/**
 * Compares two Lua values at the specified indices in the Lua stack.
 *
 * This function performs a comparison between the values located at `index1` and `index2`
 * in the Lua stack `L`. The type of comparison is determined by the `op` parameter, which
 * must be one of the following predefined constants:
 * - `LUA_OPEQ`: Checks if the values are equal.
 * - `LUA_OPLT`: Checks if the value at `index1` is less than the value at `index2`.
 * - `LUA_OPLE`: Checks if the value at `index1` is less than or equal to the value at `index2`.
 *
 * The function returns 1 if the comparison is true, and 0 otherwise. If the comparison
 * operation is invalid, an error is raised via `api_check`.
 *
 * @param L The Lua state.
 * @param index1 The stack index of the first value to compare.
 * @param index2 The stack index of the second value to compare.
 * @param op The comparison operation to perform (one of `LUA_OPEQ`, `LUA_OPLT`, or `LUA_OPLE`).
 * @return 1 if the comparison is true, 0 otherwise.
 */
LUA_API int lua_compare (lua_State *L, int index1, int index2, int op) {
  StkId o1, o2;
  int i = 0;
  lua_lock(L);  /* may call tag method */
  o1 = index2addr(L, index1);
  o2 = index2addr(L, index2);
  if (isvalid(o1) && isvalid(o2)) {
    switch (op) {
      case LUA_OPEQ: i = luaV_equalobj(L, o1, o2); break;
      case LUA_OPLT: i = luaV_lessthan(L, o1, o2); break;
      case LUA_OPLE: i = luaV_lessequal(L, o1, o2); break;
      default: api_check(L, 0, "invalid option");
    }
  }
  lua_unlock(L);
  return i;
}


/**
 * Converts a string to a Lua number and pushes it onto the Lua stack.
 *
 * This function attempts to convert the string `s` to a Lua number using the
 * internal `luaO_str2num` function. If the conversion is successful, the
 * resulting number is pushed onto the Lua stack, and the size of the converted
 * string is returned. If the conversion fails, the function returns 0 and does
 * not modify the stack.
 *
 * @param L The Lua state.
 * @param s The string to convert to a number.
 * @return The size of the converted string if successful, or 0 if the conversion fails.
 */
LUA_API size_t lua_stringtonumber (lua_State *L, const char *s) {
  size_t sz = luaO_str2num(s, L->top);
  if (sz != 0)
    api_incr_top(L);
  return sz;
}


/**
 * @brief Converts the Lua value at the specified index to a Lua number.
 *
 * This function attempts to convert the Lua value at the given index `idx` in the Lua stack `L` to a Lua number (`lua_Number`). 
 * If the conversion is successful, the resulting number is returned. If the conversion fails, the function returns 0.
 *
 * The function also provides an optional output parameter `pisnum` to indicate whether the conversion was successful. 
 * If `pisnum` is not NULL, it will be set to 1 if the conversion succeeded, or 0 if it failed.
 *
 * @param L Pointer to the Lua state.
 * @param idx Index of the value in the Lua stack to convert.
 * @param pisnum Optional pointer to an integer where the success of the conversion will be stored. 
 *               If NULL, the success status is not returned.
 * @return The converted Lua number, or 0 if the conversion failed.
 */
LUA_API lua_Number lua_tonumberx (lua_State *L, int idx, int *pisnum) {
  lua_Number n;
  const TValue *o = index2addr(L, idx);
  int isnum = tonumber(o, &n);
  if (!isnum)
    n = 0;  /* call to 'tonumber' may change 'n' even if it fails */
  if (pisnum) *pisnum = isnum;
  return n;
}


/**
 * Converts the Lua value at the given index to a Lua integer.
 *
 * This function attempts to convert the value at the specified index `idx` in the Lua stack
 * to a Lua integer (`lua_Integer`). If the conversion is successful, the result is returned,
 * and the optional `pisnum` parameter is set to a non-zero value. If the conversion fails,
 * the function returns 0, and `pisnum` is set to 0.
 *
 * @param L        The Lua state.
 * @param idx      The index of the value in the Lua stack to convert.
 * @param pisnum   A pointer to an integer that indicates whether the conversion was successful.
 *                 If non-NULL, it will be set to 1 if the conversion succeeded, or 0 otherwise.
 *
 * @return         The converted Lua integer. If the conversion fails, 0 is returned.
 */
LUA_API lua_Integer lua_tointegerx (lua_State *L, int idx, int *pisnum) {
  lua_Integer res;
  const TValue *o = index2addr(L, idx);
  int isnum = tointeger(o, &res);
  if (!isnum)
    res = 0;  /* call to 'tointeger' may change 'n' even if it fails */
  if (pisnum) *pisnum = isnum;
  return res;
}


/**
 * Converts the Lua value at the given index to a boolean.
 *
 * This function retrieves the value at the specified index `idx` on the Lua stack
 * and converts it to a boolean. The conversion follows Lua's rules for truthiness:
 * - `false` and `nil` are considered false.
 * - All other values (including numbers, strings, tables, etc.) are considered true.
 *
 * @param L The Lua state.
 * @param idx The index of the value on the Lua stack.
 * @return 1 if the value is considered true, 0 if it is considered false.
 */
LUA_API int lua_toboolean (lua_State *L, int idx) {
  const TValue *o = index2addr(L, idx);
  return !l_isfalse(o);
}


/**
 * Converts the Lua value at the given index to a C string.
 *
 * This function attempts to convert the Lua value at index `idx` in the stack `L` to a C string.
 * If the value is already a string, it returns the string directly. If the value is not a string
 * but can be converted to one (e.g., a number or a table with a `__tostring` metamethod), it
 * performs the conversion and returns the resulting string. If the value cannot be converted to
 * a string, the function returns `NULL`.
 *
 * If the `len` parameter is not `NULL`, the length of the string is stored in the location pointed
 * to by `len`. If the value cannot be converted to a string, `len` is set to 0.
 *
 * @param L The Lua state.
 * @param idx The index of the value in the stack to convert to a string.
 * @param len A pointer to store the length of the resulting string, or `NULL` if the length is not needed.
 * @return A pointer to the C string representation of the Lua value, or `NULL` if the value cannot be converted.
 */
LUA_API const char *lua_tolstring (lua_State *L, int idx, size_t *len) {
  StkId o = index2addr(L, idx);
  if (!ttisstring(o)) {
    if (!cvt2str(o)) {  /* not convertible? */
      if (len != NULL) *len = 0;
      return NULL;
    }
    lua_lock(L);  /* 'luaO_tostring' may create a new string */
    luaO_tostring(L, o);
    luaC_checkGC(L);
    o = index2addr(L, idx);  /* previous call may reallocate the stack */
    lua_unlock(L);
  }
  if (len != NULL)
    *len = vslen(o);
  return svalue(o);
}


/**
 * @brief Returns the raw length of the value at the given index in the Lua stack.
 *
 * This function retrieves the raw length of the value located at the specified index `idx` in the Lua stack `L`. 
 * The length is determined based on the type of the value:
 * - For short strings (`LUA_TSHRSTR`), it returns the length of the string.
 * - For long strings (`LUA_TLNGSTR`), it returns the length of the string.
 * - For userdata (`LUA_TUSERDATA`), it returns the length of the userdata.
 * - For tables (`LUA_TTABLE`), it returns the number of elements in the table.
 * - For all other types, it returns 0.
 *
 * @param L Pointer to the Lua state.
 * @param idx Index of the value in the Lua stack.
 * @return The raw length of the value at the specified index, or 0 if the value does not have a length.
 */
LUA_API size_t lua_rawlen (lua_State *L, int idx) {
  StkId o = index2addr(L, idx);
  switch (ttype(o)) {
    case LUA_TSHRSTR: return tsvalue(o)->shrlen;
    case LUA_TLNGSTR: return tsvalue(o)->u.lnglen;
    case LUA_TUSERDATA: return uvalue(o)->len;
    case LUA_TTABLE: return luaH_getn(hvalue(o));
    default: return 0;
  }
}


/**
 * Retrieves the C function associated with the value at the given index on the Lua stack.
 *
 * This function checks the type of the value at the specified index `idx` on the Lua stack `L`.
 * If the value is a light C function (a C function without upvalues), it returns the function directly.
 * If the value is a C closure (a C function with upvalues), it returns the underlying C function.
 * If the value is not a C function or C closure, the function returns `NULL`.
 *
 * @param L Pointer to the Lua state.
 * @param idx Index of the value on the Lua stack.
 * @return The C function associated with the value, or `NULL` if the value is not a C function.
 */
LUA_API lua_CFunction lua_tocfunction (lua_State *L, int idx) {
  StkId o = index2addr(L, idx);
  if (ttislcf(o)) return fvalue(o);
  else if (ttisCclosure(o))
    return clCvalue(o)->f;
  else return NULL;  /* not a C function */
}


/**
 * @brief Retrieves a pointer to the userdata or light userdata at the given stack index.
 *
 * This function checks the type of the Lua value at the specified stack index `idx`. 
 * If the value is a full userdata (LUA_TUSERDATA), it returns a pointer to the memory block 
 * associated with the userdata. If the value is a light userdata (LUA_TLIGHTUSERDATA), 
 * it returns the pointer stored in the light userdata. For any other type, the function 
 * returns `NULL`.
 *
 * @param L Pointer to the Lua state.
 * @param idx Stack index of the value to retrieve.
 * @return Pointer to the userdata or light userdata, or `NULL` if the value is not a userdata.
 */
LUA_API void *lua_touserdata (lua_State *L, int idx) {
  StkId o = index2addr(L, idx);
  switch (ttnov(o)) {
    case LUA_TUSERDATA: return getudatamem(uvalue(o));
    case LUA_TLIGHTUSERDATA: return pvalue(o);
    default: return NULL;
  }
}


/**
 * Retrieves a Lua thread (coroutine) from the Lua stack at the specified index.
 *
 * This function checks if the value at the given index `idx` in the Lua stack is a thread
 * (coroutine). If it is, the function returns the corresponding `lua_State` representing
 * the thread. If the value is not a thread, the function returns `NULL`.
 *
 * @param L The Lua state from which to retrieve the thread.
 * @param idx The stack index of the value to check.
 * @return Returns the `lua_State` representing the thread if the value is a thread,
 *         otherwise returns `NULL`.
 */
LUA_API lua_State *lua_tothread (lua_State *L, int idx) {
  StkId o = index2addr(L, idx);
  return (!ttisthread(o)) ? NULL : thvalue(o);
}


/**
 * @brief Converts the Lua value at the specified index to a generic C pointer.
 *
 * This function retrieves the Lua value at the given stack index `idx` and returns
 * a pointer to its internal representation, depending on the type of the value.
 * The returned pointer is type-cast to `const void*` to allow for generic handling.
 *
 * The function handles the following Lua types:
 * - `LUA_TTABLE`: Returns a pointer to the table.
 * - `LUA_TLCL`: Returns a pointer to the Lua closure.
 * - `LUA_TCCL`: Returns a pointer to the C closure.
 * - `LUA_TLCF`: Returns a pointer to the light C function.
 * - `LUA_TTHREAD`: Returns a pointer to the Lua thread.
 * - `LUA_TUSERDATA`: Returns a pointer to the userdata memory.
 * - `LUA_TLIGHTUSERDATA`: Returns the raw pointer stored in the light userdata.
 *
 * For all other types, the function returns `NULL`.
 *
 * @param L The Lua state.
 * @param idx The stack index of the Lua value to convert.
 * @return A `const void*` pointer to the internal representation of the Lua value,
 *         or `NULL` if the value cannot be converted to a pointer.
 */
LUA_API const void *lua_topointer (lua_State *L, int idx) {
  StkId o = index2addr(L, idx);
  switch (ttype(o)) {
    case LUA_TTABLE: return hvalue(o);
    case LUA_TLCL: return clLvalue(o);
    case LUA_TCCL: return clCvalue(o);
    case LUA_TLCF: return cast(void *, cast(size_t, fvalue(o)));
    case LUA_TTHREAD: return thvalue(o);
    case LUA_TUSERDATA: return getudatamem(uvalue(o));
    case LUA_TLIGHTUSERDATA: return pvalue(o);
    default: return NULL;
  }
}



/*
** push functions (C -> stack)
*/


LUA_API void lua_pushnil (lua_State *L) {
  lua_lock(L);
  setnilvalue(L->top);
  api_incr_top(L);
  lua_unlock(L);
}


/**
 * Pushes a number onto the Lua stack.
 *
 * This function takes a Lua state `L` and a number `n`, and pushes `n` onto the stack.
 * The stack is automatically incremented to accommodate the new value. The function
 * ensures thread safety by locking and unlocking the Lua state during the operation.
 *
 * @param L Pointer to the Lua state.
 * @param n The number to be pushed onto the stack.
 *
 * @note This function is part of the Lua C API and is used to interact with the Lua stack.
 *       It is typically used to pass numeric values from C to Lua.
 */
LUA_API void lua_pushnumber (lua_State *L, lua_Number n) {
  lua_lock(L);
  setfltvalue(L->top, n);
  api_incr_top(L);
  lua_unlock(L);
}


/**
 * Pushes an integer value onto the Lua stack.
 *
 * This function takes a Lua state `L` and an integer `n`, and pushes `n` onto 
 * the stack as a Lua integer. The stack's top pointer is incremented to reflect 
 * the new element. The function ensures thread safety by locking and unlocking 
 * the Lua state during the operation.
 *
 * @param L Pointer to the Lua state.
 * @param n The integer value to be pushed onto the stack.
 */
LUA_API void lua_pushinteger (lua_State *L, lua_Integer n) {
  lua_lock(L);
  setivalue(L->top, n);
  api_incr_top(L);
  lua_unlock(L);
}


/*
** Pushes on the stack a string with given length. Avoid using 's' when
** 'len' == 0 (as 's' can be NULL in that case), due to later use of
** 'memcmp' and 'memcpy'.
*/
LUA_API const char *lua_pushlstring (lua_State *L, const char *s, size_t len) {
  TString *ts;
  lua_lock(L);
  ts = (len == 0) ? luaS_new(L, "") : luaS_newlstr(L, s, len);
  setsvalue2s(L, L->top, ts);
  api_incr_top(L);
  luaC_checkGC(L);
  lua_unlock(L);
  return getstr(ts);
}


/**
 * Pushes a string onto the Lua stack. If the input string `s` is NULL, 
 * a nil value is pushed instead. If `s` is not NULL, a new internal 
 * copy of the string is created and pushed onto the stack. The function 
 * returns the address of the internal copy of the string.
 *
 * @param L The Lua state.
 * @param s The string to be pushed onto the stack. If NULL, a nil value is pushed.
 * @return The address of the internal copy of the string, or NULL if a nil value was pushed.
 */
LUA_API const char *lua_pushstring (lua_State *L, const char *s) {
  lua_lock(L);
  if (s == NULL)
    setnilvalue(L->top);
  else {
    TString *ts;
    ts = luaS_new(L, s);
    setsvalue2s(L, L->top, ts);
    s = getstr(ts);  /* internal copy's address */
  }
  api_incr_top(L);
  luaC_checkGC(L);
  lua_unlock(L);
  return s;
}


/**
 * Pushes a formatted string onto the Lua stack, using a `va_list` for variable arguments.
 * This function is similar to `lua_pushfstring`, but it accepts a `va_list` instead of a variable number of arguments.
 * 
 * @param L The Lua state.
 * @param fmt The format string, which follows the same rules as `printf` in C.
 * @param argp A `va_list` containing the arguments to be formatted into the string.
 * 
 * @return Returns a pointer to the formatted string that was pushed onto the stack.
 * 
 * @note This function locks the Lua state to ensure thread safety during the operation.
 * It also triggers a garbage collection check after pushing the string to the stack.
 */
LUA_API const char *lua_pushvfstring (lua_State *L, const char *fmt, va_list argp) {
  const char *ret;
  lua_lock(L);
  ret = luaO_pushvfstring(L, fmt, argp);
  luaC_checkGC(L);
  lua_unlock(L);
  return ret;
}


/**
 * Pushes a formatted string onto the Lua stack and returns the resulting string.
 *
 * This function formats a string according to the format specifier `fmt` and the
 * additional arguments provided. The formatted string is then pushed onto the Lua
 * stack. The function returns a pointer to the resulting string.
 *
 * @param L The Lua state.
 * @param fmt The format string, which may contain format specifiers like `%s`, `%d`, etc.
 * @param ... Variable arguments corresponding to the format specifiers in `fmt`.
 * @return A pointer to the formatted string pushed onto the Lua stack.
 *
 * @note This function is thread-safe as it locks the Lua state during execution.
 * @note The function ensures garbage collection is checked after the operation.
 */
LUA_API const char *lua_pushfstring (lua_State *L, const char *fmt, ...) {
  const char *ret;
  va_list argp;
  lua_lock(L);
  va_start(argp, fmt);
  ret = luaO_pushvfstring(L, fmt, argp);
  va_end(argp);
  luaC_checkGC(L);
  lua_unlock(L);
  return ret;
}


/**
 * Pushes a new C closure onto the Lua stack.
 *
 * This function creates a new C closure, which is a Lua function that can call a C function.
 * The closure can optionally capture up to `n` values from the Lua stack as upvalues.
 * If `n` is 0, the function behaves like `lua_pushcfunction`, pushing the C function directly
 * onto the stack without any upvalues.
 *
 * @param L The Lua state.
 * @param fn The C function to be associated with the closure.
 * @param n The number of upvalues to capture from the stack. These values are taken from the
 *          top `n` elements of the stack and are stored in the closure's upvalue array.
 *          If `n` is 0, no upvalues are captured.
 *
 * @note The function locks the Lua state during execution to ensure thread safety.
 * @note If `n` is greater than 0, the top `n` elements of the stack are consumed and
 *       replaced by the new closure.
 * @note The function checks for available stack space and triggers garbage collection if necessary.
 *
 * @see lua_pushcfunction
 * @see luaF_newCclosure
 * @see luaC_checkGC
 */
LUA_API void lua_pushcclosure (lua_State *L, lua_CFunction fn, int n) {
  lua_lock(L);
  if (n == 0) {
    setfvalue(L->top, fn);
  }
  else {
    CClosure *cl;
    api_checknelems(L, n);
    api_check(L, n <= MAXUPVAL, "upvalue index too large");
    cl = luaF_newCclosure(L, n);
    cl->f = fn;
    L->top -= n;
    while (n--) {
      setobj2n(L, &cl->upvalue[n], L->top + n);
      /* does not need barrier because closure is white */
    }
    setclCvalue(L, L->top, cl);
  }
  api_incr_top(L);
  luaC_checkGC(L);
  lua_unlock(L);
}


/**
 * Pushes a boolean value onto the Lua stack.
 *
 * This function takes a boolean value `b` (represented as an integer) and pushes
 * it onto the top of the Lua stack. The value is converted to a Lua boolean, where
 * any non-zero integer is considered `true` and zero is considered `false`.
 *
 * @param L Pointer to the Lua state.
 * @param b Integer representing the boolean value to push (0 for false, non-zero for true).
 *
 * @note This function locks the Lua state to ensure thread safety during the operation.
 *       The stack top is incremented after the value is pushed.
 */
LUA_API void lua_pushboolean (lua_State *L, int b) {
  lua_lock(L);
  setbvalue(L->top, (b != 0));  /* ensure that true is 1 */
  api_incr_top(L);
  lua_unlock(L);
}


/**
 * Pushes a light userdata onto the Lua stack.
 *
 * Light userdata is a raw pointer value that Lua treats as a first-class value.
 * It is not managed by Lua's garbage collector and does not have a metatable.
 * This function is useful for passing C pointers to Lua without creating a full
 * userdata object.
 *
 * @param L The Lua state.
 * @param p A pointer to the userdata to be pushed onto the stack.
 *
 * @note Light userdata is not tracked by Lua's garbage collector, so the
 *       caller must ensure the pointer remains valid for the duration of its
 *       use in Lua.
 */
LUA_API void lua_pushlightuserdata (lua_State *L, void *p) {
  lua_lock(L);
  setpvalue(L->top, p);
  api_incr_top(L);
  lua_unlock(L);
}


/**
 * Pushes the current thread (coroutine) onto the stack.
 * 
 * This function is used to push the Lua thread (coroutine) represented by the 
 * `lua_State` parameter `L` onto the stack. If the thread being pushed is the 
 * main thread of the Lua state, the function returns 1; otherwise, it returns 0.
 * 
 * @param L A pointer to the Lua state (thread) to be pushed onto the stack.
 * @return Returns 1 if the pushed thread is the main thread, otherwise returns 0.
 */
LUA_API int lua_pushthread (lua_State *L) {
  lua_lock(L);
  setthvalue(L, L->top, L);
  api_incr_top(L);
  lua_unlock(L);
  return (G(L)->mainthread == L);
}



/*
** get functions (Lua -> stack)
*/


static int auxgetstr (lua_State *L, const TValue *t, const char *k) {
  const TValue *slot;
  TString *str = luaS_new(L, k);
  if (luaV_fastget(L, t, str, slot, luaH_getstr)) {
    setobj2s(L, L->top, slot);
    api_incr_top(L);
  }
  else {
    setsvalue2s(L, L->top, str);
    api_incr_top(L);
    luaV_finishget(L, t, L->top - 1, L->top - 1, slot);
  }
  lua_unlock(L);
  return ttnov(L->top - 1);
}


/**
 * Retrieves the value of a global variable from the Lua state.
 *
 * This function looks up the global variable with the given name in the global 
 * environment of the Lua state `L`. The global environment is stored in the 
 * registry at the predefined index `LUA_RIDX_GLOBALS`. The function retrieves 
 * the value associated with the global variable and pushes it onto the stack.
 *
 * @param L A pointer to the Lua state.
 * @param name The name of the global variable to retrieve.
 * @return The type of the retrieved value, which corresponds to one of the 
 *         `LUA_T*` constants (e.g., `LUA_TNIL`, `LUA_TNUMBER`, `LUA_TSTRING`, etc.).
 *
 * @note This function locks the Lua state during its execution to ensure thread safety.
 */
LUA_API int lua_getglobal (lua_State *L, const char *name) {
  Table *reg = hvalue(&G(L)->l_registry);
  lua_lock(L);
  return auxgetstr(L, luaH_getint(reg, LUA_RIDX_GLOBALS), name);
}


/**
 * Pushes onto the stack the value of the table element at the given index.
 *
 * This function retrieves the value associated with the key at the top of the stack
 * from the table located at the specified index `idx` in the Lua stack. The key is
 * popped from the stack, and the resulting value is pushed onto the stack.
 *
 * @param L A pointer to the Lua state.
 * @param idx The stack index of the table.
 * @return The type of the pushed value, as returned by `ttnov`.
 *
 * @note This function locks the Lua state during its execution to ensure thread safety.
 *       The key must be a valid Lua value (e.g., string, number, etc.) and should be at
 *       the top of the stack before calling this function.
 */
LUA_API int lua_gettable (lua_State *L, int idx) {
  StkId t;
  lua_lock(L);
  t = index2addr(L, idx);
  luaV_gettable(L, t, L->top - 1, L->top - 1);
  lua_unlock(L);
  return ttnov(L->top - 1);
}


/**
 * Retrieves the value associated with the key `k` from the table at index `idx` on the Lua stack.
 * 
 * This function pushes the retrieved value onto the top of the stack and returns its type.
 * The table is located at the specified stack index `idx`, and the key `k` is used to access
 * the corresponding value within the table.
 * 
 * @param L A pointer to the Lua state.
 * @param idx The stack index of the table.
 * @param k The key to look up in the table.
 * @return The type of the retrieved value (as defined by `LUA_T*` constants).
 * 
 * @note This function locks the Lua state during execution to ensure thread safety.
 */
LUA_API int lua_getfield (lua_State *L, int idx, const char *k) {
  lua_lock(L);
  return auxgetstr(L, index2addr(L, idx), k);
}


/**
 * Retrieves the value at index `n` from the table or userdata at stack position `idx` in the Lua state `L`.
 * 
 * This function first attempts a fast access to the value using `luaV_fastget`. If successful, the value is pushed onto the stack.
 * If the fast access fails, it performs a standard table access by pushing the key `n` onto the stack and then calling `luaV_finishget`
 * to retrieve the value. The retrieved value is then pushed onto the stack.
 *
 * @param L The Lua state.
 * @param idx The stack index of the table or userdata.
 * @param n The integer key/index to access in the table or userdata.
 * @return The type of the retrieved value (as returned by `ttnov`).
 */
LUA_API int lua_geti (lua_State *L, int idx, lua_Integer n) {
  StkId t;
  const TValue *slot;
  lua_lock(L);
  t = index2addr(L, idx);
  if (luaV_fastget(L, t, n, slot, luaH_getint)) {
    setobj2s(L, L->top, slot);
    api_incr_top(L);
  }
  else {
    setivalue(L->top, n);
    api_incr_top(L);
    luaV_finishget(L, t, L->top - 1, L->top - 1, slot);
  }
  lua_unlock(L);
  return ttnov(L->top - 1);
}


/**
 * Retrieves the value associated with a key from a table without invoking any metamethods.
 *
 * This function performs a raw access to the table at the specified index `idx` on the Lua stack.
 * It retrieves the value associated with the key located at the top of the stack (`L->top - 1`).
 * The function does not invoke any metamethods, making it suitable for direct table access.
 *
 * @param L Pointer to the Lua state.
 * @param idx Index of the table on the Lua stack.
 * @return The type of the retrieved value (as an integer), which corresponds to one of the Lua type constants.
 *
 * @note The key is expected to be at the top of the stack (`L->top - 1`). After the operation, the key is replaced
 *       by the retrieved value on the stack.
 * @note The function assumes that the value at index `idx` is a table. If it is not, an error is raised.
 * @see lua_gettable for a version that invokes metamethods.
 */
LUA_API int lua_rawget (lua_State *L, int idx) {
  StkId t;
  lua_lock(L);
  t = index2addr(L, idx);
  api_check(L, ttistable(t), "table expected");
  setobj2s(L, L->top - 1, luaH_get(hvalue(t), L->top - 1));
  lua_unlock(L);
  return ttnov(L->top - 1);
}


/**
 * Retrieves the value associated with the integer key `n` from the table at index `idx` on the Lua stack.
 * This function performs a raw access, meaning it does not invoke any metamethods.
 *
 * @param L A pointer to the Lua state.
 * @param idx The stack index of the table.
 * @param n The integer key to look up in the table.
 * @return The type of the retrieved value, which is one of the `LUA_T*` constants.
 *
 * @note The table must be at the specified index `idx` on the stack. The function does not check for
 *       the existence of the key `n` in the table; if the key is not present, the function returns
 *       `LUA_TNIL`.
 *
 * @see lua_gettable
 * @see lua_rawget
 */
LUA_API int lua_rawgeti (lua_State *L, int idx, lua_Integer n) {
  StkId t;
  lua_lock(L);
  t = index2addr(L, idx);
  api_check(L, ttistable(t), "table expected");
  setobj2s(L, L->top, luaH_getint(hvalue(t), n));
  api_incr_top(L);
  lua_unlock(L);
  return ttnov(L->top - 1);
}


/**
 * Retrieves the value associated with a given pointer key from a table without invoking any metamethods.
 *
 * This function is similar to `lua_rawget`, but instead of using a Lua value as the key, it uses a raw pointer.
 * The function retrieves the value associated with the pointer `p` from the table at index `idx` on the Lua stack.
 * The lookup is performed directly on the table's hash part, bypassing any metamethods.
 *
 * @param L The Lua state.
 * @param idx The stack index of the table.
 * @param p The pointer key to look up in the table.
 * @return The type of the value retrieved from the table (as defined by `ttnov`).
 *
 * @note This function assumes that the value at index `idx` is a table. If it is not, an error is raised.
 * @note The function locks the Lua state during the operation to ensure thread safety.
 */
LUA_API int lua_rawgetp (lua_State *L, int idx, const void *p) {
  StkId t;
  TValue k;
  lua_lock(L);
  t = index2addr(L, idx);
  api_check(L, ttistable(t), "table expected");
  setpvalue(&k, cast(void *, p));
  setobj2s(L, L->top, luaH_get(hvalue(t), &k));
  api_incr_top(L);
  lua_unlock(L);
  return ttnov(L->top - 1);
}


/**
 * Creates a new table and pushes it onto the Lua stack.
 *
 * This function allocates a new table with optional preallocated storage for
 * array and hash parts. The table is then pushed onto the top of the Lua stack.
 *
 * @param L Pointer to the Lua state.
 * @param narray Number of preallocated array slots. If 0, no array part is preallocated.
 * @param nrec Number of preallocated hash slots. If 0, no hash part is preallocated.
 *
 * @note The table is created with minimal size if both `narray` and `nrec` are 0.
 *       If either `narray` or `nrec` is greater than 0, the table is resized to
 *       accommodate the specified number of elements.
 * @note This function locks the Lua state to ensure thread safety during table creation.
 * @see luaH_new
 * @see luaH_resize
 * @see luaC_checkGC
 */
LUA_API void lua_createtable (lua_State *L, int narray, int nrec) {
  Table *t;
  lua_lock(L);
  t = luaH_new(L);
  sethvalue(L, L->top, t);
  api_incr_top(L);
  if (narray > 0 || nrec > 0)
    luaH_resize(L, t, narray, nrec);
  luaC_checkGC(L);
  lua_unlock(L);
}


/**
 * Retrieves the metatable associated with the object at the given index on the Lua stack.
 *
 * This function checks the type of the object at `objindex` and retrieves its metatable
 * based on the object's type. If the object is a table or userdata, it directly accesses
 * the metatable stored in the object. For other types, it retrieves the metatable from
 * the global metatable registry.
 *
 * If a metatable is found, it is pushed onto the Lua stack, and the function returns 1.
 * If no metatable is associated with the object, the function returns 0 and does not
 * modify the stack.
 *
 * @param L The Lua state.
 * @param objindex The stack index of the object whose metatable is to be retrieved.
 * @return 1 if a metatable was found and pushed onto the stack, 0 otherwise.
 */
LUA_API int lua_getmetatable (lua_State *L, int objindex) {
  const TValue *obj;
  Table *mt;
  int res = 0;
  lua_lock(L);
  obj = index2addr(L, objindex);
  switch (ttnov(obj)) {
    case LUA_TTABLE:
      mt = hvalue(obj)->metatable;
      break;
    case LUA_TUSERDATA:
      mt = uvalue(obj)->metatable;
      break;
    default:
      mt = G(L)->mt[ttnov(obj)];
      break;
  }
  if (mt != NULL) {
    sethvalue(L, L->top, mt);
    api_incr_top(L);
    res = 1;
  }
  lua_unlock(L);
  return res;
}


/**
 * Retrieves the associated value (user value) from a full userdata at the given index on the Lua stack.
 *
 * This function is used to access the user value stored in a full userdata object. A full userdata is a
 * Lua object that can hold arbitrary C data and an associated Lua value (the user value). The user value
 * is typically used to store additional metadata or context for the userdata.
 *
 * @param L The Lua state.
 * @param idx The stack index of the full userdata from which to retrieve the user value.
 * @return The type of the retrieved user value, as returned by `ttnov`. The user value is pushed onto
 *         the top of the Lua stack.
 *
 * @note This function assumes that the object at the given index is a full userdata. If it is not,
 *       an error is raised via `api_check`.
 * @note The Lua stack is locked during the operation to ensure thread safety.
 */
LUA_API int lua_getuservalue (lua_State *L, int idx) {
  StkId o;
  lua_lock(L);
  o = index2addr(L, idx);
  api_check(L, ttisfulluserdata(o), "full userdata expected");
  getuservalue(L, uvalue(o), L->top);
  api_incr_top(L);
  lua_unlock(L);
  return ttnov(L->top - 1);
}


/*
** set functions (stack -> Lua)
*/

/*
** t[k] = value at the top of the stack (where 'k' is a string)
*/
static void auxsetstr (lua_State *L, const TValue *t, const char *k) {
  const TValue *slot;
  TString *str = luaS_new(L, k);
  api_checknelems(L, 1);
  if (luaV_fastset(L, t, str, slot, luaH_getstr, L->top - 1))
    L->top--;  /* pop value */
  else {
    setsvalue2s(L, L->top, str);  /* push 'str' (to make it a TValue) */
    api_incr_top(L);
    luaV_finishset(L, t, L->top - 1, L->top - 2, slot);
    L->top -= 2;  /* pop value and key */
  }
  lua_unlock(L);  /* lock done by caller */
}


/**
 * Sets the value of a global variable in the Lua environment.
 *
 * This function assigns the value at the top of the Lua stack to the global variable
 * named `name`. The value is then popped from the stack. The global variable is stored
 * in the global table, which is located in the Lua registry under the key `LUA_RIDX_GLOBALS`.
 *
 * @param L Pointer to the Lua state.
 * @param name The name of the global variable to set.
 *
 * @note This function locks the Lua state to ensure thread safety. The lock is released
 *       internally by the `auxsetstr` function.
 *
 * @see lua_getglobal
 * @see luaH_getint
 * @see auxsetstr
 */
LUA_API void lua_setglobal (lua_State *L, const char *name) {
  Table *reg = hvalue(&G(L)->l_registry);
  lua_lock(L);  /* unlock done in 'auxsetstr' */
  auxsetstr(L, luaH_getint(reg, LUA_RIDX_GLOBALS), name);
}


/**
 * Sets a table element at the specified index.
 *
 * This function assigns a value to a key in a table. The table is located at the given index `idx` 
 * in the Lua stack. The key and value are expected to be at the top of the stack, with the key 
 * being the second-to-top element and the value being the top element. After the assignment, both 
 * the key and value are popped from the stack.
 *
 * @param L A pointer to the Lua state.
 * @param idx The stack index of the table where the key-value pair will be assigned.
 *
 * @note This function assumes that the stack contains at least two elements (the key and the value) 
 *       above the table. It performs a check to ensure this condition is met.
 * @note The function locks the Lua state during the operation to ensure thread safety.
 */
LUA_API void lua_settable (lua_State *L, int idx) {
  StkId t;
  lua_lock(L);
  api_checknelems(L, 2);
  t = index2addr(L, idx);
  luaV_settable(L, t, L->top - 2, L->top - 1);
  L->top -= 2;  /* pop index and value */
  lua_unlock(L);
}


/**
 * Sets the value of a field in a table or userdata on the Lua stack.
 *
 * This function assigns a value to the field `k` in the table or userdata located
 * at the index `idx` on the Lua stack. The value to be assigned is the top element
 * of the stack, which is popped after the assignment.
 *
 * @param L The Lua state.
 * @param idx The stack index of the table or userdata.
 * @param k The key (field name) in the table or userdata.
 *
 * @note This function locks the Lua state to ensure thread safety. The lock is
 *       released internally by the `auxsetstr` function.
 *
 * Example usage:
 *


/**
 * Sets the value of a table element at a specific integer index.
 *
 * This function assigns the value at the top of the Lua stack to the element at index `n` in the table
 * located at position `idx` in the Lua stack. The table can be any Lua table, including arrays or
 * user-defined tables.
 *
 * The function first attempts to perform a fast assignment using `luaV_fastset`. If the fast assignment
 * fails, it falls back to a more general assignment mechanism using `luaV_finishset`. After the assignment,
 * the value (and key, if used) are popped from the stack.
 *
 * @param L Pointer to the Lua state.
 * @param idx Stack index of the table.
 * @param n Integer index of the element to set in the table.
 *
 * @note This function assumes that the value to be assigned is at the top of the Lua stack.
 * @note The function locks the Lua state during execution to ensure thread safety.
 */
LUA_API void lua_seti (lua_State *L, int idx, lua_Integer n) {
  StkId t;
  const TValue *slot;
  lua_lock(L);
  api_checknelems(L, 1);
  t = index2addr(L, idx);
  if (luaV_fastset(L, t, n, slot, luaH_getint, L->top - 1))
    L->top--;  /* pop value */
  else {
    setivalue(L->top, n);
    api_incr_top(L);
    luaV_finishset(L, t, L->top - 1, L->top - 2, slot);
    L->top -= 2;  /* pop value and key */
  }
  lua_unlock(L);
}


/**
 * Sets a key-value pair in a table without invoking any metamethods.
 *
 * This function assigns the value at the top of the Lua stack to the key 
 * that is just below it in the stack, within the table located at the 
 * specified index `idx`. The function performs a raw assignment, meaning 
 * it bypasses any metamethods (e.g., `__newindex`) that might be defined 
 * for the table.
 *
 * @param L The Lua state.
 * @param idx The stack index of the table where the key-value pair will be set.
 *
 * @note The function expects the table to be at index `idx`, the key to be 
 *       at the second topmost position in the stack (`L->top - 2`), and the 
 *       value to be at the top of the stack (`L->top - 1`). After the operation, 
 *       both the key and the value are popped from the stack.
 *
 * @warning This function does not trigger any table-related events or 
 *          metamethods, making it suitable for low-level operations where 
 *          such behavior is undesirable.
 */
LUA_API void lua_rawset (lua_State *L, int idx) {
  StkId o;
  TValue *slot;
  lua_lock(L);
  api_checknelems(L, 2);
  o = index2addr(L, idx);
  api_check(L, ttistable(o), "table expected");
  slot = luaH_set(L, hvalue(o), L->top - 2);
  setobj2t(L, slot, L->top - 1);
  invalidateTMcache(hvalue(o));
  luaC_barrierback(L, hvalue(o), L->top-1);
  L->top -= 2;
  lua_unlock(L);
}


/**
 * Sets the value at index `n` in the table located at stack index `idx` to the value at the top of the stack.
 * 
 * This function performs a raw set operation, meaning it does not invoke any metamethods. It directly 
 * assigns the value at the top of the stack to the specified index `n` in the table. The table must be 
 * located at the stack index `idx`, and the value to be assigned must be at the top of the stack.
 * 
 * After the assignment, the value is popped from the stack. This function is typically used for efficient 
 * manipulation of table elements without triggering metamethods.
 * 
 * @param L The Lua state.
 * @param idx The stack index of the table.
 * @param n The integer index in the table where the value will be set.
 * 
 * @note This function locks the Lua state during its execution to ensure thread safety.
 * @note The table must be valid and located at the specified stack index, otherwise an error is raised.
 * @note The value at the top of the stack is consumed (popped) after the assignment.
 */
LUA_API void lua_rawseti (lua_State *L, int idx, lua_Integer n) {
  StkId o;
  lua_lock(L);
  api_checknelems(L, 1);
  o = index2addr(L, idx);
  api_check(L, ttistable(o), "table expected");
  luaH_setint(L, hvalue(o), n, L->top - 1);
  luaC_barrierback(L, hvalue(o), L->top-1);
  L->top--;
  lua_unlock(L);
}


/**
 * Associates a value with a key in a table without invoking any metamethods.
 *
 * This function sets the value at the top of the Lua stack as the value associated
 * with the key `p` in the table located at index `idx` on the Lua stack. The key `p`
 * is treated as a light userdata pointer, and the association is performed without
 * invoking any metamethods (e.g., `__newindex` or `__index`).
 *
 * @param L The Lua state.
 * @param idx The stack index of the table.
 * @param p The key to associate with the value, treated as a light userdata pointer.
 *
 * @note The value to be associated with the key must be at the top of the Lua stack.
 *       After the operation, the value is popped from the stack.
 *
 * @remark This function is useful for raw manipulation of tables, bypassing any
 *         metamethods that might otherwise be triggered by table operations.
 */
LUA_API void lua_rawsetp (lua_State *L, int idx, const void *p) {
  StkId o;
  TValue k, *slot;
  lua_lock(L);
  api_checknelems(L, 1);
  o = index2addr(L, idx);
  api_check(L, ttistable(o), "table expected");
  setpvalue(&k, cast(void *, p));
  slot = luaH_set(L, hvalue(o), &k);
  setobj2t(L, slot, L->top - 1);
  luaC_barrierback(L, hvalue(o), L->top - 1);
  L->top--;
  lua_unlock(L);
}


/**
 * Sets the metatable for the object at the given index on the Lua stack.
 *
 * This function assigns a metatable to the object located at `objindex` in the Lua stack.
 * The metatable to be assigned is expected to be at the top of the stack. If the value at the
 * top of the stack is `nil`, the metatable of the object is removed (i.e., set to `NULL`).
 *
 * The function handles different types of objects:
 * - For tables (`LUA_TTABLE`), the metatable is directly assigned to the table's `metatable` field.
 * - For userdata (`LUA_TUSERDATA`), the metatable is assigned to the userdata's `metatable` field.
 * - For other types, the metatable is assigned to the corresponding entry in the global metatable
 *   array `G(L)->mt`.
 *
 * After assigning the metatable, the function ensures proper memory management by invoking
 * the garbage collector barriers (`luaC_objbarrier`) and finalizer checks (`luaC_checkfinalizer`)
 * when necessary.
 *
 * @param L The Lua state.
 * @param objindex The stack index of the object whose metatable is to be set.
 * @return Always returns 1, indicating that the operation was successful.
 */
LUA_API int lua_setmetatable (lua_State *L, int objindex) {
  TValue *obj;
  Table *mt;
  lua_lock(L);
  api_checknelems(L, 1);
  obj = index2addr(L, objindex);
  if (ttisnil(L->top - 1))
    mt = NULL;
  else {
    api_check(L, ttistable(L->top - 1), "table expected");
    mt = hvalue(L->top - 1);
  }
  switch (ttnov(obj)) {
    case LUA_TTABLE: {
      hvalue(obj)->metatable = mt;
      if (mt) {
        luaC_objbarrier(L, gcvalue(obj), mt);
        luaC_checkfinalizer(L, gcvalue(obj), mt);
      }
      break;
    }
    case LUA_TUSERDATA: {
      uvalue(obj)->metatable = mt;
      if (mt) {
        luaC_objbarrier(L, uvalue(obj), mt);
        luaC_checkfinalizer(L, gcvalue(obj), mt);
      }
      break;
    }
    default: {
      G(L)->mt[ttnov(obj)] = mt;
      break;
    }
  }
  L->top--;
  lua_unlock(L);
  return 1;
}


/**
 * Sets the associated value (user value) of a full userdata at the specified index.
 * 
 * This function takes a value from the top of the Lua stack and assigns it as the
 * user value of the full userdata located at the given index `idx`. The user value
 * is typically used to store additional data or metadata associated with the userdata.
 * 
 * @param L The Lua state.
 * @param idx The index of the full userdata in the stack.
 * 
 * @note The function expects a full userdata at the specified index. If the object
 *       at `idx` is not a full userdata, an error is raised.
 * 
 * @note The value to be set as the user value is taken from the top of the stack.
 *       After the operation, the stack is popped by one element.
 * 
 * @see lua_getuservalue
 * @see lua_newuserdata
 */
LUA_API void lua_setuservalue (lua_State *L, int idx) {
  StkId o;
  lua_lock(L);
  api_checknelems(L, 1);
  o = index2addr(L, idx);
  api_check(L, ttisfulluserdata(o), "full userdata expected");
  setuservalue(L, uvalue(o), L->top - 1);
  luaC_barrier(L, gcvalue(o), L->top - 1);
  L->top--;
  lua_unlock(L);
}


/*
** 'load' and 'call' functions (run Lua code)
*/


#define checkresults(L,na,nr) \
     api_check(L, (nr) == LUA_MULTRET || (L->ci->top - L->top >= (nr) - (na)), \
	"results from function overflow current stack size")


/**
 * Calls a Lua function with the specified number of arguments and expected results.
 * This function supports continuations, allowing the function to yield and resume
 * execution later if a continuation function is provided.
 *
 * @param L         The Lua state.
 * @param nargs     The number of arguments passed to the function.
 * @param nresults  The number of expected results from the function.
 * @param ctx       The context to be passed to the continuation function.
 * @param k         The continuation function to be called if the function yields.
 *                  If NULL, no continuation is used.
 *
 * @note This function cannot be used inside Lua hooks.
 * @note The Lua state must be in a normal state (LUA_OK) to perform the call.
 * @note If a continuation is provided and the Lua state is yieldable, the function
 *       will prepare the continuation and execute the call. Otherwise, it will
 *       execute the call without yielding.
 *
 * @see luaD_call
 * @see luaD_callnoyield
 */
LUA_API void lua_callk (lua_State *L, int nargs, int nresults,
                        lua_KContext ctx, lua_KFunction k) {
  StkId func;
  lua_lock(L);
  api_check(L, k == NULL || !isLua(L->ci),
    "cannot use continuations inside hooks");
  api_checknelems(L, nargs+1);
  api_check(L, L->status == LUA_OK, "cannot do calls on non-normal thread");
  checkresults(L, nargs, nresults);
  func = L->top - (nargs+1);
  if (k != NULL && L->nny == 0) {  /* need to prepare continuation? */
    L->ci->u.c.k = k;  /* save continuation */
    L->ci->u.c.ctx = ctx;  /* save context */
    luaD_call(L, func, nresults);  /* do the call */
  }
  else  /* no continuation or no yieldable */
    luaD_callnoyield(L, func, nresults);  /* just do the call */
  adjustresults(L, nresults);
  lua_unlock(L);
}



/*
** Execute a protected call.
*/
struct CallS {  /* data to 'f_call' */
  StkId func;
  int nresults;
};


/**
 * @brief Executes a Lua function call without yielding.
 *
 * This function is used to invoke a Lua function stored in a `CallS` structure.
 * It calls the function using `luaD_callnoyield`, which ensures that the function
 * is executed without yielding, meaning it will not pause or allow other
 * coroutines to run during its execution.
 *
 * @param L Pointer to the Lua state.
 * @param ud Pointer to user data, expected to be a `CallS` structure containing
 *           the function to call and the number of expected results.
 */
static void f_call (lua_State *L, void *ud) {
  struct CallS *c = cast(struct CallS *, ud);
  luaD_callnoyield(L, c->func, c->nresults);
}



/**
 * Calls a Lua function in a protected environment, optionally with a continuation.
 *
 * This function is used to call a Lua function in a way that catches any errors
 * that occur during its execution. It is similar to `lua_call`, but it provides
 * additional features such as error handling and support for continuations.
 *
 * @param L The Lua state.
 * @param nargs The number of arguments passed to the function.
 * @param nresults The number of results expected from the function.
 * @param errfunc The index of an error handler function in the stack, or 0 to use the default handler.
 * @param ctx A context value to be passed to the continuation function.
 * @param k A continuation function to be called if the function yields, or NULL if no continuation is needed.
 *
 * @return Returns `LUA_OK` if the function executes successfully. If an error occurs,
 *         it returns one of the error codes (e.g., `LUA_ERRRUN`, `LUA_ERRMEM`).
 *
 * @note If `k` is not NULL, the function supports continuations, allowing the
 *       function to yield and resume later. However, continuations cannot be used
 *       inside hooks (e.g., debug hooks).
 * @note The function must be called with a valid Lua state and a normal thread status.
 * @note The stack must have enough space for the function and its arguments.
 */
LUA_API int lua_pcallk (lua_State *L, int nargs, int nresults, int errfunc,
                        lua_KContext ctx, lua_KFunction k) {
  struct CallS c;
  int status;
  ptrdiff_t func;
  lua_lock(L);
  api_check(L, k == NULL || !isLua(L->ci),
    "cannot use continuations inside hooks");
  api_checknelems(L, nargs+1);
  api_check(L, L->status == LUA_OK, "cannot do calls on non-normal thread");
  checkresults(L, nargs, nresults);
  if (errfunc == 0)
    func = 0;
  else {
    StkId o = index2addr(L, errfunc);
    api_checkstackindex(L, errfunc, o);
    func = savestack(L, o);
  }
  c.func = L->top - (nargs+1);  /* function to be called */
  if (k == NULL || L->nny > 0) {  /* no continuation or no yieldable? */
    c.nresults = nresults;  /* do a 'conventional' protected call */
    status = luaD_pcall(L, f_call, &c, savestack(L, c.func), func);
  }
  else {  /* prepare continuation (call is already protected by 'resume') */
    CallInfo *ci = L->ci;
    ci->u.c.k = k;  /* save continuation */
    ci->u.c.ctx = ctx;  /* save context */
    /* save information for error recovery */
    ci->extra = savestack(L, c.func);
    ci->u.c.old_errfunc = L->errfunc;
    L->errfunc = func;
    setoah(ci->callstatus, L->allowhook);  /* save value of 'allowhook' */
    ci->callstatus |= CIST_YPCALL;  /* function can do error recovery */
    luaD_call(L, c.func, nresults);  /* do the call */
    ci->callstatus &= ~CIST_YPCALL;
    L->errfunc = ci->u.c.old_errfunc;
    status = LUA_OK;  /* if it is here, there were no errors */
  }
  adjustresults(L, nresults);
  lua_unlock(L);
  return status;
}


/**
 * Loads a Lua chunk from a reader function and compiles it into a Lua function.
 *
 * This function reads a Lua chunk using the provided `reader` function and compiles it into a Lua function.
 * The compiled function is pushed onto the stack. The `chunkname` is used for error messages and debug information.
 * The `mode` parameter controls whether the chunk is loaded as text, binary, or both.
 *
 * @param L The Lua state.
 * @param reader A function that provides the chunk data. It should return a pointer to the next chunk of data
 *               or NULL if there is no more data. The `data` parameter is passed to the reader function.
 * @param data User-defined data passed to the reader function.
 * @param chunkname The name of the chunk, used in error messages and debug information. If NULL, "?" is used.
 * @param mode A string that controls the loading mode: "t" for text, "b" for binary, or NULL for both.
 * @return LUA_OK if the chunk was successfully loaded and compiled. Otherwise, returns an error code.
 *
 * @note The function locks the Lua state during execution to ensure thread safety.
 * @note If the loaded function has at least one upvalue, the global table is set as its first upvalue.
 */
LUA_API int lua_load (lua_State *L, lua_Reader reader, void *data,
                      const char *chunkname, const char *mode) {
  ZIO z;
  int status;
  lua_lock(L);
  if (!chunkname) chunkname = "?";
  luaZ_init(L, &z, reader, data);
  status = luaD_protectedparser(L, &z, chunkname, mode);
  if (status == LUA_OK) {  /* no errors? */
    LClosure *f = clLvalue(L->top - 1);  /* get newly created function */
    if (f->nupvalues >= 1) {  /* does it have an upvalue? */
      /* get global table from registry */
      Table *reg = hvalue(&G(L)->l_registry);
      const TValue *gt = luaH_getint(reg, LUA_RIDX_GLOBALS);
      /* set global table as 1st upvalue of 'f' (may be LUA_ENV) */
      setobj(L, f->upvals[0]->v, gt);
      luaC_upvalbarrier(L, f->upvals[0]);
    }
  }
  lua_unlock(L);
  return status;
}


/**
 * Serializes a Lua function into a binary chunk and writes it using the provided writer function.
 *
 * This function takes a Lua function from the top of the stack and converts it into a binary
 * representation suitable for saving or transmitting. The binary chunk is written in a series of
 * calls to the `writer` function, which is responsible for handling the output (e.g., writing to
 * a file or memory buffer). The `data` parameter is passed to the `writer` function as context.
 *
 * @param L The Lua state from which the function is retrieved.
 * @param writer A callback function used to write the binary chunk. It should match the
 *               `lua_Writer` signature: `int (*lua_Writer)(lua_State *L, const void *p, size_t sz, void *ud)`.
 * @param data User-defined data passed to the `writer` function.
 * @param strip If non-zero, debug information (e.g., line numbers, local variable names) is
 *              removed from the binary chunk to reduce its size.
 * @return Returns 0 on success. If the top stack element is not a Lua function, returns 1.
 *
 * @note The function assumes that the Lua function to be dumped is at the top of the stack.
 *       The stack is not modified by this function.
 */
LUA_API int lua_dump (lua_State *L, lua_Writer writer, void *data, int strip) {
  int status;
  TValue *o;
  lua_lock(L);
  api_checknelems(L, 1);
  o = L->top - 1;
  if (isLfunction(o))
    status = luaU_dump(L, getproto(o), writer, data, strip);
  else
    status = 1;
  lua_unlock(L);
  return status;
}


/**
 * @brief Retrieves the current status of the Lua thread.
 *
 * This function returns the status of the Lua thread associated with the given 
 * `lua_State` pointer. The status indicates the result of the last operation 
 * executed by the Lua interpreter, such as a successful execution, a runtime 
 * error, or a yield from a coroutine.
 *
 * @param L Pointer to the Lua state.
 * @return The status of the Lua thread. Possible values include:
 *         - LUA_OK (0): No errors, successful execution.
 *         - LUA_YIELD: The thread is suspended (yielded).
 *         - LUA_ERRRUN: A runtime error occurred.
 *         - LUA_ERRSYNTAX: A syntax error occurred during compilation.
 *         - LUA_ERRMEM: Memory allocation error.
 *         - LUA_ERRERR: Error while running the error handler.
 */
LUA_API int lua_status (lua_State *L) {
  return L->status;
}


/*
** Garbage-collection function
*/

LUA_API int lua_gc (lua_State *L, int what, int data) {
  int res = 0;
  global_State *g;
  lua_lock(L);
  g = G(L);
  switch (what) {
    case LUA_GCSTOP: {
      g->gcrunning = 0;
      break;
    }
    case LUA_GCRESTART: {
      luaE_setdebt(g, 0);
      g->gcrunning = 1;
      break;
    }
    case LUA_GCCOLLECT: {
      luaC_fullgc(L, 0);
      break;
    }
    case LUA_GCCOUNT: {
      /* GC values are expressed in Kbytes: #bytes/2^10 */
      res = cast_int(gettotalbytes(g) >> 10);
      break;
    }
    case LUA_GCCOUNTB: {
      res = cast_int(gettotalbytes(g) & 0x3ff);
      break;
    }
    case LUA_GCSTEP: {
      l_mem debt = 1;  /* =1 to signal that it did an actual step */
      lu_byte oldrunning = g->gcrunning;
      g->gcrunning = 1;  /* allow GC to run */
      if (data == 0) {
        luaE_setdebt(g, -GCSTEPSIZE);  /* to do a "small" step */
        luaC_step(L);
      }
      else {  /* add 'data' to total debt */
        debt = cast(l_mem, data) * 1024 + g->GCdebt;
        luaE_setdebt(g, debt);
        luaC_checkGC(L);
      }
      g->gcrunning = oldrunning;  /* restore previous state */
      if (debt > 0 && g->gcstate == GCSpause)  /* end of cycle? */
        res = 1;  /* signal it */
      break;
    }
    case LUA_GCSETPAUSE: {
      res = g->gcpause;
      g->gcpause = data;
      break;
    }
    case LUA_GCSETSTEPMUL: {
      res = g->gcstepmul;
      if (data < 40) data = 40;  /* avoid ridiculous low values (and 0) */
      g->gcstepmul = data;
      break;
    }
    case LUA_GCISRUNNING: {
      res = g->gcrunning;
      break;
    }
    default: res = -1;  /* invalid option */
  }
  lua_unlock(L);
  return res;
}



/*
** miscellaneous functions
*/


LUA_API int lua_error (lua_State *L) {
  lua_lock(L);
  api_checknelems(L, 1);
  luaG_errormsg(L);
  /* code unreachable; will unlock when control actually leaves the kernel */
  return 0;  /* to avoid warnings */
}


/**
 * @brief Iterates over a table in the Lua stack.
 *
 * This function is used to traverse a table at the given index `idx` in the Lua stack.
 * It expects a key to be on the top of the stack (left there by a previous call to `lua_next`).
 * It pops the key and pushes the next key-value pair from the table onto the stack.
 * If there are no more elements, it removes the key and returns 0.
 *
 * @param L The Lua state.
 * @param idx The stack index of the table to iterate over.
 * @return Returns 1 if there are more elements to iterate over, 0 otherwise.
 *
 * @note The table must be at the specified index `idx`, and the key must be on the top of the stack.
 *       The function modifies the stack by popping the key and pushing the next key-value pair.
 */
LUA_API int lua_next (lua_State *L, int idx) {
  StkId t;
  int more;
  lua_lock(L);
  t = index2addr(L, idx);
  api_check(L, ttistable(t), "table expected");
  more = luaH_next(L, hvalue(t), L->top - 1);
  if (more) {
    api_incr_top(L);
  }
  else  /* no more elements */
    L->top -= 1;  /* remove key */
  lua_unlock(L);
  return more;
}


/**
 * Concatenates the top `n` values on the Lua stack and replaces them with the result.
 * 
 * This function performs string concatenation of the top `n` elements on the Lua stack.
 * If `n` is greater than or equal to 2, the function concatenates all `n` values using
 * the Lua concatenation operation and replaces them with the resulting string. If `n`
 * is 0, the function pushes an empty string onto the stack. If `n` is 1, the function
 * does nothing, as there is only one value to "concatenate."
 * 
 * The function ensures proper handling of garbage collection and thread safety by
 * locking and unlocking the Lua state during execution.
 * 
 * @param L The Lua state.
 * @param n The number of values to concatenate from the top of the stack.
 */
LUA_API void lua_concat (lua_State *L, int n) {
  lua_lock(L);
  api_checknelems(L, n);
  if (n >= 2) {
    luaV_concat(L, n);
  }
  else if (n == 0) {  /* push empty string */
    setsvalue2s(L, L->top, luaS_newlstr(L, "", 0));
    api_incr_top(L);
  }
  /* else n == 1; nothing to do */
  luaC_checkGC(L);
  lua_unlock(L);
}


/**
 * Computes the length of the value at the given index `idx` on the Lua stack and pushes the result onto the stack.
 * 
 * This function is equivalent to the Lua `#` operator. It handles different types of values as follows:
 * - For strings, it returns the number of bytes in the string.
 * - For tables, it returns the result of the `__len` metamethod if defined, otherwise it returns the length of the sequence part of the table.
 * - For userdata, it invokes the `__len` metamethod if defined.
 * - For other types, it raises an error.
 * 
 * The result is pushed onto the stack, and the stack top is incremented by one.
 * 
 * @param L The Lua state.
 * @param idx The index of the value on the stack whose length is to be computed.
 */
LUA_API void lua_len (lua_State *L, int idx) {
  StkId t;
  lua_lock(L);
  t = index2addr(L, idx);
  luaV_objlen(L, L->top, t);
  api_incr_top(L);
  lua_unlock(L);
}


/**
 * Retrieves the memory allocation function and its associated user data from the Lua state.
 *
 * This function returns the memory allocator function currently used by the Lua state `L`.
 * If the `ud` parameter is not `NULL`, it also stores the user data associated with the allocator
 * in the provided pointer.
 *
 * @param L The Lua state from which to retrieve the allocator function.
 * @param ud A pointer to store the user data associated with the allocator. If `NULL`, the user data is not retrieved.
 * @return The memory allocator function (`lua_Alloc`) currently used by the Lua state.
 *
 * @note This function is thread-safe as it locks the Lua state during the operation.
 */
LUA_API lua_Alloc lua_getallocf (lua_State *L, void **ud) {
  lua_Alloc f;
  lua_lock(L);
  if (ud) *ud = G(L)->ud;
  f = G(L)->frealloc;
  lua_unlock(L);
  return f;
}


/**
 * Sets the memory allocator function for the Lua state.
 *
 * This function allows the user to specify a custom memory allocator function
 * to be used by the Lua state. The allocator function is responsible for
 * allocating, reallocating, and freeing memory blocks used by Lua. The user
 * can also provide a user data pointer (`ud`) that will be passed to the
 * allocator function whenever it is called.
 *
 * @param L The Lua state for which the allocator function is being set.
 * @param f The memory allocator function of type `lua_Alloc`. This function
 *          should handle memory allocation, reallocation, and deallocation.
 * @param ud A user-defined pointer that will be passed to the allocator function
 *           whenever it is called. This can be used to pass context or state
 *           information to the allocator.
 *
 * @note This function is thread-safe and locks the Lua state during the
 *       modification of the allocator function and user data.
 */
LUA_API void lua_setallocf (lua_State *L, lua_Alloc f, void *ud) {
  lua_lock(L);
  G(L)->ud = ud;
  G(L)->frealloc = f;
  lua_unlock(L);
}


/**
 * Allocates a new block of user data and pushes it onto the Lua stack.
 * 
 * This function creates a new user data object of the specified size and associates it with the Lua state.
 * User data is a raw memory block that can be used to store arbitrary C data, allowing it to be manipulated
 * from Lua scripts. The allocated memory is managed by Lua's garbage collector.
 * 
 * @param L The Lua state in which the user data is created.
 * @param size The size in bytes of the user data block to allocate.
 * @return A pointer to the allocated memory block, which can be used to store C data.
 * 
 * @note The user data object is pushed onto the Lua stack, so it will be accessible from Lua scripts.
 *       The caller is responsible for ensuring that the memory is used correctly and does not cause
 *       memory corruption or leaks.
 */
LUA_API void *lua_newuserdata (lua_State *L, size_t size) {
  Udata *u;
  lua_lock(L);
  u = luaS_newudata(L, size);
  setuvalue(L, L->top, u);
  api_incr_top(L);
  luaC_checkGC(L);
  lua_unlock(L);
  return getudatamem(u);
}



/**
 * Retrieves the nth upvalue of a closure and optionally returns additional information.
 *
 * This function takes a stack index `fi` pointing to a closure and an integer `n` specifying
 * the upvalue index (1-based). It retrieves the nth upvalue of the closure and stores its
 * value in `*val`. Depending on the type of closure (C or Lua), it may also return the
 * closure owner (`*owner`), the upvalue itself (`*uv`), or the upvalue's name.
 *
 * @param fi    The stack index pointing to the closure.
 * @param n     The 1-based index of the upvalue to retrieve.
 * @param val   Pointer to store the value of the nth upvalue.
 * @param owner Pointer to store the C closure owner (if applicable).
 * @param uv    Pointer to store the upvalue itself (if applicable).
 * @return      The name of the upvalue if it exists, "(*no name)" if the upvalue has no name,
 *              or NULL if the upvalue index is out of bounds or `fi` does not point to a closure.
 */
static const char *aux_upvalue (StkId fi, int n, TValue **val,
                                CClosure **owner, UpVal **uv) {
  switch (ttype(fi)) {
    case LUA_TCCL: {  /* C closure */
      CClosure *f = clCvalue(fi);
      if (!(1 <= n && n <= f->nupvalues)) return NULL;
      *val = &f->upvalue[n-1];
      if (owner) *owner = f;
      return "";
    }
    case LUA_TLCL: {  /* Lua closure */
      LClosure *f = clLvalue(fi);
      TString *name;
      Proto *p = f->p;
      if (!(1 <= n && n <= p->sizeupvalues)) return NULL;
      *val = f->upvals[n-1]->v;
      if (uv) *uv = f->upvals[n - 1];
      name = p->upvalues[n-1].name;
      return (name == NULL) ? "(*no name)" : getstr(name);
    }
    default: return NULL;  /* not a closure */
  }
}


/**
 * Retrieves the name and value of the nth upvalue of the function at the specified index on the Lua stack.
 * 
 * This function is used to access the upvalues (external local variables captured by a function) of a Lua function.
 * The function at `funcindex` on the stack is inspected, and the nth upvalue is retrieved. If the upvalue exists,
 * its value is pushed onto the Lua stack, and its name is returned. If the upvalue does not exist, NULL is returned.
 * 
 * @param L The Lua state.
 * @param funcindex The index of the function on the Lua stack whose upvalue is to be retrieved.
 * @param n The index of the upvalue to retrieve (1-based).
 * @return The name of the nth upvalue if it exists, or NULL if the upvalue does not exist.
 */
LUA_API const char *lua_getupvalue (lua_State *L, int funcindex, int n) {
  const char *name;
  TValue *val = NULL;  /* to avoid warnings */
  lua_lock(L);
  name = aux_upvalue(index2addr(L, funcindex), n, &val, NULL, NULL);
  if (name) {
    setobj2s(L, L->top, val);
    api_incr_top(L);
  }
  lua_unlock(L);
  return name;
}


/**
 * Sets the value of the nth upvalue of the function at the given index in the Lua stack.
 *
 * This function retrieves the nth upvalue of the function located at `funcindex` in the Lua stack
 * and sets its value to the value currently at the top of the stack. The function then removes the
 * top element from the stack. If the upvalue exists, the function returns the name of the upvalue;
 * otherwise, it returns NULL.
 *
 * @param L The Lua state.
 * @param funcindex The index of the function in the Lua stack whose upvalue is to be set.
 * @param n The index of the upvalue to set (1-based).
 * @return The name of the upvalue if it exists, or NULL if the upvalue does not exist.
 *
 * @note This function locks the Lua state during execution to ensure thread safety.
 * @note The function assumes that the value to be assigned to the upvalue is at the top of the stack.
 * @note If the upvalue is associated with a closure, a barrier is set to ensure proper garbage collection.
 */
LUA_API const char *lua_setupvalue (lua_State *L, int funcindex, int n) {
  const char *name;
  TValue *val = NULL;  /* to avoid warnings */
  CClosure *owner = NULL;
  UpVal *uv = NULL;
  StkId fi;
  lua_lock(L);
  fi = index2addr(L, funcindex);
  api_checknelems(L, 1);
  name = aux_upvalue(fi, n, &val, &owner, &uv);
  if (name) {
    L->top--;
    setobj(L, val, L->top);
    if (owner) { luaC_barrier(L, owner, L->top); }
    else if (uv) { luaC_upvalbarrier(L, uv); }
  }
  lua_unlock(L);
  return name;
}


/**
 * Retrieves a reference to the nth upvalue of a Lua function.
 *
 * This function takes a Lua state `L`, a stack index `fidx` pointing to a Lua function,
 * an integer `n` representing the upvalue index, and a pointer to a pointer to an LClosure `pf`.
 * It returns a pointer to the nth upvalue of the specified Lua function.
 *
 * @param L The Lua state.
 * @param fidx The stack index of the Lua function.
 * @param n The index of the upvalue to retrieve (1-based).
 * @param pf If not NULL, stores the LClosure of the function in the provided pointer.
 * @return A pointer to the nth upvalue of the Lua function.
 *
 * @throws api_check Throws an error if the object at `fidx` is not a Lua function.
 * @throws api_check Throws an error if `n` is out of bounds for the function's upvalues.
 */
static UpVal **getupvalref (lua_State *L, int fidx, int n, LClosure **pf) {
  LClosure *f;
  StkId fi = index2addr(L, fidx);
  api_check(L, ttisLclosure(fi), "Lua function expected");
  f = clLvalue(fi);
  api_check(L, (1 <= n && n <= f->p->sizeupvalues), "invalid upvalue index");
  if (pf) *pf = f;
  return &f->upvals[n - 1];  /* get its upvalue pointer */
}


/**
 * Retrieves the unique identifier for the specified upvalue of a function.
 *
 * This function returns a pointer to the upvalue at index `n` of the function located at stack index `fidx` in the Lua state `L`. 
 * The function can be either a Lua closure (LUA_TLCL) or a C closure (LUA_TCCL). For Lua closures, the upvalue reference is 
 * directly retrieved. For C closures, the upvalue pointer is calculated based on the closure's upvalue array.
 *
 * @param L Pointer to the Lua state.
 * @param fidx Stack index of the function whose upvalue is to be retrieved.
 * @param n Index of the upvalue to retrieve (1-based).
 * @return Pointer to the upvalue if successful, or NULL if the function is not a closure or the upvalue index is invalid.
 *
 * @throws Raises an error if `fidx` does not refer to a closure or if `n` is out of bounds for the closure's upvalues.
 */
LUA_API void *lua_upvalueid (lua_State *L, int fidx, int n) {
  StkId fi = index2addr(L, fidx);
  switch (ttype(fi)) {
    case LUA_TLCL: {  /* lua closure */
      return *getupvalref(L, fidx, n, NULL);
    }
    case LUA_TCCL: {  /* C closure */
      CClosure *f = clCvalue(fi);
      api_check(L, 1 <= n && n <= f->nupvalues, "invalid upvalue index");
      return &f->upvalue[n - 1];
    }
    default: {
      api_check(L, 0, "closure expected");
      return NULL;
    }
  }
}


/**
 * Joins two upvalues in a Lua closure.
 *
 * This function connects the upvalue at index `n1` in the closure at stack index `fidx1`
 * to the upvalue at index `n2` in the closure at stack index `fidx2`. After this operation,
 * both upvalues will reference the same value, and the reference count of the joined upvalue
 * will be incremented.
 *
 * @param L The Lua state.
 * @param fidx1 The stack index of the first closure.
 * @param n1 The index of the upvalue in the first closure to be joined.
 * @param fidx2 The stack index of the second closure.
 * @param n2 The index of the upvalue in the second closure to be joined.
 *
 * @note This function assumes that the closures and upvalues exist and are valid.
 *       It decrements the reference count of the original upvalue in the first closure
 *       and increments the reference count of the upvalue in the second closure.
 *       If the joined upvalue is open, it marks it as touched to ensure proper
 *       garbage collection behavior.
 */
LUA_API void lua_upvaluejoin (lua_State *L, int fidx1, int n1,
                                            int fidx2, int n2) {
  LClosure *f1;
  UpVal **up1 = getupvalref(L, fidx1, n1, &f1);
  UpVal **up2 = getupvalref(L, fidx2, n2, NULL);
  luaC_upvdeccount(L, *up1);
  *up1 = *up2;
  (*up1)->refcount++;
  if (upisopen(*up1)) (*up1)->u.open.touched = 1;
  luaC_upvalbarrier(L, *up1);
}


