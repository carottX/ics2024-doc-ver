/*
** $Id: lbitlib.c,v 1.30 2015/11/11 19:08:09 roberto Exp $
** Standard library for bitwise operations
** See Copyright Notice in lua.h
*/

#define lbitlib_c
#define LUA_LIB

#include "lprefix.h"


#include "lua.h"

#include "lauxlib.h"
#include "lualib.h"


#if defined(LUA_COMPAT_BITLIB)		/* { */


#define pushunsigned(L,n)	lua_pushinteger(L, (lua_Integer)(n))
#define checkunsigned(L,i)	((lua_Unsigned)luaL_checkinteger(L,i))


/* number of bits to consider in a number */
#if !defined(LUA_NBITS)
#define LUA_NBITS	32
#endif


/*
** a lua_Unsigned with its first LUA_NBITS bits equal to 1. (Shift must
** be made in two parts to avoid problems when LUA_NBITS is equal to the
** number of bits in a lua_Unsigned.)
*/
#define ALLONES		(~(((~(lua_Unsigned)0) << (LUA_NBITS - 1)) << 1))


/* macro to trim extra bits */
#define trim(x)		((x) & ALLONES)


/* builds a number with 'n' ones (1 <= n <= LUA_NBITS) */
#define mask(n)		(~((ALLONES << 1) << ((n) - 1)))



/**
 * Performs a bitwise AND operation on all unsigned integer arguments passed to the Lua state.
 *
 * This function retrieves all arguments from the Lua stack, treats them as unsigned integers,
 * and computes their bitwise AND. The result is then trimmed (if necessary) and returned.
 *
 * @param L A pointer to the Lua state, which contains the arguments to be processed.
 * @return The result of the bitwise AND operation on all arguments, trimmed to fit within
 *         the range of an unsigned integer.
 *
 * @details The function starts by initializing the result `r` with all bits set to 1 (i.e., ~0).
 *          It then iterates over all arguments on the Lua stack, converting each to an unsigned
 *          integer using `checkunsigned`, and performs a bitwise AND with the current result.
 *          Finally, the result is trimmed using the `trim` function to ensure it fits within
 *          the valid range of an unsigned integer before being returned.
 */
static lua_Unsigned andaux (lua_State *L) {
  int i, n = lua_gettop(L);
  lua_Unsigned r = ~(lua_Unsigned)0;
  for (i = 1; i <= n; i++)
    r &= checkunsigned(L, i);
  return trim(r);
}


/**
 * Performs a bitwise AND operation on the top two unsigned integers on the Lua stack.
 * This function retrieves the two unsigned integers from the Lua stack using the `andaux` helper function,
 * performs a bitwise AND operation on them, and then pushes the result back onto the Lua stack as an unsigned integer.
 * 
 * @param L Pointer to the Lua state.
 * @return Returns 1, indicating that one value (the result of the bitwise AND operation) has been pushed onto the Lua stack.
 */
static int b_and (lua_State *L) {
  lua_Unsigned r = andaux(L);
  pushunsigned(L, r);
  return 1;
}


/**
 * @brief Performs a bitwise AND operation on the top two values on the Lua stack and pushes the result as a boolean.
 *
 * This function retrieves the top two values from the Lua stack, performs a bitwise AND operation
 * using the `andaux` function, and then pushes the result as a boolean value onto the stack. 
 * The boolean value is `true` if the result of the AND operation is non-zero, and `false` otherwise.
 *
 * @param L The Lua state from which the top two values are retrieved and to which the result is pushed.
 * @return int Returns 1, indicating that one value (the boolean result) has been pushed onto the stack.
 */
static int b_test (lua_State *L) {
  lua_Unsigned r = andaux(L);
  lua_pushboolean(L, r != 0);
  return 1;
}


/**
 * Performs a bitwise OR operation on all the unsigned integer arguments passed to the function.
 *
 * This function retrieves all the arguments from the Lua stack, treats them as unsigned integers,
 * and computes their bitwise OR. The result is then pushed back onto the Lua stack as an unsigned integer.
 *
 * @param L A pointer to the Lua state.
 * @return Returns 1, indicating that one value (the result of the bitwise OR operation) is pushed onto the Lua stack.
 *
 * @note The function expects all arguments to be convertible to unsigned integers. If an argument cannot be
 *       converted, the behavior is undefined.
 */
static int b_or (lua_State *L) {
  int i, n = lua_gettop(L);
  lua_Unsigned r = 0;
  for (i = 1; i <= n; i++)
    r |= checkunsigned(L, i);
  pushunsigned(L, trim(r));
  return 1;
}


/**
 * Performs a bitwise XOR operation on all the unsigned integer arguments passed to the function.
 * 
 * This function iterates over all the arguments provided on the Lua stack, treating each as an 
 * unsigned integer. It computes the bitwise XOR of all these values and pushes the result back 
 * onto the Lua stack as an unsigned integer.
 *
 * @param L A pointer to the Lua state.
 * @return Returns 1, indicating that one value (the result of the XOR operation) has been pushed 
 *         onto the Lua stack.
 *
 * @note The function assumes that all arguments are valid unsigned integers. If an argument is 
 *       not an unsigned integer, the behavior is determined by the `checkunsigned` function.
 */
static int b_xor (lua_State *L) {
  int i, n = lua_gettop(L);
  lua_Unsigned r = 0;
  for (i = 1; i <= n; i++)
    r ^= checkunsigned(L, i);
  pushunsigned(L, trim(r));
  return 1;
}


/**
 * Performs a bitwise NOT operation on an unsigned integer value.
 *
 * This function retrieves an unsigned integer from the Lua stack, applies a bitwise NOT operation
 * to it, trims the result to fit within the range of an unsigned integer, and pushes the result
 * back onto the Lua stack.
 *
 * @param L The Lua state from which the unsigned integer is retrieved and to which the result is pushed.
 * @return Returns 1, indicating that one value (the result of the bitwise NOT operation) has been pushed
 *         onto the Lua stack.
 */
static int b_not (lua_State *L) {
  lua_Unsigned r = ~checkunsigned(L, 1);
  pushunsigned(L, trim(r));
  return 1;
}


/**
 * Performs a bitwise shift operation on an unsigned integer value.
 *
 * This function shifts the unsigned integer `r` by `i` bits. If `i` is negative,
 * the shift is a right shift; if `i` is positive, the shift is a left shift.
 * The result is trimmed to ensure it fits within the bit size defined by `LUA_NBITS`.
 * If the shift amount `i` is greater than or equal to `LUA_NBITS`, the result is set to 0.
 * The resulting value is then pushed onto the Lua stack as an unsigned integer.
 *
 * @param L The Lua state.
 * @param r The unsigned integer value to be shifted.
 * @param i The number of bits to shift. Negative values indicate a right shift,
 *          while positive values indicate a left shift.
 * @return Returns 1, indicating that one value (the shifted result) has been pushed onto the Lua stack.
 */
static int b_shift (lua_State *L, lua_Unsigned r, lua_Integer i) {
  if (i < 0) {  /* shift right? */
    i = -i;
    r = trim(r);
    if (i >= LUA_NBITS) r = 0;
    else r >>= i;
  }
  else {  /* shift left */
    if (i >= LUA_NBITS) r = 0;
    else r <<= i;
    r = trim(r);
  }
  pushunsigned(L, r);
  return 1;
}


/**
 * Performs a bitwise left shift operation on an unsigned integer.
 *
 * This function takes two arguments from the Lua stack:
 * 1. An unsigned integer value to be shifted.
 * 2. An integer specifying the number of bits to shift the value to the left.
 *
 * The function returns the result of the left shift operation as an integer.
 *
 * @param L The Lua state.
 * @return The result of the bitwise left shift operation.
 */
static int b_lshift (lua_State *L) {
  return b_shift(L, checkunsigned(L, 1), luaL_checkinteger(L, 2));
}


/**
 * Performs a bitwise right shift operation on an unsigned integer.
 *
 * This function retrieves two arguments from the Lua stack: 
 * 1. An unsigned integer value to be shifted.
 * 2. An integer specifying the number of bits to shift the value to the right.
 *
 * The function then calls `b_shift` with the unsigned integer and the negated 
 * shift value (to indicate a right shift). The result of the shift operation 
 * is returned to the Lua state.
 *
 * @param L The Lua state from which the arguments are retrieved and to which 
 *          the result is pushed.
 * @return The number of return values pushed to the Lua stack (1 in this case).
 */
static int b_rshift (lua_State *L) {
  return b_shift(L, checkunsigned(L, 1), -luaL_checkinteger(L, 2));
}


/**
 * Performs an arithmetic right shift operation on an unsigned integer.
 * 
 * This function takes two arguments from the Lua stack:
 * 1. An unsigned integer `r` to be shifted.
 * 2. An integer `i` specifying the number of bits to shift.
 *
 * If `i` is negative or `r` does not have the most significant bit set,
 * the function delegates to `b_shift` to perform a logical right shift.
 * 
 * If `i` is positive and `r` has the most significant bit set, the function
 * performs an arithmetic right shift. This means that the sign bit (most significant bit)
 * is preserved during the shift. If `i` is greater than or equal to the number of bits
 * in the integer type (`LUA_NBITS`), the result is set to all ones (i.e., -1 in two's complement).
 * Otherwise, the result is computed by shifting `r` right by `i` bits and then ORing
 * with the complement of the result of shifting a mask of all ones right by `i` bits,
 * effectively preserving the sign bit.
 *
 * The result is then pushed onto the Lua stack.
 *
 * @param L The Lua state.
 * @return Returns 1, indicating that one value (the result of the shift) has been pushed onto the Lua stack.
 */
static int b_arshift (lua_State *L) {
  lua_Unsigned r = checkunsigned(L, 1);
  lua_Integer i = luaL_checkinteger(L, 2);
  if (i < 0 || !(r & ((lua_Unsigned)1 << (LUA_NBITS - 1))))
    return b_shift(L, r, -i);
  else {  /* arithmetic shift for 'negative' number */
    if (i >= LUA_NBITS) r = ALLONES;
    else
      r = trim((r >> i) | ~(trim(~(lua_Unsigned)0) >> i));  /* add signal bit */
    pushunsigned(L, r);
    return 1;
  }
}


/**
 * Performs a bitwise rotation on an unsigned integer value.
 *
 * This function takes an unsigned integer value `r` from the Lua stack and a rotation distance `d`.
 * It rotates the bits of `r` by `d` positions to the left. If `d` is negative, the rotation is effectively
 * to the right. The rotation is performed modulo `LUA_NBITS` to ensure the result is within the valid
 * range of bits.
 *
 * @param L The Lua state from which the unsigned integer value is retrieved and to which the result is pushed.
 * @param d The number of positions to rotate the bits. Positive values rotate left, negative values rotate right.
 *          The actual rotation distance is `d % LUA_NBITS`.
 *
 * @return Returns 1, indicating that one value (the rotated result) has been pushed onto the Lua stack.
 *
 * @note The function avoids undefined behavior by not performing a shift operation when the rotation distance
 *       is zero. The result is always trimmed to ensure it fits within `LUA_NBITS`.
 */
static int b_rot (lua_State *L, lua_Integer d) {
  lua_Unsigned r = checkunsigned(L, 1);
  int i = d & (LUA_NBITS - 1);  /* i = d % NBITS */
  r = trim(r);
  if (i != 0)  /* avoid undefined shift of LUA_NBITS when i == 0 */
    r = (r << i) | (r >> (LUA_NBITS - i));
  pushunsigned(L, trim(r));
  return 1;
}


/**
 * Rotates the bits of the integer at the top of the Lua stack to the left by a specified number of positions.
 * 
 * This function retrieves the rotation count from the second argument on the Lua stack using `luaL_checkinteger`.
 * It then calls the `b_rot` function to perform the actual bit rotation, passing the Lua state and the rotation count.
 * 
 * @param L The Lua state containing the stack with the integer to rotate and the rotation count.
 * @return The number of results returned to Lua, which is determined by the `b_rot` function.
 */
static int b_lrot (lua_State *L) {
  return b_rot(L, luaL_checkinteger(L, 2));
}


/**
 * Rotates the bits of the integer at the specified stack index to the right.
 * 
 * This function retrieves the integer value from the second stack position (index 2) 
 * and uses it as the number of bits to rotate the integer at the first stack position 
 * (index 1) to the right. The rotation is performed by calling the `b_rot` function 
 * with the negated value of the retrieved integer, effectively rotating the bits 
 * in the opposite direction (right instead of left).
 *
 * @param L The Lua state.
 * @return The number of results pushed onto the stack, which is 1 (the rotated integer).
 */
static int b_rrot (lua_State *L) {
  return b_rot(L, -luaL_checkinteger(L, 2));
}


/*
** get field and width arguments for field-manipulation functions,
** checking whether they are valid.
** ('luaL_error' called without 'return' to avoid later warnings about
** 'width' being used uninitialized.)
*/
static int fieldargs (lua_State *L, int farg, int *width) {
  lua_Integer f = luaL_checkinteger(L, farg);
  lua_Integer w = luaL_optinteger(L, farg + 1, 1);
  luaL_argcheck(L, 0 <= f, farg, "field cannot be negative");
  luaL_argcheck(L, 0 < w, farg + 1, "width must be positive");
  if (f + w > LUA_NBITS)
    luaL_error(L, "trying to access non-existent bits");
  *width = (int)w;
  return (int)f;
}


/**
 * Extracts a bit field from an unsigned integer and pushes the result onto the Lua stack.
 *
 * This function takes an unsigned integer `r` and a bit field specification, extracts the specified
 * bit field from `r`, and pushes the result as an unsigned integer onto the Lua stack.
 *
 * @param L The Lua state.
 * @return Returns 1, indicating that one value (the extracted bit field) has been pushed onto the Lua stack.
 *
 * The function performs the following steps:
 * 1. Retrieves the unsigned integer `r` from the first Lua argument using `checkunsigned` and trims it.
 * 2. Parses the bit field specification from the second Lua argument using `fieldargs`, which returns
 *    the starting bit position `f` and the width `w` of the bit field.
 * 3. Extracts the bit field by shifting `r` right by `f` bits and applying a mask of width `w`.
 * 4. Pushes the extracted bit field as an unsigned integer onto the Lua stack using `pushunsigned`.
 */
static int b_extract (lua_State *L) {
  int w;
  lua_Unsigned r = trim(checkunsigned(L, 1));
  int f = fieldargs(L, 2, &w);
  r = (r >> f) & mask(w);
  pushunsigned(L, r);
  return 1;
}


/**
 * Replaces a specific bit field within an unsigned integer value.
 *
 * This function takes an unsigned integer `r` and replaces a contiguous bit field
 * of width `w` starting at position `f` with the value `v`. The bit field is
 * specified by its width `w` and its starting position `f` (0-based index).
 * The value `v` is masked to fit within the specified bit field width.
 *
 * @param L The Lua state.
 * @return Returns 1, pushing the modified unsigned integer value onto the Lua stack.
 *
 * @param[in] r The original unsigned integer value whose bit field is to be replaced.
 * @param[in] v The new value to be placed into the specified bit field.
 * @param[in] w The width of the bit field to be replaced.
 * @param[in] f The starting position (0-based index) of the bit field within `r`.
 *
 * @details The function performs the following steps:
 * 1. Extracts and trims the unsigned integer values `r` and `v` from the Lua stack.
 * 2. Determines the bit field width `w` and starting position `f` from the Lua stack.
 * 3. Creates a mask `m` based on the bit field width `w`.
 * 4. Clears the specified bit field in `r` using the mask and position.
 * 5. Inserts the masked value `v` into the cleared bit field.
 * 6. Pushes the modified value `r` back onto the Lua stack.
 */
static int b_replace (lua_State *L) {
  int w;
  lua_Unsigned r = trim(checkunsigned(L, 1));
  lua_Unsigned v = trim(checkunsigned(L, 2));
  int f = fieldargs(L, 3, &w);
  lua_Unsigned m = mask(w);
  r = (r & ~(m << f)) | ((v & m) << f);
  pushunsigned(L, r);
  return 1;
}


static const luaL_Reg bitlib[] = {
  {"arshift", b_arshift},
  {"band", b_and},
  {"bnot", b_not},
  {"bor", b_or},
  {"bxor", b_xor},
  {"btest", b_test},
  {"extract", b_extract},
  {"lrotate", b_lrot},
  {"lshift", b_lshift},
  {"replace", b_replace},
  {"rrotate", b_rrot},
  {"rshift", b_rshift},
  {NULL, NULL}
};



/**
 * @brief Opens the bit32 library and registers its functions with the Lua state.
 *
 * This function is the entry point for the bit32 library in Lua. It creates a new
 * library table containing the bitwise operations defined in the `bitlib` array
 * and pushes it onto the Lua stack. The library provides functions for performing
 * bitwise operations on 32-bit integers, such as AND, OR, XOR, NOT, shifts, and
 * rotations.
 *
 * @param L A pointer to the Lua state.
 * @return Returns 1, indicating that the library table has been pushed onto the stack.
 */
LUAMOD_API int luaopen_bit32 (lua_State *L) {
  luaL_newlib(L, bitlib);
  return 1;
}


#else					/* }{ */


LUAMOD_API int luaopen_bit32 (lua_State *L) {
  return luaL_error(L, "library 'bit32' has been deprecated");
}

#endif					/* } */
