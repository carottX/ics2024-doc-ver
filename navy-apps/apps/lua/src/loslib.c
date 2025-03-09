/*
** $Id: loslib.c,v 1.65 2016/07/18 17:58:58 roberto Exp $
** Standard Operating System library
** See Copyright Notice in lua.h
*/

#define loslib_c
#define LUA_LIB

#include "lprefix.h"


#include <errno.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "lua.h"

#include "lauxlib.h"
#include "lualib.h"


/*
** {==================================================================
** List of valid conversion specifiers for the 'strftime' function;
** options are grouped by length; group of length 2 start with '||'.
** ===================================================================
*/
#if !defined(LUA_STRFTIMEOPTIONS)	/* { */

/* options for ANSI C 89 (only 1-char options) */
#define L_STRFTIMEC89		"aAbBcdHIjmMpSUwWxXyYZ%"

/* options for ISO C 99 and POSIX */
#define L_STRFTIMEC99 "aAbBcCdDeFgGhHIjmMnprRStTuUVwWxXyYzZ%" \
    "||" "EcECExEXEyEY" "OdOeOHOIOmOMOSOuOUOVOwOWOy"  /* two-char options */

/* options for Windows */
#define L_STRFTIMEWIN "aAbBcdHIjmMpSUwWxXyYzZ%" \
    "||" "#c#x#d#H#I#j#m#M#S#U#w#W#y#Y"  /* two-char options */

#if defined(LUA_USE_WINDOWS)
#define LUA_STRFTIMEOPTIONS	L_STRFTIMEWIN
#elif defined(LUA_USE_C89)
#define LUA_STRFTIMEOPTIONS	L_STRFTIMEC89
#else  /* C99 specification */
#define LUA_STRFTIMEOPTIONS	L_STRFTIMEC99
#endif

#endif					/* } */
/* }================================================================== */


/*
** {==================================================================
** Configuration for time-related stuff
** ===================================================================
*/

#if !defined(l_time_t)		/* { */
/*
** type to represent time_t in Lua
*/
#define l_timet			lua_Integer
#define l_pushtime(L,t)		lua_pushinteger(L,(lua_Integer)(t))

/**
 * @brief Converts a Lua integer to a `time_t` value, ensuring it is within valid bounds.
 *
 * This function retrieves an integer value from the Lua stack at the specified index
 * and checks if it can be safely cast to a `time_t` type without causing overflow or
 * loss of precision. If the value is out of bounds for the `time_t` type, an error is
 * raised using `luaL_argcheck`. If the value is valid, it is cast to `time_t` and returned.
 *
 * @param L The Lua state from which the integer is retrieved.
 * @param arg The stack index of the Lua integer to be checked and converted.
 * @return The converted `time_t` value.
 * @throws Raises a Lua error if the integer is out of bounds for the `time_t` type.
 */
static time_t l_checktime (lua_State *L, int arg) {
  lua_Integer t = luaL_checkinteger(L, arg);
  luaL_argcheck(L, (time_t)t == t, arg, "time out-of-bounds");
  return (time_t)t;
}

#endif				/* } */


#if !defined(l_gmtime)		/* { */
/*
** By default, Lua uses gmtime/localtime, except when POSIX is available,
** where it uses gmtime_r/localtime_r
*/

#if defined(LUA_USE_POSIX)	/* { */

#define l_gmtime(t,r)		gmtime_r(t,r)
#define l_localtime(t,r)	localtime_r(t,r)

#else				/* }{ */

/* ISO C definitions */
#define l_gmtime(t,r)		((void)(r)->tm_sec, gmtime(t))
#define l_localtime(t,r)  	((void)(r)->tm_sec, localtime(t))

#endif				/* } */

#endif				/* } */

/* }================================================================== */


/*
** {==================================================================
** Configuration for 'tmpnam':
** By default, Lua uses tmpnam except when POSIX is available, where
** it uses mkstemp.
** ===================================================================
*/
#if !defined(lua_tmpnam)	/* { */

#if defined(LUA_USE_POSIX)	/* { */

#include <unistd.h>

#define LUA_TMPNAMBUFSIZE	32

#if !defined(LUA_TMPNAMTEMPLATE)
#define LUA_TMPNAMTEMPLATE	"/tmp/lua_XXXXXX"
#endif

#define lua_tmpnam(b,e) { \
        strcpy(b, LUA_TMPNAMTEMPLATE); \
        e = mkstemp(b); \
        if (e != -1) close(e); \
        e = (e == -1); }

#else				/* }{ */

/* ISO C definitions */
#define LUA_TMPNAMBUFSIZE	L_tmpnam
#define lua_tmpnam(b,e)		{ e = (tmpnam(b) == NULL); }

#endif				/* } */

#endif				/* } */
/* }================================================================== */




static int os_execute (lua_State *L) {
  const char *cmd = luaL_optstring(L, 1, NULL);
  int stat = system(cmd);
  if (cmd != NULL)
    return luaL_execresult(L, stat);
  else {
    lua_pushboolean(L, stat);  /* true if there is a shell */
    return 1;
  }
}


/**
 * Removes the file specified by the given filename.
 *
 * This function takes a filename as a Lua string from the Lua stack (position 1),
 * attempts to remove the file using the `remove` function from the C standard library,
 * and returns the result to Lua.
 *
 * @param L The Lua state from which the filename is retrieved and to which the result is pushed.
 * @return Returns 1 on success, pushing `true` and the filename to the Lua stack.
 *         Returns 1 on failure, pushing `false`, the filename, and an error message to the Lua stack.
 */
static int os_remove (lua_State *L) {
  const char *filename = luaL_checkstring(L, 1);
  return luaL_fileresult(L, remove(filename) == 0, filename);
}


/**
 * Renames a file or directory from `fromname` to `toname`.
 *
 * This function is a Lua C API method that takes two string arguments:
 * 1. `fromname`: The current name of the file or directory to be renamed.
 * 2. `toname`: The new name for the file or directory.
 *
 * It uses the `rename` system call to perform the renaming operation. If the
 * operation is successful, it returns `true` to Lua; otherwise, it returns
 * `false` along with an error message describing the failure.
 *
 * @param L The Lua state.
 * @return Returns 1 or 2 values to Lua: a boolean indicating success or failure,
 *         and optionally an error message if the operation fails.
 */
static int os_rename (lua_State *L) {
  const char *fromname = luaL_checkstring(L, 1);
  const char *toname = luaL_checkstring(L, 2);
  return luaL_fileresult(L, rename(fromname, toname) == 0, NULL);
}


/**
 * Generates a unique temporary filename and pushes it onto the Lua stack.
 * 
 * This function creates a unique temporary filename using the `lua_tmpnam` function.
 * The generated filename is stored in a buffer and then pushed onto the Lua stack
 * as a string. If the generation of the temporary filename fails, an error is raised
 * in Lua using `luaL_error`.
 * 
 * @param L The Lua state.
 * @return Returns 1 on success, pushing the generated filename onto the stack.
 *         On failure, raises a Lua error and does not return.
 */
static int os_tmpname (lua_State *L) {
  char buff[LUA_TMPNAMBUFSIZE];
  int err;
  lua_tmpnam(buff, err);
  if (err)
    return luaL_error(L, "unable to generate a unique filename");
  lua_pushstring(L, buff);
  return 1;
}


/**
 * Retrieves the value of an environment variable and pushes it onto the Lua stack.
 *
 * This function takes a single string argument from the Lua stack, which represents
 * the name of the environment variable to retrieve. It uses the `getenv` function to
 * fetch the value of the specified environment variable. If the environment variable
 * exists and has a value, that value is pushed onto the Lua stack as a string. If the
 * environment variable does not exist or has no value, `nil` is pushed onto the stack.
 *
 * @param L A pointer to the Lua state.
 * @return The number of values pushed onto the Lua stack (always 1).
 */
static int os_getenv (lua_State *L) {
  lua_pushstring(L, getenv(luaL_checkstring(L, 1)));  /* if NULL push nil */
  return 1;
}


/**
 * Pushes the current processor time used by the program onto the Lua stack as a number.
 * The time is expressed in seconds and is calculated by dividing the value returned by
 * the `clock()` function by `CLOCKS_PER_SEC`. This function is typically used to measure
 * the elapsed CPU time since the start of the program.
 *
 * @param L A pointer to the Lua state.
 * @return Returns 1, indicating that one value (the processor time in seconds) has been pushed onto the stack.
 */
static int os_clock (lua_State *L) {
  lua_pushnumber(L, ((lua_Number)clock())/(lua_Number)CLOCKS_PER_SEC);
  return 1;
}


/*
** {======================================================
** Time/Date operations
** { year=%Y, month=%m, day=%d, hour=%H, min=%M, sec=%S,
**   wday=%w+1, yday=%j, isdst=? }
** =======================================================
*/

static void setfield (lua_State *L, const char *key, int value) {
  lua_pushinteger(L, value);
  lua_setfield(L, -2, key);
}

/**
 * Sets a boolean field in a Lua table at the top of the Lua stack.
 *
 * This function checks if the provided `value` is non-negative. If `value` is negative,
 * the function returns without setting the field, treating it as an undefined value.
 * If `value` is non-negative, it pushes the boolean representation of `value` onto the
 * Lua stack and associates it with the specified `key` in the table at the top of the stack.
 *
 * @param L Pointer to the Lua state.
 * @param key The key (string) in the table where the boolean value will be stored.
 * @param value The integer value to be converted to a boolean. If negative, the field is not set.
 */
static void setboolfield (lua_State *L, const char *key, int value) {
  if (value < 0)  /* undefined? */
    return;  /* does not set field */
  lua_pushboolean(L, value);
  lua_setfield(L, -2, key);
}


/*
** Set all fields from structure 'tm' in the table on top of the stack
*/
static void setallfields (lua_State *L, struct tm *stm) {
  setfield(L, "sec", stm->tm_sec);
  setfield(L, "min", stm->tm_min);
  setfield(L, "hour", stm->tm_hour);
  setfield(L, "day", stm->tm_mday);
  setfield(L, "month", stm->tm_mon + 1);
  setfield(L, "year", stm->tm_year + 1900);
  setfield(L, "wday", stm->tm_wday + 1);
  setfield(L, "yday", stm->tm_yday + 1);
  setboolfield(L, "isdst", stm->tm_isdst);
}


/**
 * Retrieves a boolean value from a Lua table field.
 *
 * This function fetches the value associated with the given key from the table at the top of the Lua stack.
 * If the key does not exist in the table (i.e., the field is `nil`), the function returns `-1`. Otherwise, it
 * converts the value to a boolean (using `lua_toboolean`) and returns the result (0 for false, 1 for true).
 * The fetched value is removed from the stack before returning.
 *
 * @param L The Lua state.
 * @param key The key to look up in the table.
 * @return Returns `-1` if the field is `nil`, `0` if the field is `false`, or `1` if the field is `true`.
 */
static int getboolfield (lua_State *L, const char *key) {
  int res;
  res = (lua_getfield(L, -1, key) == LUA_TNIL) ? -1 : lua_toboolean(L, -1);
  lua_pop(L, 1);
  return res;
}


/* maximum value for date fields (to avoid arithmetic overflows with 'int') */
#if !defined(L_MAXDATEFIELD)
#define L_MAXDATEFIELD	(INT_MAX / 2)
#endif

/**
 * Retrieves an integer field from a Lua table and performs validation and adjustment.
 *
 * This function retrieves the value associated with the given `key` from the Lua table
 * at the top of the Lua stack. The value is expected to be an integer. If the value is
 * not an integer, the function checks if it is `nil` and if a default value `d` is provided.
 * If the value is `nil` and no default is provided, or if the value is not an integer,
 * an error is raised. If the value is an integer, it is checked against the valid range
 * defined by `L_MAXDATEFIELD`. If the value is within the valid range, it is adjusted by
 * subtracting `delta`. The function returns the resulting integer value.
 *
 * @param L The Lua state.
 * @param key The key to retrieve from the Lua table.
 * @param d The default value to use if the field is `nil`. If `d` is negative, no default
 *          is used, and an error is raised if the field is `nil`.
 * @param delta The value to subtract from the retrieved integer field.
 * @return The retrieved and adjusted integer value.
 * @throws Lua error if the field is not an integer, is missing, or is out of bounds.
 */
static int getfield (lua_State *L, const char *key, int d, int delta) {
  int isnum;
  int t = lua_getfield(L, -1, key);  /* get field and its type */
  lua_Integer res = lua_tointegerx(L, -1, &isnum);
  if (!isnum) {  /* field is not an integer? */
    if (t != LUA_TNIL)  /* some other value? */
      return luaL_error(L, "field '%s' is not an integer", key);
    else if (d < 0)  /* absent field; no default? */
      return luaL_error(L, "field '%s' missing in date table", key);
    res = d;
  }
  else {
    if (!(-L_MAXDATEFIELD <= res && res <= L_MAXDATEFIELD))
      return luaL_error(L, "field '%s' is out-of-bound", key);
    res -= delta;
  }
  lua_pop(L, 1);
  return (int)res;
}


/**
 * @brief Checks if a given conversion specifier is valid and copies it to a buffer.
 *
 * This function iterates through a predefined list of valid conversion specifiers
 * (defined by `LUA_STRFTIMEOPTIONS`) and checks if the provided `conv` string matches
 * any of the valid options. If a match is found, the valid conversion specifier is
 * copied to the `buff` buffer, and a pointer to the next character in `conv` is returned.
 * If no match is found, the function raises an error using `luaL_argerror` with a
 * descriptive message indicating the invalid conversion specifier.
 *
 * @param L The Lua state, used for error handling.
 * @param conv The conversion specifier string to be checked.
 * @param convlen The length of the `conv` string.
 * @param buff A buffer to store the valid conversion specifier if found.
 * @return A pointer to the next character in `conv` if a valid specifier is found,
 *         or the original `conv` pointer if an error occurs.
 */
static const char *checkoption (lua_State *L, const char *conv,
                                ptrdiff_t convlen, char *buff) {
  const char *option = LUA_STRFTIMEOPTIONS;
  int oplen = 1;  /* length of options being checked */
  for (; *option != '\0' && oplen <= convlen; option += oplen) {
    if (*option == '|')  /* next block? */
      oplen++;  /* will check options with next length (+1) */
    else if (memcmp(conv, option, oplen) == 0) {  /* match? */
      memcpy(buff, conv, oplen);  /* copy valid option to buffer */
      buff[oplen] = '\0';
      return conv + oplen;  /* return next item */
    }
  }
  luaL_argerror(L, 1,
    lua_pushfstring(L, "invalid conversion specifier '%%%s'", conv));
  return conv;  /* to avoid warnings */
}


/* maximum size for an individual 'strftime' item */
#define SIZETIMEFMT	250


/**
 * Formats and returns a date string or a table representing the current or specified time.
 *
 * This function retrieves the current or specified time and formats it according to the provided
 * format string. If the format string is "*t", it returns a Lua table with fields representing
 * the date and time components (e.g., year, month, day, hour, minute, second). Otherwise, it
 * formats the time according to the format string, which can include standard `strftime` specifiers.
 *
 * @param L The Lua state.
 * @return Returns 1, pushing the formatted date string or table onto the Lua stack.
 *
 * @details
 * - The first optional argument is the format string (default: "%c"). If it starts with '!', the
 *   time is interpreted as UTC. If the format string is "*t", a table with date/time fields is returned.
 * - The second optional argument is the time to format (default: current time).
 * - The function handles conversion specifiers (e.g., "%Y", "%m", "%d") and non-specifier characters
 *   in the format string.
 * - If the time cannot be represented (e.g., invalid timestamp), an error is raised.
 *
 * @example
 * -- Get current date as a table
 * local date = os_date("*t")
 * print(date.year, date.month, date.day)
 *
 * -- Format current time as a string
 * local formatted = os_date("%Y-%m-%d %H:%M:%S")
 * print(formatted)
 */
static int os_date (lua_State *L) {
  size_t slen;
  const char *s = luaL_optlstring(L, 1, "%c", &slen);
  time_t t = luaL_opt(L, l_checktime, 2, time(NULL));
  const char *se = s + slen;  /* 's' end */
  struct tm tmr, *stm;
  if (*s == '!') {  /* UTC? */
    stm = l_gmtime(&t, &tmr);
    s++;  /* skip '!' */
  }
  else
    stm = l_localtime(&t, &tmr);
  if (stm == NULL)  /* invalid date? */
    luaL_error(L, "time result cannot be represented in this installation");
  if (strcmp(s, "*t") == 0) {
    lua_createtable(L, 0, 9);  /* 9 = number of fields */
    setallfields(L, stm);
  }
  else {
    char cc[4];  /* buffer for individual conversion specifiers */
    luaL_Buffer b;
    cc[0] = '%';
    luaL_buffinit(L, &b);
    while (s < se) {
      if (*s != '%')  /* not a conversion specifier? */
        luaL_addchar(&b, *s++);
      else {
        size_t reslen;
        char *buff = luaL_prepbuffsize(&b, SIZETIMEFMT);
        s++;  /* skip '%' */
        s = checkoption(L, s, se - s, cc + 1);  /* copy specifier to 'cc' */
        reslen = strftime(buff, SIZETIMEFMT, cc, stm);
        luaL_addsize(&b, reslen);
      }
    }
    luaL_pushresult(&b);
  }
  return 1;
}


/**
 * Retrieves the current time or converts a table representing a date and time to a time value.
 *
 * This function can be called in two ways:
 * 1. Without arguments: It retrieves the current system time.
 * 2. With a table argument: It interprets the table as a date and time, converts it to a time value,
 *    and updates the table with normalized values.
 *
 * The table should contain the following fields:
 * - "sec": Seconds (0-59)
 * - "min": Minutes (0-59)
 * - "hour": Hours (0-23)
 * - "day": Day of the month (1-31)
 * - "month": Month (1-12)
 * - "year": Year (e.g., 2023)
 * - "isdst": Daylight Saving Time flag (boolean)
 *
 * If the table is provided, the function will normalize the values and update the table accordingly.
 * The function then pushes the resulting time value onto the Lua stack.
 *
 * @param L The Lua state.
 * @return 1, as it pushes one value (the time) onto the Lua stack.
 * @throws Lua error if the time result cannot be represented in the current installation.
 */
static int os_time (lua_State *L) {
  time_t t;
  if (lua_isnoneornil(L, 1))  /* called without args? */
    t = time(NULL);  /* get current time */
  else {
    struct tm ts;
    luaL_checktype(L, 1, LUA_TTABLE);
    lua_settop(L, 1);  /* make sure table is at the top */
    ts.tm_sec = getfield(L, "sec", 0, 0);
    ts.tm_min = getfield(L, "min", 0, 0);
    ts.tm_hour = getfield(L, "hour", 12, 0);
    ts.tm_mday = getfield(L, "day", -1, 0);
    ts.tm_mon = getfield(L, "month", -1, 1);
    ts.tm_year = getfield(L, "year", -1, 1900);
    ts.tm_isdst = getboolfield(L, "isdst");
    t = mktime(&ts);
    setallfields(L, &ts);  /* update fields with normalized values */
  }
  if (t != (time_t)(l_timet)t || t == (time_t)(-1))
    luaL_error(L, "time result cannot be represented in this installation");
  l_pushtime(L, t);
  return 1;
}


/**
 * Calculates the difference in seconds between two time values.
 *
 * This function takes two time values (`t1` and `t2`) from the Lua stack, 
 * computes the difference in seconds using the `difftime` function, and 
 * pushes the result as a Lua number onto the stack.
 *
 * @param L The Lua state from which the time values are retrieved and to 
 *          which the result is pushed.
 * @return Returns 1, indicating that one value (the difference in seconds) 
 *         has been pushed onto the Lua stack.
 */
static int os_difftime (lua_State *L) {
  time_t t1 = l_checktime(L, 1);
  time_t t2 = l_checktime(L, 2);
  lua_pushnumber(L, (lua_Number)difftime(t1, t2));
  return 1;
}

/* }====================================================== */


static int os_setlocale (lua_State *L) {
  static const int cat[] = {LC_ALL, LC_COLLATE, LC_CTYPE, LC_MONETARY,
                      LC_NUMERIC, LC_TIME};
  static const char *const catnames[] = {"all", "collate", "ctype", "monetary",
     "numeric", "time", NULL};
  const char *l = luaL_optstring(L, 1, NULL);
  int op = luaL_checkoption(L, 2, "all", catnames);
  lua_pushstring(L, setlocale(cat[op], l));
  return 1;
}


/**
 * Terminates the program with a specified exit status.
 *
 * This function is designed to be called from Lua and allows for controlled
 * program termination. It accepts two optional arguments:
 * 1. The exit status, which can be a boolean or an integer. If a boolean is
 *    provided, `EXIT_SUCCESS` is used for `true` and `EXIT_FAILURE` for `false`.
 *    If an integer is provided, it is used directly as the exit status. If no
 *    status is provided, `EXIT_SUCCESS` is used by default.
 * 2. A boolean flag indicating whether to close the Lua state (`lua_close(L)`)
 *    before exiting. If `true`, the Lua state is closed; otherwise, it is left
 *    open.
 *
 * After processing the arguments, the function calls `exit(status)` to terminate
 * the program. If the Lua state `L` is `NULL`, the function returns `0` to avoid
 * warnings for unreachable code.
 *
 * @param L The Lua state from which the function is called.
 * @return Always returns `0` if `L` is `NULL`; otherwise, the function does not
 *         return as the program exits.
 */
static int os_exit (lua_State *L) {
  int status;
  if (lua_isboolean(L, 1))
    status = (lua_toboolean(L, 1) ? EXIT_SUCCESS : EXIT_FAILURE);
  else
    status = (int)luaL_optinteger(L, 1, EXIT_SUCCESS);
  if (lua_toboolean(L, 2))
    lua_close(L);
  if (L) exit(status);  /* 'if' to avoid warnings for unreachable 'return' */
  return 0;
}


static const luaL_Reg syslib[] = {
  {"clock",     os_clock},
  {"date",      os_date},
  {"difftime",  os_difftime},
  {"execute",   os_execute},
  {"exit",      os_exit},
  {"getenv",    os_getenv},
  {"remove",    os_remove},
  {"rename",    os_rename},
  {"setlocale", os_setlocale},
  {"time",      os_time},
  {"tmpname",   os_tmpname},
  {NULL, NULL}
};

/* }====================================================== */



LUAMOD_API int luaopen_os (lua_State *L) {
  luaL_newlib(L, syslib);
  return 1;
}

