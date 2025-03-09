/*
** $Id: lobject.c,v 2.113 2016/12/22 13:08:50 roberto Exp $
** Some generic functions over Lua objects
** See Copyright Notice in lua.h
*/

#define lobject_c
#define LUA_CORE

#include "lprefix.h"


#include <locale.h>
#include <mymath.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lua.h"

#include "lctype.h"
#include "ldebug.h"
#include "ldo.h"
#include "lmem.h"
#include "lobject.h"
#include "lstate.h"
#include "lstring.h"
#include "lvm.h"



LUAI_DDEF const TValue luaO_nilobject_ = {NILCONSTANT};


/*
** converts an integer to a "floating point byte", represented as
** (eeeeexxx), where the real value is (1xxx) * 2^(eeeee - 1) if
** eeeee != 0 and (xxx) otherwise.
*/
int luaO_int2fb (unsigned int x) {
  int e = 0;  /* exponent */
  if (x < 8) return x;
  while (x >= (8 << 4)) {  /* coarse steps */
    x = (x + 0xf) >> 4;  /* x = ceil(x / 16) */
    e += 4;
  }
  while (x >= (8 << 1)) {  /* fine steps */
    x = (x + 1) >> 1;  /* x = ceil(x / 2) */
    e++;
  }
  return ((e+1) << 3) | (cast_int(x) - 8);
}


/* converts back */
int luaO_fb2int (int x) {
  return (x < 8) ? x : ((x & 7) + 8) << ((x >> 3) - 1);
}


/*
** Computes ceil(log2(x))
*/
int luaO_ceillog2 (unsigned int x) {
  static const lu_byte log_2[256] = {  /* log_2[i] = ceil(log2(i - 1)) */
    0,1,2,2,3,3,3,3,4,4,4,4,4,4,4,4,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
    6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
    7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
    7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
    8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
    8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
    8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
    8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8
  };
  int l = 0;
  x--;
  while (x >= 256) { l += 8; x >>= 8; }
  return l + log_2[x];
}


/**
 * Performs arithmetic and bitwise operations on two Lua integers based on the specified operation.
 *
 * This function takes two Lua integers (`v1` and `v2`) and an operation (`op`) and applies the
 * corresponding arithmetic or bitwise operation. The operation is determined by the `op` parameter,
 * which must be one of the predefined constants (e.g., `LUA_OPADD`, `LUA_OPSUB`, etc.).
 *
 * Supported operations include:
 * - Addition (`LUA_OPADD`): Returns `v1 + v2`.
 * - Subtraction (`LUA_OPSUB`): Returns `v1 - v2`.
 * - Multiplication (`LUA_OPMUL`): Returns `v1 * v2`.
 * - Modulo (`LUA_OPMOD`): Returns `v1 % v2` using `luaV_mod`.
 * - Integer division (`LUA_OPIDIV`): Returns `v1 / v2` using `luaV_div`.
 * - Bitwise AND (`LUA_OPBAND`): Returns `v1 & v2`.
 * - Bitwise OR (`LUA_OPBOR`): Returns `v1 | v2`.
 * - Bitwise XOR (`LUA_OPBXOR`): Returns `v1 ^ v2`.
 * - Left shift (`LUA_OPSHL`): Returns `v1 << v2` using `luaV_shiftl`.
 * - Right shift (`LUA_OPSHR`): Returns `v1 >> v2` using `luaV_shiftl`.
 * - Unary minus (`LUA_OPUNM`): Returns `-v1`.
 * - Bitwise NOT (`LUA_OPBNOT`): Returns `~v1`.
 *
 * If an unsupported operation is provided, the function asserts and returns 0.
 *
 * @param L The Lua state (used for certain operations like modulo and division).
 * @param op The operation to perform (must be a valid operation constant).
 * @param v1 The first integer operand.
 * @param v2 The second integer operand.
 * @return The result of the specified operation applied to `v1` and `v2`.
 */
static lua_Integer intarith (lua_State *L, int op, lua_Integer v1,
                                                   lua_Integer v2) {
  switch (op) {
    case LUA_OPADD: return intop(+, v1, v2);
    case LUA_OPSUB:return intop(-, v1, v2);
    case LUA_OPMUL:return intop(*, v1, v2);
    case LUA_OPMOD: return luaV_mod(L, v1, v2);
    case LUA_OPIDIV: return luaV_div(L, v1, v2);
    case LUA_OPBAND: return intop(&, v1, v2);
    case LUA_OPBOR: return intop(|, v1, v2);
    case LUA_OPBXOR: return intop(^, v1, v2);
    case LUA_OPSHL: return luaV_shiftl(v1, v2);
    case LUA_OPSHR: return luaV_shiftl(v1, -v2);
    case LUA_OPUNM: return intop(-, 0, v1);
    case LUA_OPBNOT: return intop(^, ~l_castS2U(0), v1);
    default: lua_assert(0); return 0;
  }
}


/**
 * Performs arithmetic operations on two Lua numbers based on the specified operation.
 *
 * This function takes two Lua numbers (`v1` and `v2`) and an operation (`op`) and performs
 * the corresponding arithmetic operation. The operation is determined by the `op` parameter,
 * which should be one of the predefined constants (e.g., `LUA_OPADD`, `LUA_OPSUB`, etc.).
 * The function delegates the actual computation to specialized helper functions (e.g., `luai_numadd`,
 * `luai_numsub`, etc.) that handle the specific arithmetic operation.
 *
 * Supported operations include:
 * - Addition (`LUA_OPADD`)
 * - Subtraction (`LUA_OPSUB`)
 * - Multiplication (`LUA_OPMUL`)
 * - Division (`LUA_OPDIV`)
 * - Exponentiation (`LUA_OPPOW`)
 * - Integer division (`LUA_OPIDIV`)
 * - Unary minus (`LUA_OPUNM`)
 * - Modulo (`LUA_OPMOD`)
 *
 * If an unsupported operation is provided, the function asserts and returns 0.
 *
 * @param L The Lua state.
 * @param op The arithmetic operation to perform (e.g., `LUA_OPADD`, `LUA_OPSUB`).
 * @param v1 The first operand.
 * @param v2 The second operand.
 * @return The result of the arithmetic operation.
 */
static lua_Number numarith (lua_State *L, int op, lua_Number v1,
                                                  lua_Number v2) {
  switch (op) {
    case LUA_OPADD: return luai_numadd(L, v1, v2);
    case LUA_OPSUB: return luai_numsub(L, v1, v2);
    case LUA_OPMUL: return luai_nummul(L, v1, v2);
    case LUA_OPDIV: return luai_numdiv(L, v1, v2);
    case LUA_OPPOW: return luai_numpow(L, v1, v2);
    case LUA_OPIDIV: return luai_numidiv(L, v1, v2);
    case LUA_OPUNM: return luai_numunm(L, v1);
    case LUA_OPMOD: {
      lua_Number m;
      luai_nummod(L, v1, v2, m);
      return m;
    }
    default: lua_assert(0); return 0;
  }
}


/**
 * Performs arithmetic operations on two Lua values (`p1` and `p2`) based on the specified operator (`op`).
 * The result of the operation is stored in the `res` value.
 *
 * The function handles both integer and floating-point operations, depending on the operator and the types of the input values.
 * Supported operators include bitwise operations (e.g., `LUA_OPBAND`, `LUA_OPBOR`, `LUA_OPBXOR`, `LUA_OPSHL`, `LUA_OPSHR`, `LUA_OPBNOT`),
 * division (`LUA_OPDIV`), exponentiation (`LUA_OPPOW`), and other arithmetic operations.
 *
 * If the input values are integers and the operator supports integer operations, the function performs the operation directly on integers.
 * If the input values are floating-point numbers or the operator requires floating-point operations, the function performs the operation on floating-point numbers.
 *
 * If the raw operation cannot be performed (e.g., due to incompatible types), the function attempts to invoke the corresponding metamethod to handle the operation.
 *
 * @param L The Lua state.
 * @param op The arithmetic operator to apply (e.g., `LUA_OPADD`, `LUA_OPBAND`, etc.).
 * @param p1 The first operand (Lua value).
 * @param p2 The second operand (Lua value).
 * @param res The result of the arithmetic operation (Lua value).
 */
void luaO_arith (lua_State *L, int op, const TValue *p1, const TValue *p2,
                 TValue *res) {
  switch (op) {
    case LUA_OPBAND: case LUA_OPBOR: case LUA_OPBXOR:
    case LUA_OPSHL: case LUA_OPSHR:
    case LUA_OPBNOT: {  /* operate only on integers */
      lua_Integer i1; lua_Integer i2;
      if (tointeger(p1, &i1) && tointeger(p2, &i2)) {
        setivalue(res, intarith(L, op, i1, i2));
        return;
      }
      else break;  /* go to the end */
    }
    case LUA_OPDIV: case LUA_OPPOW: {  /* operate only on floats */
      lua_Number n1; lua_Number n2;
      if (tonumber(p1, &n1) && tonumber(p2, &n2)) {
        setfltvalue(res, numarith(L, op, n1, n2));
        return;
      }
      else break;  /* go to the end */
    }
    default: {  /* other operations */
      lua_Number n1; lua_Number n2;
      if (ttisinteger(p1) && ttisinteger(p2)) {
        setivalue(res, intarith(L, op, ivalue(p1), ivalue(p2)));
        return;
      }
      else if (tonumber(p1, &n1) && tonumber(p2, &n2)) {
        setfltvalue(res, numarith(L, op, n1, n2));
        return;
      }
      else break;  /* go to the end */
    }
  }
  /* could not perform raw operation; try metamethod */
  lua_assert(L != NULL);  /* should not fail when folding (compile time) */
  luaT_trybinTM(L, p1, p2, res, cast(TMS, (op - LUA_OPADD) + TM_ADD));
}


/**
 * Converts a hexadecimal character to its corresponding integer value.
 * 
 * This function takes a single character `c` as input and returns the integer
 * value that the character represents in hexadecimal. If the character is a
 * digit (0-9), it returns the corresponding integer value. If the character
 * is a hexadecimal letter (a-f or A-F), it converts the character to lowercase
 * and returns the corresponding integer value (10-15).
 * 
 * @param c The character to be converted. It should be a valid hexadecimal
 *          character (0-9, a-f, or A-F).
 * 
 * @return The integer value corresponding to the hexadecimal character.
 *         Returns -1 if the character is not a valid hexadecimal character.
 */
int luaO_hexavalue (int c) {
  if (lisdigit(c)) return c - '0';
  else return (ltolower(c) - 'a') + 10;
}


/**
 * Checks if the current character in the string is a negative sign ('-') or a positive sign ('+'). 
 * If the character is '-', the method increments the string pointer and returns 1 to indicate a negative sign.
 * If the character is '+', the method increments the string pointer but returns 0, as it indicates a positive sign.
 * If neither sign is present, the method returns 0 without modifying the string pointer.
 *
 * @param s A pointer to a pointer to a character in the string to be checked.
 * @return 1 if a negative sign is found, 0 otherwise.
 */
static int isneg (const char **s) {
  if (**s == '-') { (*s)++; return 1; }
  else if (**s == '+') (*s)++;
  return 0;
}



/*
** {==================================================================
** Lua's implementation for 'lua_strx2number'
** ===================================================================
*/

#if !defined(lua_strx2number)

/* maximum number of significant digits to read (to avoid overflows
   even with single floats) */
#define MAXSIGDIG	30

/*
** convert an hexadecimal numeric string to a number, following
** C99 specification for 'strtod'
*/
static lua_Number lua_strx2number (const char *s, char **endptr) {
  int dot = lua_getlocaledecpoint();
  lua_Number r = 0.0;  /* result (accumulator) */
  int sigdig = 0;  /* number of significant digits */
  int nosigdig = 0;  /* number of non-significant digits */
  int e = 0;  /* exponent correction */
  int neg;  /* 1 if number is negative */
  int hasdot = 0;  /* true after seen a dot */
  *endptr = cast(char *, s);  /* nothing is valid yet */
  while (lisspace(cast_uchar(*s))) s++;  /* skip initial spaces */
  neg = isneg(&s);  /* check signal */
  if (!(*s == '0' && (*(s + 1) == 'x' || *(s + 1) == 'X')))  /* check '0x' */
    return 0.0;  /* invalid format (no '0x') */
  for (s += 2; ; s++) {  /* skip '0x' and read numeral */
    if (*s == dot) {
      if (hasdot) break;  /* second dot? stop loop */
      else hasdot = 1;
    }
    else if (lisxdigit(cast_uchar(*s))) {
      if (sigdig == 0 && *s == '0')  /* non-significant digit (zero)? */
        nosigdig++;
      else if (++sigdig <= MAXSIGDIG)  /* can read it without overflow? */
          r = (r * cast_num(16.0)) + luaO_hexavalue(*s);
      else e++; /* too many digits; ignore, but still count for exponent */
      if (hasdot) e--;  /* decimal digit? correct exponent */
    }
    else break;  /* neither a dot nor a digit */
  }
  if (nosigdig + sigdig == 0)  /* no digits? */
    return 0.0;  /* invalid format */
  *endptr = cast(char *, s);  /* valid up to here */
  e *= 4;  /* each digit multiplies/divides value by 2^4 */
  if (*s == 'p' || *s == 'P') {  /* exponent part? */
    int exp1 = 0;  /* exponent value */
    int neg1;  /* exponent signal */
    s++;  /* skip 'p' */
    neg1 = isneg(&s);  /* signal */
    if (!lisdigit(cast_uchar(*s)))
      return 0.0;  /* invalid; must have at least one digit */
    while (lisdigit(cast_uchar(*s)))  /* read exponent */
      exp1 = exp1 * 10 + *(s++) - '0';
    if (neg1) exp1 = -exp1;
    e += exp1;
    *endptr = cast(char *, s);  /* valid up to here */
  }
  if (neg) r = -r;
  return l_mathop(ldexp)(r, e);
}

#endif
/* }====================================================== */


/* maximum length of a numeral */
#if !defined (L_MAXLENNUM)
#define L_MAXLENNUM	200
#endif

/**
 * Converts a string to a Lua number and stores the result in the provided pointer.
 *
 * This function attempts to convert the string `s` to a Lua number based on the specified `mode`.
 * If `mode` is 'x', the function uses `lua_strx2number` to handle hexadecimal conversion.
 * Otherwise, it uses `lua_str2number` for standard numeric conversion.
 *
 * The function skips any trailing whitespace characters after the numeric value.
 * If the conversion is successful and there are no trailing non-whitespace characters,
 * the function returns a pointer to the end of the converted string. If the conversion fails
 * or there are trailing non-whitespace characters, the function returns `NULL`.
 *
 * @param s The input string to be converted.
 * @param result A pointer to store the converted Lua number.
 * @param mode The conversion mode: 'x' for hexadecimal, otherwise for standard numeric conversion.
 * @return A pointer to the end of the converted string if successful, otherwise `NULL`.
 */
static const char *l_str2dloc (const char *s, lua_Number *result, int mode) {
  char *endptr;
  *result = (mode == 'x') ? lua_strx2number(s, &endptr)  /* try to convert */
                          : lua_str2number(s, &endptr);
  if (endptr == s) return NULL;  /* nothing recognized? */
  while (lisspace(cast_uchar(*endptr))) endptr++;  /* skip trailing spaces */
  return (*endptr == '\0') ? endptr : NULL;  /* OK if no trailing characters */
}


/*
** Convert string 's' to a Lua number (put in 'result'). Return NULL
** on fail or the address of the ending '\0' on success.
** 'pmode' points to (and 'mode' contains) special things in the string:
** - 'x'/'X' means an hexadecimal numeral
** - 'n'/'N' means 'inf' or 'nan' (which should be rejected)
** - '.' just optimizes the search for the common case (nothing special)
** This function accepts both the current locale or a dot as the radix
** mark. If the convertion fails, it may mean number has a dot but
** locale accepts something else. In that case, the code copies 's'
** to a buffer (because 's' is read-only), changes the dot to the
** current locale radix mark, and tries to convert again.
*/
static const char *l_str2d (const char *s, lua_Number *result) {
  const char *endptr;
  const char *pmode = strpbrk(s, ".xXnN");
  int mode = pmode ? ltolower(cast_uchar(*pmode)) : 0;
  if (mode == 'n')  /* reject 'inf' and 'nan' */
    return NULL;
  endptr = l_str2dloc(s, result, mode);  /* try to convert */
  if (endptr == NULL) {  /* failed? may be a different locale */
    char buff[L_MAXLENNUM + 1];
    const char *pdot = strchr(s, '.');
    if (strlen(s) > L_MAXLENNUM || pdot == NULL)
      return NULL;  /* string too long or no dot; fail */
    strcpy(buff, s);  /* copy string to buffer */
    buff[pdot - s] = lua_getlocaledecpoint();  /* correct decimal point */
    endptr = l_str2dloc(buff, result, mode);  /* try again */
    if (endptr != NULL)
      endptr = s + (endptr - buff);  /* make relative to 's' */
  }
  return endptr;
}


#define MAXBY10		cast(lua_Unsigned, LUA_MAXINTEGER / 10)
#define MAXLASTD	cast_int(LUA_MAXINTEGER % 10)

/**
 * Converts a string representation of a number to a Lua integer.
 *
 * This function parses a string `s` and converts it to a Lua integer, storing the result in `*result`.
 * The string can represent a number in either decimal or hexadecimal format. Hexadecimal numbers must
 * be prefixed with "0x" or "0X". The function skips leading and trailing whitespace.
 *
 * The function handles negative numbers by interpreting a leading '-' character. It also checks for
 * overflow when converting decimal numbers. If the string does not represent a valid number or if
 * overflow occurs, the function returns `NULL` to indicate an error.
 *
 * @param s The input string to be converted. It can contain leading and trailing whitespace.
 * @param result A pointer to a `lua_Integer` where the result of the conversion will be stored.
 * @return Returns a pointer to the end of the parsed number in the string if successful, or `NULL` if
 *         the string does not represent a valid number or if overflow occurs.
 */
static const char *l_str2int (const char *s, lua_Integer *result) {
  lua_Unsigned a = 0;
  int empty = 1;
  int neg;
  while (lisspace(cast_uchar(*s))) s++;  /* skip initial spaces */
  neg = isneg(&s);
  if (s[0] == '0' &&
      (s[1] == 'x' || s[1] == 'X')) {  /* hex? */
    s += 2;  /* skip '0x' */
    for (; lisxdigit(cast_uchar(*s)); s++) {
      a = a * 16 + luaO_hexavalue(*s);
      empty = 0;
    }
  }
  else {  /* decimal */
    for (; lisdigit(cast_uchar(*s)); s++) {
      int d = *s - '0';
      if (a >= MAXBY10 && (a > MAXBY10 || d > MAXLASTD + neg))  /* overflow? */
        return NULL;  /* do not accept it (as integer) */
      a = a * 10 + d;
      empty = 0;
    }
  }
  while (lisspace(cast_uchar(*s))) s++;  /* skip trailing spaces */
  if (empty || *s != '\0') return NULL;  /* something wrong in the numeral */
  else {
    *result = l_castU2S((neg) ? 0u - a : a);
    return s;
  }
}


/**
 * Converts a string to a Lua number (either integer or float) and stores the result in a TValue.
 * 
 * This function attempts to convert the input string `s` to a Lua number. It first tries to parse
 * the string as an integer using `l_str2int`. If that fails, it attempts to parse the string as a
 * floating-point number using `l_str2d`. If both conversions fail, the function returns 0 to
 * indicate failure. On successful conversion, the result is stored in the TValue `o`, and the
 * function returns the number of characters consumed from the input string plus one.
 *
 * @param s The input string to be converted to a number.
 * @param o A pointer to a TValue where the converted number will be stored.
 * @return The number of characters consumed from the input string plus one on success, or 0 if
 *         the conversion failed.
 */
size_t luaO_str2num (const char *s, TValue *o) {
  lua_Integer i; lua_Number n;
  const char *e;
  if ((e = l_str2int(s, &i)) != NULL) {  /* try as an integer */
    setivalue(o, i);
  }
  else if ((e = l_str2d(s, &n)) != NULL) {  /* else try as a float */
    setfltvalue(o, n);
  }
  else
    return 0;  /* conversion failed */
  return (e - s) + 1;  /* success; return string size */
}


/**
 * Encodes a Unicode code point `x` into its UTF-8 representation and stores it in the buffer `buff`.
 * The buffer must have at least `UTF8BUFFSZ` bytes of space. The encoding is performed backwards,
 * starting from the end of the buffer. The method ensures the code point is within the valid range
 * for Unicode (0x000000 to 0x10FFFF) and handles both ASCII and multi-byte UTF-8 sequences.
 *
 * @param buff The buffer where the UTF-8 encoded bytes will be stored. Must have at least `UTF8BUFFSZ` bytes.
 * @param x The Unicode code point to encode. Must be a valid Unicode code point (<= 0x10FFFF).
 * @return The number of bytes used to encode the code point in UTF-8. For ASCII characters, this is 1;
 *         for multi-byte sequences, it ranges from 2 to 4.
 */
int luaO_utf8esc (char *buff, unsigned long x) {
  int n = 1;  /* number of bytes put in buffer (backwards) */
  lua_assert(x <= 0x10FFFF);
  if (x < 0x80)  /* ascii? */
    buff[UTF8BUFFSZ - 1] = cast(char, x);
  else {  /* need continuation bytes */
    unsigned int mfb = 0x3f;  /* maximum that fits in first byte */
    do {  /* add continuation bytes */
      buff[UTF8BUFFSZ - (n++)] = cast(char, 0x80 | (x & 0x3f));
      x >>= 6;  /* remove added bits */
      mfb >>= 1;  /* now there is one less bit available in first byte */
    } while (x > mfb);  /* still needs continuation byte? */
    buff[UTF8BUFFSZ - n] = cast(char, (~mfb << 1) | x);  /* add first byte */
  }
  return n;
}


/* maximum length of the conversion of a number to a string */
#define MAXNUMBER2STR	50


/*
** Convert a number object to a string
*/
void luaO_tostring (lua_State *L, StkId obj) {
  char buff[MAXNUMBER2STR];
  size_t len;
  lua_assert(ttisnumber(obj));
  if (ttisinteger(obj))
    len = lua_integer2str(buff, sizeof(buff), ivalue(obj));
  else {
    len = lua_number2str(buff, sizeof(buff), fltvalue(obj));
#if !defined(LUA_COMPAT_FLOATSTRING)
    if (buff[strspn(buff, "-0123456789")] == '\0') {  /* looks like an int? */
      buff[len++] = lua_getlocaledecpoint();
      buff[len++] = '0';  /* adds '.0' to result */
    }
#endif
  }
  setsvalue2s(L, obj, luaS_newlstr(L, buff, len));
}


/**
 * Pushes a string onto the Lua stack.
 *
 * This function creates a new Lua string from the given C string `str` with length `l`
 * and pushes it onto the top of the Lua stack. The string is created using `luaS_newlstr`,
 * which ensures proper memory management and string interning. After pushing the string,
 * the stack top is incremented using `luaD_inctop` to reflect the new stack size.
 *
 * @param L Pointer to the Lua state.
 * @param str Pointer to the C string to be pushed onto the stack.
 * @param l Length of the C string.
 */
static void pushstr (lua_State *L, const char *str, size_t l) {
  setsvalue2s(L, L->top, luaS_newlstr(L, str, l));
  luaD_inctop(L);
}


/*
** this function handles only '%d', '%c', '%f', '%p', and '%s'
   conventional formats, plus Lua-specific '%I' and '%U'
*/
const char *luaO_pushvfstring (lua_State *L, const char *fmt, va_list argp) {
  int n = 0;
  for (;;) {
    const char *e = strchr(fmt, '%');
    if (e == NULL) break;
    pushstr(L, fmt, e - fmt);
    switch (*(e+1)) {
      case 's': {  /* zero-terminated string */
        const char *s = va_arg(argp, char *);
        if (s == NULL) s = "(null)";
        pushstr(L, s, strlen(s));
        break;
      }
      case 'c': {  /* an 'int' as a character */
        char buff = cast(char, va_arg(argp, int));
        if (lisprint(cast_uchar(buff)))
          pushstr(L, &buff, 1);
        else  /* non-printable character; print its code */
          luaO_pushfstring(L, "<\\%d>", cast_uchar(buff));
        break;
      }
      case 'd': {  /* an 'int' */
        setivalue(L->top, va_arg(argp, int));
        goto top2str;
      }
      case 'I': {  /* a 'lua_Integer' */
        setivalue(L->top, cast(lua_Integer, va_arg(argp, l_uacInt)));
        goto top2str;
      }
      case 'f': {  /* a 'lua_Number' */
        setfltvalue(L->top, cast_num(va_arg(argp, l_uacNumber)));
      top2str:  /* convert the top element to a string */
        luaD_inctop(L);
        luaO_tostring(L, L->top - 1);
        break;
      }
      case 'p': {  /* a pointer */
        char buff[4*sizeof(void *) + 8]; /* should be enough space for a '%p' */
        int l = l_sprintf(buff, sizeof(buff), "%p", va_arg(argp, void *));
        pushstr(L, buff, l);
        break;
      }
      case 'U': {  /* an 'int' as a UTF-8 sequence */
        char buff[UTF8BUFFSZ];
        int l = luaO_utf8esc(buff, cast(long, va_arg(argp, long)));
        pushstr(L, buff + UTF8BUFFSZ - l, l);
        break;
      }
      case '%': {
        pushstr(L, "%", 1);
        break;
      }
      default: {
        luaG_runerror(L, "invalid option '%%%c' to 'lua_pushfstring'",
                         *(e + 1));
      }
    }
    n += 2;
    fmt = e+2;
  }
  luaD_checkstack(L, 1);
  pushstr(L, fmt, strlen(fmt));
  if (n > 0) luaV_concat(L, n + 1);
  return svalue(L->top - 1);
}


/**
 * Formats a string and pushes it onto the Lua stack.
 *
 * This function takes a format string `fmt` and a variable number of arguments,
 * similar to `printf`. It formats the string using the provided arguments and
 * pushes the resulting string onto the Lua stack. The function returns the
 * formatted string.
 *
 * @param L The Lua state.
 * @param fmt The format string, which may contain format specifiers.
 * @param ... Variable arguments to be formatted according to `fmt`.
 * @return The formatted string that was pushed onto the Lua stack.
 */
const char *luaO_pushfstring (lua_State *L, const char *fmt, ...) {
  const char *msg;
  va_list argp;
  va_start(argp, fmt);
  msg = luaO_pushvfstring(L, fmt, argp);
  va_end(argp);
  return msg;
}


/* number of chars of a literal string without the ending \0 */
#define LL(x)	(sizeof(x)/sizeof(char) - 1)

#define RETS	"..."
#define PRE	"[string \""
#define POS	"\"]"

#define addstr(a,b,l)	( memcpy(a,b,(l) * sizeof(char)), a += (l) )

/**
 * Generates a chunk identifier from the given source string and stores it in the output buffer.
 * The function handles three types of source strings:
 * 1. Literal source: If the source starts with '=', it is treated as a literal string, and the '=' is removed.
 * 2. File name: If the source starts with '@', it is treated as a file name, and the '@' is removed.
 * 3. String: If the source does not start with '=' or '@', it is treated as a string and formatted as [string "source"].
 * 
 * The function ensures that the output fits within the provided buffer length. If the source string is too long,
 * it is truncated, and an appropriate suffix (e.g., "...") is added to indicate truncation.
 *
 * @param out      The output buffer where the chunk identifier will be stored.
 * @param source   The source string to generate the chunk identifier from.
 * @param bufflen  The length of the output buffer, ensuring the result does not exceed this size.
 */
void luaO_chunkid (char *out, const char *source, size_t bufflen) {
  size_t l = strlen(source);
  if (*source == '=') {  /* 'literal' source */
    if (l <= bufflen)  /* small enough? */
      memcpy(out, source + 1, l * sizeof(char));
    else {  /* truncate it */
      addstr(out, source + 1, bufflen - 1);
      *out = '\0';
    }
  }
  else if (*source == '@') {  /* file name */
    if (l <= bufflen)  /* small enough? */
      memcpy(out, source + 1, l * sizeof(char));
    else {  /* add '...' before rest of name */
      addstr(out, RETS, LL(RETS));
      bufflen -= LL(RETS);
      memcpy(out, source + 1 + l - bufflen, bufflen * sizeof(char));
    }
  }
  else {  /* string; format as [string "source"] */
    const char *nl = strchr(source, '\n');  /* find first new line (if any) */
    addstr(out, PRE, LL(PRE));  /* add prefix */
    bufflen -= LL(PRE RETS POS) + 1;  /* save space for prefix+suffix+'\0' */
    if (l < bufflen && nl == NULL) {  /* small one-line source? */
      addstr(out, source, l);  /* keep it */
    }
    else {
      if (nl != NULL) l = nl - source;  /* stop at first newline */
      if (l > bufflen) l = bufflen;
      addstr(out, source, l);
      addstr(out, RETS, LL(RETS));
    }
    memcpy(out, POS, (LL(POS) + 1) * sizeof(char));
  }
}

