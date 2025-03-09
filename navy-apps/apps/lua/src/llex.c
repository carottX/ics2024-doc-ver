/*
** $Id: llex.c,v 2.96 2016/05/02 14:02:12 roberto Exp $
** Lexical Analyzer
** See Copyright Notice in lua.h
*/

#define llex_c
#define LUA_CORE

#include "lprefix.h"


#include <locale.h>
#include <string.h>

#include "lua.h"

#include "lctype.h"
#include "ldebug.h"
#include "ldo.h"
#include "lgc.h"
#include "llex.h"
#include "lobject.h"
#include "lparser.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "lzio.h"



#define next(ls) (ls->current = zgetc(ls->z))



#define currIsNewline(ls)	(ls->current == '\n' || ls->current == '\r')


/* ORDER RESERVED */
static const char *const luaX_tokens [] = {
    "and", "break", "do", "else", "elseif",
    "end", "false", "for", "function", "goto", "if",
    "in", "local", "nil", "not", "or", "repeat",
    "return", "then", "true", "until", "while",
    "//", "..", "...", "==", ">=", "<=", "~=",
    "<<", ">>", "::", "<eof>",
    "<number>", "<integer>", "<name>", "<string>"
};


#define save_and_next(ls) (save(ls, ls->current), next(ls))


static l_noret lexerror (LexState *ls, const char *msg, int token);


/**
 * Saves a character `c` into the buffer associated with the lexical state `ls`.
 * If the buffer is full, it dynamically resizes the buffer to accommodate the new character.
 * The buffer size is doubled each time it needs to grow, ensuring efficient memory usage.
 * If the buffer size exceeds `MAX_SIZE / 2`, an error is raised indicating that the lexical
 * element is too long.
 *
 * @param ls Pointer to the lexical state containing the buffer.
 * @param c The character to be saved into the buffer.
 */
static void save (LexState *ls, int c) {
  Mbuffer *b = ls->buff;
  if (luaZ_bufflen(b) + 1 > luaZ_sizebuffer(b)) {
    size_t newsize;
    if (luaZ_sizebuffer(b) >= MAX_SIZE/2)
      lexerror(ls, "lexical element too long", 0);
    newsize = luaZ_sizebuffer(b) * 2;
    luaZ_resizebuffer(ls->L, b, newsize);
  }
  b->buffer[luaZ_bufflen(b)++] = cast(char, c);
}


/**
 * Initializes the lexer for the Lua state by performing the following tasks:
 * 1. Creates the environment name string (`LUA_ENV`) and marks it as a non-collectable object.
 * 2. Iterates over all reserved keywords defined in `luaX_tokens`:
 *    - Creates a string for each reserved keyword.
 *    - Marks the string as a non-collectable object.
 *    - Assigns an extra byte to the string to indicate its status as a reserved word.
 * This function ensures that the environment name and reserved keywords are permanently 
 * available in the Lua state and are not subject to garbage collection.
 *
 * @param L The Lua state to initialize the lexer for.
 */
void luaX_init (lua_State *L) {
  int i;
  TString *e = luaS_newliteral(L, LUA_ENV);  /* create env name */
  luaC_fix(L, obj2gco(e));  /* never collect this name */
  for (i=0; i<NUM_RESERVED; i++) {
    TString *ts = luaS_new(L, luaX_tokens[i]);
    luaC_fix(L, obj2gco(ts));  /* reserved words are never collected */
    ts->extra = cast_byte(i+1);  /* reserved word */
  }
}


/**
 * Converts a given token into its corresponding string representation.
 *
 * This function takes a token (an integer) and converts it into a human-readable
 * string. If the token represents a single-byte symbol (e.g., punctuation or
 * operators), it returns the symbol enclosed in single quotes. If the token is a
 * reserved word or a multi-character symbol, it returns the word or symbol enclosed
 * in single quotes. For other types of tokens (e.g., identifiers, strings, or
 * numerals), it returns the token's name directly without any formatting.
 *
 * @param ls Pointer to the LexState structure, which contains the lexical state
 *           of the parser, including the Lua state (L).
 * @param token The token to be converted into a string.
 * @return A string representation of the token. The returned string is either
 *         dynamically allocated or a pointer to a static string, depending on
 *         the token type.
 */
const char *luaX_token2str (LexState *ls, int token) {
  if (token < FIRST_RESERVED) {  /* single-byte symbols? */
    lua_assert(token == cast_uchar(token));
    return luaO_pushfstring(ls->L, "'%c'", token);
  }
  else {
    const char *s = luaX_tokens[token - FIRST_RESERVED];
    if (token < TK_EOS)  /* fixed format (symbols and reserved words)? */
      return luaO_pushfstring(ls->L, "'%s'", s);
    else  /* names, strings, and numerals */
      return s;
  }
}


/**
 * Retrieves the textual representation of a given token in the lexical state.
 *
 * This function handles specific token types (TK_NAME, TK_STRING, TK_FLT, TK_INT) by saving a null character
 * to the buffer and returning a formatted string representation of the token's content. For all other token types,
 * it returns the corresponding string representation using `luaX_token2str`.
 *
 * @param ls Pointer to the LexState structure containing the lexical analyzer state.
 * @param token The token type to be converted to a textual representation.
 * @return A string representing the token. For specific token types, it returns a formatted string with the token's content.
 *         For other tokens, it returns the standard string representation.
 */
static const char *txtToken (LexState *ls, int token) {
  switch (token) {
    case TK_NAME: case TK_STRING:
    case TK_FLT: case TK_INT:
      save(ls, '\0');
      return luaO_pushfstring(ls->L, "'%s'", luaZ_buffer(ls->buff));
    default:
      return luaX_token2str(ls, token);
  }
}


/**
 * Reports a lexical error during the parsing process.
 *
 * This function constructs an error message by adding source file information
 * (such as the file name and line number) to the provided error message `msg`.
 * If a `token` is provided, it appends the token's textual representation to
 * the error message to indicate where the error occurred. Finally, the function
 * throws a syntax error using `luaD_throw`, which terminates the current
 * execution with a `LUA_ERRSYNTAX` error code.
 *
 * @param ls The LexState object containing lexical analyzer state information.
 * @param msg The base error message to be reported.
 * @param token The token associated with the error, or 0 if no specific token is involved.
 * @note This function does not return; it terminates execution by throwing an error.
 */
static l_noret lexerror (LexState *ls, const char *msg, int token) {
  msg = luaG_addinfo(ls->L, msg, ls->source, ls->linenumber);
  if (token)
    luaO_pushfstring(ls->L, "%s near %s", msg, txtToken(ls, token));
  luaD_throw(ls->L, LUA_ERRSYNTAX);
}


/**
 * @brief Reports a syntax error during lexical analysis.
 *
 * This function is used to report a syntax error encountered during the lexical
 * analysis phase of Lua parsing. It delegates the error reporting to the
 * `lexerror` function, passing the current lexical state (`ls`), the error
 * message (`msg`), and the current token (`ls->t.token`) that caused the error.
 *
 * @param ls Pointer to the LexState structure representing the current lexical
 *           state of the parser.
 * @param msg A string containing the error message to be reported.
 * @return This function does not return; it triggers an error handling mechanism
 *         that typically terminates the parsing process.
 */
l_noret luaX_syntaxerror (LexState *ls, const char *msg) {
  lexerror(ls, msg, ls->t.token);
}


/*
** creates a new string and anchors it in scanner's table so that
** it will not be collected until the end of the compilation
** (by that time it should be anchored somewhere)
*/
TString *luaX_newstring (LexState *ls, const char *str, size_t l) {
  lua_State *L = ls->L;
  TValue *o;  /* entry for 'str' */
  TString *ts = luaS_newlstr(L, str, l);  /* create new string */
  setsvalue2s(L, L->top++, ts);  /* temporarily anchor it in stack */
  o = luaH_set(L, ls->h, L->top - 1);
  if (ttisnil(o)) {  /* not in use yet? */
    /* boolean value does not need GC barrier;
       table has no metatable, so it does not need to invalidate cache */
    setbvalue(o, 1);  /* t[string] = true */
    luaC_checkGC(L);
  }
  else {  /* string already present */
    ts = tsvalue(keyfromval(o));  /* re-use value previously stored */
  }
  L->top--;  /* remove string from stack */
  return ts;
}


/*
** increment line number and skips newline sequence (any of
** \n, \r, \n\r, or \r\n)
*/
static void inclinenumber (LexState *ls) {
  int old = ls->current;
  lua_assert(currIsNewline(ls));
  next(ls);  /* skip '\n' or '\r' */
  if (currIsNewline(ls) && ls->current != old)
    next(ls);  /* skip '\n\r' or '\r\n' */
  if (++ls->linenumber >= MAX_INT)
    lexerror(ls, "chunk has too many lines", 0);
}


/**
 * Initializes the lexical state (`LexState`) for a new input source.
 *
 * This function sets up the lexical analyzer state (`ls`) to begin processing
 * input from the provided source. It initializes various fields of the `LexState`
 * structure, including the associated Lua state (`L`), the input source (`z`),
 * the source name (`source`), and the first character of the input (`firstchar`).
 *
 * The function also prepares the lexical analyzer for tokenization by setting
 * the initial state of the token buffer, lookahead token, and line numbering.
 * Additionally, it initializes the environment name and resizes the internal
 * buffer to the minimum required size.
 *
 * @param L The Lua state associated with the lexical analyzer.
 * @param ls The lexical state structure to initialize.
 * @param z The input source (ZIO stream) to read from.
 * @param source The name of the input source (e.g., file name or string).
 * @param firstchar The first character of the input to process.
 */
void luaX_setinput (lua_State *L, LexState *ls, ZIO *z, TString *source,
                    int firstchar) {
  ls->t.token = 0;
  ls->L = L;
  ls->current = firstchar;
  ls->lookahead.token = TK_EOS;  /* no look-ahead token */
  ls->z = z;
  ls->fs = NULL;
  ls->linenumber = 1;
  ls->lastline = 1;
  ls->source = source;
  ls->envn = luaS_newliteral(L, LUA_ENV);  /* get env name */
  luaZ_resizebuffer(ls->L, ls->buff, LUA_MINBUFFER);  /* initialize buffer */
}



/*
** =======================================================
** LEXICAL ANALYZER
** =======================================================
*/


static int check_next1 (LexState *ls, int c) {
  if (ls->current == c) {
    next(ls);
    return 1;
  }
  else return 0;
}


/*
** Check whether current char is in set 'set' (with two chars) and
** saves it
*/
static int check_next2 (LexState *ls, const char *set) {
  lua_assert(set[2] == '\0');
  if (ls->current == set[0] || ls->current == set[1]) {
    save_and_next(ls);
    return 1;
  }
  else return 0;
}


/* LUA_NUMBER */
/*
** this function is quite liberal in what it accepts, as 'luaO_str2num'
** will reject ill-formed numerals.
*/
static int read_numeral (LexState *ls, SemInfo *seminfo) {
  TValue obj;
  const char *expo = "Ee";
  int first = ls->current;
  lua_assert(lisdigit(ls->current));
  save_and_next(ls);
  if (first == '0' && check_next2(ls, "xX"))  /* hexadecimal? */
    expo = "Pp";
  for (;;) {
    if (check_next2(ls, expo))  /* exponent part? */
      check_next2(ls, "-+");  /* optional exponent sign */
    if (lisxdigit(ls->current))
      save_and_next(ls);
    else if (ls->current == '.')
      save_and_next(ls);
    else break;
  }
  save(ls, '\0');
  if (luaO_str2num(luaZ_buffer(ls->buff), &obj) == 0)  /* format error? */
    lexerror(ls, "malformed number", TK_FLT);
  if (ttisinteger(&obj)) {
    seminfo->i = ivalue(&obj);
    return TK_INT;
  }
  else {
    lua_assert(ttisfloat(&obj));
    seminfo->r = fltvalue(&obj);
    return TK_FLT;
  }
}


/*
** skip a sequence '[=*[' or ']=*]'; if sequence is well formed, return
** its number of '='s; otherwise, return a negative number (-1 iff there
** are no '='s after initial bracket)
*/
static int skip_sep (LexState *ls) {
  int count = 0;
  int s = ls->current;
  lua_assert(s == '[' || s == ']');
  save_and_next(ls);
  while (ls->current == '=') {
    save_and_next(ls);
    count++;
  }
  return (ls->current == s) ? count : (-count) - 1;
}


/**
 * Reads a long string or long comment from the input stream and processes it.
 *
 * This function is used to handle long strings or long comments in the lexer. 
 * A long string is enclosed in double square brackets (e.g., `[[...]]`), and 
 * a long comment is similarly enclosed but typically starts with `--[[...]]`.
 *
 * The function starts by skipping the second '[' character and checks if the 
 * string starts with a newline. If it does, the newline is skipped. The function 
 * then reads characters until it encounters the corresponding closing sequence 
 * (e.g., `]]`). If the input ends prematurely (EOZ), an error is raised.
 *
 * During processing, newlines are saved and the line counter is incremented. 
 * If the function is processing a comment (indicated by `seminfo` being NULL), 
 * the buffer is reset to avoid wasting space. For long strings, the content is 
 * saved and eventually stored in the `seminfo` structure as a new string.
 *
 * @param ls Pointer to the LexState structure, which holds the lexer's state.
 * @param seminfo Pointer to the SemInfo structure where the resulting string 
 *                will be stored. If NULL, the function processes a long comment.
 * @param sep The number of '=' characters between the square brackets that 
 *            define the long string or comment. This helps in matching the 
 *            closing sequence.
 */
static void read_long_string (LexState *ls, SemInfo *seminfo, int sep) {
  int line = ls->linenumber;  /* initial line (for error message) */
  save_and_next(ls);  /* skip 2nd '[' */
  if (currIsNewline(ls))  /* string starts with a newline? */
    inclinenumber(ls);  /* skip it */
  for (;;) {
    switch (ls->current) {
      case EOZ: {  /* error */
        const char *what = (seminfo ? "string" : "comment");
        const char *msg = luaO_pushfstring(ls->L,
                     "unfinished long %s (starting at line %d)", what, line);
        lexerror(ls, msg, TK_EOS);
        break;  /* to avoid warnings */
      }
      case ']': {
        if (skip_sep(ls) == sep) {
          save_and_next(ls);  /* skip 2nd ']' */
          goto endloop;
        }
        break;
      }
      case '\n': case '\r': {
        save(ls, '\n');
        inclinenumber(ls);
        if (!seminfo) luaZ_resetbuffer(ls->buff);  /* avoid wasting space */
        break;
      }
      default: {
        if (seminfo) save_and_next(ls);
        else next(ls);
      }
    }
  } endloop:
  if (seminfo)
    seminfo->ts = luaX_newstring(ls, luaZ_buffer(ls->buff) + (2 + sep),
                                     luaZ_bufflen(ls->buff) - 2*(2 + sep));
}


/**
 * Checks if the given character `c` is valid in the current lexical context.
 * If `c` is null (0), it indicates an invalid escape sequence or unexpected
 * character. In such cases, the method appends the current character from the
 * LexState buffer (if not at the end of input) to provide context in the error
 * message, and then raises a lexical error with the specified message `msg`.
 *
 * @param ls Pointer to the LexState object representing the current lexical state.
 * @param c The character to validate.
 * @param msg The error message to display if `c` is invalid.
 */
static void esccheck (LexState *ls, int c, const char *msg) {
  if (!c) {
    if (ls->current != EOZ)
      save_and_next(ls);  /* add current to buffer for error message */
    lexerror(ls, msg, TK_STRING);
  }
}


/**
 * Reads a hexadecimal digit from the input stream and returns its integer value.
 *
 * This function advances the lexer's input stream by one character and checks
 * if the current character is a valid hexadecimal digit (0-9, a-f, or A-F). If the
 * character is not a valid hexadecimal digit, an error is raised. If it is valid,
 * the function converts the hexadecimal digit to its corresponding integer value
 * and returns it.
 *
 * @param ls Pointer to the LexState object representing the lexer's state.
 * @return The integer value of the hexadecimal digit read from the input stream.
 */
static int gethexa (LexState *ls) {
  save_and_next(ls);
  esccheck (ls, lisxdigit(ls->current), "hexadecimal digit expected");
  return luaO_hexavalue(ls->current);
}


/**
 * Reads a hexadecimal escape sequence from the input buffer and converts it to an integer.
 * 
 * This function reads two hexadecimal characters from the input buffer, combines them to form
 * a single 8-bit integer value, and then removes the two characters from the buffer. The function
 * assumes that the input characters are valid hexadecimal digits (0-9, a-f, A-F).
 *
 * @param ls Pointer to the LexState object containing the input buffer and other lexical state information.
 * @return The integer value corresponding to the hexadecimal escape sequence.
 */
static int readhexaesc (LexState *ls) {
  int r = gethexa(ls);
  r = (r << 4) + gethexa(ls);
  luaZ_buffremove(ls->buff, 2);  /* remove saved chars from buffer */
  return r;
}


/**
 * Reads a UTF-8 escape sequence from the input stream and returns its Unicode code point.
 * 
 * The method expects a sequence starting with 'u' followed by a hexadecimal number enclosed in curly braces.
 * The hexadecimal number represents a Unicode code point, which must be within the valid UTF-8 range (0x0 to 0x10FFFF).
 * 
 * The method performs the following steps:
 * 1. Skips the 'u' character.
 * 2. Validates that the next character is '{'.
 * 3. Reads the hexadecimal digits, converting them to a Unicode code point.
 * 4. Validates that the code point is within the valid UTF-8 range.
 * 5. Validates that the sequence ends with '}'.
 * 6. Removes the processed characters from the buffer.
 * 
 * @param ls The LexState object representing the current lexical state.
 * @return The Unicode code point represented by the UTF-8 escape sequence.
 * @throws Lua error if the sequence is malformed or the code point is invalid.
 */
static unsigned long readutf8esc (LexState *ls) {
  unsigned long r;
  int i = 4;  /* chars to be removed: '\', 'u', '{', and first digit */
  save_and_next(ls);  /* skip 'u' */
  esccheck(ls, ls->current == '{', "missing '{'");
  r = gethexa(ls);  /* must have at least one digit */
  while ((save_and_next(ls), lisxdigit(ls->current))) {
    i++;
    r = (r << 4) + luaO_hexavalue(ls->current);
    esccheck(ls, r <= 0x10FFFF, "UTF-8 value too large");
  }
  esccheck(ls, ls->current == '}', "missing '}'");
  next(ls);  /* skip '}' */
  luaZ_buffremove(ls->buff, i);  /* remove saved chars from buffer */
  return r;
}


/**
 * Encodes a Unicode code point into UTF-8 and appends it to the current string being processed.
 *
 * This function reads a Unicode code point using `readutf8esc`, encodes it into UTF-8 using
 * `luaO_utf8esc`, and stores the resulting bytes in a buffer. The encoded bytes are then
 * appended to the string being constructed by calling the `save` function for each byte.
 *
 * @param ls Pointer to the LexState structure, which holds the current state of the lexer.
 *           This includes the string being constructed and other relevant context.
 *
 * The function uses a buffer of size `UTF8BUFFSZ` to temporarily store the UTF-8 encoded bytes.
 * The `luaO_utf8esc` function encodes the code point into this buffer and returns the number
 * of bytes used. The `save` function is then called for each byte to append it to the string.
 *
 * This function is typically used during lexing to handle Unicode escape sequences in the input.
 */
static void utf8esc (LexState *ls) {
  char buff[UTF8BUFFSZ];
  int n = luaO_utf8esc(buff, readutf8esc(ls));
  for (; n > 0; n--)  /* add 'buff' to string */
    save(ls, buff[UTF8BUFFSZ - n]);
}


/**
 * Reads and parses a decimal escape sequence from the input stream.
 * 
 * This function reads up to 3 consecutive digits from the input stream and 
 * converts them into a single integer value. The digits are expected to 
 * represent a decimal escape sequence, typically used in string literals.
 * 
 * The function ensures that the resulting value does not exceed the maximum 
 * value that can be stored in an unsigned char (UCHAR_MAX). If the value 
 * exceeds this limit, an error is raised using `esccheck`.
 * 
 * After reading the digits, they are removed from the input buffer to prevent 
 * them from being processed again.
 * 
 * @param ls Pointer to the LexState structure containing the current state of 
 *           the lexical analyzer, including the input buffer and current 
 *           character.
 * @return The integer value corresponding to the parsed decimal escape 
 *         sequence.
 */
static int readdecesc (LexState *ls) {
  int i;
  int r = 0;  /* result accumulator */
  for (i = 0; i < 3 && lisdigit(ls->current); i++) {  /* read up to 3 digits */
    r = 10*r + ls->current - '0';
    save_and_next(ls);
  }
  esccheck(ls, r <= UCHAR_MAX, "decimal escape too large");
  luaZ_buffremove(ls->buff, i);  /* remove read digits from buffer */
  return r;
}


/**
 * Reads a string from the lexer state, handling escape sequences and delimiters.
 *
 * This function processes a string literal from the lexer state `ls`, starting from the current
 * character and continuing until the specified delimiter `del` is encountered. It handles various
 * escape sequences (e.g., `\n`, `\t`, `\x`, `\u`, etc.) and ensures proper handling of newlines and
 * other special characters. The resulting string is stored in the `seminfo` structure.
 *
 * @param ls The lexer state containing the input stream and buffer.
 * @param del The delimiter character that marks the end of the string (e.g., `"` or `'`).
 * @param seminfo A pointer to the semantic information structure where the resulting string is stored.
 *
 * @note This function assumes that the lexer state is properly initialized and that the current
 *       character is the opening delimiter of the string. It raises errors for unfinished strings
 *       or invalid escape sequences.
 */
static void read_string (LexState *ls, int del, SemInfo *seminfo) {
  save_and_next(ls);  /* keep delimiter (for error messages) */
  while (ls->current != del) {
    switch (ls->current) {
      case EOZ:
        lexerror(ls, "unfinished string", TK_EOS);
        break;  /* to avoid warnings */
      case '\n':
      case '\r':
        lexerror(ls, "unfinished string", TK_STRING);
        break;  /* to avoid warnings */
      case '\\': {  /* escape sequences */
        int c;  /* final character to be saved */
        save_and_next(ls);  /* keep '\\' for error messages */
        switch (ls->current) {
          case 'a': c = '\a'; goto read_save;
          case 'b': c = '\b'; goto read_save;
          case 'f': c = '\f'; goto read_save;
          case 'n': c = '\n'; goto read_save;
          case 'r': c = '\r'; goto read_save;
          case 't': c = '\t'; goto read_save;
          case 'v': c = '\v'; goto read_save;
          case 'x': c = readhexaesc(ls); goto read_save;
          case 'u': utf8esc(ls);  goto no_save;
          case '\n': case '\r':
            inclinenumber(ls); c = '\n'; goto only_save;
          case '\\': case '\"': case '\'':
            c = ls->current; goto read_save;
          case EOZ: goto no_save;  /* will raise an error next loop */
          case 'z': {  /* zap following span of spaces */
            luaZ_buffremove(ls->buff, 1);  /* remove '\\' */
            next(ls);  /* skip the 'z' */
            while (lisspace(ls->current)) {
              if (currIsNewline(ls)) inclinenumber(ls);
              else next(ls);
            }
            goto no_save;
          }
          default: {
            esccheck(ls, lisdigit(ls->current), "invalid escape sequence");
            c = readdecesc(ls);  /* digital escape '\ddd' */
            goto only_save;
          }
        }
       read_save:
         next(ls);
         /* go through */
       only_save:
         luaZ_buffremove(ls->buff, 1);  /* remove '\\' */
         save(ls, c);
         /* go through */
       no_save: break;
      }
      default:
        save_and_next(ls);
    }
  }
  save_and_next(ls);  /* skip delimiter */
  seminfo->ts = luaX_newstring(ls, luaZ_buffer(ls->buff) + 1,
                                   luaZ_bufflen(ls->buff) - 2);
}


/**
 * Lexical analyzer for Lua source code.
 *
 * This function processes the input source code character by character, identifying
 * and returning tokens that represent the lexical elements of the language. It handles
 * various constructs such as comments, strings, numbers, operators, identifiers, and
 * reserved words.
 *
 * @param ls Pointer to the LexState structure, which holds the current state of the
 *           lexical analysis, including the input buffer and current character.
 * @param seminfo Pointer to the SemInfo structure, which is used to store semantic
 *                information (e.g., string values, numbers) associated with certain tokens.
 *
 * @return An integer representing the type of token identified. The return value can be
 *         a single-character token (e.g., '+', '-', '/'), a token type constant (e.g.,
 *         TK_STRING, TK_EOS, TK_NAME), or a reserved word token (e.g., TK_IF, TK_WHILE).
 *
 * The function processes the input in a loop, examining the current character and
 * determining the appropriate action based on its value. It handles line breaks,
 * whitespace, comments, strings, numbers, operators, and identifiers. For multi-character
 * tokens (e.g., '==', '..', '--'), it checks subsequent characters to determine the
 * correct token type. The function also updates the LexState structure as it processes
 * the input, advancing the current character and resetting the buffer as needed.
 */
static int llex (LexState *ls, SemInfo *seminfo) {
  luaZ_resetbuffer(ls->buff);
  for (;;) {
    switch (ls->current) {
      case '\n': case '\r': {  /* line breaks */
        inclinenumber(ls);
        break;
      }
      case ' ': case '\f': case '\t': case '\v': {  /* spaces */
        next(ls);
        break;
      }
      case '-': {  /* '-' or '--' (comment) */
        next(ls);
        if (ls->current != '-') return '-';
        /* else is a comment */
        next(ls);
        if (ls->current == '[') {  /* long comment? */
          int sep = skip_sep(ls);
          luaZ_resetbuffer(ls->buff);  /* 'skip_sep' may dirty the buffer */
          if (sep >= 0) {
            read_long_string(ls, NULL, sep);  /* skip long comment */
            luaZ_resetbuffer(ls->buff);  /* previous call may dirty the buff. */
            break;
          }
        }
        /* else short comment */
        while (!currIsNewline(ls) && ls->current != EOZ)
          next(ls);  /* skip until end of line (or end of file) */
        break;
      }
      case '[': {  /* long string or simply '[' */
        int sep = skip_sep(ls);
        if (sep >= 0) {
          read_long_string(ls, seminfo, sep);
          return TK_STRING;
        }
        else if (sep != -1)  /* '[=...' missing second bracket */
          lexerror(ls, "invalid long string delimiter", TK_STRING);
        return '[';
      }
      case '=': {
        next(ls);
        if (check_next1(ls, '=')) return TK_EQ;
        else return '=';
      }
      case '<': {
        next(ls);
        if (check_next1(ls, '=')) return TK_LE;
        else if (check_next1(ls, '<')) return TK_SHL;
        else return '<';
      }
      case '>': {
        next(ls);
        if (check_next1(ls, '=')) return TK_GE;
        else if (check_next1(ls, '>')) return TK_SHR;
        else return '>';
      }
      case '/': {
        next(ls);
        if (check_next1(ls, '/')) return TK_IDIV;
        else return '/';
      }
      case '~': {
        next(ls);
        if (check_next1(ls, '=')) return TK_NE;
        else return '~';
      }
      case ':': {
        next(ls);
        if (check_next1(ls, ':')) return TK_DBCOLON;
        else return ':';
      }
      case '"': case '\'': {  /* short literal strings */
        read_string(ls, ls->current, seminfo);
        return TK_STRING;
      }
      case '.': {  /* '.', '..', '...', or number */
        save_and_next(ls);
        if (check_next1(ls, '.')) {
          if (check_next1(ls, '.'))
            return TK_DOTS;   /* '...' */
          else return TK_CONCAT;   /* '..' */
        }
        else if (!lisdigit(ls->current)) return '.';
        else return read_numeral(ls, seminfo);
      }
      case '0': case '1': case '2': case '3': case '4':
      case '5': case '6': case '7': case '8': case '9': {
        return read_numeral(ls, seminfo);
      }
      case EOZ: {
        return TK_EOS;
      }
      default: {
        if (lislalpha(ls->current)) {  /* identifier or reserved word? */
          TString *ts;
          do {
            save_and_next(ls);
          } while (lislalnum(ls->current));
          ts = luaX_newstring(ls, luaZ_buffer(ls->buff),
                                  luaZ_bufflen(ls->buff));
          seminfo->ts = ts;
          if (isreserved(ts))  /* reserved word? */
            return ts->extra - 1 + FIRST_RESERVED;
          else {
            return TK_NAME;
          }
        }
        else {  /* single-char tokens (+ - / ...) */
          int c = ls->current;
          next(ls);
          return c;
        }
      }
    }
  }
}


/**
 * Advances the lexer to the next token in the input stream.
 * 
 * This function updates the lexer state by consuming the next token. If a look-ahead token 
 * is already available (i.e., `ls->lookahead.token` is not `TK_EOS`), it is used as the current 
 * token and the look-ahead token is discharged by setting it to `TK_EOS`. If no look-ahead token 
 * is available, the next token is read from the input stream using the `llex` function and 
 * stored in `ls->t`.
 * 
 * Additionally, the function updates the `lastline` field of the lexer state to the current 
 * line number (`ls->linenumber`) before processing the token.
 * 
 * @param ls Pointer to the LexState structure representing the current state of the lexer.
 */
void luaX_next (LexState *ls) {
  ls->lastline = ls->linenumber;
  if (ls->lookahead.token != TK_EOS) {  /* is there a look-ahead token? */
    ls->t = ls->lookahead;  /* use this one */
    ls->lookahead.token = TK_EOS;  /* and discharge it */
  }
  else
    ls->t.token = llex(ls, &ls->t.seminfo);  /* read next token */
}


/**
 * @brief Advances the lexer to the next token and stores it as the lookahead token.
 *
 * This function is used to fetch the next token from the input stream and store it 
 * in the `lookahead` field of the `LexState` structure. It asserts that the current 
 * lookahead token is `TK_EOS` (End of Stream) before proceeding, ensuring that the 
 * lookahead token is not already set. The new token is obtained by calling the `llex` 
 * function, which performs lexical analysis. The function then returns the newly 
 * fetched token.
 *
 * @param ls Pointer to the `LexState` structure containing the lexer state.
 * @return The token that was fetched and stored as the lookahead token.
 */
int luaX_lookahead (LexState *ls) {
  lua_assert(ls->lookahead.token == TK_EOS);
  ls->lookahead.token = llex(ls, &ls->lookahead.seminfo);
  return ls->lookahead.token;
}

