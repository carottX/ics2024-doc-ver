/*
** $Id: liolib.c,v 2.151 2016/12/20 18:37:00 roberto Exp $
** Standard I/O (and system) library
** See Copyright Notice in lua.h
*/

#define liolib_c
#define LUA_LIB

#include "lprefix.h"


#include <ctype.h>
#include <errno.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lua.h"

#include "lauxlib.h"
#include "lualib.h"




/*
** Change this macro to accept other modes for 'fopen' besides
** the standard ones.
*/
#if !defined(l_checkmode)

/* accepted extensions to 'mode' in 'fopen' */
#if !defined(L_MODEEXT)
#define L_MODEEXT	"b"
#endif

/* Check whether 'mode' matches '[rwa]%+?[L_MODEEXT]*' */
static int l_checkmode (const char *mode) {
  return (*mode != '\0' && strchr("rwa", *(mode++)) != NULL &&
         (*mode != '+' || (++mode, 1)) &&  /* skip if char is '+' */
         (strspn(mode, L_MODEEXT) == strlen(mode)));  /* check extensions */
}

#endif

/*
** {======================================================
** l_popen spawns a new process connected to the current
** one through the file streams.
** =======================================================
*/

#if !defined(l_popen)		/* { */

#if defined(LUA_USE_POSIX)	/* { */

#define l_popen(L,c,m)		(fflush(NULL), popen(c,m))
#define l_pclose(L,file)	(pclose(file))

#elif defined(LUA_USE_WINDOWS)	/* }{ */

#define l_popen(L,c,m)		(_popen(c,m))
#define l_pclose(L,file)	(_pclose(file))

#else				/* }{ */

/* ISO C definitions */
#define l_popen(L,c,m)  \
	  ((void)((void)c, m), \
	  luaL_error(L, "'popen' not supported"), \
	  (FILE*)0)
#define l_pclose(L,file)		((void)L, (void)file, -1)

#endif				/* } */

#endif				/* } */

/* }====================================================== */


#if !defined(l_getc)		/* { */

#if defined(LUA_USE_POSIX)
#define l_getc(f)		getc_unlocked(f)
#define l_lockfile(f)		flockfile(f)
#define l_unlockfile(f)		funlockfile(f)
#else
#define l_getc(f)		getc(f)
#define l_lockfile(f)		((void)0)
#define l_unlockfile(f)		((void)0)
#endif

#endif				/* } */


/*
** {======================================================
** l_fseek: configuration for longer offsets
** =======================================================
*/

#if !defined(l_fseek)		/* { */

#if defined(LUA_USE_POSIX)	/* { */

#include <sys/types.h>

#define l_fseek(f,o,w)		fseeko(f,o,w)
#define l_ftell(f)		ftello(f)
#define l_seeknum		off_t

#elif defined(LUA_USE_WINDOWS) && !defined(_CRTIMP_TYPEINFO) \
   && defined(_MSC_VER) && (_MSC_VER >= 1400)	/* }{ */

/* Windows (but not DDK) and Visual C++ 2005 or higher */
#define l_fseek(f,o,w)		_fseeki64(f,o,w)
#define l_ftell(f)		_ftelli64(f)
#define l_seeknum		__int64

#else				/* }{ */

/* ISO C definitions */
#define l_fseek(f,o,w)		fseek(f,o,w)
#define l_ftell(f)		ftell(f)
#define l_seeknum		long

#endif				/* } */

#endif				/* } */

/* }====================================================== */


#define IO_PREFIX	"_IO_"
#define IOPREF_LEN	(sizeof(IO_PREFIX)/sizeof(char) - 1)
#define IO_INPUT	(IO_PREFIX "input")
#define IO_OUTPUT	(IO_PREFIX "output")


typedef luaL_Stream LStream;


#define tolstream(L)	((LStream *)luaL_checkudata(L, 1, LUA_FILEHANDLE))

#define isclosed(p)	((p)->closef == NULL)


/**
 * Determines the type of the Lua I/O object at the given stack index.
 * 
 * This function checks if the object at index 1 in the Lua stack is a file handle.
 * If the object is not a file handle, it pushes `nil` onto the stack. If the object
 * is a file handle, it checks whether the file is closed. If the file is closed, it
 * pushes the string "closed file" onto the stack; otherwise, it pushes the string
 * "file" onto the stack.
 * 
 * @param L The Lua state.
 * @return Always returns 1, indicating that one value has been pushed onto the stack.
 */
static int io_type (lua_State *L) {
  LStream *p;
  luaL_checkany(L, 1);
  p = (LStream *)luaL_testudata(L, 1, LUA_FILEHANDLE);
  if (p == NULL)
    lua_pushnil(L);  /* not a file */
  else if (isclosed(p))
    lua_pushliteral(L, "closed file");
  else
    lua_pushliteral(L, "file");
  return 1;
}


/**
 * Converts a Lua stream object to a string representation.
 *
 * This function is typically used to provide a human-readable string representation
 * of a Lua stream object (`LStream`). If the stream is closed, the string "file (closed)"
 * is pushed onto the Lua stack. If the stream is open, a string containing the file pointer
 * address in the format "file (%p)" is pushed, where `%p` is replaced with the pointer
 * address of the underlying file (`p->f`).
 *
 * @param L The Lua state.
 * @return Always returns 1, indicating that one value (the string representation) is pushed
 *         onto the Lua stack.
 */
static int f_tostring (lua_State *L) {
  LStream *p = tolstream(L);
  if (isclosed(p))
    lua_pushliteral(L, "file (closed)");
  else
    lua_pushfstring(L, "file (%p)", p->f);
  return 1;
}


/**
 * Converts a Lua stream object to a FILE pointer.
 * 
 * This function retrieves the LStream object associated with the Lua state `L`
 * and checks if the stream is closed. If the stream is closed, it raises a Lua
 * error with the message "attempt to use a closed file". If the stream is valid,
 * it asserts that the FILE pointer `p->f` is not NULL and returns it.
 * 
 * @param L The Lua state from which the stream object is retrieved.
 * @return The FILE pointer associated with the Lua stream object.
 * @throws Lua error if the stream is closed.
 */
static FILE *tofile (lua_State *L) {
  LStream *p = tolstream(L);
  if (isclosed(p))
    luaL_error(L, "attempt to use a closed file");
  lua_assert(p->f);
  return p->f;
}


/*
** When creating file handles, always creates a 'closed' file handle
** before opening the actual file; so, if there is a memory error, the
** handle is in a consistent state.
*/
static LStream *newprefile (lua_State *L) {
  LStream *p = (LStream *)lua_newuserdata(L, sizeof(LStream));
  p->closef = NULL;  /* mark file handle as 'closed' */
  luaL_setmetatable(L, LUA_FILEHANDLE);
  return p;
}


/*
** Calls the 'close' function from a file handle. The 'volatile' avoids
** a bug in some versions of the Clang compiler (e.g., clang 3.0 for
** 32 bits).
*/
static int aux_close (lua_State *L) {
  LStream *p = tolstream(L);
  volatile lua_CFunction cf = p->closef;
  p->closef = NULL;  /* mark stream as closed */
  return (*cf)(L);  /* close it */
}


/**
 * Closes the file stream associated with the Lua state.
 * 
 * This function is used to close a file stream that is either passed as an argument
 * or retrieved from the Lua registry if no argument is provided. If no argument is
 * given, the function defaults to closing the standard output stream. The function
 * ensures that the provided argument (or the default stream) is a valid open file
 * stream before attempting to close it.
 * 
 * @param L The Lua state from which the file stream is accessed or retrieved.
 * 
 * @return The result of the `aux_close` function, which typically returns 0 on success
 *         or an error code if the stream could not be closed.
 */
static int io_close (lua_State *L) {
  if (lua_isnone(L, 1))  /* no argument? */
    lua_getfield(L, LUA_REGISTRYINDEX, IO_OUTPUT);  /* use standard output */
  tofile(L);  /* make sure argument is an open stream */
  return aux_close(L);
}


/**
 * Closes the file associated with the Lua stream if it is not already closed.
 * This function is typically used as a garbage collection finalizer for Lua
 * streams. It ensures that the file is properly closed when the stream object
 * is collected by the garbage collector.
 *
 * @param L The Lua state.
 * @return Always returns 0, indicating no values are pushed onto the Lua stack.
 */
static int f_gc (lua_State *L) {
  LStream *p = tolstream(L);
  if (!isclosed(p) && p->f != NULL)
    aux_close(L);  /* ignore closed and incompletely open files */
  return 0;
}


/*
** function to close regular files
*/
static int io_fclose (lua_State *L) {
  LStream *p = tolstream(L);
  int res = fclose(p->f);
  return luaL_fileresult(L, (res == 0), NULL);
}


/**
 * Creates and initializes a new LStream object for file operations.
 * This function allocates a new LStream object using `newprefile`, initializes
 * its file pointer (`f`) to NULL, and sets its close function (`closef`) to
 * `io_fclose`. The resulting LStream object is returned and can be used for
 * subsequent file operations.
 *
 * @param L The Lua state in which the LStream object is created.
 * @return A pointer to the newly created and initialized LStream object.
 */
static LStream *newfile (lua_State *L) {
  LStream *p = newprefile(L);
  p->f = NULL;
  p->closef = &io_fclose;
  return p;
}


/**
 * Opens a file with the specified name and mode, and associates it with a Lua stream.
 * 
 * This function creates a new Lua stream object using `newfile(L)`, then attempts to open
 * the file specified by `fname` with the given `mode` using `fopen`. If the file cannot be
 * opened (e.g., due to permissions or the file not existing), the function raises a Lua
 * error with a descriptive message indicating the failure and the reason (using `strerror(errno)`).
 * 
 * @param L The Lua state.
 * @param fname The name of the file to open.
 * @param mode The mode in which to open the file (e.g., "r" for reading, "w" for writing).
 * 
 * @throws Raises a Lua error if the file cannot be opened.
 */
static void opencheck (lua_State *L, const char *fname, const char *mode) {
  LStream *p = newfile(L);
  p->f = fopen(fname, mode);
  if (p->f == NULL)
    luaL_error(L, "cannot open file '%s' (%s)", fname, strerror(errno));
}


/**
 * Opens a file and associates it with a Lua stream.
 *
 * This function is designed to be called from Lua. It takes a filename and an optional mode string
 * as arguments. The filename specifies the file to be opened, and the mode string specifies the
 * mode in which the file should be opened (e.g., "r" for reading, "w" for writing). If the mode
 * is not provided, it defaults to "r" (read mode).
 *
 * The function creates a new Lua stream object and attempts to open the specified file in the
 * given mode. If the file cannot be opened, the function returns an error message to Lua.
 * If the file is successfully opened, the function returns 1 to indicate success.
 *
 * @param L The Lua state.
 * @return Returns 1 if the file is successfully opened, otherwise returns an error message to Lua.
 */
static int io_open (lua_State *L) {
  const char *filename = luaL_checkstring(L, 1);
  const char *mode = luaL_optstring(L, 2, "r");
  LStream *p = newfile(L);
  const char *md = mode;  /* to traverse/check mode */
  luaL_argcheck(L, l_checkmode(md), 2, "invalid mode");
  p->f = fopen(filename, mode);
  return (p->f == NULL) ? luaL_fileresult(L, 0, filename) : 1;
}


/*
** function to close 'popen' files
*/
static int io_pclose (lua_State *L) {
  LStream *p = tolstream(L);
  return luaL_execresult(L, l_pclose(L, p->f));
}


/**
 * Opens a file or process for reading or writing using the `popen` function.
 * 
 * This function takes a filename and an optional mode string as arguments from the Lua stack.
 * It creates a new file stream object, associates it with the opened file or process,
 * and returns the result to the Lua caller. If the file or process cannot be opened,
 * an error is raised using `luaL_fileresult`.
 *
 * @param L The Lua state.
 * @return Returns 1 if the file or process was successfully opened, or raises an error otherwise.
 *
 * @details
 * - The first argument `filename` is the name of the file or command to execute.
 * - The second argument `mode` is optional and specifies the mode ("r" for reading, "w" for writing).
 *   If not provided, it defaults to "r".
 * - A new file stream object (`LStream`) is created using `newprefile`.
 * - The `popen` function is called to open the file or process, and the result is stored in the stream object.
 * - The `closef` function pointer is set to `io_pclose` to ensure proper cleanup.
 * - If the file or process cannot be opened, `luaL_fileresult` is called to raise an error.
 */
static int io_popen (lua_State *L) {
  const char *filename = luaL_checkstring(L, 1);
  const char *mode = luaL_optstring(L, 2, "r");
  LStream *p = newprefile(L);
  p->f = l_popen(L, filename, mode);
  p->closef = &io_pclose;
  return (p->f == NULL) ? luaL_fileresult(L, 0, filename) : 1;
}


/**
 * Creates a temporary file and associates it with a new file stream.
 * 
 * This function creates a temporary file using the `tmpfile()` function from the standard library,
 * which automatically opens the file in binary read/write mode (w+b). The file is automatically
 * deleted when closed or when the program terminates. The function then associates this file with
 * a new `LStream` object, which is pushed onto the Lua stack.
 *
 * @param L A pointer to the Lua state.
 * @return Returns 1 if the temporary file was successfully created and associated with the stream.
 *         If the file creation fails, the function calls `luaL_fileresult` to push an error message
 *         onto the Lua stack and returns the result of that function (typically 0 or an error code).
 */
static int io_tmpfile (lua_State *L) {
  LStream *p = newfile(L);
  p->f = tmpfile();
  return (p->f == NULL) ? luaL_fileresult(L, 0, NULL) : 1;
}


/**
 * Retrieves a standard I/O file stream associated with the given index from the Lua registry.
 * 
 * This function looks up a file stream in the Lua registry using the provided index (`findex`).
 * The file stream is expected to be stored as user data in the registry. If the file stream is
 * found but is closed, the function raises a Lua error indicating that the standard file is closed.
 * 
 * @param L The Lua state from which to retrieve the file stream.
 * @param findex The index used to look up the file stream in the Lua registry.
 * 
 * @return A pointer to the FILE stream associated with the given index. If the file stream is
 *         closed, the function raises a Lua error and does not return.
 * 
 * @note The `findex` parameter is expected to point to a string in the registry that represents
 *       the file stream. The function assumes that the file stream is stored as user data and
 *       that the `IOPREF_LEN` constant is used to skip a prefix in the `findex` string when
 *       constructing the error message.
 */
static FILE *getiofile (lua_State *L, const char *findex) {
  LStream *p;
  lua_getfield(L, LUA_REGISTRYINDEX, findex);
  p = (LStream *)lua_touserdata(L, -1);
  if (isclosed(p))
    luaL_error(L, "standard %s file is closed", findex + IOPREF_LEN);
  return p->f;
}


/**
 * Handles file operations in a Lua environment by either opening a new file or 
 * validating an existing file handle, and then storing or retrieving the file 
 * handle in the Lua registry.
 *
 * @param L The Lua state.
 * @param f The key used to store or retrieve the file handle in the Lua registry.
 * @param mode The mode in which to open the file (e.g., "r", "w", "a").
 * @return Returns 1, pushing the current file handle onto the Lua stack.
 *
 * If the first argument on the Lua stack is not `nil` or `none`, it is treated 
 * as a filename or an existing file handle:
 * - If it is a string, the file is opened with the specified mode and the handle 
 *   is stored in the Lua registry under the key `f`.
 * - If it is not a string, it is assumed to be a valid file handle, which is 
 *   then stored in the Lua registry under the key `f`.
 * 
 * The function then retrieves the file handle from the Lua registry using the 
 * key `f` and pushes it onto the Lua stack.
 */
static int g_iofile (lua_State *L, const char *f, const char *mode) {
  if (!lua_isnoneornil(L, 1)) {
    const char *filename = lua_tostring(L, 1);
    if (filename)
      opencheck(L, filename, mode);
    else {
      tofile(L);  /* check that it's a valid file handle */
      lua_pushvalue(L, 1);
    }
    lua_setfield(L, LUA_REGISTRYINDEX, f);
  }
  /* return current value */
  lua_getfield(L, LUA_REGISTRYINDEX, f);
  return 1;
}


/**
 * Reads input from the standard input stream or a specified file.
 *
 * This function is a Lua wrapper that delegates to the `g_iofile` function to handle
 * input operations. It opens the standard input stream or a specified file in read mode ("r")
 * and returns the corresponding file handle or data to the Lua state.
 *
 * @param L The Lua state, which is used to interact with the Lua runtime and pass arguments.
 * @return The number of results returned to Lua, typically 1 (the file handle or input data).
 *         In case of an error, the function may return an error message or code.
 */
static int io_input(lua_State *L) {
  return g_iofile(L, IO_INPUT, "r");
}


/**
 * @brief Sets the default output file for Lua's I/O operations.
 *
 * This function is a Lua C API wrapper that sets the default output file for Lua's I/O operations.
 * It calls the `g_iofile` function with the specified mode ("w" for write) to open or set the output file.
 * The function is typically used to redirect Lua's `print` and other output functions to a specific file.
 *
 * @param L A pointer to the Lua state.
 * @return int Returns 1 on success, pushing the file handle onto the Lua stack, or 0 on failure.
 */
static int io_output (lua_State *L) {
  return g_iofile(L, IO_OUTPUT, "w");
}


static int io_readline (lua_State *L);


/*
** maximum number of arguments to 'f:lines'/'io.lines' (it + 3 must fit
** in the limit for upvalues of a closure)
*/
#define MAXARGLINE	250

/**
 * Prepares the Lua state for reading lines from a file with optional closure.
 *
 * This function calculates the number of arguments passed to the Lua function,
 * validates that the number of arguments does not exceed the maximum allowed (`MAXARGLINE`),
 * and then prepares the Lua stack for reading lines from a file. It pushes the number of
 * arguments and a boolean indicating whether the file should be closed after reading.
 * The function then rearranges the stack to position these values correctly and creates
 * a closure using `io_readline` with the appropriate number of upvalues.
 *
 * @param L The Lua state.
 * @param toclose A boolean flag indicating whether to close the file after reading (1 for true, 0 for false).
 */
static void aux_lines (lua_State *L, int toclose) {
  int n = lua_gettop(L) - 1;  /* number of arguments to read */
  luaL_argcheck(L, n <= MAXARGLINE, MAXARGLINE + 2, "too many arguments");
  lua_pushinteger(L, n);  /* number of arguments to read */
  lua_pushboolean(L, toclose);  /* close/not close file when finished */
  lua_rotate(L, 2, 2);  /* move 'n' and 'toclose' to their positions */
  lua_pushcclosure(L, io_readline, 3 + n);
}


/**
 * @brief Processes lines from a file handle and pushes the result onto the Lua stack.
 *
 * This function first validates the file handle provided in the Lua state `L` by calling `tofile(L)`.
 * If the file handle is valid, it processes the lines from the file using the `aux_lines` function,
 * starting from the beginning of the file (offset 0). The result of processing the lines is pushed
 * onto the Lua stack, and the function returns 1 to indicate that one value has been returned to Lua.
 *
 * @param L The Lua state containing the file handle to process.
 * @return int Always returns 1, indicating that one value has been pushed onto the Lua stack.
 */
static int f_lines (lua_State *L) {
  tofile(L);  /* check that it's a valid file handle */
  aux_lines(L, 0);
  return 1;
}


/**
 * Reads lines from a file specified by the argument or the default input.
 *
 * This function is designed to read lines from a file. If no file is specified (i.e., the first argument is `nil` or not provided), 
 * it defaults to reading from the standard input file handle stored in the Lua registry under the key `IO_INPUT`. If a file name is 
 * provided as the first argument, the function opens the file in read mode and reads lines from it. 
 *
 * The function ensures that the file handle is valid before proceeding. If the file was opened by this function (i.e., a file name 
 * was provided), it will be closed after reading is complete. If the default input is used, the file handle will not be closed.
 *
 * @param L The Lua state.
 * @return Returns 1, pushing an iterator function onto the stack that can be used to read lines from the file.
 */
static int io_lines (lua_State *L) {
  int toclose;
  if (lua_isnone(L, 1)) lua_pushnil(L);  /* at least one argument */
  if (lua_isnil(L, 1)) {  /* no file name? */
    lua_getfield(L, LUA_REGISTRYINDEX, IO_INPUT);  /* get default input */
    lua_replace(L, 1);  /* put it at index 1 */
    tofile(L);  /* check that it's a valid file handle */
    toclose = 0;  /* do not close it after iteration */
  }
  else {  /* open a new file */
    const char *filename = luaL_checkstring(L, 1);
    opencheck(L, filename, "r");
    lua_replace(L, 1);  /* put file at index 1 */
    toclose = 1;  /* close it after iteration */
  }
  aux_lines(L, toclose);
  return 1;
}


/*
** {======================================================
** READ
** =======================================================
*/


/* maximum length of a numeral */
#if !defined (L_MAXLENNUM)
#define L_MAXLENNUM     200
#endif


/* auxiliary structure used by 'read_number' */
typedef struct {
  FILE *f;  /* file being read */
  int c;  /* current character (look ahead) */
  int n;  /* number of elements in buffer 'buff' */
  char buff[L_MAXLENNUM + 1];  /* +1 for ending '\0' */
} RN;


/*
** Add current char to buffer (if not out of space) and read next one
*/
static int nextc (RN *rn) {
  if (rn->n >= L_MAXLENNUM) {  /* buffer overflow? */
    rn->buff[0] = '\0';  /* invalidate result */
    return 0;  /* fail */
  }
  else {
    rn->buff[rn->n++] = rn->c;  /* save current char */
    rn->c = l_getc(rn->f);  /* read next one */
    return 1;
  }
}


/*
** Accept current char if it is in 'set' (of size 2)
*/
static int test2 (RN *rn, const char *set) {
  if (rn->c == set[0] || rn->c == set[1])
    return nextc(rn);
  else return 0;
}


/*
** Read a sequence of (hex)digits
*/
static int readdigits (RN *rn, int hex) {
  int count = 0;
  while ((hex ? isxdigit(rn->c) : isdigit(rn->c)) && nextc(rn))
    count++;
  return count;
}


/*
** Read a number: first reads a valid prefix of a numeral into a buffer.
** Then it calls 'lua_stringtonumber' to check whether the format is
** correct and to convert it to a Lua number
*/
static int read_number (lua_State *L, FILE *f) {
  RN rn;
  int count = 0;
  int hex = 0;
  char decp[2];
  rn.f = f; rn.n = 0;
  decp[0] = lua_getlocaledecpoint();  /* get decimal point from locale */
  decp[1] = '.';  /* always accept a dot */
  l_lockfile(rn.f);
  do { rn.c = l_getc(rn.f); } while (isspace(rn.c));  /* skip spaces */
  test2(&rn, "-+");  /* optional signal */
  if (test2(&rn, "00")) {
    if (test2(&rn, "xX")) hex = 1;  /* numeral is hexadecimal */
    else count = 1;  /* count initial '0' as a valid digit */
  }
  count += readdigits(&rn, hex);  /* integral part */
  if (test2(&rn, decp))  /* decimal point? */
    count += readdigits(&rn, hex);  /* fractional part */
  if (count > 0 && test2(&rn, (hex ? "pP" : "eE"))) {  /* exponent mark? */
    test2(&rn, "-+");  /* exponent signal */
    readdigits(&rn, 0);  /* exponent digits */
  }
  ungetc(rn.c, rn.f);  /* unread look-ahead char */
  l_unlockfile(rn.f);
  rn.buff[rn.n] = '\0';  /* finish string */
  if (lua_stringtonumber(L, rn.buff))  /* is this a valid number? */
    return 1;  /* ok */
  else {  /* invalid format */
   lua_pushnil(L);  /* "result" to be removed */
   return 0;  /* read fails */
  }
}


/**
 * Checks if the given file stream has reached the end-of-file (EOF).
 * 
 * This function reads a single character from the provided file stream `f` using `getc()`.
 * The character is then pushed back into the stream using `ungetc()`, which is a no-op if the
 * character is EOF. The function then pushes an empty string onto the Lua stack using
 * `lua_pushliteral()`. Finally, it returns a boolean value indicating whether the file stream
 * has not reached EOF (i.e., `1` if the character is not EOF, `0` if it is EOF).
 *
 * @param L The Lua state.
 * @param f The file stream to check for EOF.
 * @return Returns `1` if the file stream has not reached EOF, `0` otherwise.
 */
static int test_eof (lua_State *L, FILE *f) {
  int c = getc(f);
  ungetc(c, f);  /* no-op when c == EOF */
  lua_pushliteral(L, "");
  return (c != EOF);
}


/**
 * Reads a line from the specified file and pushes it onto the Lua stack.
 *
 * This function reads characters from the file `f` until it encounters a newline
 * (`'\n'`) or the end of the file (EOF). The read line is stored in a Lua buffer
 * and pushed onto the Lua stack as a string. The `chop` parameter determines
 * whether the newline character should be included in the result.
 *
 * @param L The Lua state.
 * @param f The file to read from.
 * @param chop If non-zero, the newline character is excluded from the result.
 *             If zero, the newline character is included.
 * @return Returns 1 if a line was successfully read (either a newline or some
 *         other characters), otherwise returns 0.
 */
static int read_line (lua_State *L, FILE *f, int chop) {
  luaL_Buffer b;
  int c = '\0';
  luaL_buffinit(L, &b);
  while (c != EOF && c != '\n') {  /* repeat until end of line */
    char *buff = luaL_prepbuffer(&b);  /* preallocate buffer */
    int i = 0;
    l_lockfile(f);  /* no memory errors can happen inside the lock */
    while (i < LUAL_BUFFERSIZE && (c = l_getc(f)) != EOF && c != '\n')
      buff[i++] = c;
    l_unlockfile(f);
    luaL_addsize(&b, i);
  }
  if (!chop && c == '\n')  /* want a newline and have one? */
    luaL_addchar(&b, c);  /* add ending newline to result */
  luaL_pushresult(&b);  /* close buffer */
  /* return ok if read something (either a newline or something else) */
  return (c == '\n' || lua_rawlen(L, -1) > 0);
}


/**
 * Reads the entire contents of a file into a Lua string.
 *
 * This function reads the contents of the file `f` in chunks of `LUAL_BUFFERSIZE` bytes
 * and appends them to a Lua buffer. Once the entire file has been read, the buffer is
 * converted into a Lua string and pushed onto the Lua stack.
 *
 * @param L The Lua state in which the resulting string will be pushed.
 * @param f The file pointer from which to read the data. The file should be opened
 *          in binary mode for consistent behavior across platforms.
 *
 * @note This function does not handle errors that may occur during file reading.
 *       It is the caller's responsibility to ensure that the file is valid and
 *       accessible.
 */
static void read_all (lua_State *L, FILE *f) {
  size_t nr;
  luaL_Buffer b;
  luaL_buffinit(L, &b);
  do {  /* read file in chunks of LUAL_BUFFERSIZE bytes */
    char *p = luaL_prepbuffer(&b);
    nr = fread(p, sizeof(char), LUAL_BUFFERSIZE, f);
    luaL_addsize(&b, nr);
  } while (nr == LUAL_BUFFERSIZE);
  luaL_pushresult(&b);  /* close buffer */
}


/**
 * Reads a specified number of characters from a file and pushes them onto the Lua stack.
 *
 * This function initializes a Lua buffer, prepares it to hold up to `n` characters, and then
 * attempts to read `n` characters from the given file `f`. The characters read are added to
 * the buffer, which is then pushed onto the Lua stack as a string. The function returns a
 * boolean indicating whether any characters were successfully read.
 *
 * @param L The Lua state.
 * @param f The file to read from.
 * @param n The number of characters to read.
 * @return Returns 1 (true) if any characters were read, otherwise 0 (false).
 */
static int read_chars (lua_State *L, FILE *f, size_t n) {
  size_t nr;  /* number of chars actually read */
  char *p;
  luaL_Buffer b;
  luaL_buffinit(L, &b);
  p = luaL_prepbuffsize(&b, n);  /* prepare buffer to read whole block */
  nr = fread(p, sizeof(char), n, f);  /* try to read 'n' chars */
  luaL_addsize(&b, nr);
  luaL_pushresult(&b);  /* close buffer */
  return (nr > 0);  /* true iff read something */
}


/**
 * Reads data from a file based on the provided arguments and pushes the results onto the Lua stack.
 *
 * This function reads data from the file `f` according to the format specified by the arguments on the Lua stack.
 * The function supports reading lines, characters, numbers, or the entire file, depending on the arguments.
 * The results are pushed onto the Lua stack, and the number of results is returned.
 *
 * @param L The Lua state.
 * @param f The file to read from.
 * @param first The index of the first argument on the Lua stack.
 *
 * @return The number of results pushed onto the Lua stack.
 *
 * The function handles the following cases:
 * - If no arguments are provided, it reads a line from the file.
 * - If arguments are provided, it interprets them as follows:
 *   - A number specifies the number of characters to read.
 *   - A string specifies the format for reading:
 *     - 'n' reads a number.
 *     - 'l' reads a line (without the end-of-line character).
 *     - 'L' reads a line (including the end-of-line character).
 *     - 'a' reads the entire file.
 *   - An optional '*' can precede the format string for compatibility.
 *
 * If an error occurs during reading, the function returns an error result.
 * If the end of the file is reached, the function pushes `nil` onto the stack.
 */
static int g_read (lua_State *L, FILE *f, int first) {
  int nargs = lua_gettop(L) - 1;
  int success;
  int n;
  clearerr(f);
  if (nargs == 0) {  /* no arguments? */
    success = read_line(L, f, 1);
    n = first+1;  /* to return 1 result */
  }
  else {  /* ensure stack space for all results and for auxlib's buffer */
    luaL_checkstack(L, nargs+LUA_MINSTACK, "too many arguments");
    success = 1;
    for (n = first; nargs-- && success; n++) {
      if (lua_type(L, n) == LUA_TNUMBER) {
        size_t l = (size_t)luaL_checkinteger(L, n);
        success = (l == 0) ? test_eof(L, f) : read_chars(L, f, l);
      }
      else {
        const char *p = luaL_checkstring(L, n);
        if (*p == '*') p++;  /* skip optional '*' (for compatibility) */
        switch (*p) {
          case 'n':  /* number */
            success = read_number(L, f);
            break;
          case 'l':  /* line */
            success = read_line(L, f, 1);
            break;
          case 'L':  /* line with end-of-line */
            success = read_line(L, f, 0);
            break;
          case 'a':  /* file */
            read_all(L, f);  /* read entire file */
            success = 1; /* always success */
            break;
          default:
            return luaL_argerror(L, n, "invalid format");
        }
      }
    }
  }
  if (ferror(f))
    return luaL_fileresult(L, 0, NULL);
  if (!success) {
    lua_pop(L, 1);  /* remove last result */
    lua_pushnil(L);  /* push nil instead */
  }
  return n - first;
}


/**
 * Reads data from the standard input file associated with the Lua state.
 *
 * This function retrieves the standard input file handle from the Lua state
 * and reads data from it using the `g_read` function. The `g_read` function
 * is called with the Lua state `L`, the standard input file handle, and a
 * count of 1, indicating that a single item should be read.
 *
 * @param L The Lua state from which to retrieve the standard input file handle.
 * @return The number of results pushed onto the Lua stack by `g_read`.
 */
static int io_read (lua_State *L) {
  return g_read(L, getiofile(L, IO_INPUT), 1);
}


/**
 * Reads data from a file associated with the Lua state.
 *
 * This function is a wrapper around `g_read` and is used to read data from a file
 * that is associated with the Lua state `L`. It retrieves the file object using
 * `tofile(L)` and passes it along with the integer `2` to `g_read`. The integer `2`
 * typically represents the starting index for reading operations.
 *
 * @param L The Lua state from which the file object is retrieved.
 * @return The result of the `g_read` function, which is typically the number of
 *         values pushed onto the Lua stack or an error code.
 */
static int f_read (lua_State *L) {
  return g_read(L, tofile(L), 2);
}


/**
 * Reads a line from a file associated with the given Lua state.
 *
 * This function is designed to be used as a Lua C function. It reads a line from the file
 * associated with the Lua state `L`. The file is expected to be stored as user data in the
 * first upvalue, and the number of arguments to be passed to the `g_read` function is stored
 * in the second upvalue. Additional arguments to `g_read` are stored in subsequent upvalues.
 *
 * The function first checks if the file is already closed. If it is, an error is raised.
 * It then ensures the Lua stack has enough space for the arguments and pushes them onto
 * the stack. The `g_read` function is called to perform the actual reading, and the number
 * of results is returned.
 *
 * If the read operation is successful (i.e., at least one value is read), the results are
 * returned to Lua. If the first result is `nil` (indicating EOF or an error), the function
 * checks for additional error information. If an error message is present, it is raised as
 * a Lua error. If the file was created by a generator and no error information is present,
 * the file is closed.
 *
 * @param L The Lua state.
 * @return The number of results returned to Lua, or 0 if EOF is reached without error.
 */
static int io_readline (lua_State *L) {
  LStream *p = (LStream *)lua_touserdata(L, lua_upvalueindex(1));
  int i;
  int n = (int)lua_tointeger(L, lua_upvalueindex(2));
  if (isclosed(p))  /* file is already closed? */
    return luaL_error(L, "file is already closed");
  lua_settop(L , 1);
  luaL_checkstack(L, n, "too many arguments");
  for (i = 1; i <= n; i++)  /* push arguments to 'g_read' */
    lua_pushvalue(L, lua_upvalueindex(3 + i));
  n = g_read(L, p->f, 2);  /* 'n' is number of results */
  lua_assert(n > 0);  /* should return at least a nil */
  if (lua_toboolean(L, -n))  /* read at least one value? */
    return n;  /* return them */
  else {  /* first result is nil: EOF or error */
    if (n > 1) {  /* is there error information? */
      /* 2nd result is error message */
      return luaL_error(L, "%s", lua_tostring(L, -n + 1));
    }
    if (lua_toboolean(L, lua_upvalueindex(3))) {  /* generator created file? */
      lua_settop(L, 0);
      lua_pushvalue(L, lua_upvalueindex(1));
      aux_close(L);  /* close it */
    }
    return 0;
  }
}

/* }====================================================== */


static int g_write (lua_State *L, FILE *f, int arg) {
  int nargs = lua_gettop(L) - arg;
  int status = 1;
  for (; nargs--; arg++) {
    if (lua_type(L, arg) == LUA_TNUMBER) {
      /* optimization: could be done exactly as for strings */
      int len = lua_isinteger(L, arg)
                ? fprintf(f, LUA_INTEGER_FMT,
                             (LUAI_UACINT)lua_tointeger(L, arg))
                : fprintf(f, LUA_NUMBER_FMT,
                             (LUAI_UACNUMBER)lua_tonumber(L, arg));
      status = status && (len > 0);
    }
    else {
      size_t l;
      const char *s = luaL_checklstring(L, arg, &l);
      status = status && (fwrite(s, sizeof(char), l, f) == l);
    }
  }
  if (status) return 1;  /* file handle already on stack top */
  else return luaL_fileresult(L, status, NULL);
}


/**
 * Writes data to the standard output file associated with the Lua state.
 *
 * This function retrieves the standard output file handle from the Lua state `L`
 * using `getiofile(L, IO_OUTPUT)`, and then calls `g_write` to perform the actual
 * write operation. The data to be written is expected to be on the Lua stack.
 *
 * @param L The Lua state from which the output file handle and data are retrieved.
 * @return The number of values returned to Lua, typically the result of `g_write`.
 */
static int io_write (lua_State *L) {
  return g_write(L, getiofile(L, IO_OUTPUT), 1);
}


/**
 * Writes data to a file from the Lua stack.
 *
 * This function retrieves the file handle from the Lua stack using `tofile(L)`,
 * which is expected to be at position 1. It then pushes the file handle back to
 * the top of the Lua stack to ensure it remains accessible after the operation.
 * The actual writing is delegated to the `g_write` function, which writes the
 * data provided in the arguments starting from position 2 on the Lua stack.
 *
 * @param L The Lua state, which contains the file handle and data to be written.
 * @return The number of results returned to Lua, typically the file handle.
 */
static int f_write (lua_State *L) {
  FILE *f = tofile(L);
  lua_pushvalue(L, 1);  /* push file at the stack top (to be returned) */
  return g_write(L, f, 2);
}


/**
 * Seeks within a file based on the specified mode and offset.
 *
 * This function is designed to be called from Lua and performs a seek operation
 * on a file handle. It supports three seek modes: "set", "cur", and "end", which
 * correspond to `SEEK_SET`, `SEEK_CUR`, and `SEEK_END` respectively. The function
 * takes three arguments: the Lua state, the seek mode (defaulting to "cur"), and
 * the offset (defaulting to 0).
 *
 * @param L The Lua state containing the arguments and used to return results.
 * @return Returns 1 on success, pushing the new file position as an integer.
 *         On error, returns a Lua error message.
 *
 * The function first retrieves the file handle from the Lua state. It then checks
 * the seek mode and offset provided by the user. The offset is validated to ensure
 * it is within the proper range for the `l_seeknum` type. The seek operation is
 * performed using `l_fseek`, and if successful, the new file position is returned
 * to Lua using `l_ftell`. If the seek operation fails, an error is returned.
 */
static int f_seek (lua_State *L) {
  static const int mode[] = {SEEK_SET, SEEK_CUR, SEEK_END};
  static const char *const modenames[] = {"set", "cur", "end", NULL};
  FILE *f = tofile(L);
  int op = luaL_checkoption(L, 2, "cur", modenames);
  lua_Integer p3 = luaL_optinteger(L, 3, 0);
  l_seeknum offset = (l_seeknum)p3;
  luaL_argcheck(L, (lua_Integer)offset == p3, 3,
                  "not an integer in proper range");
  op = l_fseek(f, offset, mode[op]);
  if (op)
    return luaL_fileresult(L, 0, NULL);  /* error */
  else {
    lua_pushinteger(L, (lua_Integer)l_ftell(f));
    return 1;
  }
}


/**
 * Sets the buffering mode for a file stream.
 *
 * This function configures the buffering mode for the file stream associated with the given Lua state.
 * The function expects the following arguments:
 * 1. A file object (userdata) representing the file stream.
 * 2. A string specifying the buffering mode, which can be one of the following:
 *    - "no": No buffering.
 *    - "full": Full buffering.
 *    - "line": Line buffering.
 * 3. An optional integer specifying the buffer size. If not provided, the default buffer size `LUAL_BUFFERSIZE` is used.
 *
 * The function maps the provided mode string to the corresponding `setvbuf` mode (`_IONBF`, `_IOFBF`, or `_IOLBF`)
 * and calls `setvbuf` with the specified mode and buffer size. The result of the `setvbuf` call is then returned
 * as a Lua boolean indicating success or failure.
 *
 * @param L The Lua state.
 * @return Returns a boolean indicating whether the buffering mode was successfully set.
 */
static int f_setvbuf (lua_State *L) {
  static const int mode[] = {_IONBF, _IOFBF, _IOLBF};
  static const char *const modenames[] = {"no", "full", "line", NULL};
  FILE *f = tofile(L);
  int op = luaL_checkoption(L, 2, NULL, modenames);
  lua_Integer sz = luaL_optinteger(L, 3, LUAL_BUFFERSIZE);
  int res = setvbuf(f, NULL, mode[op], (size_t)sz);
  return luaL_fileresult(L, res == 0, NULL);
}



/**
 * Flushes the output file associated with the Lua state.
 *
 * This function retrieves the output file stream from the Lua state `L` using `getiofile(L, IO_OUTPUT)`,
 * and then flushes the stream using `fflush`. The result of the flush operation is passed to `luaL_fileresult`,
 * which pushes a boolean indicating success or failure onto the Lua stack, along with an optional error message.
 *
 * @param L The Lua state from which the output file stream is retrieved.
 * @return Returns the number of results pushed onto the Lua stack by `luaL_fileresult`.
 *         Typically, this is 1 (a boolean indicating success or failure) or 2 (a boolean and an error message).
 */
static int io_flush (lua_State *L) {
  return luaL_fileresult(L, fflush(getiofile(L, IO_OUTPUT)) == 0, NULL);
}


/**
 * Flushes the file associated with the Lua state and returns the result.
 *
 * This function retrieves the file handle from the Lua state using `tofile(L)`,
 * then calls `fflush` to flush any buffered data to the file. The result of the
 * `fflush` operation is passed to `luaL_fileresult`, which converts it into a
 * Lua-compatible result. If the flush operation is successful (i.e., `fflush`
 * returns 0), `luaL_fileresult` returns `true`; otherwise, it returns `false`
 * and may set an error message.
 *
 * @param L The Lua state.
 * @return The result of the flush operation as a Lua boolean, or `nil` and an
 *         error message if the operation fails.
 */
static int f_flush (lua_State *L) {
  return luaL_fileresult(L, fflush(tofile(L)) == 0, NULL);
}


/*
** functions for 'io' library
*/
static const luaL_Reg iolib[] = {
  {"close", io_close},
  {"flush", io_flush},
  {"input", io_input},
  {"lines", io_lines},
  {"open", io_open},
  {"output", io_output},
  {"popen", io_popen},
  {"read", io_read},
  {"tmpfile", io_tmpfile},
  {"type", io_type},
  {"write", io_write},
  {NULL, NULL}
};


/*
** methods for file handles
*/
static const luaL_Reg flib[] = {
  {"close", io_close},
  {"flush", f_flush},
  {"lines", f_lines},
  {"read", f_read},
  {"seek", f_seek},
  {"setvbuf", f_setvbuf},
  {"write", f_write},
  {"__gc", f_gc},
  {"__tostring", f_tostring},
  {NULL, NULL}
};


/**
 * Creates a new metatable for file handles and associates it with the Lua state.
 * This metatable is used to define the behavior of file objects in Lua. The metatable
 * is created with the name specified by `LUA_FILEHANDLE`. The `__index` field of the
 * metatable is set to point to itself, allowing file objects to inherit methods from
 * the metatable. Additionally, file-related methods defined in the `flib` table are
 * added to the metatable. After setting up the metatable, it is popped from the Lua stack.
 *
 * @param L The Lua state in which the metatable is created.
 */
static void createmeta (lua_State *L) {
  luaL_newmetatable(L, LUA_FILEHANDLE);  /* create metatable for file handles */
  lua_pushvalue(L, -1);  /* push metatable */
  lua_setfield(L, -2, "__index");  /* metatable.__index = metatable */
  luaL_setfuncs(L, flib, 0);  /* add file methods to new metatable */
  lua_pop(L, 1);  /* pop new metatable */
}


/*
** function to (not) close the standard files stdin, stdout, and stderr
*/
static int io_noclose (lua_State *L) {
  LStream *p = tolstream(L);
  p->closef = &io_noclose;  /* keep file opened */
  lua_pushnil(L);
  lua_pushliteral(L, "cannot close standard file");
  return 2;
}


/**
 * Creates a standard file object and associates it with the Lua state.
 *
 * This function creates a new LStream object, associates it with the provided
 * FILE pointer, and adds it to the Lua module. If a registry key `k` is provided,
 * the file object is also stored in the Lua registry under that key. The file
 * is marked with a no-close function, meaning it will not be automatically closed
 * when the Lua object is garbage collected.
 *
 * @param L The Lua state in which to create the file object.
 * @param f The FILE pointer to associate with the Lua file object.
 * @param k The optional registry key under which to store the file object.
 *          If NULL, the file is not added to the registry.
 * @param fname The name of the field in the Lua module where the file object
 *              will be stored.
 */
static void createstdfile (lua_State *L, FILE *f, const char *k,
                           const char *fname) {
  LStream *p = newprefile(L);
  p->f = f;
  p->closef = &io_noclose;
  if (k != NULL) {
    lua_pushvalue(L, -1);
    lua_setfield(L, LUA_REGISTRYINDEX, k);  /* add file to registry */
  }
  lua_setfield(L, -2, fname);  /* add file to module */
}


/**
 * Initializes the I/O library module for Lua.
 *
 * This function is the entry point for the I/O module in Lua. It creates a new module
 * table and registers the I/O library functions into it. Additionally, it sets up
 * the standard I/O files (stdin, stdout, and stderr) as default file handles within
 * the module.
 *
 * @param L The Lua state in which the module is being initialized.
 * @return Returns 1, indicating that the module table is pushed onto the Lua stack.
 */
LUAMOD_API int luaopen_io (lua_State *L) {
  luaL_newlib(L, iolib);  /* new module */
  createmeta(L);
  /* create (and set) default files */
  createstdfile(L, stdin, IO_INPUT, "stdin");
  createstdfile(L, stdout, IO_OUTPUT, "stdout");
  createstdfile(L, stderr, NULL, "stderr");
  return 1;
}

