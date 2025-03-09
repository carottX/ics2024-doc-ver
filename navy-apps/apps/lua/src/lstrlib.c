/*
** $Id: lstrlib.c,v 1.254 2016/12/22 13:08:50 roberto Exp $
** Standard library for string operations and pattern-matching
** See Copyright Notice in lua.h
*/

#define lstrlib_c
#define LUA_LIB

#include "lprefix.h"


#include <ctype.h>
#include <float.h>
#include <limits.h>
#include <locale.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lua.h"

#include "lauxlib.h"
#include "lualib.h"


/*
** maximum number of captures that a pattern can do during
** pattern-matching. This limit is arbitrary, but must fit in
** an unsigned char.
*/
#if !defined(LUA_MAXCAPTURES)
#define LUA_MAXCAPTURES		32
#endif


/* macro to 'unsign' a character */
#define uchar(c)	((unsigned char)(c))


/*
** Some sizes are better limited to fit in 'int', but must also fit in
** 'size_t'. (We assume that 'lua_Integer' cannot be smaller than 'int'.)
*/
#define MAX_SIZET	((size_t)(~(size_t)0))

#define MAXSIZE  \
	(sizeof(size_t) < sizeof(int) ? MAX_SIZET : (size_t)(INT_MAX))




/**
 * Calculates the length of a string passed as an argument from the Lua stack.
 * 
 * This function expects a string as its first argument on the Lua stack. It retrieves
 * the string and its length using `luaL_checklstring`, then pushes the length as an
 * integer back onto the Lua stack.
 * 
 * @param L The Lua state from which the string argument is retrieved and to which
 *          the result is pushed.
 * @return Returns 1, indicating that one value (the string length) has been pushed
 *         onto the Lua stack.
 */
static int str_len (lua_State *L) {
  size_t l;
  luaL_checklstring(L, 1, &l);
  lua_pushinteger(L, (lua_Integer)l);
  return 1;
}


/* translate a relative string position: negative means back from end */
static lua_Integer posrelat (lua_Integer pos, size_t len) {
  if (pos >= 0) return pos;
  else if (0u - (size_t)pos > len) return 0;
  else return (lua_Integer)len + pos + 1;
}


/**
 * Extracts a substring from a given string based on specified start and end positions.
 *
 * This function takes a string and two integer indices (start and end) and returns the substring
 * that lies between these indices. The indices are 1-based, meaning the first character of the
 * string is at position 1. The function handles negative indices by interpreting them as positions
 * relative to the end of the string (e.g., -1 refers to the last character).
 *
 * @param L The Lua state object.
 *
 * @return Returns 1, pushing the extracted substring onto the Lua stack. If the start position
 *         is greater than the end position, an empty string is pushed onto the stack.
 *
 * @note The function ensures that the start and end positions are within the bounds of the string.
 *       If the start position is less than 1, it is adjusted to 1. If the end position exceeds the
 *       length of the string, it is adjusted to the length of the string.
 */
static int str_sub (lua_State *L) {
  size_t l;
  const char *s = luaL_checklstring(L, 1, &l);
  lua_Integer start = posrelat(luaL_checkinteger(L, 2), l);
  lua_Integer end = posrelat(luaL_optinteger(L, 3, -1), l);
  if (start < 1) start = 1;
  if (end > (lua_Integer)l) end = l;
  if (start <= end)
    lua_pushlstring(L, s + start - 1, (size_t)(end - start) + 1);
  else lua_pushliteral(L, "");
  return 1;
}


/**
 * Reverses a given string and pushes the result onto the Lua stack.
 *
 * This function takes a string argument from the Lua stack, reverses it,
 * and pushes the reversed string back onto the stack. It uses a Lua buffer
 * to efficiently handle the string manipulation.
 *
 * @param L The Lua state pointer.
 * @return Returns 1, indicating that one value (the reversed string) is pushed onto the Lua stack.
 *
 * @note The input string is expected to be at index 1 of the Lua stack.
 *       The function uses `luaL_checklstring` to retrieve the string and its length,
 *       `luaL_buffinitsize` to initialize a buffer of the same size, and
 *       `luaL_pushresultsize` to push the reversed string onto the stack.
 */
static int str_reverse (lua_State *L) {
  size_t l, i;
  luaL_Buffer b;
  const char *s = luaL_checklstring(L, 1, &l);
  char *p = luaL_buffinitsize(L, &b, l);
  for (i = 0; i < l; i++)
    p[i] = s[l - i - 1];
  luaL_pushresultsize(&b, l);
  return 1;
}


/**
 * Converts a string to lowercase and pushes the result onto the Lua stack.
 *
 * This function takes a string from the Lua stack (at index 1), converts all its characters
 * to lowercase using the `tolower` function, and then pushes the resulting string back onto
 * the Lua stack. The original string is not modified.
 *
 * @param L Pointer to the Lua state.
 * @return Always returns 1, indicating that one value (the lowercase string) is pushed onto the stack.
 *
 * @note The function assumes that the input at index 1 is a string. If not, a Lua error is raised.
 * @note The function uses `luaL_Buffer` for efficient string manipulation and avoids unnecessary allocations.
 */
static int str_lower (lua_State *L) {
  size_t l;
  size_t i;
  luaL_Buffer b;
  const char *s = luaL_checklstring(L, 1, &l);
  char *p = luaL_buffinitsize(L, &b, l);
  for (i=0; i<l; i++)
    p[i] = tolower(uchar(s[i]));
  luaL_pushresultsize(&b, l);
  return 1;
}


/**
 * Converts a given string to uppercase and pushes the result onto the Lua stack.
 *
 * This function takes a string from the Lua stack, converts each character to its
 * uppercase equivalent, and then pushes the resulting string back onto the Lua stack.
 *
 * @param L Pointer to the Lua state.
 * @return Returns 1, indicating that one value (the uppercase string) has been pushed onto the stack.
 *
 * @note The function expects a string as the first argument on the Lua stack.
 *       It uses `luaL_checklstring` to retrieve the string and its length.
 *       The converted string is stored in a buffer managed by `luaL_Buffer`, and
 *       `luaL_pushresultsize` is used to finalize and push the result onto the stack.
 */
static int str_upper (lua_State *L) {
  size_t l;
  size_t i;
  luaL_Buffer b;
  const char *s = luaL_checklstring(L, 1, &l);
  char *p = luaL_buffinitsize(L, &b, l);
  for (i=0; i<l; i++)
    p[i] = toupper(uchar(s[i]));
  luaL_pushresultsize(&b, l);
  return 1;
}


/**
 * Repeats a string `s` `n` times, optionally separated by a separator string `sep`.
 * 
 * This function takes three arguments from the Lua stack:
 * 1. `s` (string): The string to be repeated.
 * 2. `n` (integer): The number of times to repeat the string.
 * 3. `sep` (string, optional): The separator string to insert between repetitions. Defaults to an empty string.
 *
 * The function returns a new string that is the result of repeating `s` `n` times, with `sep` inserted between each repetition.
 * 
 * If `n` is less than or equal to 0, the function returns an empty string.
 * 
 * The function checks for potential overflow in the resulting string size. If the resulting string would be too large,
 * it raises a Lua error with the message "resulting string too large".
 *
 * @param L The Lua state.
 * @return Returns 1, pushing the resulting string onto the Lua stack.
 */
static int str_rep (lua_State *L) {
  size_t l, lsep;
  const char *s = luaL_checklstring(L, 1, &l);
  lua_Integer n = luaL_checkinteger(L, 2);
  const char *sep = luaL_optlstring(L, 3, "", &lsep);
  if (n <= 0) lua_pushliteral(L, "");
  else if (l + lsep < l || l + lsep > MAXSIZE / n)  /* may overflow? */
    return luaL_error(L, "resulting string too large");
  else {
    size_t totallen = (size_t)n * l + (size_t)(n - 1) * lsep;
    luaL_Buffer b;
    char *p = luaL_buffinitsize(L, &b, totallen);
    while (n-- > 1) {  /* first n-1 copies (followed by separator) */
      memcpy(p, s, l * sizeof(char)); p += l;
      if (lsep > 0) {  /* empty 'memcpy' is not that cheap */
        memcpy(p, sep, lsep * sizeof(char));
        p += lsep;
      }
    }
    memcpy(p, s, l * sizeof(char));  /* last copy (not followed by separator) */
    luaL_pushresultsize(&b, totallen);
  }
  return 1;
}


/**
 * Extracts a sequence of bytes from a string and pushes them as integers onto the Lua stack.
 *
 * This function takes a string and two optional integer positions (start and end) as arguments.
 * It extracts the bytes from the string between the specified positions (inclusive) and pushes
 * each byte as an integer onto the Lua stack. If the start or end positions are out of bounds,
 * they are adjusted to the nearest valid position within the string. If the start position is
 * greater than the end position, the function returns 0, indicating an empty interval. If the
 * interval is too large to handle, an error is raised.
 *
 * @param L The Lua state.
 * @return The number of bytes pushed onto the Lua stack.
 *
 * @note The positions are 1-based, meaning the first byte of the string is at position 1.
 * @note If the end position is not provided, it defaults to the start position, effectively
 *       extracting a single byte.
 * @note If the start position is not provided, it defaults to 1, the beginning of the string.
 * @note The function ensures that the Lua stack has enough space to accommodate the extracted
 *       bytes. If not, an error is raised.
 */
static int str_byte (lua_State *L) {
  size_t l;
  const char *s = luaL_checklstring(L, 1, &l);
  lua_Integer posi = posrelat(luaL_optinteger(L, 2, 1), l);
  lua_Integer pose = posrelat(luaL_optinteger(L, 3, posi), l);
  int n, i;
  if (posi < 1) posi = 1;
  if (pose > (lua_Integer)l) pose = l;
  if (posi > pose) return 0;  /* empty interval; return no values */
  if (pose - posi >= INT_MAX)  /* arithmetic overflow? */
    return luaL_error(L, "string slice too long");
  n = (int)(pose -  posi) + 1;
  luaL_checkstack(L, n, "string slice too long");
  for (i=0; i<n; i++)
    lua_pushinteger(L, uchar(s[posi+i-1]));
  return n;
}


/**
 * Converts a list of integer values to a string of corresponding characters.
 * 
 * This function takes a variable number of integer arguments from the Lua stack,
 * where each integer represents a character code. It then constructs a string
 * by converting each integer to its corresponding character. The function ensures
 * that each integer is within the valid range for an unsigned char (0 to 255).
 * 
 * @param L The Lua state, which contains the stack with the integer arguments.
 * 
 * @return Returns 1, pushing the resulting string onto the Lua stack.
 * 
 * @throws Lua error if any integer argument is out of the valid range for an
 *         unsigned char.
 */
static int str_char (lua_State *L) {
  int n = lua_gettop(L);  /* number of arguments */
  int i;
  luaL_Buffer b;
  char *p = luaL_buffinitsize(L, &b, n);
  for (i=1; i<=n; i++) {
    lua_Integer c = luaL_checkinteger(L, i);
    luaL_argcheck(L, uchar(c) == c, i, "value out of range");
    p[i - 1] = uchar(c);
  }
  luaL_pushresultsize(&b, n);
  return 1;
}


/**
 * Writes a block of data to a Lua buffer.
 *
 * This function is designed to be used as a writer function in Lua's buffer operations.
 * It appends a block of data to a Lua buffer without performing any operations on the Lua state.
 *
 * @param L The Lua state. This parameter is unused in the function.
 * @param b A pointer to the block of data to be written.
 * @param size The size of the block of data in bytes.
 * @param B A pointer to the Lua buffer where the data will be appended.
 * @return Always returns 0, indicating success.
 */
static int writer (lua_State *L, const void *b, size_t size, void *B) {
  (void)L;
  luaL_addlstring((luaL_Buffer *) B, (const char *)b, size);
  return 0;
}


/**
 * Serializes a Lua function into a binary string representation.
 *
 * This function takes a Lua function from the top of the stack and converts it into a binary string
 * that can be later loaded back into a Lua state using `lua_load`. The resulting binary string is 
 * pushed onto the stack as the return value.
 *
 * @param L The Lua state.
 * @return Returns 1 on success, pushing the binary string representation of the function onto the stack.
 *         On failure, raises a Lua error with the message "unable to dump given function".
 *
 * @note The function expects a Lua function as its first argument (at stack index 1). The second 
 *       argument (at stack index 2) is optional and, if provided, should be a boolean indicating 
 *       whether to strip debug information from the dumped function (true to strip, false otherwise).
 *
 * @see lua_dump, luaL_Buffer, luaL_buffinit, luaL_pushresult, luaL_error
 */
static int str_dump (lua_State *L) {
  luaL_Buffer b;
  int strip = lua_toboolean(L, 2);
  luaL_checktype(L, 1, LUA_TFUNCTION);
  lua_settop(L, 1);
  luaL_buffinit(L,&b);
  if (lua_dump(L, writer, &b, strip) != 0)
    return luaL_error(L, "unable to dump given function");
  luaL_pushresult(&b);
  return 1;
}



/*
** {======================================================
** PATTERN MATCHING
** =======================================================
*/


#define CAP_UNFINISHED	(-1)
#define CAP_POSITION	(-2)


typedef struct MatchState {
  const char *src_init;  /* init of source string */
  const char *src_end;  /* end ('\0') of source string */
  const char *p_end;  /* end ('\0') of pattern */
  lua_State *L;
  int matchdepth;  /* control for recursive depth (to avoid C stack overflow) */
  unsigned char level;  /* total number of captures (finished or unfinished) */
  struct {
    const char *init;
    ptrdiff_t len;
  } capture[LUA_MAXCAPTURES];
} MatchState;


/* recursive function */
static const char *match (MatchState *ms, const char *s, const char *p);


/* maximum recursion depth for 'match' */
#if !defined(MAXCCALLS)
#define MAXCCALLS	200
#endif


#define L_ESC		'%'
#define SPECIALS	"^$*+?.([%-"


/**
 * Validates and adjusts the capture index for a given match state.
 *
 * This function checks if the provided capture index `l` is valid within the
 * context of the match state `ms`. The index is adjusted by subtracting the
 * ASCII value of '1' to convert it from a 1-based index to a 0-based index.
 * The function then verifies that the adjusted index is within the bounds of
 * the capture levels in `ms` and that the corresponding capture is not marked
 * as unfinished (`CAP_UNFINISHED`). If any of these checks fail, the function
 * raises an error using `luaL_error` with a descriptive message indicating the
 * invalid capture index. If the index is valid, it is returned in its adjusted
 * form.
 *
 * @param ms Pointer to the MatchState structure containing the capture levels
 *           and other match-related data.
 * @param l  The capture index to be validated and adjusted.
 *
 * @return The adjusted capture index if it is valid.
 * @throws Raises an error if the capture index is invalid.
 */
static int check_capture (MatchState *ms, int l) {
  l -= '1';
  if (l < 0 || l >= ms->level || ms->capture[l].len == CAP_UNFINISHED)
    return luaL_error(ms->L, "invalid capture index %%%d", l + 1);
  return l;
}


/**
 * Captures the level of the most recent unfinished capture in the MatchState.
 * 
 * This function iterates through the capture levels in the MatchState, starting
 * from the current level minus one, and searches for the most recent capture
 * that is marked as unfinished (i.e., its length is CAP_UNFINISHED). If such a
 * capture is found, the function returns the level of that capture. If no
 * unfinished capture is found, the function raises an error using `luaL_error`
 * with the message "invalid pattern capture".
 *
 * @param ms A pointer to the MatchState structure containing the capture levels
 *           and related data.
 * @return The level of the most recent unfinished capture, or an error if no
 *         unfinished capture is found.
 */
static int capture_to_close (MatchState *ms) {
  int level = ms->level;
  for (level--; level>=0; level--)
    if (ms->capture[level].len == CAP_UNFINISHED) return level;
  return luaL_error(ms->L, "invalid pattern capture");
}


/**
 * Determines the end of a character class or escape sequence in a pattern string.
 *
 * This function processes the input string `p` starting from the current character.
 * It handles two main cases:
 * 1. If the current character is an escape sequence (L_ESC), it skips the escape character
 *    and returns the position after the escaped character.
 * 2. If the current character is the start of a character class ('['), it scans the string
 *    to find the corresponding closing bracket (']'), skipping any escape sequences within
 *    the character class, and returns the position after the closing bracket.
 * If the pattern is malformed (e.g., missing closing bracket or ends with an escape character),
 * the function raises an error via `luaL_error`.
 *
 * @param ms The MatchState object containing the pattern and state information.
 * @param p The current position in the pattern string to process.
 * @return A pointer to the character in the pattern string immediately after the processed
 *         character class or escape sequence.
 */
static const char *classend (MatchState *ms, const char *p) {
  switch (*p++) {
    case L_ESC: {
      if (p == ms->p_end)
        luaL_error(ms->L, "malformed pattern (ends with '%%')");
      return p+1;
    }
    case '[': {
      if (*p == '^') p++;
      do {  /* look for a ']' */
        if (p == ms->p_end)
          luaL_error(ms->L, "malformed pattern (missing ']')");
        if (*(p++) == L_ESC && p < ms->p_end)
          p++;  /* skip escapes (e.g. '%]') */
      } while (*p != ']');
      return p+1;
    }
    default: {
      return p;
    }
  }
}


/**
 * Determines if a given character matches a specified character class.
 *
 * The function checks if the character `c` belongs to the character class specified by `cl`.
 * The character class is determined by the lowercase value of `cl`:
 * - 'a': Checks if `c` is an alphabetic character (isalpha).
 * - 'c': Checks if `c` is a control character (iscntrl).
 * - 'd': Checks if `c` is a digit (isdigit).
 * - 'g': Checks if `c` is a printable character except space (isgraph).
 * - 'l': Checks if `c` is a lowercase letter (islower).
 * - 'p': Checks if `c` is a punctuation character (ispunct).
 * - 's': Checks if `c` is a whitespace character (isspace).
 * - 'u': Checks if `c` is an uppercase letter (isupper).
 * - 'w': Checks if `c` is an alphanumeric character (isalnum).
 * - 'x': Checks if `c` is a hexadecimal digit (isxdigit).
 * - 'z': Checks if `c` is the null character (deprecated).
 * 
 * If `cl` does not match any of the above character classes, the function returns 1 if `cl` is equal to `c`, otherwise 0.
 * 
 * @param c The character to be checked.
 * @param cl The character class identifier or a specific character to compare with `c`.
 * @return Returns 1 if `c` matches the character class or is equal to `cl`, otherwise returns 0.
 *         If `cl` is uppercase, the result of the character class check is negated.
 */
static int match_class (int c, int cl) {
  int res;
  switch (tolower(cl)) {
    case 'a' : res = isalpha(c); break;
    case 'c' : res = iscntrl(c); break;
    case 'd' : res = isdigit(c); break;
    case 'g' : res = isgraph(c); break;
    case 'l' : res = islower(c); break;
    case 'p' : res = ispunct(c); break;
    case 's' : res = isspace(c); break;
    case 'u' : res = isupper(c); break;
    case 'w' : res = isalnum(c); break;
    case 'x' : res = isxdigit(c); break;
    case 'z' : res = (c == 0); break;  /* deprecated option */
    default: return (cl == c);
  }
  return (islower(cl) ? res : !res);
}


/**
 * Determines if a character matches a bracket class pattern.
 *
 * This function checks if the character `c` matches the pattern specified by the
 * bracket class `p` (starting at `p` and ending at `ec`). The bracket class can
 * include:
 * - A single character match.
 * - A range of characters (e.g., `a-z`).
 * - An escaped character (e.g., `\d`).
 * - A negated class if the pattern starts with `^` (e.g., `[^a-z]`).
 *
 * @param c The character to check against the bracket class.
 * @param p Pointer to the start of the bracket class pattern.
 * @param ec Pointer to the end of the bracket class pattern.
 * @return 1 if `c` matches the bracket class, 0 otherwise. If the bracket class
 *         is negated (starts with `^`), the return value is inverted.
 */
static int matchbracketclass (int c, const char *p, const char *ec) {
  int sig = 1;
  if (*(p+1) == '^') {
    sig = 0;
    p++;  /* skip the '^' */
  }
  while (++p < ec) {
    if (*p == L_ESC) {
      p++;
      if (match_class(c, uchar(*p)))
        return sig;
    }
    else if ((*(p+1) == '-') && (p+2 < ec)) {
      p+=2;
      if (uchar(*(p-2)) <= c && c <= uchar(*p))
        return sig;
    }
    else if (uchar(*p) == c) return sig;
  }
  return !sig;
}


/**
 * @brief Matches a single character in the input string against a pattern.
 *
 * This function is used to determine if a single character in the input string `s` matches
 * the pattern specified by `p`. The pattern can be a literal character, a wildcard (`.`),
 * a character class (e.g., `[a-z]`), or an escaped character (e.g., `\d` for digits).
 *
 * @param ms Pointer to the MatchState structure, which contains the end of the source string.
 * @param s Pointer to the current position in the input string to be matched.
 * @param p Pointer to the current position in the pattern string.
 * @param ep Pointer to the end of the pattern string, used for character class matching.
 *
 * @return Returns 1 if the character matches the pattern, 0 otherwise. If the input string
 *         has reached its end (`s >= ms->src_end`), the function returns 0.
 */
static int singlematch (MatchState *ms, const char *s, const char *p,
                        const char *ep) {
  if (s >= ms->src_end)
    return 0;
  else {
    int c = uchar(*s);
    switch (*p) {
      case '.': return 1;  /* matches any char */
      case L_ESC: return match_class(c, uchar(*(p+1)));
      case '[': return matchbracketclass(c, p, ep-1);
      default:  return (uchar(*p) == c);
    }
  }
}


/**
 * Matches a balanced pair of characters in the input string `s` based on the pattern `p`.
 * 
 * This function is used to find a substring in `s` that starts with the character `b` (the first character in `p`)
 * and ends with the character `e` (the second character in `p`), ensuring that the characters are balanced.
 * The function counts the occurrences of `b` and `e` to ensure that the substring is properly balanced.
 * 
 * @param ms A pointer to the MatchState structure containing the current match state and limits.
 * @param s The input string to search for the balanced pair.
 * @param p The pattern string specifying the start and end characters for the balanced pair.
 * 
 * @return Returns a pointer to the character in `s` immediately after the balanced pair if found.
 *         Returns NULL if the balanced pair is not found or if the string ends out of balance.
 * 
 * @throws Throws a Lua error if the pattern is malformed (e.g., missing arguments to '%b').
 */
static const char *matchbalance (MatchState *ms, const char *s,
                                   const char *p) {
  if (p >= ms->p_end - 1)
    luaL_error(ms->L, "malformed pattern (missing arguments to '%%b')");
  if (*s != *p) return NULL;
  else {
    int b = *p;
    int e = *(p+1);
    int cont = 1;
    while (++s < ms->src_end) {
      if (*s == e) {
        if (--cont == 0) return s+1;
      }
      else if (*s == b) cont++;
    }
  }
  return NULL;  /* string ends out of balance */
}


/**
 * Expands the maximum possible repetitions of a pattern in a string and attempts to match the remaining pattern.
 *
 * This function is used in pattern matching to find the longest possible match for a pattern
 * that allows for repetition (e.g., `*` or `+` in regular expressions). It first determines the
 * maximum number of repetitions of the pattern that can be matched at the current position in
 * the string. Then, it attempts to match the remaining pattern after these repetitions. If the
 * match fails, it reduces the number of repetitions and tries again until a match is found or
 * all possibilities are exhausted.
 *
 * @param ms The MatchState object containing the current match state.
 * @param s The current position in the string being matched.
 * @param p The current position in the pattern being matched.
 * @param ep The end position of the current pattern element (used to determine the pattern to repeat).
 *
 * @return If a match is found, returns a pointer to the position in the string where the match ends.
 *         If no match is found, returns NULL.
 */
static const char *max_expand (MatchState *ms, const char *s,
                                 const char *p, const char *ep) {
  ptrdiff_t i = 0;  /* counts maximum expand for item */
  while (singlematch(ms, s + i, p, ep))
    i++;
  /* keeps trying to match with the maximum repetitions */
  while (i>=0) {
    const char *res = match(ms, (s+i), ep+1);
    if (res) return res;
    i--;  /* else didn't match; reduce 1 repetition to try again */
  }
  return NULL;
}


/**
 * Expands the match by attempting to find the shortest possible match starting from the current position.
 * 
 * This function iteratively attempts to match the pattern `p` against the string `s`, starting from the
 * current position. It uses the `match` function to check for a match and the `singlematch` function to
 * determine if a single character matches the pattern. If a match is found, it returns the position of
 * the match. If no match is found, it returns NULL.
 *
 * @param ms Pointer to the MatchState structure containing the current match state.
 * @param s Pointer to the current position in the string being matched.
 * @param p Pointer to the pattern to match against the string.
 * @param ep Pointer to the end of the pattern.
 * @return Returns a pointer to the position in the string where the match ends if successful, otherwise NULL.
 */
static const char *min_expand (MatchState *ms, const char *s,
                                 const char *p, const char *ep) {
  for (;;) {
    const char *res = match(ms, s, ep+1);
    if (res != NULL)
      return res;
    else if (singlematch(ms, s, p, ep))
      s++;  /* try with one more repetition */
    else return NULL;
  }
}


/**
 * @brief Starts a capture in the match state and attempts to match the pattern.
 *
 * This function initiates a capture in the provided MatchState `ms` at the current level.
 * It sets the initial position of the capture to the string `s` and the length of the capture
 * to the value specified by `what`. The capture level is incremented, and the function then
 * attempts to match the pattern `p` starting from the position `s`. If the match fails, the
 * capture level is decremented to undo the capture. The function returns the result of the
 * match attempt.
 *
 * @param ms Pointer to the MatchState structure where the capture is initiated.
 * @param s The starting position in the string where the capture begins.
 * @param p The pattern to match against the string.
 * @param what The length of the capture or a specific capture type.
 * @return const char* Returns the result of the match attempt. If the match is successful,
 *         it returns the position in the string where the match ends. If the match fails,
 *         it returns NULL.
 */
static const char *start_capture (MatchState *ms, const char *s,
                                    const char *p, int what) {
  const char *res;
  int level = ms->level;
  if (level >= LUA_MAXCAPTURES) luaL_error(ms->L, "too many captures");
  ms->capture[level].init = s;
  ms->capture[level].len = what;
  ms->level = level+1;
  if ((res=match(ms, s, p)) == NULL)  /* match failed? */
    ms->level--;  /* undo capture */
  return res;
}


/**
 * Closes the most recent capture in the MatchState and attempts to match the pattern `p` 
 * starting from the current position `s`. If the match fails, the capture is marked as 
 * unfinished.
 *
 * @param ms Pointer to the MatchState structure containing capture information.
 * @param s  Current position in the input string where the match is being attempted.
 * @param p  Pattern to match against the input string.
 *
 * @return If the match is successful, returns a pointer to the position in the input string
 *         where the match ends. If the match fails, returns NULL and marks the capture as
 *         unfinished.
 */
static const char *end_capture (MatchState *ms, const char *s,
                                  const char *p) {
  int l = capture_to_close(ms);
  const char *res;
  ms->capture[l].len = s - ms->capture[l].init;  /* close capture */
  if ((res = match(ms, s, p)) == NULL)  /* match failed? */
    ms->capture[l].len = CAP_UNFINISHED;  /* undo capture */
  return res;
}


/**
 * Attempts to match a captured substring at the current position in the input string.
 *
 * This function checks if the substring captured at the specified capture index `l`
 * matches the substring starting at the current position `s` in the input string.
 * The capture index `l` is first validated using `check_capture` to ensure it is within
 * the valid range of captures stored in the `MatchState` structure.
 *
 * If the captured substring matches the input string at the current position, the function
 * returns a pointer to the position in the input string immediately after the matched
 * substring. If there is no match, the function returns `NULL`.
 *
 * @param ms Pointer to the `MatchState` structure containing the capture information.
 * @param s Pointer to the current position in the input string where the match is attempted.
 * @param l The capture index to check against the input string.
 * @return A pointer to the position in the input string after the matched substring if a match
 *         is found, otherwise `NULL`.
 */
static const char *match_capture (MatchState *ms, const char *s, int l) {
  size_t len;
  l = check_capture(ms, l);
  len = ms->capture[l].len;
  if ((size_t)(ms->src_end-s) >= len &&
      memcmp(ms->capture[l].init, s, len) == 0)
    return s+len;
  else return NULL;
}


/**
 * @brief Matches a pattern against a string using a recursive approach.
 *
 * This function attempts to match the pattern `p` against the string `s` using a 
 * recursive matching algorithm. It handles various pattern elements such as 
 * captures, balanced strings, frontiers, and optional suffixes. The function 
 * uses a `MatchState` structure to keep track of the matching state, including 
 * the current depth of recursion to prevent stack overflow due to complex patterns.
 *
 * @param ms Pointer to the `MatchState` structure containing the matching state.
 * @param s Pointer to the current position in the string being matched.
 * @param p Pointer to the current position in the pattern being matched.
 * @return Returns a pointer to the position in the string where the match ends, 
 *         or `NULL` if no match is found. The `MatchState` structure is updated 
 *         with the results of any captures.
 */
static const char *match (MatchState *ms, const char *s, const char *p) {
  if (ms->matchdepth-- == 0)
    luaL_error(ms->L, "pattern too complex");
  init: /* using goto's to optimize tail recursion */
  if (p != ms->p_end) {  /* end of pattern? */
    switch (*p) {
      case '(': {  /* start capture */
        if (*(p + 1) == ')')  /* position capture? */
          s = start_capture(ms, s, p + 2, CAP_POSITION);
        else
          s = start_capture(ms, s, p + 1, CAP_UNFINISHED);
        break;
      }
      case ')': {  /* end capture */
        s = end_capture(ms, s, p + 1);
        break;
      }
      case '$': {
        if ((p + 1) != ms->p_end)  /* is the '$' the last char in pattern? */
          goto dflt;  /* no; go to default */
        s = (s == ms->src_end) ? s : NULL;  /* check end of string */
        break;
      }
      case L_ESC: {  /* escaped sequences not in the format class[*+?-]? */
        switch (*(p + 1)) {
          case 'b': {  /* balanced string? */
            s = matchbalance(ms, s, p + 2);
            if (s != NULL) {
              p += 4; goto init;  /* return match(ms, s, p + 4); */
            }  /* else fail (s == NULL) */
            break;
          }
          case 'f': {  /* frontier? */
            const char *ep; char previous;
            p += 2;
            if (*p != '[')
              luaL_error(ms->L, "missing '[' after '%%f' in pattern");
            ep = classend(ms, p);  /* points to what is next */
            previous = (s == ms->src_init) ? '\0' : *(s - 1);
            if (!matchbracketclass(uchar(previous), p, ep - 1) &&
               matchbracketclass(uchar(*s), p, ep - 1)) {
              p = ep; goto init;  /* return match(ms, s, ep); */
            }
            s = NULL;  /* match failed */
            break;
          }
          case '0': case '1': case '2': case '3':
          case '4': case '5': case '6': case '7':
          case '8': case '9': {  /* capture results (%0-%9)? */
            s = match_capture(ms, s, uchar(*(p + 1)));
            if (s != NULL) {
              p += 2; goto init;  /* return match(ms, s, p + 2) */
            }
            break;
          }
          default: goto dflt;
        }
        break;
      }
      default: dflt: {  /* pattern class plus optional suffix */
        const char *ep = classend(ms, p);  /* points to optional suffix */
        /* does not match at least once? */
        if (!singlematch(ms, s, p, ep)) {
          if (*ep == '*' || *ep == '?' || *ep == '-') {  /* accept empty? */
            p = ep + 1; goto init;  /* return match(ms, s, ep + 1); */
          }
          else  /* '+' or no suffix */
            s = NULL;  /* fail */
        }
        else {  /* matched once */
          switch (*ep) {  /* handle optional suffix */
            case '?': {  /* optional */
              const char *res;
              if ((res = match(ms, s + 1, ep + 1)) != NULL)
                s = res;
              else {
                p = ep + 1; goto init;  /* else return match(ms, s, ep + 1); */
              }
              break;
            }
            case '+':  /* 1 or more repetitions */
              s++;  /* 1 match already done */
              /* FALLTHROUGH */
            case '*':  /* 0 or more repetitions */
              s = max_expand(ms, s, p, ep);
              break;
            case '-':  /* 0 or more repetitions (minimum) */
              s = min_expand(ms, s, p, ep);
              break;
            default:  /* no suffix */
              s++; p = ep; goto init;  /* return match(ms, s + 1, ep); */
          }
        }
        break;
      }
    }
  }
  ms->matchdepth++;
  return s;
}



/**
 * Searches for the first occurrence of the string `s2` within the string `s1`.
 * Both strings are treated as memory blocks of specified lengths `l1` and `l2`.
 * 
 * @param s1 Pointer to the memory block to search within.
 * @param l1 Length of the memory block `s1`.
 * @param s2 Pointer to the memory block to search for.
 * @param l2 Length of the memory block `s2`.
 * 
 * @return If `s2` is found within `s1`, returns a pointer to the first occurrence of `s2` in `s1`.
 *         If `s2` is an empty string, returns `s1` (empty strings are considered to be everywhere).
 *         If `s2` is not found within `s1`, returns `NULL`.
 *         If `l2` is greater than `l1`, returns `NULL` (since `s2` cannot fit within `s1`).
 * 
 * @note This function uses `memchr` to locate the first character of `s2` within `s1` and then
 *       uses `memcmp` to compare the remaining characters of `s2` with the corresponding characters
 *       in `s1`. The search is performed efficiently by adjusting the search range after each
 *       unsuccessful attempt.
 */
static const char *lmemfind (const char *s1, size_t l1,
                               const char *s2, size_t l2) {
  if (l2 == 0) return s1;  /* empty strings are everywhere */
  else if (l2 > l1) return NULL;  /* avoids a negative 'l1' */
  else {
    const char *init;  /* to search for a '*s2' inside 's1' */
    l2--;  /* 1st char will be checked by 'memchr' */
    l1 = l1-l2;  /* 's2' cannot be found after that */
    while (l1 > 0 && (init = (const char *)memchr(s1, *s2, l1)) != NULL) {
      init++;   /* 1st char is already checked */
      if (memcmp(init, s2+1, l2) == 0)
        return init-1;
      else {  /* correct 'l1' and 's1' to try again */
        l1 -= init-s1;
        s1 = init;
      }
    }
    return NULL;  /* not found */
  }
}


/**
 * Pushes a captured substring or position to the Lua stack based on the capture index.
 *
 * This function is used to handle captures in a pattern match. It takes a capture index `i`
 * and pushes the corresponding captured substring or position to the Lua stack. If the capture
 * index is invalid or the capture is unfinished, an error is raised.
 *
 * @param ms Pointer to the MatchState structure containing match information and captures.
 * @param i The capture index to be processed. If `i` is 0, the entire match is pushed.
 * @param s Pointer to the start of the string being matched (used when `i` is 0).
 * @param e Pointer to the end of the string being matched (used when `i` is 0).
 *
 * @throws Raises a Lua error if:
 *         - The capture index `i` is invalid (i.e., `i >= ms->level` and `i != 0`).
 *         - The capture is unfinished (i.e., `ms->capture[i].len == CAP_UNFINISHED`).
 *
 * @details If `i` is 0, the entire match (from `s` to `e`) is pushed to the Lua stack. For other
 *          valid indices, the function checks the capture's length. If the length is `CAP_POSITION`,
 *          the position of the capture is pushed as a Lua integer. Otherwise, the captured substring
 *          is pushed as a Lua string.
 */
static void push_onecapture (MatchState *ms, int i, const char *s,
                                                    const char *e) {
  if (i >= ms->level) {
    if (i == 0)  /* ms->level == 0, too */
      lua_pushlstring(ms->L, s, e - s);  /* add whole match */
    else
      luaL_error(ms->L, "invalid capture index %%%d", i + 1);
  }
  else {
    ptrdiff_t l = ms->capture[i].len;
    if (l == CAP_UNFINISHED) luaL_error(ms->L, "unfinished capture");
    if (l == CAP_POSITION)
      lua_pushinteger(ms->L, (ms->capture[i].init - ms->src_init) + 1);
    else
      lua_pushlstring(ms->L, ms->capture[i].init, l);
  }
}


/**
 * Pushes captured substrings from a match operation onto the Lua stack.
 *
 * This function iterates over the capture levels in the MatchState `ms` and pushes
 * each captured substring onto the Lua stack. If `ms->level` is 0 and the start
 * position `s` is non-null, it assumes a single capture level. The function ensures
 * that the Lua stack has enough space to accommodate all captures.
 *
 * @param ms Pointer to the MatchState containing the capture information.
 * @param s The start position of the subject string where the match occurred.
 * @param e The end position of the subject string where the match occurred.
 * @return The number of captured substrings pushed onto the Lua stack.
 */
static int push_captures (MatchState *ms, const char *s, const char *e) {
  int i;
  int nlevels = (ms->level == 0 && s) ? 1 : ms->level;
  luaL_checkstack(ms->L, nlevels, "too many captures");
  for (i = 0; i < nlevels; i++)
    push_onecapture(ms, i, s, e);
  return nlevels;  /* number of strings pushed */
}


/* check whether pattern has no special characters */
static int nospecials (const char *p, size_t l) {
  size_t upto = 0;
  do {
    if (strpbrk(p + upto, SPECIALS))
      return 0;  /* pattern has a special character */
    upto += strlen(p + upto) + 1;  /* may have more after \0 */
  } while (upto <= l);
  return 1;  /* no special chars found */
}


/**
 * Prepares the MatchState structure for pattern matching operations.
 *
 * This function initializes the MatchState structure with the necessary 
 * parameters to perform pattern matching on the given strings. It sets the 
 * Lua state, initializes the match depth counter, and sets the start and end 
 * pointers for both the source string and the pattern string.
 *
 * @param ms Pointer to the MatchState structure to be initialized.
 * @param L Lua state associated with the matching operation.
 * @param s Pointer to the start of the source string to be matched.
 * @param ls Length of the source string.
 * @param p Pointer to the start of the pattern string.
 * @param lp Length of the pattern string.
 */
static void prepstate (MatchState *ms, lua_State *L,
                       const char *s, size_t ls, const char *p, size_t lp) {
  ms->L = L;
  ms->matchdepth = MAXCCALLS;
  ms->src_init = s;
  ms->src_end = s + ls;
  ms->p_end = p + lp;
}


/**
 * Resets the level of the MatchState structure to 0 and asserts that the match depth
 * is equal to the maximum allowed number of recursive calls (MAXCCALLS). This function
 * is typically used to prepare the MatchState for a new pattern matching operation,
 * ensuring that the internal state is properly initialized and that the maximum
 * recursion depth has not been exceeded.
 *
 * @param ms A pointer to the MatchState structure to be reset.
 */
static void reprepstate (MatchState *ms) {
  ms->level = 0;
  lua_assert(ms->matchdepth == MAXCCALLS);
}


/**
 * Searches for a pattern or substring within a string and returns the position(s) of the match.
 *
 * This function is designed to be called from Lua. It takes the following arguments:
 * 1. `s` (string): The string in which to search.
 * 2. `p` (string): The pattern or substring to search for.
 * 3. `init` (optional integer): The starting position for the search (default is 1).
 * 4. `find` (optional boolean): If true, performs a plain substring search instead of pattern matching.
 *
 * The function first validates the input strings and the starting position. If the starting position
 * is invalid (e.g., beyond the string's length), it returns `nil`. If `find` is true or the pattern
 * contains no special characters, it performs a plain substring search using `lmemfind`. Otherwise,
 * it uses pattern matching with the `MatchState` structure.
 *
 * If a match is found, the function returns:
 * - For plain searches: The start and end positions of the substring.
 * - For pattern matches: The start and end positions of the match, along with any captured groups.
 *
 * If no match is found, the function returns `nil`.
 *
 * @param L The Lua state.
 * @param find A flag indicating whether to perform a plain search (1) or pattern matching (0).
 * @return The number of values pushed onto the Lua stack (1 for `nil`, 2 for plain search results,
 *         or more for pattern match results).
 */
static int str_find_aux (lua_State *L, int find) {
  size_t ls, lp;
  const char *s = luaL_checklstring(L, 1, &ls);
  const char *p = luaL_checklstring(L, 2, &lp);
  lua_Integer init = posrelat(luaL_optinteger(L, 3, 1), ls);
  if (init < 1) init = 1;
  else if (init > (lua_Integer)ls + 1) {  /* start after string's end? */
    lua_pushnil(L);  /* cannot find anything */
    return 1;
  }
  /* explicit request or no special characters? */
  if (find && (lua_toboolean(L, 4) || nospecials(p, lp))) {
    /* do a plain search */
    const char *s2 = lmemfind(s + init - 1, ls - (size_t)init + 1, p, lp);
    if (s2) {
      lua_pushinteger(L, (s2 - s) + 1);
      lua_pushinteger(L, (s2 - s) + lp);
      return 2;
    }
  }
  else {
    MatchState ms;
    const char *s1 = s + init - 1;
    int anchor = (*p == '^');
    if (anchor) {
      p++; lp--;  /* skip anchor character */
    }
    prepstate(&ms, L, s, ls, p, lp);
    do {
      const char *res;
      reprepstate(&ms);
      if ((res=match(&ms, s1, p)) != NULL) {
        if (find) {
          lua_pushinteger(L, (s1 - s) + 1);  /* start */
          lua_pushinteger(L, res - s);   /* end */
          return push_captures(&ms, NULL, 0) + 2;
        }
        else
          return push_captures(&ms, s1, res);
      }
    } while (s1++ < ms.src_end && !anchor);
  }
  lua_pushnil(L);  /* not found */
  return 1;
}


/**
 * Searches for the first occurrence of a substring within a string.
 *
 * This function is a Lua C API wrapper that calls the auxiliary function `str_find_aux`
 * to perform the actual search operation. It is designed to be called from Lua scripts.
 *
 * @param L A pointer to the Lua state, which contains the Lua stack with the input arguments.
 *          The stack is expected to contain the string to search in and the substring to search for.
 * @return Returns the result of the search operation as an integer, which is typically the index
 *         of the first occurrence of the substring in the string, or `nil` if the substring is not found.
 *         The result is pushed onto the Lua stack.
 */
static int str_find (lua_State *L) {
  return str_find_aux(L, 1);
}


/**
 * Matches a pattern in a string using Lua's string matching functionality.
 * This function is a wrapper around `str_find_aux` with the `find` parameter set to 0,
 * indicating that it should perform a match operation rather than a find operation.
 * It is typically used in Lua scripts to check if a string matches a specific pattern.
 *
 * @param L Pointer to the Lua state, which holds the stack with the string and pattern to match.
 * @return Returns the result of the match operation as an integer, typically 1 if a match is found
 *         and 0 otherwise. The result is pushed onto the Lua stack.
 */
static int str_match (lua_State *L) {
  return str_find_aux(L, 0);
}


/* state for 'gmatch' */
typedef struct GMatchState {
  const char *src;  /* current position */
  const char *p;  /* pattern */
  const char *lastmatch;  /* end of last match */
  MatchState ms;  /* match state */
} GMatchState;


/**
 * gmatch_aux - Auxiliary function for the gmatch iterator.
 *
 * This function is used internally by the gmatch iterator to perform pattern
 * matching on a given string. It searches for the next match of the pattern
 * `gm->p` in the string `gm->src`, starting from the current position in the
 * string. If a match is found, it updates the state and pushes the captured
 * substrings onto the Lua stack. If no match is found, it returns 0.
 *
 * @param L The Lua state.
 * @return The number of values pushed onto the Lua stack (captured substrings)
 *         if a match is found, or 0 if no match is found.
 */
static int gmatch_aux (lua_State *L) {
  GMatchState *gm = (GMatchState *)lua_touserdata(L, lua_upvalueindex(3));
  const char *src;
  gm->ms.L = L;
  for (src = gm->src; src <= gm->ms.src_end; src++) {
    const char *e;
    reprepstate(&gm->ms);
    if ((e = match(&gm->ms, src, gm->p)) != NULL && e != gm->lastmatch) {
      gm->src = gm->lastmatch = e;
      return push_captures(&gm->ms, src, e);
    }
  }
  return 0;  /* not found */
}


/**
 * Initializes a global match iterator for Lua strings.
 *
 * This function creates a new iterator for matching a pattern `p` against a string `s`.
 * It prepares the match state and returns a closure that can be called repeatedly to
 * find successive matches in the string.
 *
 * @param L The Lua state.
 * @return Returns 1, pushing the iterator closure onto the Lua stack.
 *
 * The function expects two arguments on the Lua stack:
 * 1. `s` (string): The string to be searched.
 * 2. `p` (string): The pattern to search for in the string.
 *
 * The iterator closure returned by this function can be called to find the next match
 * in the string. Each call to the iterator will return the next match until no more
 * matches are found.
 *
 * The match state is stored in a userdata object to avoid garbage collection, and
 * the closure is set up with the match state, the source string, and the pattern.
 */
static int gmatch (lua_State *L) {
  size_t ls, lp;
  const char *s = luaL_checklstring(L, 1, &ls);
  const char *p = luaL_checklstring(L, 2, &lp);
  GMatchState *gm;
  lua_settop(L, 2);  /* keep them on closure to avoid being collected */
  gm = (GMatchState *)lua_newuserdata(L, sizeof(GMatchState));
  prepstate(&gm->ms, L, s, ls, p, lp);
  gm->src = s; gm->p = p; gm->lastmatch = NULL;
  lua_pushcclosure(L, gmatch_aux, 3);
  return 1;
}


/**
 * Adds a replacement string to the buffer `b` based on the provided match state `ms`,
 * start position `s`, and end position `e`. The replacement string is taken from the Lua
 * stack at index 3. This function processes the replacement string, handling escape
 * sequences and capture references.
 *
 * - If a character in the replacement string is not an escape character (`L_ESC`), it is
 *   directly added to the buffer.
 * - If an escape character is encountered, the next character is processed:
 *   - If the next character is not a digit or another escape character, an error is raised.
 *   - If the next character is '0', the entire matched substring (from `s` to `e`) is added
 *     to the buffer.
 *   - If the next character is a digit (1-9), the corresponding capture group is retrieved
 *     from the match state and added to the buffer. If the capture is a number, it is
 *     converted to a string before being added.
 *
 * @param ms The match state containing the Lua state and capture information.
 * @param b The buffer to which the replacement string is added.
 * @param s The start position of the matched substring.
 * @param e The end position of the matched substring.
 */
static void add_s (MatchState *ms, luaL_Buffer *b, const char *s,
                                                   const char *e) {
  size_t l, i;
  lua_State *L = ms->L;
  const char *news = lua_tolstring(L, 3, &l);
  for (i = 0; i < l; i++) {
    if (news[i] != L_ESC)
      luaL_addchar(b, news[i]);
    else {
      i++;  /* skip ESC */
      if (!isdigit(uchar(news[i]))) {
        if (news[i] != L_ESC)
          luaL_error(L, "invalid use of '%c' in replacement string", L_ESC);
        luaL_addchar(b, news[i]);
      }
      else if (news[i] == '0')
          luaL_addlstring(b, s, e - s);
      else {
        push_onecapture(ms, news[i] - '1', s, e);
        luaL_tolstring(L, -1, NULL);  /* if number, convert it to string */
        lua_remove(L, -2);  /* remove original value */
        luaL_addvalue(b);  /* add capture to accumulated result */
      }
    }
  }
}


/**
 * Adds a value to the buffer based on the type of the transformation.
 *
 * This function processes the given string range [s, e) and adds the result to the buffer `b`.
 * The behavior depends on the type of the transformation `tr`:
 * - If `tr` is `LUA_TFUNCTION`, the function pushes the captures from the match state `ms`
 *   onto the Lua stack, calls the function, and adds the result to the buffer.
 * - If `tr` is `LUA_TTABLE`, the function pushes the first capture onto the Lua stack,
 *   retrieves the corresponding value from the table, and adds it to the buffer.
 * - If `tr` is `LUA_TNUMBER` or `LUA_TSTRING`, the function directly adds the string range
 *   [s, e) to the buffer.
 *
 * If the result of the transformation is `nil` or `false`, the original text is preserved.
 * If the result is not a string, an error is raised.
 *
 * @param ms The match state containing the Lua state and captures.
 * @param b The buffer to which the result will be added.
 * @param s The start of the string range to process.
 * @param e The end of the string range to process.
 * @param tr The type of transformation to apply (LUA_TFUNCTION, LUA_TTABLE, LUA_TNUMBER, or LUA_TSTRING).
 */
static void add_value (MatchState *ms, luaL_Buffer *b, const char *s,
                                       const char *e, int tr) {
  lua_State *L = ms->L;
  switch (tr) {
    case LUA_TFUNCTION: {
      int n;
      lua_pushvalue(L, 3);
      n = push_captures(ms, s, e);
      lua_call(L, n, 1);
      break;
    }
    case LUA_TTABLE: {
      push_onecapture(ms, 0, s, e);
      lua_gettable(L, 3);
      break;
    }
    default: {  /* LUA_TNUMBER or LUA_TSTRING */
      add_s(ms, b, s, e);
      return;
    }
  }
  if (!lua_toboolean(L, -1)) {  /* nil or false? */
    lua_pop(L, 1);
    lua_pushlstring(L, s, e - s);  /* keep original text */
  }
  else if (!lua_isstring(L, -1))
    luaL_error(L, "invalid replacement value (a %s)", luaL_typename(L, -1));
  luaL_addvalue(b);  /* add result to accumulator */
}


/**
 * Performs a global substitution on a string based on a pattern and a replacement.
 * 
 * This function takes a subject string, a pattern, and a replacement (which can be a string,
 * number, function, or table) and replaces occurrences of the pattern in the subject string
 * with the replacement. The function supports an optional maximum number of replacements
 * and an anchor character ('^') to force the pattern to match only at the beginning of the
 * subject string.
 *
 * @param L The Lua state.
 * @return Returns 2 values: the modified string and the number of substitutions made.
 *
 * @param[in] L The Lua state.
 * @param[in] 1 The subject string to perform substitutions on.
 * @param[in] 2 The pattern to search for in the subject string.
 * @param[in] 3 The replacement value, which can be a string, number, function, or table.
 * @param[in] 4 (Optional) The maximum number of substitutions to perform. Defaults to the length of the subject string + 1.
 *
 * @note If the pattern starts with '^', the function will only attempt to match the pattern
 * at the beginning of the subject string.
 *
 * @note The replacement can be:
 * - A string: Directly replaces the matched pattern.
 * - A number: Converts the number to a string and replaces the matched pattern.
 * - A function: Calls the function with the matched pattern and uses the return value as the replacement.
 * - A table: Looks up the matched pattern in the table and uses the corresponding value as the replacement.
 */
static int str_gsub (lua_State *L) {
  size_t srcl, lp;
  const char *src = luaL_checklstring(L, 1, &srcl);  /* subject */
  const char *p = luaL_checklstring(L, 2, &lp);  /* pattern */
  const char *lastmatch = NULL;  /* end of last match */
  int tr = lua_type(L, 3);  /* replacement type */
  lua_Integer max_s = luaL_optinteger(L, 4, srcl + 1);  /* max replacements */
  int anchor = (*p == '^');
  lua_Integer n = 0;  /* replacement count */
  MatchState ms;
  luaL_Buffer b;
  luaL_argcheck(L, tr == LUA_TNUMBER || tr == LUA_TSTRING ||
                   tr == LUA_TFUNCTION || tr == LUA_TTABLE, 3,
                      "string/function/table expected");
  luaL_buffinit(L, &b);
  if (anchor) {
    p++; lp--;  /* skip anchor character */
  }
  prepstate(&ms, L, src, srcl, p, lp);
  while (n < max_s) {
    const char *e;
    reprepstate(&ms);  /* (re)prepare state for new match */
    if ((e = match(&ms, src, p)) != NULL && e != lastmatch) {  /* match? */
      n++;
      add_value(&ms, &b, src, e, tr);  /* add replacement to buffer */
      src = lastmatch = e;
    }
    else if (src < ms.src_end)  /* otherwise, skip one character */
      luaL_addchar(&b, *src++);
    else break;  /* end of subject */
    if (anchor) break;
  }
  luaL_addlstring(&b, src, ms.src_end-src);
  luaL_pushresult(&b);
  lua_pushinteger(L, n);  /* number of substitutions */
  return 2;
}

/* }====================================================== */



/*
** {======================================================
** STRING FORMAT
** =======================================================
*/

#if !defined(lua_number2strx)	/* { */

/*
** Hexadecimal floating-point formatter
*/

#include <mymath.h>

#define SIZELENMOD	(sizeof(LUA_NUMBER_FRMLEN)/sizeof(char))


/*
** Number of bits that goes into the first digit. It can be any value
** between 1 and 4; the following definition tries to align the number
** to nibble boundaries by making what is left after that first digit a
** multiple of 4.
*/
#define L_NBFD		((l_mathlim(MANT_DIG) - 1)%4 + 1)


/*
** Add integer part of 'x' to buffer and return new 'x'
*/
static lua_Number adddigit (char *buff, int n, lua_Number x) {
  lua_Number dd = l_mathop(floor)(x);  /* get integer part from 'x' */
  int d = (int)dd;
  buff[n] = (d < 10 ? d + '0' : d - 10 + 'a');  /* add to buffer */
  return x - dd;  /* return what is left */
}


/**
 * Converts a Lua number `x` to a string representation and stores it in the buffer `buff`.
 * The buffer has a maximum size of `sz` bytes. The function handles special cases such as
 * infinity, NaN, and zero, and formats the number in a hexadecimal floating-point notation
 * for other values.
 *
 * @param buff The buffer where the resulting string will be stored.
 * @param sz The maximum size of the buffer `buff`.
 * @param x The Lua number to be converted to a string.
 *
 * @return The number of characters written to the buffer, excluding the null terminator.
 *
 * @note If `LUA_FLOAT_TYPE` is defined as `LUA_FLOAT_INT`, the function delegates the
 * conversion to `lua_number2str`. Otherwise, it handles the conversion internally, ensuring
 * that special values like infinity and NaN are formatted correctly. For non-special values,
 * the number is converted to a hexadecimal floating-point format, including the exponent.
 */
static int num2straux (char *buff, int sz, lua_Number x) {
#if LUA_FLOAT_TYPE == LUA_FLOAT_INT
  return lua_number2str(buff, sz, x);
#else
  /* if 'inf' or 'NaN', format it like '%g' */
  if (x != x || x == (lua_Number)HUGE_VAL || x == -(lua_Number)HUGE_VAL)
    return l_sprintf(buff, sz, LUA_NUMBER_FMT, (LUAI_UACNUMBER)x);
  else if (x == 0) {  /* can be -0... */
    /* create "0" or "-0" followed by exponent */
    return l_sprintf(buff, sz, LUA_NUMBER_FMT "x0p+0", (LUAI_UACNUMBER)x);
  }
  else {
    int e;
    lua_Number m = l_mathop(frexp)(x, &e);  /* 'x' fraction and exponent */
    int n = 0;  /* character count */
    if (m < 0) {  /* is number negative? */
      buff[n++] = '-';  /* add signal */
      m = -m;  /* make it positive */
    }
    buff[n++] = '0'; buff[n++] = 'x';  /* add "0x" */
    m = adddigit(buff, n++, m * (1 << L_NBFD));  /* add first digit */
    e -= L_NBFD;  /* this digit goes before the radix point */
    if (m > 0) {  /* more digits? */
      buff[n++] = lua_getlocaledecpoint();  /* add radix point */
      do {  /* add as many digits as needed */
        m = adddigit(buff, n++, m * 16);
      } while (m > 0);
    }
    n += l_sprintf(buff + n, sz - n, "p%+d", e);  /* add exponent */
    lua_assert(n < sz);
    return n;
  }
#endif
}


/**
 * Converts a Lua number to a string representation based on the specified format
 * and stores it in the provided buffer. The format can include modifiers to
 * control the output, such as converting the string to uppercase.
 *
 * @param L       The Lua state, used for error reporting.
 * @param buff    The buffer where the resulting string will be stored.
 * @param sz      The size of the buffer, ensuring it does not overflow.
 * @param fmt     The format string, which may include modifiers like 'A' or 'a'.
 * @param x       The Lua number to be converted to a string.
 *
 * @return        The number of characters written to the buffer, excluding the
 *                null terminator.
 *
 * @note          The format modifier 'A' converts the output string to uppercase.
 *                The modifier 'a' leaves the string in lowercase. Any other
 *                modifier will result in an error being raised via `luaL_error`.
 */
static int lua_number2strx (lua_State *L, char *buff, int sz,
                            const char *fmt, lua_Number x) {
  int n = num2straux(buff, sz, x);
  if (fmt[SIZELENMOD] == 'A') {
    int i;
    for (i = 0; i < n; i++)
      buff[i] = toupper(uchar(buff[i]));
  }
  else if (fmt[SIZELENMOD] != 'a')
    luaL_error(L, "modifiers for format '%%a'/'%%A' not implemented");
  return n;
}

#endif				/* } */


/*
** Maximum size of each formatted item. This maximum size is produced
** by format('%.99f', -maxfloat), and is equal to 99 + 3 ('-', '.',
** and '\0') + number of decimal digits to represent maxfloat (which
** is maximum exponent + 1). (99+3+1 then rounded to 120 for "extra
** expenses", such as locale-dependent stuff)
*/
#if LUA_FLOAT_TYPE == LUA_FLOAT_INT
#define MAX_ITEM        (120 + 20)
#else
#define MAX_ITEM        (120 + l_mathlim(MAX_10_EXP))
#endif


/* valid flags in a format specification */
#define FLAGS	"-+ #0"

/*
** maximum size of each format specification (such as "%-099.99d")
*/
#define MAX_FORMAT	32


/**
 * Adds a quoted string to the Lua buffer, escaping special characters and control characters.
 * 
 * This function takes a string `s` of length `len` and appends it to the Lua buffer `b` as a
 * quoted string. Special characters such as double quotes (`"`), backslashes (`\`), and newlines
 * (`\n`) are escaped with a backslash. Control characters (non-printable characters) are
 * represented as escape sequences in the form `\ddd`, where `ddd` is the decimal ASCII value of
 * the character. If the control character is followed by a digit, it is padded with leading zeros
 * to ensure it is interpreted correctly (e.g., `\007` instead of `\7`).
 * 
 * @param b The Lua buffer to which the quoted string will be appended.
 * @param s The string to be quoted and added to the buffer.
 * @param len The length of the string `s`.
 */
static void addquoted (luaL_Buffer *b, const char *s, size_t len) {
  luaL_addchar(b, '"');
  while (len--) {
    if (*s == '"' || *s == '\\' || *s == '\n') {
      luaL_addchar(b, '\\');
      luaL_addchar(b, *s);
    }
    else if (iscntrl(uchar(*s))) {
      char buff[10];
      if (!isdigit(uchar(*(s+1))))
        l_sprintf(buff, sizeof(buff), "\\%d", (int)uchar(*s));
      else
        l_sprintf(buff, sizeof(buff), "\\%03d", (int)uchar(*s));
      luaL_addstring(b, buff);
    }
    else
      luaL_addchar(b, *s);
    s++;
  }
  luaL_addchar(b, '"');
}


/*
** Ensures the 'buff' string uses a dot as the radix character.
*/
static void checkdp (char *buff, int nb) {
  if (memchr(buff, '.', nb) == NULL) {  /* no dot? */
    char point = lua_getlocaledecpoint();  /* try locale point */
    char *ppoint = (char *)memchr(buff, point, nb);
    if (ppoint) *ppoint = '.';  /* change it to a dot */
  }
}


/**
 * Adds a literal representation of the Lua value at the specified stack index 
 * to the buffer. The method handles different types of Lua values:
 * - Strings: The string is added to the buffer in a quoted format.
 * - Numbers: Floats are converted to hexadecimal format with a dot, while 
 *   integers are formatted as decimal or hexadecimal (for corner cases).
 * - Nil and Boolean: These values are converted to their string representations 
 *   and added to the buffer.
 * - Other types: An error is raised for values that do not have a literal form.
 *
 * @param L The Lua state.
 * @param b The buffer to which the literal representation will be added.
 * @param arg The stack index of the Lua value to be converted and added.
 */
static void addliteral (lua_State *L, luaL_Buffer *b, int arg) {
  switch (lua_type(L, arg)) {
    case LUA_TSTRING: {
      size_t len;
      const char *s = lua_tolstring(L, arg, &len);
      addquoted(b, s, len);
      break;
    }
    case LUA_TNUMBER: {
      char *buff = luaL_prepbuffsize(b, MAX_ITEM);
      int nb;
      if (!lua_isinteger(L, arg)) {  /* float? */
        lua_Number n = lua_tonumber(L, arg);  /* write as hexa ('%a') */
        nb = lua_number2strx(L, buff, MAX_ITEM, "%" LUA_NUMBER_FRMLEN "a", n);
        checkdp(buff, nb);  /* ensure it uses a dot */
      }
      else {  /* integers */
        lua_Integer n = lua_tointeger(L, arg);
        const char *format = (n == LUA_MININTEGER)  /* corner case? */
                           ? "0x%" LUA_INTEGER_FRMLEN "x"  /* use hexa */
                           : LUA_INTEGER_FMT;  /* else use default format */
        nb = l_sprintf(buff, MAX_ITEM, format, (LUAI_UACINT)n);
      }
      luaL_addsize(b, nb);
      break;
    }
    case LUA_TNIL: case LUA_TBOOLEAN: {
      luaL_tolstring(L, arg, NULL);
      luaL_addvalue(b);
      break;
    }
    default: {
      luaL_argerror(L, arg, "value has no literal form");
    }
  }
}


/**
 * Scans a format string and constructs a valid format specifier for use in printf-like functions.
 * 
 * This function processes the input format string `strfrmt` to extract and validate its components,
 * including flags, width, and precision. It ensures that the format string adheres to the expected
 * structure and constructs a valid format specifier in the `form` buffer. The function returns a
 * pointer to the position in `strfrmt` where the scanning stopped.
 *
 * @param L The Lua state, used for error reporting.
 * @param strfrmt The input format string to be scanned.
 * @param form A buffer where the constructed format specifier will be stored.
 * @return A pointer to the position in `strfrmt` where the scanning stopped.
 *
 * The function performs the following steps:
 * 1. Skips any valid flags in the format string.
 * 2. Validates that the number of flags does not exceed the maximum allowed.
 * 3. Skips the width specifier if present (up to 2 digits).
 * 4. Skips the precision specifier if present (up to 2 digits).
 * 5. Ensures that the format string does not contain invalid or excessively long width/precision.
 * 6. Constructs a valid format specifier in the `form` buffer, prefixed with '%'.
 * 7. Null-terminates the constructed format specifier.
 *
 * If any invalid format is detected, the function raises a Lua error using `luaL_error`.
 */
static const char *scanformat (lua_State *L, const char *strfrmt, char *form) {
  const char *p = strfrmt;
  while (*p != '\0' && strchr(FLAGS, *p) != NULL) p++;  /* skip flags */
  if ((size_t)(p - strfrmt) >= sizeof(FLAGS)/sizeof(char))
    luaL_error(L, "invalid format (repeated flags)");
  if (isdigit(uchar(*p))) p++;  /* skip width */
  if (isdigit(uchar(*p))) p++;  /* (2 digits at most) */
  if (*p == '.') {
    p++;
    if (isdigit(uchar(*p))) p++;  /* skip precision */
    if (isdigit(uchar(*p))) p++;  /* (2 digits at most) */
  }
  if (isdigit(uchar(*p)))
    luaL_error(L, "invalid format (width or precision too long)");
  *(form++) = '%';
  memcpy(form, strfrmt, ((p - strfrmt) + 1) * sizeof(char));
  form += (p - strfrmt) + 1;
  *form = '\0';
  return p;
}


/*
** add length modifier into formats
*/
static void addlenmod (char *form, const char *lenmod) {
  size_t l = strlen(form);
  size_t lm = strlen(lenmod);
  char spec = form[l - 1];
  strcpy(form + l - 1, lenmod);
  form[l + lm - 1] = spec;
  form[l + lm] = '\0';
}


/**
 * Formats a string according to a format specification, similar to `sprintf`.
 * 
 * This function takes a format string and a variable number of arguments from the Lua stack,
 * and produces a formatted string based on the format specifiers. The format string can contain
 * literal characters and format items, which are placeholders for the arguments. Format items
 * start with the '%' character and are followed by a format specifier (e.g., 'd', 'f', 's').
 * 
 * Supported format specifiers include:
 * - 'c': Character (integer as ASCII code).
 * - 'd', 'i': Signed decimal integer.
 * - 'o': Unsigned octal integer.
 * - 'u': Unsigned decimal integer.
 * - 'x', 'X': Unsigned hexadecimal integer (lowercase or uppercase).
 * - 'a', 'A': Hexadecimal floating-point number (lowercase or uppercase).
 * - 'e', 'E': Scientific notation floating-point number (lowercase or uppercase).
 * - 'f': Decimal floating-point number.
 * - 'g', 'G': Shorter of 'e'/'E' or 'f' (lowercase or uppercase).
 * - 'q': Adds a string literal, escaping special characters.
 * - 's': String.
 * 
 * The function processes the format string character by character. If a character is not a '%',
 * it is added directly to the result. If a '%' is encountered, it is treated as the start of a
 * format item, and the corresponding argument is formatted according to the specifier. The formatted
 * result is appended to the output buffer.
 * 
 * @param L The Lua state, which contains the format string and arguments on the stack.
 * @return Returns 1, pushing the formatted string onto the Lua stack.
 */
static int str_format (lua_State *L) {
  int top = lua_gettop(L);
  int arg = 1;
  size_t sfl;
  const char *strfrmt = luaL_checklstring(L, arg, &sfl);
  const char *strfrmt_end = strfrmt+sfl;
  luaL_Buffer b;
  luaL_buffinit(L, &b);
  while (strfrmt < strfrmt_end) {
    if (*strfrmt != L_ESC)
      luaL_addchar(&b, *strfrmt++);
    else if (*++strfrmt == L_ESC)
      luaL_addchar(&b, *strfrmt++);  /* %% */
    else { /* format item */
      char form[MAX_FORMAT];  /* to store the format ('%...') */
      char *buff = luaL_prepbuffsize(&b, MAX_ITEM);  /* to put formatted item */
      int nb = 0;  /* number of bytes in added item */
      if (++arg > top)
        luaL_argerror(L, arg, "no value");
      strfrmt = scanformat(L, strfrmt, form);
      switch (*strfrmt++) {
        case 'c': {
          nb = l_sprintf(buff, MAX_ITEM, form, (int)luaL_checkinteger(L, arg));
          break;
        }
        case 'd': case 'i':
        case 'o': case 'u': case 'x': case 'X': {
          lua_Integer n = luaL_checkinteger(L, arg);
          addlenmod(form, LUA_INTEGER_FRMLEN);
          nb = l_sprintf(buff, MAX_ITEM, form, (LUAI_UACINT)n);
          break;
        }
        case 'a': case 'A':
          addlenmod(form, LUA_NUMBER_FRMLEN);
          nb = lua_number2strx(L, buff, MAX_ITEM, form,
                                  luaL_checknumber(L, arg));
          break;
        case 'e': case 'E': case 'f':
        case 'g': case 'G': {
          lua_Number n = luaL_checknumber(L, arg);
          addlenmod(form, LUA_NUMBER_FRMLEN);
          nb = l_sprintf(buff, MAX_ITEM, form, (LUAI_UACNUMBER)n);
          break;
        }
        case 'q': {
          addliteral(L, &b, arg);
          break;
        }
        case 's': {
          size_t l;
          const char *s = luaL_tolstring(L, arg, &l);
          if (form[2] == '\0')  /* no modifiers? */
            luaL_addvalue(&b);  /* keep entire string */
          else {
            luaL_argcheck(L, l == strlen(s), arg, "string contains zeros");
            if (!strchr(form, '.') && l >= 100) {
              /* no precision and string is too long to be formatted */
              luaL_addvalue(&b);  /* keep entire string */
            }
            else {  /* format the string into 'buff' */
              nb = l_sprintf(buff, MAX_ITEM, form, s);
              lua_pop(L, 1);  /* remove result from 'luaL_tolstring' */
            }
          }
          break;
        }
        default: {  /* also treat cases 'pnLlh' */
          return luaL_error(L, "invalid option '%%%c' to 'format'",
                               *(strfrmt - 1));
        }
      }
      lua_assert(nb < MAX_ITEM);
      luaL_addsize(&b, nb);
    }
  }
  luaL_pushresult(&b);
  return 1;
}

/* }====================================================== */


/*
** {======================================================
** PACK/UNPACK
** =======================================================
*/


/* value used for padding */
#if !defined(LUAL_PACKPADBYTE)
#define LUAL_PACKPADBYTE		0x00
#endif

/* maximum size for the binary representation of an integer */
#define MAXINTSIZE	16

/* number of bits in a character */
#define NB	CHAR_BIT

/* mask for one character (NB 1's) */
#define MC	((1 << NB) - 1)

/* size of a lua_Integer */
#define SZINT	((int)sizeof(lua_Integer))


/* dummy union to get native endianness */
static const union {
  int dummy;
  char little;  /* true iff machine is little endian */
} nativeendian = {1};


/* dummy structure to get native alignment requirements */
struct cD {
  char c;
  union { double d; void *p; lua_Integer i; lua_Number n; } u;
};

#define MAXALIGN	(offsetof(struct cD, u))


/*
** Union for serializing floats
*/
typedef union Ftypes {
  float f;
  double d;
  lua_Number n;
  char buff[5 * sizeof(lua_Number)];  /* enough for any float type */
} Ftypes;


/*
** information to pack/unpack stuff
*/
typedef struct Header {
  lua_State *L;
  int islittle;
  int maxalign;
} Header;


/*
** options for pack/unpack
*/
typedef enum KOption {
  Kint,		/* signed integers */
  Kuint,	/* unsigned integers */
  Kfloat,	/* floating-point numbers */
  Kchar,	/* fixed-length strings */
  Kstring,	/* strings with prefixed length */
  Kzstr,	/* zero-terminated strings */
  Kpadding,	/* padding */
  Kpaddalign,	/* padding for alignment */
  Knop		/* no-op (configuration or spaces) */
} KOption;


/*
** Read an integer numeral from string 'fmt' or return 'df' if
** there is no numeral
*/
static int digit (int c) { return '0' <= c && c <= '9'; }

/**
 * Parses a numeric value from a string and returns it.
 *
 * This method reads a numeric value from the string pointed to by `fmt`. If the current character
 * is not a digit, it returns the default value `df`. If the current character is a digit, it continues
 * to read consecutive digits until a non-digit character is encountered or the value exceeds the safe
 * limit for the integer type. The parsed integer value is returned.
 *
 * @param fmt A pointer to a pointer to the string to parse. The pointer is advanced as digits are read.
 * @param df The default value to return if no numeric value is found at the current position.
 * @return The parsed integer value if digits are found, otherwise the default value `df`.
 */
static int getnum(const char **fmt, int df) {
  if (!digit(**fmt))  /* no number? */
    return df;  /* return default value */
  else {
    int a = 0;
    do {
      a = a*10 + (*((*fmt)++) - '0');
    } while (digit(**fmt) && a <= ((int)MAXSIZE - 9)/10);
    return a;
  }
}


/*
** Read an integer numeral and raises an error if it is larger
** than the maximum size for integers.
*/
static int getnumlimit (Header *h, const char **fmt, int df) {
  int sz = getnum(fmt, df);
  if (sz > MAXINTSIZE || sz <= 0)
    luaL_error(h->L, "integral size (%d) out of limits [1,%d]",
                     sz, MAXINTSIZE);
  return sz;
}


/*
** Initialize Header
*/
static void initheader (lua_State *L, Header *h) {
  h->L = L;
  h->islittle = nativeendian.little;
  h->maxalign = 1;
}


/*
** Read and classify next option. 'size' is filled with option's size.
*/
static KOption getoption (Header *h, const char **fmt, int *size) {
  int opt = *((*fmt)++);
  *size = 0;  /* default */
  switch (opt) {
    case 'b': *size = sizeof(char); return Kint;
    case 'B': *size = sizeof(char); return Kuint;
    case 'h': *size = sizeof(short); return Kint;
    case 'H': *size = sizeof(short); return Kuint;
    case 'l': *size = sizeof(long); return Kint;
    case 'L': *size = sizeof(long); return Kuint;
    case 'j': *size = sizeof(lua_Integer); return Kint;
    case 'J': *size = sizeof(lua_Integer); return Kuint;
    case 'T': *size = sizeof(size_t); return Kuint;
    case 'f': *size = sizeof(float); return Kfloat;
    case 'd': *size = sizeof(double); return Kfloat;
    case 'n': *size = sizeof(lua_Number); return Kfloat;
    case 'i': *size = getnumlimit(h, fmt, sizeof(int)); return Kint;
    case 'I': *size = getnumlimit(h, fmt, sizeof(int)); return Kuint;
    case 's': *size = getnumlimit(h, fmt, sizeof(size_t)); return Kstring;
    case 'c':
      *size = getnum(fmt, -1);
      if (*size == -1)
        luaL_error(h->L, "missing size for format option 'c'");
      return Kchar;
    case 'z': return Kzstr;
    case 'x': *size = 1; return Kpadding;
    case 'X': return Kpaddalign;
    case ' ': break;
    case '<': h->islittle = 1; break;
    case '>': h->islittle = 0; break;
    case '=': h->islittle = nativeendian.little; break;
    case '!': h->maxalign = getnumlimit(h, fmt, MAXALIGN); break;
    default: luaL_error(h->L, "invalid format option '%c'", opt);
  }
  return Knop;
}


/*
** Read, classify, and fill other details about the next option.
** 'psize' is filled with option's size, 'notoalign' with its
** alignment requirements.
** Local variable 'size' gets the size to be aligned. (Kpadal option
** always gets its full alignment, other options are limited by
** the maximum alignment ('maxalign'). Kchar option needs no alignment
** despite its size.
*/
static KOption getdetails (Header *h, size_t totalsize,
                           const char **fmt, int *psize, int *ntoalign) {
  KOption opt = getoption(h, fmt, psize);
  int align = *psize;  /* usually, alignment follows size */
  if (opt == Kpaddalign) {  /* 'X' gets alignment from following option */
    if (**fmt == '\0' || getoption(h, fmt, &align) == Kchar || align == 0)
      luaL_argerror(h->L, 1, "invalid next option for option 'X'");
  }
  if (align <= 1 || opt == Kchar)  /* need no alignment? */
    *ntoalign = 0;
  else {
    if (align > h->maxalign)  /* enforce maximum alignment */
      align = h->maxalign;
    if ((align & (align - 1)) != 0)  /* is 'align' not a power of 2? */
      luaL_argerror(h->L, 1, "format asks for alignment not power of 2");
    *ntoalign = (align - (int)(totalsize & (align - 1))) & (align - 1);
  }
  return opt;
}


/*
** Pack integer 'n' with 'size' bytes and 'islittle' endianness.
** The final 'if' handles the case when 'size' is larger than
** the size of a Lua integer, correcting the extra sign-extension
** bytes if necessary (by default they would be zeros).
*/
static void packint (luaL_Buffer *b, lua_Unsigned n,
                     int islittle, int size, int neg) {
  char *buff = luaL_prepbuffsize(b, size);
  int i;
  buff[islittle ? 0 : size - 1] = (char)(n & MC);  /* first byte */
  for (i = 1; i < size; i++) {
    n >>= NB;
    buff[islittle ? i : size - 1 - i] = (char)(n & MC);
  }
  if (neg && size > SZINT) {  /* negative number need sign extension? */
    for (i = SZINT; i < size; i++)  /* correct extra bytes */
      buff[islittle ? i : size - 1 - i] = (char)MC;
  }
  luaL_addsize(b, size);  /* add result to buffer */
}


/*
** Copy 'size' bytes from 'src' to 'dest', correcting endianness if
** given 'islittle' is different from native endianness.
*/
static void copywithendian (volatile char *dest, volatile const char *src,
                            int size, int islittle) {
  if (islittle == nativeendian.little) {
    while (size-- != 0)
      *(dest++) = *(src++);
  }
  else {
    dest += size - 1;
    while (size-- != 0)
      *(dest--) = *(src++);
  }
}


/**
 * Packs Lua arguments into a binary string according to a specified format.
 *
 * This function takes a format string and a variable number of Lua arguments,
 * and packs them into a binary string based on the format specification. The
 * format string controls the layout and types of the packed data, including
 * integers, floats, strings, and padding.
 *
 * @param L The Lua state.
 * @return Returns 1, pushing the packed binary string onto the Lua stack.
 *
 * The format string consists of characters that specify the type and size of
 * the data to be packed. Supported format options include:
 * - 'i': Signed integer.
 * - 'u': Unsigned integer.
 * - 'f': Floating-point number.
 * - 'c': Fixed-size string.
 * - 's': String with length count.
 * - 'z': Zero-terminated string.
 * - 'x': Padding byte.
 * - ' ': Alignment padding.
 *
 * The function handles alignment, endianness, and overflow checks as needed.
 * The resulting binary string is returned on the Lua stack.
 */
static int str_pack (lua_State *L) {
  luaL_Buffer b;
  Header h;
  const char *fmt = luaL_checkstring(L, 1);  /* format string */
  int arg = 1;  /* current argument to pack */
  size_t totalsize = 0;  /* accumulate total size of result */
  initheader(L, &h);
  lua_pushnil(L);  /* mark to separate arguments from string buffer */
  luaL_buffinit(L, &b);
  while (*fmt != '\0') {
    int size, ntoalign;
    KOption opt = getdetails(&h, totalsize, &fmt, &size, &ntoalign);
    totalsize += ntoalign + size;
    while (ntoalign-- > 0)
     luaL_addchar(&b, LUAL_PACKPADBYTE);  /* fill alignment */
    arg++;
    switch (opt) {
      case Kint: {  /* signed integers */
        lua_Integer n = luaL_checkinteger(L, arg);
        if (size < SZINT) {  /* need overflow check? */
          lua_Integer lim = (lua_Integer)1 << ((size * NB) - 1);
          luaL_argcheck(L, -lim <= n && n < lim, arg, "integer overflow");
        }
        packint(&b, (lua_Unsigned)n, h.islittle, size, (n < 0));
        break;
      }
      case Kuint: {  /* unsigned integers */
        lua_Integer n = luaL_checkinteger(L, arg);
        if (size < SZINT)  /* need overflow check? */
          luaL_argcheck(L, (lua_Unsigned)n < ((lua_Unsigned)1 << (size * NB)),
                           arg, "unsigned overflow");
        packint(&b, (lua_Unsigned)n, h.islittle, size, 0);
        break;
      }
      case Kfloat: {  /* floating-point options */
        volatile Ftypes u;
        char *buff = luaL_prepbuffsize(&b, size);
        lua_Number n = luaL_checknumber(L, arg);  /* get argument */
        if (size == sizeof(u.f)) u.f = (float)n;  /* copy it into 'u' */
        else if (size == sizeof(u.d)) u.d = (double)n;
        else u.n = n;
        /* move 'u' to final result, correcting endianness if needed */
        copywithendian(buff, u.buff, size, h.islittle);
        luaL_addsize(&b, size);
        break;
      }
      case Kchar: {  /* fixed-size string */
        size_t len;
        const char *s = luaL_checklstring(L, arg, &len);
        luaL_argcheck(L, len <= (size_t)size, arg,
                         "string longer than given size");
        luaL_addlstring(&b, s, len);  /* add string */
        while (len++ < (size_t)size)  /* pad extra space */
          luaL_addchar(&b, LUAL_PACKPADBYTE);
        break;
      }
      case Kstring: {  /* strings with length count */
        size_t len;
        const char *s = luaL_checklstring(L, arg, &len);
        luaL_argcheck(L, size >= (int)sizeof(size_t) ||
                         len < ((size_t)1 << (size * NB)),
                         arg, "string length does not fit in given size");
        packint(&b, (lua_Unsigned)len, h.islittle, size, 0);  /* pack length */
        luaL_addlstring(&b, s, len);
        totalsize += len;
        break;
      }
      case Kzstr: {  /* zero-terminated string */
        size_t len;
        const char *s = luaL_checklstring(L, arg, &len);
        luaL_argcheck(L, strlen(s) == len, arg, "string contains zeros");
        luaL_addlstring(&b, s, len);
        luaL_addchar(&b, '\0');  /* add zero at the end */
        totalsize += len + 1;
        break;
      }
      case Kpadding: luaL_addchar(&b, LUAL_PACKPADBYTE);  /* FALLTHROUGH */
      case Kpaddalign: case Knop:
        arg--;  /* undo increment */
        break;
    }
  }
  luaL_pushresult(&b);
  return 1;
}


/**
 * Calculates the total size of a packed binary structure based on a format string.
 *
 * This function takes a format string and computes the total size of the binary
 * structure that would be created by packing data according to the format. The
 * format string specifies the types and alignment of the data fields.
 *
 * @param L The Lua state.
 * @return Returns 1, pushing the total size of the packed structure as an integer
 *         onto the Lua stack.
 *
 * The format string is passed as the first argument to the function. The function
 * iterates through the format string, processing each format specifier to determine
 * the size and alignment requirements of the corresponding data field. The total
 * size is accumulated and checked against a maximum size limit to prevent overflow.
 *
 * If the format string includes variable-length fields (e.g., strings), the function
 * raises an error, as the size of such fields cannot be determined statically.
 *
 * @note The function assumes that the format string is valid and does not contain
 *       unsupported format specifiers.
 */
static int str_packsize (lua_State *L) {
  Header h;
  const char *fmt = luaL_checkstring(L, 1);  /* format string */
  size_t totalsize = 0;  /* accumulate total size of result */
  initheader(L, &h);
  while (*fmt != '\0') {
    int size, ntoalign;
    KOption opt = getdetails(&h, totalsize, &fmt, &size, &ntoalign);
    size += ntoalign;  /* total space used by option */
    luaL_argcheck(L, totalsize <= MAXSIZE - size, 1,
                     "format result too large");
    totalsize += size;
    switch (opt) {
      case Kstring:  /* strings with length count */
      case Kzstr:    /* zero-terminated string */
        luaL_argerror(L, 1, "variable-length format");
        /* call never return, but to avoid warnings: *//* FALLTHROUGH */
      default:  break;
    }
  }
  lua_pushinteger(L, (lua_Integer)totalsize);
  return 1;
}


/*
** Unpack an integer with 'size' bytes and 'islittle' endianness.
** If size is smaller than the size of a Lua integer and integer
** is signed, must do sign extension (propagating the sign to the
** higher bits); if size is larger than the size of a Lua integer,
** it must check the unread bytes to see whether they do not cause an
** overflow.
*/
static lua_Integer unpackint (lua_State *L, const char *str,
                              int islittle, int size, int issigned) {
  lua_Unsigned res = 0;
  int i;
  int limit = (size  <= SZINT) ? size : SZINT;
  for (i = limit - 1; i >= 0; i--) {
    res <<= NB;
    res |= (lua_Unsigned)(unsigned char)str[islittle ? i : size - 1 - i];
  }
  if (size < SZINT) {  /* real size smaller than lua_Integer? */
    if (issigned) {  /* needs sign extension? */
      lua_Unsigned mask = (lua_Unsigned)1 << (size*NB - 1);
      res = ((res ^ mask) - mask);  /* do sign extension */
    }
  }
  else if (size > SZINT) {  /* must check unread bytes */
    int mask = (!issigned || (lua_Integer)res >= 0) ? 0 : MC;
    for (i = limit; i < size; i++) {
      if ((unsigned char)str[islittle ? i : size - 1 - i] != mask)
        luaL_error(L, "%d-byte integer does not fit into Lua Integer", size);
    }
  }
  return (lua_Integer)res;
}


/**
 * Unpacks binary data from a string according to a specified format.
 *
 * This function interprets the binary data in the string `data` using the format
 * string `fmt`. The format string specifies the types and sizes of the data to
 * be unpacked. The function starts unpacking from the specified position `pos`
 * in the data string.
 *
 * The format string can include the following options:
 * - 'i' or 'I': Unpacks a signed or unsigned integer.
 * - 'f' or 'd': Unpacks a float or double.
 * - 'c': Unpacks a character.
 * - 's': Unpacks a string with a length prefix.
 * - 'z': Unpacks a null-terminated string.
 * - 'x': Skips padding or alignment bytes.
 *
 * The function pushes the unpacked values onto the Lua stack and returns the
 * number of results. It also pushes the next position in the data string after
 * unpacking as the last result.
 *
 * @param L The Lua state.
 * @return The number of results pushed onto the Lua stack, including the next
 *         position in the data string.
 */
static int str_unpack (lua_State *L) {
  Header h;
  const char *fmt = luaL_checkstring(L, 1);
  size_t ld;
  const char *data = luaL_checklstring(L, 2, &ld);
  size_t pos = (size_t)posrelat(luaL_optinteger(L, 3, 1), ld) - 1;
  int n = 0;  /* number of results */
  luaL_argcheck(L, pos <= ld, 3, "initial position out of string");
  initheader(L, &h);
  while (*fmt != '\0') {
    int size, ntoalign;
    KOption opt = getdetails(&h, pos, &fmt, &size, &ntoalign);
    if ((size_t)ntoalign + size > ~pos || pos + ntoalign + size > ld)
      luaL_argerror(L, 2, "data string too short");
    pos += ntoalign;  /* skip alignment */
    /* stack space for item + next position */
    luaL_checkstack(L, 2, "too many results");
    n++;
    switch (opt) {
      case Kint:
      case Kuint: {
        lua_Integer res = unpackint(L, data + pos, h.islittle, size,
                                       (opt == Kint));
        lua_pushinteger(L, res);
        break;
      }
      case Kfloat: {
        volatile Ftypes u;
        lua_Number num;
        copywithendian(u.buff, data + pos, size, h.islittle);
        if (size == sizeof(u.f)) num = (lua_Number)u.f;
        else if (size == sizeof(u.d)) num = (lua_Number)u.d;
        else num = u.n;
        lua_pushnumber(L, num);
        break;
      }
      case Kchar: {
        lua_pushlstring(L, data + pos, size);
        break;
      }
      case Kstring: {
        size_t len = (size_t)unpackint(L, data + pos, h.islittle, size, 0);
        luaL_argcheck(L, pos + len + size <= ld, 2, "data string too short");
        lua_pushlstring(L, data + pos + size, len);
        pos += len;  /* skip string */
        break;
      }
      case Kzstr: {
        size_t len = (int)strlen(data + pos);
        lua_pushlstring(L, data + pos, len);
        pos += len + 1;  /* skip string plus final '\0' */
        break;
      }
      case Kpaddalign: case Kpadding: case Knop:
        n--;  /* undo increment */
        break;
    }
    pos += size;
  }
  lua_pushinteger(L, pos + 1);  /* next position */
  return n + 1;
}

/* }====================================================== */


static const luaL_Reg strlib[] = {
  {"byte", str_byte},
  {"char", str_char},
  {"dump", str_dump},
  {"find", str_find},
  {"format", str_format},
  {"gmatch", gmatch},
  {"gsub", str_gsub},
  {"len", str_len},
  {"lower", str_lower},
  {"match", str_match},
  {"rep", str_rep},
  {"reverse", str_reverse},
  {"sub", str_sub},
  {"upper", str_upper},
  {"pack", str_pack},
  {"packsize", str_packsize},
  {"unpack", str_unpack},
  {NULL, NULL}
};


/**
 * Creates a metatable for strings in the Lua state.
 *
 * This function sets up a metatable for strings and assigns the string library
 * as the `__index` field of the metatable. The metatable is created with one
 * slot for the `__index` field. A dummy string is pushed onto the stack to
 * facilitate setting the metatable, and then the metatable is assigned to the
 * dummy string. Finally, the string library is assigned as the `__index` field
 * of the metatable, and the metatable is left on the stack.
 *
 * @param L The Lua state in which to create the metatable.
 */
static void createmetatable (lua_State *L) {
  lua_createtable(L, 0, 1);  /* table to be metatable for strings */
  lua_pushliteral(L, "");  /* dummy string */
  lua_pushvalue(L, -2);  /* copy table */
  lua_setmetatable(L, -2);  /* set table as metatable for strings */
  lua_pop(L, 1);  /* pop dummy string */
  lua_pushvalue(L, -2);  /* get string library */
  lua_setfield(L, -2, "__index");  /* metatable.__index = string */
  lua_pop(L, 1);  /* pop metatable */
}


/*
** Open string library
*/
LUAMOD_API int luaopen_string (lua_State *L) {
  luaL_newlib(L, strlib);
  createmetatable(L);
  return 1;
}

