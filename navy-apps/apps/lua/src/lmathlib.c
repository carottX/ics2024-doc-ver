/*
** $Id: lmathlib.c,v 1.119 2016/12/22 13:08:50 roberto Exp $
** Standard mathematical library
** See Copyright Notice in lua.h
*/

#define lmathlib_c
#define LUA_LIB

#include "lprefix.h"


#include <stdlib.h>
#include <mymath.h>

#include "lua.h"

#include "lauxlib.h"
#include "lualib.h"


#undef PI
#define PI	(l_mathop(3.141592653589793238462643383279502884))


#if !defined(l_rand)		/* { */
#if defined(LUA_USE_POSIX)
#define l_rand()	random()
#define l_srand(x)	srandom(x)
#define L_RANDMAX	2147483647	/* (2^31 - 1), following POSIX */
#else
#define l_rand()	rand()
#define l_srand(x)	srand(x)
#define L_RANDMAX	RAND_MAX
#endif
#endif				/* } */


static int math_abs (lua_State *L) {
  if (lua_isinteger(L, 1)) {
    lua_Integer n = lua_tointeger(L, 1);
    if (n < 0) n = (lua_Integer)(0u - (lua_Unsigned)n);
    lua_pushinteger(L, n);
  }
  else
    lua_pushnumber(L, l_mathop(fabs)(luaL_checknumber(L, 1)));
  return 1;
}

/**
 * Computes the sine of a given number and pushes the result onto the Lua stack.
 *
 * This function retrieves a number from the Lua stack at index 1, computes its sine
 * using the standard `sin` function from the math library, and then pushes the result
 * back onto the Lua stack. The function assumes that the input is a valid number.
 *
 * @param L Pointer to the Lua state.
 * @return Returns 1, indicating that one value (the sine result) has been pushed onto the stack.
 */
static int math_sin (lua_State *L) {
  lua_pushnumber(L, l_mathop(sin)(luaL_checknumber(L, 1)));
  return 1;
}

/**
 * Computes the cosine of a given number and pushes the result onto the Lua stack.
 *
 * This method retrieves a single numeric argument from the Lua stack, computes its cosine
 * using the standard C `cos` function, and then pushes the result back onto the Lua stack.
 * The method assumes the argument is a valid number and will raise an error if it is not.
 *
 * @param L A pointer to the Lua state from which the argument is retrieved and to which
 *          the result is pushed.
 * @return The number of values returned to Lua, which is always 1 (the computed cosine).
 */
static int math_cos (lua_State *L) {
  lua_pushnumber(L, l_mathop(cos)(luaL_checknumber(L, 1)));
  return 1;
}

/**
 * Computes the tangent of a given angle in radians and pushes the result onto the Lua stack.
 *
 * This function retrieves a single numeric argument from the Lua stack, which represents
 * an angle in radians. It then computes the tangent of this angle using the standard C
 * library `tan` function (or its long double variant if applicable). The result is pushed
 * back onto the Lua stack as a number.
 *
 * @param L Pointer to the Lua state.
 * @return Returns 1, indicating that one value (the tangent result) has been pushed onto the Lua stack.
 */
static int math_tan (lua_State *L) {
  lua_pushnumber(L, l_mathop(tan)(luaL_checknumber(L, 1)));
  return 1;
}

/**
 * Computes the arcsine (inverse sine) of a given number and pushes the result onto the Lua stack.
 *
 * This function takes a single numeric argument from the Lua stack, computes its arcsine using
 * the standard C `asin` function, and then pushes the result back onto the Lua stack. The input
 * value must be within the range [-1, 1], as the arcsine function is only defined for this interval.
 * If the input value is outside this range, the behavior is undefined.
 *
 * @param L A pointer to the Lua state.
 * @return Returns 1, indicating that one value (the arcsine result) has been pushed onto the stack.
 */
static int math_asin (lua_State *L) {
  lua_pushnumber(L, l_mathop(asin)(luaL_checknumber(L, 1)));
  return 1;
}

/**
 * Computes the arc cosine of a given number and pushes the result onto the Lua stack.
 *
 * This method takes a single numeric argument from the Lua stack, computes its arc cosine
 * using the standard C library function `acos`, and pushes the result back onto the Lua stack.
 * The arc cosine is the inverse of the cosine function, returning the angle in radians whose
 * cosine is the specified number. The input value must be in the range [-1, 1], otherwise
 * an error will be raised.
 *
 * @param L A pointer to the Lua state.
 * @return Returns 1, indicating that one value (the result of the arc cosine) has been pushed
 *         onto the Lua stack.
 */
static int math_acos (lua_State *L) {
  lua_pushnumber(L, l_mathop(acos)(luaL_checknumber(L, 1)));
  return 1;
}

/**
 * Computes the arctangent of `y / x` (in radians) and pushes the result onto the Lua stack.
 * This function uses the `atan2` function from the C standard library to handle the calculation,
 * which ensures the correct quadrant for the result based on the signs of `y` and `x`.
 *
 * @param L The Lua state.
 * @return Returns 1, as the result is pushed onto the Lua stack.
 *
 * @details The function expects at least one argument (`y`) and optionally accepts a second argument (`x`).
 *          If `x` is not provided, it defaults to 1. The result is the angle in radians between the positive
 *          x-axis and the point `(x, y)`.
 *
 * @param[in] y The y-coordinate of the point.
 * @param[in] x The x-coordinate of the point (defaults to 1 if not provided).
 * @param[out] The result of `atan2(y, x)` is pushed onto the Lua stack.
 */
static int math_atan (lua_State *L) {
  lua_Number y = luaL_checknumber(L, 1);
  lua_Number x = luaL_optnumber(L, 2, 1);
  lua_pushnumber(L, l_mathop(atan2)(y, x));
  return 1;
}


/**
 * Converts the Lua value at the top of the stack to an integer and pushes the result back onto the stack.
 * If the value cannot be converted to an integer, it pushes `nil` instead.
 *
 * @param L The Lua state.
 * @return Always returns 1, indicating that one value (the converted integer or `nil`) has been pushed onto the stack.
 *
 * @details This function attempts to convert the Lua value at index 1 of the stack to a Lua integer
 * using `lua_tointegerx`. If the conversion is successful, the integer is pushed onto the stack.
 * If the value is not convertible to an integer, the function ensures that the value exists using
 * `luaL_checkany` and then pushes `nil` onto the stack to indicate that the conversion failed.
 */
static int math_toint (lua_State *L) {
  int valid;
  lua_Integer n = lua_tointegerx(L, 1, &valid);
  if (valid)
    lua_pushinteger(L, n);
  else {
    luaL_checkany(L, 1);
    lua_pushnil(L);  /* value is not convertible to integer */
  }
  return 1;
}


/**
 * Pushes a numeric value onto the Lua stack as either an integer or a float.
 * 
 * This function checks if the given Lua number `d` can be represented as an integer
 * without loss of precision. If it can, the value is pushed onto the Lua stack as an
 * integer using `lua_pushinteger`. If the number cannot be represented as an integer,
 * it is pushed onto the stack as a floating-point number using `lua_pushnumber`.
 *
 * @param L The Lua state.
 * @param d The Lua number to be pushed onto the stack.
 */
static void pushnumint (lua_State *L, lua_Number d) {
  lua_Integer n;
  if (lua_numbertointeger(d, &n))  /* does 'd' fit in an integer? */
    lua_pushinteger(L, n);  /* result is integer */
  else
    lua_pushnumber(L, d);  /* result is float */
}


/**
 * Computes the floor of a number and pushes the result onto the Lua stack.
 * 
 * This function takes a single argument from the Lua stack, which can be either
 * an integer or a floating-point number. If the argument is an integer, it is
 * already considered its own floor, and the function simply leaves it on the stack.
 * If the argument is a floating-point number, the function computes its floor
 * (the largest integer less than or equal to the number) and pushes the result
 * as an integer onto the Lua stack.
 * 
 * @param L The Lua state.
 * @return Always returns 1, indicating that one value (the floor result) is pushed onto the stack.
 */
static int math_floor (lua_State *L) {
  if (lua_isinteger(L, 1))
    lua_settop(L, 1);  /* integer is its own floor */
  else {
    lua_Number d = l_mathop(floor)(luaL_checknumber(L, 1));
    pushnumint(L, d);
  }
  return 1;
}


/**
 * Computes the ceiling of a number and pushes the result onto the Lua stack.
 * 
 * This function takes a single argument from the Lua stack, which can be either
 * an integer or a floating-point number. If the argument is an integer, it is
 * already considered its own ceiling, and the function simply leaves it on the
 * stack. If the argument is a floating-point number, the function computes its
 * ceiling using the `ceil` function from the C standard library, converts the
 * result to an integer if possible, and pushes the result onto the Lua stack.
 * 
 * @param L Pointer to the Lua state.
 * @return Always returns 1, indicating that one value (the ceiling of the input)
 *         has been pushed onto the Lua stack.
 */
static int math_ceil (lua_State *L) {
  if (lua_isinteger(L, 1))
    lua_settop(L, 1);  /* integer is its own ceil */
  else {
    lua_Number d = l_mathop(ceil)(luaL_checknumber(L, 1));
    pushnumint(L, d);
  }
  return 1;
}


/**
 * Computes the floating-point remainder of the division of two numbers.
 * 
 * This function takes two arguments from the Lua stack and calculates the remainder
 * of the division of the first argument by the second argument. If both arguments
 * are integers, it uses the integer modulus operation (`%`). If either argument is
 * a floating-point number, it uses the `fmod` function from the math library to
 * compute the remainder.
 * 
 * Special cases:
 * - If the second argument is zero, an error is raised with the message "zero".
 * - If the second argument is -1 and the first argument is the minimum integer value,
 *   the function returns 0 to avoid overflow.
 * 
 * @param L The Lua state from which the arguments are retrieved and to which the result is pushed.
 * @return Returns 1, indicating that one value (the remainder) has been pushed onto the Lua stack.
 */
static int math_fmod (lua_State *L) {
  if (lua_isinteger(L, 1) && lua_isinteger(L, 2)) {
    lua_Integer d = lua_tointeger(L, 2);
    if ((lua_Unsigned)d + 1u <= 1u) {  /* special cases: -1 or 0 */
      luaL_argcheck(L, d != 0, 2, "zero");
      lua_pushinteger(L, 0);  /* avoid overflow with 0x80000... / -1 */
    }
    else
      lua_pushinteger(L, lua_tointeger(L, 1) % d);
  }
  else
    lua_pushnumber(L, l_mathop(fmod)(luaL_checknumber(L, 1),
                                     luaL_checknumber(L, 2)));
  return 1;
}


/*
** next function does not use 'modf', avoiding problems with 'double*'
** (which is not compatible with 'float*') when lua_Number is not
** 'double'.
*/
static int math_modf (lua_State *L) {
  if (lua_isinteger(L ,1)) {
    lua_settop(L, 1);  /* number is its own integer part */
    lua_pushnumber(L, 0);  /* no fractional part */
  }
  else {
    lua_Number n = luaL_checknumber(L, 1);
    /* integer part (rounds toward zero) */
    lua_Number ip = (n < 0) ? l_mathop(ceil)(n) : l_mathop(floor)(n);
    pushnumint(L, ip);
    /* fractional part (test needed for inf/-inf) */
    lua_pushnumber(L, (n == ip) ? l_mathop(0.0) : (n - ip));
  }
  return 2;
}


/**
 * Computes the square root of a given number and pushes the result onto the Lua stack.
 *
 * This method retrieves a number from the Lua stack at index 1, computes its square root
 * using the standard C `sqrt` function, and then pushes the result back onto the Lua stack.
 *
 * @param L A pointer to the Lua state.
 * @return Always returns 1, indicating that one value (the square root result) has been pushed onto the stack.
 */
static int math_sqrt (lua_State *L) {
  lua_pushnumber(L, l_mathop(sqrt)(luaL_checknumber(L, 1)));
  return 1;
}


/**
 * Compares two unsigned integers and pushes a boolean result onto the Lua stack.
 *
 * This function retrieves two integer values from the Lua stack, treats them as 
 * unsigned integers, and compares them. It then pushes a boolean value onto the 
 * Lua stack indicating whether the first integer is less than the second integer 
 * when interpreted as unsigned values.
 *
 * @param L The Lua state.
 * @return Returns 1, indicating that one boolean value has been pushed onto the stack.
 */
static int math_ult (lua_State *L) {
  lua_Integer a = luaL_checkinteger(L, 1);
  lua_Integer b = luaL_checkinteger(L, 2);
  lua_pushboolean(L, (lua_Unsigned)a < (lua_Unsigned)b);
  return 1;
}

/**
 * Computes the logarithm of a number with an optional base.
 *
 * This function is designed to be called from Lua and expects at least one argument:
 * the number `x` for which the logarithm is to be computed. An optional second argument
 * specifies the base of the logarithm. If the base is not provided, the natural logarithm
 * (base `e`) is computed.
 *
 * The function supports the following bases:
 * - If no base is provided, the natural logarithm (`log(x)`) is computed.
 * - If the base is `2`, the binary logarithm (`log2(x)`) is computed (only if not using C89).
 * - If the base is `10`, the common logarithm (`log10(x)`) is computed.
 * - For any other base, the logarithm is computed using the change of base formula:
 *   `log(x) / log(base)`.
 *
 * The result is pushed onto the Lua stack and the function returns `1` to indicate
 * that one value has been returned to Lua.
 *
 * @param L The Lua state.
 * @return int Always returns `1` to indicate one return value on the Lua stack.
 */
static int math_log (lua_State *L) {
  lua_Number x = luaL_checknumber(L, 1);
  lua_Number res;
  if (lua_isnoneornil(L, 2))
    res = l_mathop(log)(x);
  else {
    lua_Number base = luaL_checknumber(L, 2);
#if !defined(LUA_USE_C89)
    if (base == l_mathop(2.0))
      res = l_mathop(log2)(x); else
#endif
    if (base == l_mathop(10.0))
      res = l_mathop(log10)(x);
    else
      res = l_mathop(log)(x)/l_mathop(log)(base);
  }
  lua_pushnumber(L, res);
  return 1;
}

/**
 * Computes the exponential of a given number and pushes the result onto the Lua stack.
 *
 * This function takes a single numeric argument from the Lua stack, computes its
 * exponential value using the standard mathematical `exp` function, and pushes the
 * result back onto the Lua stack. The function assumes that the argument is a valid
 * number and will raise an error if it is not.
 *
 * @param L A pointer to the Lua state.
 * @return Returns 1, indicating that one value (the result of the exponential computation)
 *         has been pushed onto the Lua stack.
 */
static int math_exp (lua_State *L) {
  lua_pushnumber(L, l_mathop(exp)(luaL_checknumber(L, 1)));
  return 1;
}

/**
 * Converts an angle from radians to degrees.
 *
 * This function takes a single numeric argument representing an angle in radians,
 * converts it to degrees, and pushes the result onto the Lua stack. The conversion
 * is performed using the formula: degrees = radians * (180.0 / PI).
 *
 * @param L A pointer to the Lua state.
 * @return Returns 1, indicating that one value (the converted angle in degrees)
 *         has been pushed onto the stack.
 */
static int math_deg (lua_State *L) {
  lua_pushnumber(L, luaL_checknumber(L, 1) * (l_mathop(180.0) / PI));
  return 1;
}

/**
 * Converts an angle from degrees to radians.
 *
 * This function takes a single numeric argument representing an angle in degrees,
 * converts it to radians by multiplying it by (π / 180), and pushes the result
 * onto the Lua stack.
 *
 * @param L The Lua state.
 * @return Returns 1, indicating that one value (the result in radians) is pushed onto the stack.
 */
static int math_rad (lua_State *L) {
  lua_pushnumber(L, luaL_checknumber(L, 1) * (PI / l_mathop(180.0)));
  return 1;
}


/**
 * Finds the minimum value among the given arguments and pushes it onto the Lua stack.
 * 
 * This function expects at least one argument. It iterates through all the arguments,
 * compares them using the Lua less-than operator (`<`), and identifies the smallest value.
 * The minimum value is then pushed onto the Lua stack as the result.
 * 
 * @param L A pointer to the Lua state.
 * @return Returns 1, indicating that one value (the minimum) has been pushed onto the stack.
 * 
 * @throws Lua error if no arguments are provided.
 */
static int math_min (lua_State *L) {
  int n = lua_gettop(L);  /* number of arguments */
  int imin = 1;  /* index of current minimum value */
  int i;
  luaL_argcheck(L, n >= 1, 1, "value expected");
  for (i = 2; i <= n; i++) {
    if (lua_compare(L, i, imin, LUA_OPLT))
      imin = i;
  }
  lua_pushvalue(L, imin);
  return 1;
}


/**
 * Finds and returns the maximum value among the given arguments.
 *
 * This function takes a variable number of arguments from the Lua stack and
 * compares them to determine the maximum value. It expects at least one argument
 * to be provided. The function iterates through the arguments, comparing each
 * one to the current maximum using the Lua comparison operator `LUA_OPLT` (less than).
 * The index of the maximum value is tracked, and the corresponding value is pushed
 * onto the Lua stack as the result.
 *
 * @param L A pointer to the Lua state.
 * @return Returns 1, indicating that one value (the maximum) is pushed onto the Lua stack.
 * @throws Lua error if no arguments are provided.
 */
static int math_max (lua_State *L) {
  int n = lua_gettop(L);  /* number of arguments */
  int imax = 1;  /* index of current maximum value */
  int i;
  luaL_argcheck(L, n >= 1, 1, "value expected");
  for (i = 2; i <= n; i++) {
    if (lua_compare(L, imax, i, LUA_OPLT))
      imax = i;
  }
  lua_pushvalue(L, imax);
  return 1;
}

/*
** This function uses 'double' (instead of 'lua_Number') to ensure that
** all bits from 'l_rand' can be represented, and that 'RANDMAX + 1.0'
** will keep full precision (ensuring that 'r' is always less than 1.0.)
*/
static int math_random (lua_State *L) {
  lua_Integer low, up;
#if LUA_FLOAT_TYPE == LUA_FLOAT_INT
  int r = 0;
#else
  double r = (double)l_rand() * (1.0 / ((double)L_RANDMAX + 1.0));
#endif
  switch (lua_gettop(L)) {  /* check number of arguments */
    case 0: {  /* no arguments */
      lua_pushnumber(L, (lua_Number)r);  /* Number between 0 and 1 */
      return 1;
    }
    case 1: {  /* only upper limit */
      low = 1;
      up = luaL_checkinteger(L, 1);
      break;
    }
    case 2: {  /* lower and upper limits */
      low = luaL_checkinteger(L, 1);
      up = luaL_checkinteger(L, 2);
      break;
    }
    default: return luaL_error(L, "wrong number of arguments");
  }
  /* random integer in the interval [low, up] */
  luaL_argcheck(L, low <= up, 1, "interval is empty");
  luaL_argcheck(L, low >= 0 || up <= LUA_MAXINTEGER + low, 1,
                   "interval too large");
#if LUA_FLOAT_TYPE == LUA_FLOAT_INT
  r = rand() % (up - low) + low;
#else
  r *= (double)(up - low) + 1.0;
#endif
  lua_pushinteger(L, (lua_Integer)r + low);
  return 1;
}


/**
 * Sets the seed for the random number generator used by the Lua environment.
 * This function takes a single numeric argument from the Lua stack, converts it
 * to an unsigned integer, and uses it to seed the random number generator.
 * After setting the seed, the first random number generated is discarded to
 * avoid undesirable correlations in the sequence of random numbers.
 *
 * @param L The Lua state from which the seed value is retrieved.
 * @return Always returns 0, indicating no values are pushed onto the Lua stack.
 */
static int math_randomseed (lua_State *L) {
  l_srand((unsigned int)(lua_Integer)luaL_checknumber(L, 1));
  (void)l_rand(); /* discard first value to avoid undesirable correlations */
  return 0;
}


/**
 * Determines the type of the Lua value at the top of the stack and pushes a corresponding string
 * ("integer" or "float") onto the stack if the value is a number. If the value is not a number,
 * it checks that there is at least one argument and pushes `nil` onto the stack.
 *
 * @param L The Lua state.
 * @return Always returns 1, indicating that one value has been pushed onto the stack.
 */
static int math_type (lua_State *L) {
  if (lua_type(L, 1) == LUA_TNUMBER) {
      if (lua_isinteger(L, 1))
        lua_pushliteral(L, "integer");
      else
        lua_pushliteral(L, "float");
  }
  else {
    luaL_checkany(L, 1);
    lua_pushnil(L);
  }
  return 1;
}


/*
** {==================================================================
** Deprecated functions (for compatibility only)
** ===================================================================
*/
#if defined(LUA_COMPAT_MATHLIB)

/**
 * Computes the hyperbolic cosine of a given number and pushes the result onto the Lua stack.
 *
 * This function retrieves a single numeric argument from the Lua stack, computes its hyperbolic
 * cosine using the `cosh` function from the math library, and then pushes the result back onto
 * the Lua stack. The function expects exactly one numeric argument and returns 1 to indicate
 * that one value (the result) has been pushed onto the stack.
 *
 * @param L A pointer to the Lua state.
 * @return int The number of values pushed onto the Lua stack (always 1).
 */
static int math_cosh (lua_State *L) {
  lua_pushnumber(L, l_mathop(cosh)(luaL_checknumber(L, 1)));
  return 1;
}

/**
 * Computes the hyperbolic sine of a given number and pushes the result onto the Lua stack.
 *
 * This method retrieves a single numeric argument from the Lua stack, computes its hyperbolic sine
 * using the `sinh` function from the math library, and pushes the result back onto the Lua stack.
 *
 * @param L A pointer to the Lua state.
 * @return Returns 1, indicating that one value (the result of the hyperbolic sine computation) has been pushed onto the stack.
 */
static int math_sinh (lua_State *L) {
  lua_pushnumber(L, l_mathop(sinh)(luaL_checknumber(L, 1)));
  return 1;
}

/**
 * Computes the hyperbolic tangent of a given number and pushes the result onto the Lua stack.
 *
 * This method takes a single numeric argument from the Lua stack, computes its hyperbolic tangent
 * using the `tanh` function from the C standard math library, and then pushes the result back onto
 * the Lua stack. The hyperbolic tangent function is defined as:
 * 
 *     tanh(x) = sinh(x) / cosh(x)
 *
 * where `sinh(x)` is the hyperbolic sine and `cosh(x)` is the hyperbolic cosine of `x`.
 *
 * @param L A pointer to the Lua state.
 * @return Returns 1, indicating that one value (the result of `tanh`) has been pushed onto the stack.
 */
static int math_tanh (lua_State *L) {
  lua_pushnumber(L, l_mathop(tanh)(luaL_checknumber(L, 1)));
  return 1;
}

/**
 * Computes the power of a number raised to another number.
 * 
 * This method takes two numbers, `x` and `y`, from the Lua stack, computes `x` raised to the power of `y`,
 * and pushes the result back onto the Lua stack.
 *
 * @param L A pointer to the Lua state.
 * @return Returns 1, indicating that one value (the result of the computation) has been pushed onto the Lua stack.
 *
 * @note The method expects two numeric arguments on the Lua stack. If the arguments are not numbers,
 *       a Lua error will be raised.
 */
static int math_pow (lua_State *L) {
  lua_Number x = luaL_checknumber(L, 1);
  lua_Number y = luaL_checknumber(L, 2);
  lua_pushnumber(L, l_mathop(pow)(x, y));
  return 1;
}

/**
 * Extracts the significand and exponent of a floating-point number.
 *
 * This function takes a single floating-point number from the Lua stack,
 * breaks it into its significand and exponent components using the `frexp`
 * function from the C standard library, and pushes both values onto the Lua stack.
 *
 * The significand is a floating-point number in the range [0.5, 1.0) or zero,
 * and the exponent is an integer such that the original number can be
 * reconstructed as `significand * 2^exponent`.
 *
 * @param L The Lua state.
 * @return 2, indicating that two values (significand and exponent) have been pushed onto the stack.
 */
static int math_frexp (lua_State *L) {
  int e;
  lua_pushnumber(L, l_mathop(frexp)(luaL_checknumber(L, 1), &e));
  lua_pushinteger(L, e);
  return 2;
}

/**
 * Multiplies a floating-point number `x` by 2 raised to the power of `ep` (i.e., x * 2^ep).
 * This function is a Lua C API wrapper for the `ldexp` function from the C standard library.
 *
 * @param L A pointer to the Lua state.
 * @return Returns 1, pushing the result of the operation onto the Lua stack.
 *
 * @details
 * This function expects two arguments on the Lua stack:
 * 1. `x` (number): The floating-point number to be scaled.
 * 2. `ep` (integer): The exponent to which 2 is raised.
 *
 * The function retrieves these arguments, computes `x * 2^ep` using the `ldexp` function,
 * and pushes the result back onto the Lua stack.
 */
static int math_ldexp (lua_State *L) {
  lua_Number x = luaL_checknumber(L, 1);
  int ep = (int)luaL_checkinteger(L, 2);
  lua_pushnumber(L, l_mathop(ldexp)(x, ep));
  return 1;
}

/**
 * Computes the base-10 logarithm of a given number and pushes the result onto the Lua stack.
 *
 * This function takes a single numeric argument from the Lua stack, computes its base-10 logarithm
 * using the `log10` function from the standard math library, and then pushes the result back onto
 * the Lua stack. The function returns 1 to indicate that one value (the result) has been pushed onto
 * the stack.
 *
 * @param L A pointer to the Lua state.
 * @return Returns 1, indicating that one value has been pushed onto the Lua stack.
 */
static int math_log10 (lua_State *L) {
  lua_pushnumber(L, l_mathop(log10)(luaL_checknumber(L, 1)));
  return 1;
}

#endif
/* }================================================================== */



static const luaL_Reg mathlib[] = {
  {"abs",   math_abs},
  {"acos",  math_acos},
  {"asin",  math_asin},
  {"atan",  math_atan},
  {"ceil",  math_ceil},
  {"cos",   math_cos},
  {"deg",   math_deg},
  {"exp",   math_exp},
  {"tointeger", math_toint},
  {"floor", math_floor},
  {"fmod",   math_fmod},
  {"ult",   math_ult},
  {"log",   math_log},
  {"max",   math_max},
  {"min",   math_min},
  {"modf",   math_modf},
  {"rad",   math_rad},
  {"random",     math_random},
  {"randomseed", math_randomseed},
  {"sin",   math_sin},
  {"sqrt",  math_sqrt},
  {"tan",   math_tan},
  {"type", math_type},
#if defined(LUA_COMPAT_MATHLIB)
  {"atan2", math_atan},
  {"cosh",   math_cosh},
  {"sinh",   math_sinh},
  {"tanh",   math_tanh},
  {"pow",   math_pow},
  {"frexp", math_frexp},
  {"ldexp", math_ldexp},
  {"log10", math_log10},
#endif
  /* placeholders */
  {"pi", NULL},
  {"huge", NULL},
  {"maxinteger", NULL},
  {"mininteger", NULL},
  {NULL, NULL}
};


/*
** Open math library
*/
LUAMOD_API int luaopen_math (lua_State *L) {
  luaL_newlib(L, mathlib);
  //lua_pushnumber(L, PI);
  //lua_setfield(L, -2, "pi");
  //lua_pushnumber(L, (lua_Number)HUGE_VAL);
  //lua_setfield(L, -2, "huge");
  lua_pushinteger(L, LUA_MAXINTEGER);
  lua_setfield(L, -2, "maxinteger");
  lua_pushinteger(L, LUA_MININTEGER);
  lua_setfield(L, -2, "mininteger");
  return 1;
}

