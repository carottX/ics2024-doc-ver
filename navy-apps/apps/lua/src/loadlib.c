/*
** $Id: loadlib.c,v 1.130 2017/01/12 17:14:26 roberto Exp $
** Dynamic library loader for Lua
** See Copyright Notice in lua.h
**
** This module contains an implementation of loadlib for Unix systems
** that have dlfcn, an implementation for Windows, and a stub for other
** systems.
*/

#define loadlib_c
#define LUA_LIB

#include "lprefix.h"


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lua.h"

#include "lauxlib.h"
#include "lualib.h"


/*
** LUA_IGMARK is a mark to ignore all before it when building the
** luaopen_ function name.
*/
#if !defined (LUA_IGMARK)
#define LUA_IGMARK		"-"
#endif


/*
** LUA_CSUBSEP is the character that replaces dots in submodule names
** when searching for a C loader.
** LUA_LSUBSEP is the character that replaces dots in submodule names
** when searching for a Lua loader.
*/
#if !defined(LUA_CSUBSEP)
#define LUA_CSUBSEP		LUA_DIRSEP
#endif

#if !defined(LUA_LSUBSEP)
#define LUA_LSUBSEP		LUA_DIRSEP
#endif


/* prefix for open functions in C libraries */
#define LUA_POF		"luaopen_"

/* separator for open functions in C libraries */
#define LUA_OFSEP	"_"


/*
** unique key for table in the registry that keeps handles
** for all loaded C libraries
*/
static const int CLIBS = 0;

#define LIB_FAIL	"open"


#define setprogdir(L)           ((void)0)


/*
** system-dependent functions
*/

/*
** unload library 'lib'
*/
static void lsys_unloadlib (void *lib);

/*
** load C library in file 'path'. If 'seeglb', load with all names in
** the library global.
** Returns the library; in case of error, returns NULL plus an
** error string in the stack.
*/
static void *lsys_load (lua_State *L, const char *path, int seeglb);

/*
** Try to find a function named 'sym' in library 'lib'.
** Returns the function; in case of error, returns NULL plus an
** error string in the stack.
*/
static lua_CFunction lsys_sym (lua_State *L, void *lib, const char *sym);




#if defined(LUA_USE_DLOPEN)	/* { */
/*
** {========================================================================
** This is an implementation of loadlib based on the dlfcn interface.
** The dlfcn interface is available in Linux, SunOS, Solaris, IRIX, FreeBSD,
** NetBSD, AIX 4.2, HPUX 11, and  probably most other Unix flavors, at least
** as an emulation layer on top of native functions.
** =========================================================================
*/

#include <dlfcn.h>

/*
** Macro to convert pointer-to-void* to pointer-to-function. This cast
** is undefined according to ISO C, but POSIX assumes that it works.
** (The '__extension__' in gnu compilers is only to avoid warnings.)
*/
#if defined(__GNUC__)
#define cast_func(p) (__extension__ (lua_CFunction)(p))
#else
#define cast_func(p) ((lua_CFunction)(p))
#endif


/**
 * @brief Unloads a dynamically loaded library.
 *
 * This function takes a handle to a dynamically loaded library and unloads it
 * from the process's address space using the `dlclose` function. After calling
 * this function, the library is no longer accessible, and any resources
 * associated with it are freed.
 *
 * @param lib A pointer to the handle of the dynamically loaded library to be unloaded.
 *            This handle is typically obtained from a previous call to `dlopen`.
 *
 * @note It is the caller's responsibility to ensure that the library handle is
 *       valid and that the library is no longer in use before calling this function.
 *       Calling `dlclose` on an invalid handle or a handle that is still in use
 *       may result in undefined behavior.
 */
static void lsys_unloadlib(void *lib) {
    dlclose(lib);
}


/**
 * Loads a shared library specified by the given path into the process's address space.
 * The function uses `dlopen` to load the library and handles errors by pushing an error
 * message onto the Lua stack if the library cannot be loaded.
 *
 * @param L The Lua state in which the operation is performed.
 * @param path The file path of the shared library to load.
 * @param seeglb A flag indicating whether the library's symbols should be globally available.
 *               If `seeglb` is non-zero, the library is loaded with `RTLD_GLOBAL`, making its
 *               symbols available for subsequently loaded libraries. If `seeglb` is zero, the
 *               library is loaded with `RTLD_LOCAL`, limiting the visibility of its symbols
 *               to the current library.
 * @return A pointer to the loaded library handle on success, or `NULL` on failure. If the
 *         library fails to load, an error message is pushed onto the Lua stack using `lua_pushstring`.
 */
static void *lsys_load (lua_State *L, const char *path, int seeglb) {
  void *lib = dlopen(path, RTLD_NOW | (seeglb ? RTLD_GLOBAL : RTLD_LOCAL));
  if (lib == NULL) lua_pushstring(L, dlerror());
  return lib;
}


/**
 * Resolves a symbol from a dynamic library and returns it as a Lua C function.
 *
 * This function uses `dlsym` to look up the symbol `sym` in the dynamic library
 * referenced by `lib`. If the symbol is found, it is cast to a Lua C function
 * (`lua_CFunction`) and returned. If the symbol is not found, an error message
 * is pushed onto the Lua stack using `lua_pushstring`, and `NULL` is returned.
 *
 * @param L The Lua state.
 * @param lib A handle to the dynamic library from which to resolve the symbol.
 * @param sym The name of the symbol to resolve.
 * @return The resolved Lua C function, or `NULL` if the symbol could not be found.
 */
static lua_CFunction lsys_sym (lua_State *L, void *lib, const char *sym) {
  lua_CFunction f = cast_func(dlsym(lib, sym));
  if (f == NULL) lua_pushstring(L, dlerror());
  return f;
}

/* }====================================================== */



#elif defined(LUA_DL_DLL)	/* }{ */
/*
** {======================================================================
** This is an implementation of loadlib for Windows using native functions.
** =======================================================================
*/

#include <windows.h>


/*
** optional flags for LoadLibraryEx
*/
#if !defined(LUA_LLE_FLAGS)
#define LUA_LLE_FLAGS	0
#endif


#undef setprogdir


/*
** Replace in the path (on the top of the stack) any occurrence
** of LUA_EXEC_DIR with the executable's path.
*/
static void setprogdir (lua_State *L) {
  char buff[MAX_PATH + 1];
  char *lb;
  DWORD nsize = sizeof(buff)/sizeof(char);
  DWORD n = GetModuleFileNameA(NULL, buff, nsize);  /* get exec. name */
  if (n == 0 || n == nsize || (lb = strrchr(buff, '\\')) == NULL)
    luaL_error(L, "unable to get ModuleFileName");
  else {
    *lb = '\0';  /* cut name on the last '\\' to get the path */
    luaL_gsub(L, lua_tostring(L, -1), LUA_EXEC_DIR, buff);
    lua_remove(L, -2);  /* remove original string */
  }
}




/**
 * Pushes a system error message onto the Lua stack.
 *
 * This function retrieves the last system error code using `GetLastError()`, formats it into a human-readable message using `FormatMessageA`, and pushes the result as a string onto the Lua stack. If the error message cannot be formatted, it pushes a generic error message containing the error code.
 *
 * @param L A pointer to the Lua state.
 *
 * @remarks
 * - The function uses `FORMAT_MESSAGE_IGNORE_INSERTS` and `FORMAT_MESSAGE_FROM_SYSTEM` flags to ensure the message is retrieved from the system and does not include any insert sequences.
 * - The formatted message is stored in a buffer of 128 characters. If the message exceeds this length, it is truncated.
 * - If `FormatMessageA` fails, a fallback message is pushed containing the error code.
 */
static void pusherror (lua_State *L) {
  int error = GetLastError();
  char buffer[128];
  if (FormatMessageA(FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_SYSTEM,
      NULL, error, 0, buffer, sizeof(buffer)/sizeof(char), NULL))
    lua_pushstring(L, buffer);
  else
    lua_pushfstring(L, "system error %d\n", error);
}

/**
 * Unloads a dynamically loaded library from memory.
 *
 * This function is responsible for unloading a library that was previously loaded
 * using a platform-specific mechanism (e.g., `LoadLibrary` on Windows). It takes
 * a pointer to the library handle (`lib`) and passes it to the `FreeLibrary` function,
 * which releases the library from memory and frees associated resources.
 *
 * @param lib A pointer to the library handle to be unloaded. This handle is typically
 *            obtained from a platform-specific library loading function (e.g., `LoadLibrary`).
 *
 * @note The `lib` parameter must be a valid library handle. Passing an invalid or
 *       already unloaded handle may result in undefined behavior.
 * @note This function is specific to the Windows platform, as it uses the `FreeLibrary`
 *       function from the Windows API.
 */
static void lsys_unloadlib (void *lib) {
  FreeLibrary((HMODULE)lib);
}


/**
 * Loads a dynamic link library (DLL) using the specified path and returns a handle to the loaded library.
 * 
 * This function attempts to load a DLL from the given `path` using the `LoadLibraryExA` function 
 * with the flags defined by `LUA_LLE_FLAGS`. If the library fails to load, an error is pushed onto 
 * the Lua stack using the `pusherror` function. The `seeglb` parameter is ignored as symbols are 
 * considered 'global' by default in this context.
 * 
 * @param L The Lua state in which the operation is performed.
 * @param path The file path of the DLL to be loaded.
 * @param seeglb Unused parameter, retained for compatibility or future use.
 * @return A handle to the loaded library if successful, otherwise NULL.
 */
static void *lsys_load (lua_State *L, const char *path, int seeglb) {
  HMODULE lib = LoadLibraryExA(path, NULL, LUA_LLE_FLAGS);
  (void)(seeglb);  /* not used: symbols are 'global' by default */
  if (lib == NULL) pusherror(L);
  return lib;
}


/**
 * Retrieves a function pointer from a dynamic-link library (DLL) and returns it as a Lua C function.
 * This method uses the `GetProcAddress` function to look up the specified symbol (`sym`) in the given library (`lib`).
 * If the symbol is not found or an error occurs, an error is pushed onto the Lua stack using `pusherror(L)`.
 *
 * @param L       The Lua state, used to push errors if the symbol lookup fails.
 * @param lib     A handle to the DLL from which the function is to be retrieved.
 * @param sym     The name of the function to look up in the DLL.
 * @return        A pointer to the Lua C function if the symbol is found; otherwise, NULL is returned.
 */
static lua_CFunction lsys_sym (lua_State *L, void *lib, const char *sym) {
  lua_CFunction f = (lua_CFunction)GetProcAddress((HMODULE)lib, sym);
  if (f == NULL) pusherror(L);
  return f;
}

/* }====================================================== */


#else				/* }{ */
/*
** {======================================================
** Fallback for other systems
** =======================================================
*/

#undef LIB_FAIL
#define LIB_FAIL	"absent"


#define DLMSG	"dynamic libraries not enabled; check your Lua installation"


/**
 * Unloads a dynamically loaded library. This function is a placeholder that does
 * not perform any actual unloading operation. It takes a pointer to the library
 * as a parameter but does not use it, allowing the function to be called in
 * contexts where unloading is not supported or required.
 *
 * @param lib A pointer to the library to be unloaded. This parameter is unused.
 */
static void lsys_unloadlib (void *lib) {
  (void)(lib);  /* not used */
}


/**
 * @brief Loads a dynamic library message into the Lua state.
 *
 * This function is a placeholder for loading a dynamic library message into the Lua state.
 * It pushes a predefined message (DLMSG) onto the Lua stack, which can be used to indicate
 * that the dynamic library loading functionality is not implemented or available.
 *
 * @param L The Lua state in which the message is pushed.
 * @param path The path to the dynamic library (unused in this implementation).
 * @param seeglb A flag indicating whether to load the library globally (unused in this implementation).
 * @return Always returns NULL, indicating that no library was loaded.
 */
static void *lsys_load (lua_State *L, const char *path, int seeglb) {
  (void)(path); (void)(seeglb);  /* not used */
  lua_pushliteral(L, DLMSG);
  return NULL;
}


/**
 * Pushes a predefined dynamic loading error message onto the Lua stack.
 *
 * This function is a placeholder for resolving symbols in a dynamic library.
 * It does not actually attempt to resolve the symbol (`sym`) or interact with
 * the library (`lib`). Instead, it pushes a predefined error message (defined
 * by `DLMSG`) onto the Lua stack to indicate that dynamic loading functionality
 * is not supported or has failed.
 *
 * @param L The Lua state.
 * @param lib The library handle (unused).
 * @param sym The symbol name to resolve (unused).
 * @return Always returns `NULL` to indicate no function was resolved.
 */
static lua_CFunction lsys_sym (lua_State *L, void *lib, const char *sym) {
  (void)(lib); (void)(sym);  /* not used */
  lua_pushliteral(L, DLMSG);
  return NULL;
}

/* }====================================================== */
#endif				/* } */


/*
** {==================================================================
** Set Paths
** ===================================================================
*/

/*
** LUA_PATH_VAR and LUA_CPATH_VAR are the names of the environment
** variables that Lua check to set its paths.
*/
#if !defined(LUA_PATH_VAR)
#define LUA_PATH_VAR    "LUA_PATH"
#endif

#if !defined(LUA_CPATH_VAR)
#define LUA_CPATH_VAR   "LUA_CPATH"
#endif


#define AUXMARK         "\1"	/* auxiliary mark */


/*
** return registry.LUA_NOENV as a boolean
*/
static int noenv (lua_State *L) {
  int b;
  lua_getfield(L, LUA_REGISTRYINDEX, "LUA_NOENV");
  b = lua_toboolean(L, -1);
  lua_pop(L, 1);  /* remove value */
  return b;
}


/*
** Set a path
*/
static void setpath (lua_State *L, const char *fieldname,
                                   const char *envname,
                                   const char *dft) {
  const char *nver = lua_pushfstring(L, "%s%s", envname, LUA_VERSUFFIX);
  const char *path = getenv(nver);  /* use versioned name */
  if (path == NULL)  /* no environment variable? */
    path = getenv(envname);  /* try unversioned name */
  if (path == NULL || noenv(L))  /* no environment variable? */
    lua_pushstring(L, dft);  /* use default */
  else {
    /* replace ";;" by ";AUXMARK;" and then AUXMARK by default path */
    path = luaL_gsub(L, path, LUA_PATH_SEP LUA_PATH_SEP,
                              LUA_PATH_SEP AUXMARK LUA_PATH_SEP);
    luaL_gsub(L, path, AUXMARK, dft);
    lua_remove(L, -2); /* remove result from 1st 'gsub' */
  }
  setprogdir(L);
  lua_setfield(L, -3, fieldname);  /* package[fieldname] = path value */
  lua_pop(L, 1);  /* pop versioned variable name */
}

/* }================================================================== */


/*
** return registry.CLIBS[path]
*/
static void *checkclib (lua_State *L, const char *path) {
  void *plib;
  lua_rawgetp(L, LUA_REGISTRYINDEX, &CLIBS);
  lua_getfield(L, -1, path);
  plib = lua_touserdata(L, -1);  /* plib = CLIBS[path] */
  lua_pop(L, 2);  /* pop CLIBS table and 'plib' */
  return plib;
}


/*
** registry.CLIBS[path] = plib        -- for queries
** registry.CLIBS[#CLIBS + 1] = plib  -- also keep a list of all libraries
*/
static void addtoclib (lua_State *L, const char *path, void *plib) {
  lua_rawgetp(L, LUA_REGISTRYINDEX, &CLIBS);
  lua_pushlightuserdata(L, plib);
  lua_pushvalue(L, -1);
  lua_setfield(L, -3, path);  /* CLIBS[path] = plib */
  lua_rawseti(L, -2, luaL_len(L, -2) + 1);  /* CLIBS[#CLIBS + 1] = plib */
  lua_pop(L, 1);  /* pop CLIBS table */
}


/*
** __gc tag method for CLIBS table: calls 'lsys_unloadlib' for all lib
** handles in list CLIBS
*/
static int gctm (lua_State *L) {
  lua_Integer n = luaL_len(L, 1);
  for (; n >= 1; n--) {  /* for each handle, in reverse order */
    lua_rawgeti(L, 1, n);  /* get handle CLIBS[n] */
    lsys_unloadlib(lua_touserdata(L, -1));
    lua_pop(L, 1);  /* pop handle */
  }
  return 0;
}



/* error codes for 'lookforfunc' */
#define ERRLIB		1
#define ERRFUNC		2

/*
** Look for a C function named 'sym' in a dynamically loaded library
** 'path'.
** First, check whether the library is already loaded; if not, try
** to load it.
** Then, if 'sym' is '*', return true (as library has been loaded).
** Otherwise, look for symbol 'sym' in the library and push a
** C function with that symbol.
** Return 0 and 'true' or a function in the stack; in case of
** errors, return an error code and an error message in the stack.
*/
static int lookforfunc (lua_State *L, const char *path, const char *sym) {
  void *reg = checkclib(L, path);  /* check loaded C libraries */
  if (reg == NULL) {  /* must load library? */
    reg = lsys_load(L, path, *sym == '*');  /* global symbols if 'sym'=='*' */
    if (reg == NULL) return ERRLIB;  /* unable to load library */
    addtoclib(L, path, reg);
  }
  if (*sym == '*') {  /* loading only library (no function)? */
    lua_pushboolean(L, 1);  /* return 'true' */
    return 0;  /* no errors */
  }
  else {
    lua_CFunction f = lsys_sym(L, reg, sym);
    if (f == NULL)
      return ERRFUNC;  /* unable to find function */
    lua_pushcfunction(L, f);  /* else create new function */
    return 0;  /* no errors */
  }
}


/**
 * Loads a shared library and initializes a function from it.
 *
 * This function takes two arguments from the Lua stack: the path to the shared library
 * and the name of the initialization function to be loaded from the library. It attempts
 * to load the library and locate the specified function using `lookforfunc`. If successful,
 * the function is pushed onto the Lua stack and returned. If an error occurs, the function
 * returns three values: `nil`, an error message, and a string indicating where the error
 * occurred ("init" or "LIB_FAIL").
 *
 * @param L The Lua state.
 * @return If successful, returns 1 (the loaded function). On error, returns 3 values:
 *         `nil`, an error message, and a string indicating the error location.
 */
static int ll_loadlib (lua_State *L) {
  const char *path = luaL_checkstring(L, 1);
  const char *init = luaL_checkstring(L, 2);
  int stat = lookforfunc(L, path, init);
  if (stat == 0)  /* no errors? */
    return 1;  /* return the loaded function */
  else {  /* error; error message is on stack top */
    lua_pushnil(L);
    lua_insert(L, -2);
    lua_pushstring(L, (stat == ERRLIB) ?  LIB_FAIL : "init");
    return 3;  /* return nil, error message, and where */
  }
}



/*
** {======================================================
** 'require' function
** =======================================================
*/


static int readable (const char *filename) {
  FILE *f = fopen(filename, "r");  /* try to open file */
  if (f == NULL) return 0;  /* open failed */
  fclose(f);
  return 1;
}


/**
 * Extracts the next template from a given path and pushes it onto the Lua stack.
 *
 * This function processes a path string that contains one or more templates separated by `LUA_PATH_SEP`.
 * It skips any leading separators and then identifies the next template by searching for the next separator.
 * The identified template (a substring of the path) is pushed onto the Lua stack as a string.
 * If the path is empty or no more templates are found, the function returns `NULL`.
 *
 * @param L The Lua state.
 * @param path The path string containing templates separated by `LUA_PATH_SEP`.
 * @return A pointer to the remaining part of the path after the extracted template, or `NULL` if no more templates are found.
 */
static const char *pushnexttemplate (lua_State *L, const char *path) {
  const char *l;
  while (*path == *LUA_PATH_SEP) path++;  /* skip separators */
  if (*path == '\0') return NULL;  /* no more templates */
  l = strchr(path, *LUA_PATH_SEP);  /* find next separator */
  if (l == NULL) l = path + strlen(path);
  lua_pushlstring(L, path, l - path);  /* template */
  return l;
}


/**
 * Searches for a file in a given path using a template and returns the full path
 * of the first readable file found. The method replaces separators in the file
 * name with directory separators and iterates through the provided path templates
 * to construct potential file paths. If a readable file is found, its path is returned.
 * If no file is found, an error message is constructed and NULL is returned.
 *
 * @param L The Lua state.
 * @param name The name of the file to search for.
 * @param path The path template(s) to search in, separated by a specific character.
 * @param sep The separator used in the file name (e.g., '.' in package names).
 * @param dirsep The directory separator to use in the constructed file path.
 * @return The full path of the first readable file found, or NULL if no file is found.
 */
static const char *searchpath (lua_State *L, const char *name,
                                             const char *path,
                                             const char *sep,
                                             const char *dirsep) {
  luaL_Buffer msg;  /* to build error message */
  luaL_buffinit(L, &msg);
  if (*sep != '\0')  /* non-empty separator? */
    name = luaL_gsub(L, name, sep, dirsep);  /* replace it by 'dirsep' */
  while ((path = pushnexttemplate(L, path)) != NULL) {
    const char *filename = luaL_gsub(L, lua_tostring(L, -1),
                                     LUA_PATH_MARK, name);
    lua_remove(L, -2);  /* remove path template */
    if (readable(filename))  /* does file exist and is readable? */
      return filename;  /* return that file name */
    lua_pushfstring(L, "\n\tno file '%s'", filename);
    lua_remove(L, -2);  /* remove file name */
    luaL_addvalue(&msg);  /* concatenate error msg. entry */
  }
  luaL_pushresult(&msg);  /* create error message */
  return NULL;  /* not found */
}


/**
 * Searches for a file in a given path using the provided search pattern and directory separator.
 * 
 * This function takes four arguments from the Lua stack:
 * 1. The name of the file to search for (string).
 * 2. The search path pattern (string).
 * 3. An optional default directory to search in (string, defaults to ".").
 * 4. An optional directory separator (string, defaults to LUA_DIRSEP).
 * 
 * The function uses the `searchpath` utility to locate the file based on the provided arguments.
 * If the file is found, the function returns 1, pushing the file path onto the Lua stack.
 * If the file is not found, the function returns 2, pushing `nil` followed by the error message
 * onto the Lua stack.
 * 
 * @param L The Lua state.
 * @return 1 if the file is found, 2 if the file is not found (with an error message).
 */
static int ll_searchpath (lua_State *L) {
  const char *f = searchpath(L, luaL_checkstring(L, 1),
                                luaL_checkstring(L, 2),
                                luaL_optstring(L, 3, "."),
                                luaL_optstring(L, 4, LUA_DIRSEP));
  if (f != NULL) return 1;
  else {  /* error message is on top of the stack */
    lua_pushnil(L);
    lua_insert(L, -2);
    return 2;  /* return nil + error message */
  }
}


/**
 * Searches for a file in the specified Lua package path.
 *
 * This function retrieves the package path associated with `pname` from the Lua upvalue
 * at index 1. It then uses the `searchpath` function to locate the file `name` within
 * the retrieved path. The search is performed using the specified directory separator
 * `dirsep` and the default extension ".".
 *
 * @param L The Lua state.
 * @param name The name of the file to search for.
 * @param pname The name of the package path to retrieve (e.g., "path" or "cpath").
 * @param dirsep The directory separator to use in the search (e.g., "/" or "\\").
 * @return The full path to the found file, or NULL if the file is not found.
 * @throws Lua error if the package path associated with `pname` is not a string.
 */
static const char *findfile (lua_State *L, const char *name,
                                           const char *pname,
                                           const char *dirsep) {
  const char *path;
  lua_getfield(L, lua_upvalueindex(1), pname);
  path = lua_tostring(L, -1);
  if (path == NULL)
    luaL_error(L, "'package.%s' must be a string", pname);
  return searchpath(L, name, path, ".", dirsep);
}


/**
 * Checks the status of a Lua module load operation and handles the result accordingly.
 *
 * This function is used to verify whether a Lua module was successfully loaded. If the module
 * was loaded successfully (i.e., `stat` is non-zero), it pushes the module's filename onto the
 * Lua stack and returns 2, indicating that both the module's open function and the filename
 * are available on the stack. If the module failed to load (i.e., `stat` is zero), it raises
 * a Lua error with a descriptive message, including the module name, filename, and the error
 * message from the Lua stack.
 *
 * @param L The Lua state.
 * @param stat The status of the module load operation (non-zero for success, zero for failure).
 * @param filename The filename of the module being loaded.
 * @return Returns 2 if the module was loaded successfully, otherwise raises a Lua error.
 */
static int checkload (lua_State *L, int stat, const char *filename) {
  if (stat) {  /* module loaded successfully? */
    lua_pushstring(L, filename);  /* will be 2nd argument to module */
    return 2;  /* return open function and file name */
  }
  else
    return luaL_error(L, "error loading module '%s' from file '%s':\n\t%s",
                          lua_tostring(L, 1), filename, lua_tostring(L, -1));
}


/**
 * Searches for a Lua module file in the specified path and attempts to load it.
 *
 * This function is typically used as a module searcher in Lua's `package.searchers` table.
 * It takes a module name as input, searches for the corresponding Lua file in the specified
 * path, and attempts to load the file if found. The search is performed using the `findfile`
 * function, which looks for the module in the path specified by the "path" variable.
 *
 * @param L The Lua state.
 * @return Returns 1 if the module is not found in the path. If the module is found and
 *         successfully loaded, it returns the result of `checkload`, which typically
 *         returns 1 if the load was successful, or an error message if the load failed.
 *
 * @note The function assumes that the module name is passed as the first argument on the
 *       Lua stack. The "path" variable and the `LUA_LSUBSEP` separator are used to locate
 *       the module file.
 */
static int searcher_Lua (lua_State *L) {
  const char *filename;
  const char *name = luaL_checkstring(L, 1);
  filename = findfile(L, name, "path", LUA_LSUBSEP);
  if (filename == NULL) return 1;  /* module not found in this path */
  return checkload(L, (luaL_loadfile(L, filename) == LUA_OK), filename);
}


/*
** Try to find a load function for module 'modname' at file 'filename'.
** First, change '.' to '_' in 'modname'; then, if 'modname' has
** the form X-Y (that is, it has an "ignore mark"), build a function
** name "luaopen_X" and look for it. (For compatibility, if that
** fails, it also tries "luaopen_Y".) If there is no ignore mark,
** look for a function named "luaopen_modname".
*/
static int loadfunc (lua_State *L, const char *filename, const char *modname) {
  const char *openfunc;
  const char *mark;
  modname = luaL_gsub(L, modname, ".", LUA_OFSEP);
  mark = strchr(modname, *LUA_IGMARK);
  if (mark) {
    int stat;
    openfunc = lua_pushlstring(L, modname, mark - modname);
    openfunc = lua_pushfstring(L, LUA_POF"%s", openfunc);
    stat = lookforfunc(L, filename, openfunc);
    if (stat != ERRFUNC) return stat;
    modname = mark + 1;  /* else go ahead and try old-style name */
  }
  openfunc = lua_pushfstring(L, LUA_POF"%s", modname);
  return lookforfunc(L, filename, openfunc);
}


/**
 * Searches for a C module in the specified path and attempts to load it.
 *
 * This function is used to locate and load a C module by its name. It first searches for the module
 * in the path specified by the "cpath" configuration. If the module is found, it attempts to load
 * the module using the `loadfunc` function. If the module is not found or cannot be loaded, an
 * appropriate error is returned.
 *
 * @param L The Lua state.
 * @return Returns 1 if the module is not found in the specified path. If the module is found,
 *         it returns the result of `checkload`, which indicates whether the module was successfully
 *         loaded or if an error occurred during loading.
 */
static int searcher_C (lua_State *L) {
  const char *name = luaL_checkstring(L, 1);
  const char *filename = findfile(L, name, "cpath", LUA_CSUBSEP);
  if (filename == NULL) return 1;  /* module not found in this path */
  return checkload(L, (loadfunc(L, filename, name) == 0), filename);
}


/**
 * Searches for and loads a C module root based on the given module name.
 *
 * This function is designed to handle the loading of C modules by searching for the root module
 * specified in the module name. It expects the module name to be passed as the first argument
 * on the Lua stack. The module name may contain a dot ('.') to separate the root module from
 * submodules. If the module name does not contain a dot, the function assumes it is the root
 * module and returns immediately.
 *
 * The function performs the following steps:
 * 1. Extracts the root module name by finding the first dot in the module name.
 * 2. Searches for the corresponding C file using the `findfile` function with the "cpath" and
 *    `LUA_CSUBSEP` parameters.
 * 3. If the file is found, it attempts to load the module using the `loadfunc` function.
 * 4. If the module is successfully loaded, the function pushes the filename onto the Lua stack
 *    as the second argument to the module and returns 2.
 * 5. If the module cannot be loaded, the function handles errors appropriately, returning 1
 *    with an error message or calling `checkload` for real errors.
 *
 * @param L The Lua state.
 * @return Returns 0 if the module name is the root (no dot), 1 if the root module is not found
 *         or if there is an error loading the module, and 2 if the module is successfully loaded.
 */
static int searcher_Croot (lua_State *L) {
  const char *filename;
  const char *name = luaL_checkstring(L, 1);
  const char *p = strchr(name, '.');
  int stat;
  if (p == NULL) return 0;  /* is root */
  lua_pushlstring(L, name, p - name);
  filename = findfile(L, lua_tostring(L, -1), "cpath", LUA_CSUBSEP);
  if (filename == NULL) return 1;  /* root not found */
  if ((stat = loadfunc(L, filename, name)) != 0) {
    if (stat != ERRFUNC)
      return checkload(L, 0, filename);  /* real error */
    else {  /* open function not found */
      lua_pushfstring(L, "\n\tno module '%s' in file '%s'", name, filename);
      return 1;
    }
  }
  lua_pushstring(L, filename);  /* will be 2nd argument to module */
  return 2;
}


/**
 * Searches for a module in the `package.preload` table and pushes the result onto the stack.
 * 
 * This function takes a module name as a string argument from the Lua stack and checks if it exists
 * in the `package.preload` table. If the module is found, it is pushed onto the stack. If the module
 * is not found, an error message is pushed onto the stack indicating that the module does not exist
 * in the `package.preload` table.
 *
 * @param L The Lua state.
 * @return Returns 1, indicating that one value (either the module or an error message) has been pushed onto the stack.
 */
static int searcher_preload (lua_State *L) {
  const char *name = luaL_checkstring(L, 1);
  lua_getfield(L, LUA_REGISTRYINDEX, LUA_PRELOAD_TABLE);
  if (lua_getfield(L, -1, name) == LUA_TNIL)  /* not found? */
    lua_pushfstring(L, "\n\tno field package.preload['%s']", name);
  return 1;
}


/**
 * Searches for a loader function for the specified module name by iterating over
 * the searchers listed in 'package.searchers'. The searchers are Lua functions
 * that attempt to locate and return a loader for the module. If a loader is found,
 * it is left on the stack. If no loader is found after all searchers are exhausted,
 * an error is raised with a concatenated error message from the searchers.
 *
 * @param L The Lua state.
 * @param name The name of the module to find a loader for.
 *
 * @throws Raises a Lua error if 'package.searchers' is not a table or if no loader
 *         is found for the module.
 */
static void findloader (lua_State *L, const char *name) {
  int i;
  luaL_Buffer msg;  /* to build error message */
  luaL_buffinit(L, &msg);
  /* push 'package.searchers' to index 3 in the stack */
  if (lua_getfield(L, lua_upvalueindex(1), "searchers") != LUA_TTABLE)
    luaL_error(L, "'package.searchers' must be a table");
  /*  iterate over available searchers to find a loader */
  for (i = 1; ; i++) {
    if (lua_rawgeti(L, 3, i) == LUA_TNIL) {  /* no more searchers? */
      lua_pop(L, 1);  /* remove nil */
      luaL_pushresult(&msg);  /* create error message */
      luaL_error(L, "module '%s' not found:%s", name, lua_tostring(L, -1));
    }
    lua_pushstring(L, name);
    lua_call(L, 1, 2);  /* call it */
    if (lua_isfunction(L, -2))  /* did it find a loader? */
      return;  /* module loader found */
    else if (lua_isstring(L, -2)) {  /* searcher returned error message? */
      lua_pop(L, 1);  /* remove extra return */
      luaL_addvalue(&msg);  /* concatenate error message */
    }
    else
      lua_pop(L, 2);  /* remove both returns */
  }
}


/**
 * Loads a Lua module by name if it is not already loaded.
 *
 * This function checks if the module specified by `name` is already loaded by looking it up
 * in the `LUA_LOADED_TABLE` registry. If the module is already loaded, it returns the module
 * immediately. If the module is not loaded, it searches for a loader function using `findloader`,
 * calls the loader to load the module, and stores the result in the `LUA_LOADED_TABLE` registry.
 * If the module loader does not return a value, it sets the module's entry in the `LUA_LOADED_TABLE`
 * to `true` and returns `true`.
 *
 * @param L The Lua state.
 * @return Returns 1, pushing the loaded module (or `true` if no value was returned by the loader)
 *         onto the Lua stack.
 */
static int ll_require (lua_State *L) {
  const char *name = luaL_checkstring(L, 1);
  lua_settop(L, 1);  /* LOADED table will be at index 2 */
  lua_getfield(L, LUA_REGISTRYINDEX, LUA_LOADED_TABLE);
  lua_getfield(L, 2, name);  /* LOADED[name] */
  if (lua_toboolean(L, -1))  /* is it there? */
    return 1;  /* package is already loaded */
  /* else must load package */
  lua_pop(L, 1);  /* remove 'getfield' result */
  findloader(L, name);
  lua_pushstring(L, name);  /* pass name as argument to module loader */
  lua_insert(L, -2);  /* name is 1st argument (before search data) */
  lua_call(L, 2, 1);  /* run loader to load module */
  if (!lua_isnil(L, -1))  /* non-nil return? */
    lua_setfield(L, 2, name);  /* LOADED[name] = returned value */
  if (lua_getfield(L, 2, name) == LUA_TNIL) {   /* module set no value? */
    lua_pushboolean(L, 1);  /* use true as result */
    lua_pushvalue(L, -1);  /* extra copy to be returned */
    lua_setfield(L, 2, name);  /* LOADED[name] = true */
  }
  return 1;
}

/* }====================================================== */



/*
** {======================================================
** 'module' function
** =======================================================
*/
#if defined(LUA_COMPAT_MODULE)

/*
** changes the environment variable of calling function
*/
static void set_env (lua_State *L) {
  lua_Debug ar;
  if (lua_getstack(L, 1, &ar) == 0 ||
      lua_getinfo(L, "f", &ar) == 0 ||  /* get calling function */
      lua_iscfunction(L, -1))
    luaL_error(L, "'module' not called from a Lua function");
  lua_pushvalue(L, -2);  /* copy new environment table to top */
  lua_setupvalue(L, -2, 1);
  lua_pop(L, 1);  /* remove function */
}


/**
 * Executes a series of Lua functions passed as options to the method.
 *
 * This method iterates over the arguments starting from the second position up to the nth argument.
 * For each argument, it checks if the argument is a Lua function. If it is, the function is called
 * with the module (the first argument) as its parameter. This allows for flexible configuration or
 * customization of the module by passing functions as options.
 *
 * @param L Pointer to the Lua state.
 * @param n The total number of arguments passed to the method, including the module.
 */
static void dooptions (lua_State *L, int n) {
  int i;
  for (i = 2; i <= n; i++) {
    if (lua_isfunction(L, i)) {  /* avoid 'calling' extra info. */
      lua_pushvalue(L, i);  /* get option (a function) */
      lua_pushvalue(L, -2);  /* module */
      lua_call(L, 1, 0);
    }
  }
}


/**
 * Initializes a Lua module by setting its metadata fields.
 *
 * This function sets up the metadata for a Lua module by populating the following fields:
 * - `_M`: The module itself, stored as a reference to the module table.
 * - `_NAME`: The full name of the module, provided as `modname`.
 * - `_PACKAGE`: The package name, derived by removing the last part of the module name.
 *
 * The module name (`modname`) is expected to be a dot-separated string (e.g., "package.submodule").
 * The last dot in the name is used to split the module name into the package name and the module name.
 * If no dot is found, the entire `modname` is used as the package name.
 *
 * @param L The Lua state in which the module is being initialized.
 * @param modname The full name of the module, typically a dot-separated string.
 */
static void modinit (lua_State *L, const char *modname) {
  const char *dot;
  lua_pushvalue(L, -1);
  lua_setfield(L, -2, "_M");  /* module._M = module */
  lua_pushstring(L, modname);
  lua_setfield(L, -2, "_NAME");
  dot = strrchr(modname, '.');  /* look for last dot in module name */
  if (dot == NULL) dot = modname;
  else dot++;
  /* set _PACKAGE as package name (full module name minus last part) */
  lua_pushlstring(L, modname, dot - modname);
  lua_setfield(L, -2, "_PACKAGE");
}


/**
 * Initializes or retrieves a Lua module and sets its environment.
 *
 * This function is responsible for handling the creation or retrieval of a Lua module
 * identified by `modname`. It first checks if the module table already exists and is
 * initialized by verifying the presence of the `_NAME` field. If the module is not
 * initialized, it calls `modinit` to initialize it. The function then sets the module's
 * environment and processes any additional options provided as arguments.
 *
 * @param L Pointer to the Lua state.
 * @return Returns 1, pushing the module table onto the Lua stack.
 *
 * @details The function performs the following steps:
 * 1. Retrieves the module name from the Lua stack using `luaL_checkstring`.
 * 2. Determines the last argument index using `lua_gettop`.
 * 3. Pushes the module table onto the stack using `luaL_pushmodule`.
 * 4. Checks if the module table already has a `_NAME` field using `lua_getfield`.
 *    - If the field exists, it pops the field value from the stack.
 *    - If the field does not exist, it pops the nil value and initializes the module
 *      by calling `modinit`.
 * 5. Pushes a copy of the module table onto the stack.
 * 6. Sets the module's environment using `set_env`.
 * 7. Processes any additional options using `dooptions`.
 * 8. Returns 1, indicating that the module table is left on the stack.
 */
static int ll_module (lua_State *L) {
  const char *modname = luaL_checkstring(L, 1);
  int lastarg = lua_gettop(L);  /* last parameter */
  luaL_pushmodule(L, modname, 1);  /* get/create module table */
  /* check whether table already has a _NAME field */
  if (lua_getfield(L, -1, "_NAME") != LUA_TNIL)
    lua_pop(L, 1);  /* table is an initialized module */
  else {  /* no; initialize it */
    lua_pop(L, 1);
    modinit(L, modname);
  }
  lua_pushvalue(L, -1);
  set_env(L);
  dooptions(L, lastarg);
  return 1;
}


/**
 * This function ensures that the table at index 1 on the Lua stack has a metatable
 * with a global table (`_G`) as its `__index` field. If the table does not already
 * have a metatable, a new metatable is created and assigned to it. The `__index`
 * field of the metatable is then set to the global table (`_G`), allowing the table
 * to access global variables as if they were its own fields.
 *
 * @param L The Lua state.
 * @return Always returns 0, indicating no values are pushed onto the Lua stack.
 */
static int ll_seeall (lua_State *L) {
  luaL_checktype(L, 1, LUA_TTABLE);
  if (!lua_getmetatable(L, 1)) {
    lua_createtable(L, 0, 1); /* create new metatable */
    lua_pushvalue(L, -1);
    lua_setmetatable(L, 1);
  }
  lua_pushglobaltable(L);
  lua_setfield(L, -2, "__index");  /* mt.__index = _G */
  return 0;
}

#endif
/* }====================================================== */



static const luaL_Reg pk_funcs[] = {
  {"loadlib", ll_loadlib},
  {"searchpath", ll_searchpath},
#if defined(LUA_COMPAT_MODULE)
  {"seeall", ll_seeall},
#endif
  /* placeholders */
  {"preload", NULL},
  {"cpath", NULL},
  {"path", NULL},
  {"searchers", NULL},
  {"loaded", NULL},
  {NULL, NULL}
};


static const luaL_Reg ll_funcs[] = {
#if defined(LUA_COMPAT_MODULE)
  {"module", ll_module},
#endif
  {"require", ll_require},
  {NULL, NULL}
};


/**
 * Creates and initializes the 'searchers' table in the Lua package system.
 * 
 * This function constructs a table containing predefined searcher functions
 * used by Lua to locate and load modules. The searchers include:
 * - `searcher_preload`: Searches the `package.preload` table.
 * - `searcher_Lua`: Searches for Lua modules in the Lua path.
 * - `searcher_C`: Searches for C modules in the C path.
 * - `searcher_Croot`: Searches for C modules in the root of the C path.
 * 
 * The table is created with the appropriate size to hold all searchers, and
 * each searcher is assigned a position in the table. The 'package' table is
 * set as an upvalue for each searcher to provide access to package-related
 * functions and variables.
 * 
 * If `LUA_COMPAT_LOADERS` is defined, the function also creates a copy of the
 * 'searchers' table and stores it in the 'loaders' field for backward compatibility.
 * 
 * Finally, the 'searchers' table is stored in the 'searchers' field of the
 * 'package' table, making it available for use by the Lua module system.
 * 
 * @param L The Lua state in which to create and initialize the 'searchers' table.
 */
static void createsearcherstable (lua_State *L) {
  static const lua_CFunction searchers[] =
    {searcher_preload, searcher_Lua, searcher_C, searcher_Croot, NULL};
  int i;
  /* create 'searchers' table */
  lua_createtable(L, sizeof(searchers)/sizeof(searchers[0]) - 1, 0);
  /* fill it with predefined searchers */
  for (i=0; searchers[i] != NULL; i++) {
    lua_pushvalue(L, -2);  /* set 'package' as upvalue for all searchers */
    lua_pushcclosure(L, searchers[i], 1);
    lua_rawseti(L, -2, i+1);
  }
#if defined(LUA_COMPAT_LOADERS)
  lua_pushvalue(L, -1);  /* make a copy of 'searchers' table */
  lua_setfield(L, -3, "loaders");  /* put it in field 'loaders' */
#endif
  lua_setfield(L, -2, "searchers");  /* put it in field 'searchers' */
}


/*
** create table CLIBS to keep track of loaded C libraries,
** setting a finalizer to close all libraries when closing state.
*/
static void createclibstable (lua_State *L) {
  lua_newtable(L);  /* create CLIBS table */
  lua_createtable(L, 0, 1);  /* create metatable for CLIBS */
  lua_pushcfunction(L, gctm);
  lua_setfield(L, -2, "__gc");  /* set finalizer for CLIBS table */
  lua_setmetatable(L, -2);
  lua_rawsetp(L, LUA_REGISTRYINDEX, &CLIBS);  /* set CLIBS table in registry */
}


/**
 * Initializes and opens the 'package' module in the Lua environment.
 * This function sets up the package system by creating the 'package' table,
 * configuring paths for Lua modules and C libraries, and initializing
 * related subsystems such as the searchers table and the loaded/preload tables.
 * 
 * The function performs the following steps:
 * 1. Creates the 'package' table using `luaL_newlib` with the `pk_funcs` library.
 * 2. Initializes the searchers table for module loading.
 * 3. Sets the default paths for Lua modules (`path`) and C libraries (`cpath`)
 *    using environment variables and default values.
 * 4. Stores configuration information (e.g., directory separators, path markers)
 *    in the 'package' table under the 'config' field.
 * 5. Links the 'loaded' and 'preload' tables from the Lua registry to the 'package' table.
 * 6. Opens the library into the global table and sets 'package' as an upvalue.
 * 
 * @param L The Lua state.
 * @return Returns 1, indicating the 'package' table is pushed onto the stack.
 */
LUAMOD_API int luaopen_package (lua_State *L) {
  createclibstable(L);
  luaL_newlib(L, pk_funcs);  /* create 'package' table */
  createsearcherstable(L);
  /* set paths */
  setpath(L, "path", LUA_PATH_VAR, LUA_PATH_DEFAULT);
  setpath(L, "cpath", LUA_CPATH_VAR, LUA_CPATH_DEFAULT);
  /* store config information */
  lua_pushliteral(L, LUA_DIRSEP "\n" LUA_PATH_SEP "\n" LUA_PATH_MARK "\n"
                     LUA_EXEC_DIR "\n" LUA_IGMARK "\n");
  lua_setfield(L, -2, "config");
  /* set field 'loaded' */
  luaL_getsubtable(L, LUA_REGISTRYINDEX, LUA_LOADED_TABLE);
  lua_setfield(L, -2, "loaded");
  /* set field 'preload' */
  luaL_getsubtable(L, LUA_REGISTRYINDEX, LUA_PRELOAD_TABLE);
  lua_setfield(L, -2, "preload");
  lua_pushglobaltable(L);
  lua_pushvalue(L, -2);  /* set 'package' as upvalue for next lib */
  luaL_setfuncs(L, ll_funcs, 1);  /* open lib into global table */
  lua_pop(L, 1);  /* pop global table */
  return 1;  /* return 'package' table */
}

