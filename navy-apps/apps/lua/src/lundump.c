/*
** $Id: lundump.c,v 2.44 2015/11/02 16:09:30 roberto Exp $
** load precompiled Lua chunks
** See Copyright Notice in lua.h
*/

#define lundump_c
#define LUA_CORE

#include "lprefix.h"


#include <string.h>

#include "lua.h"

#include "ldebug.h"
#include "ldo.h"
#include "lfunc.h"
#include "lmem.h"
#include "lobject.h"
#include "lstring.h"
#include "lundump.h"
#include "lzio.h"


#if !defined(luai_verifycode)
#define luai_verifycode(L,b,f)  /* empty */
#endif


typedef struct {
  lua_State *L;
  ZIO *Z;
  const char *name;
} LoadState;


/**
 * Reports an error during the loading of a precompiled chunk and throws a syntax error.
 *
 * This function constructs an error message using the provided reason (`why`) and the name
 * of the chunk being loaded (`S->name`). The error message is formatted as:
 * "<chunk name>: <reason> precompiled chunk". The error message is then pushed onto the Lua
 * stack using `luaO_pushfstring`. Finally, the function throws a syntax error using
 * `luaD_throw`, which terminates the current execution with the error code `LUA_ERRSYNTAX`.
 *
 * @param S Pointer to the `LoadState` structure containing the Lua state and chunk name.
 * @param why A string describing the reason for the error.
 */
static l_noret error(LoadState *S, const char *why) {
  luaO_pushfstring(S->L, "%s: %s precompiled chunk", S->name, why);
  luaD_throw(S->L, LUA_ERRSYNTAX);
}


/*
** All high-level loads go through LoadVector; you can change it to
** adapt to the endianness of the input
*/
#define LoadVector(S,b,n)	LoadBlock(S,b,(n)*sizeof((b)[0]))

/**
 * Loads a block of data of a specified size into the provided buffer from the given LoadState.
 *
 * This function reads a block of data of size `size` from the LoadState's input stream `S->Z`
 * and stores it in the buffer `b`. If the read operation fails (e.g., due to insufficient data
 * in the input stream), the function raises an error with the message "truncated".
 *
 * @param S Pointer to the LoadState structure containing the input stream.
 * @param b Pointer to the buffer where the loaded data will be stored.
 * @param size The number of bytes to read from the input stream.
 *
 * @note The function assumes that the buffer `b` has sufficient space to hold `size` bytes.
 * @note If the read operation fails, the function will terminate the program with an error.
 */
static void LoadBlock (LoadState *S, void *b, size_t size) {
  if (luaZ_read(S->Z, b, size) != 0)
    error(S, "truncated");
}


#define LoadVar(S,x)		LoadVector(S,&x,1)


/**
 * Loads a single byte from the given LoadState.
 *
 * This function reads a byte (lu_byte) from the LoadState object `S` and returns it.
 * It uses the `LoadVar` function to perform the actual loading of the byte into the
 * variable `x`. The loaded byte is then returned to the caller.
 *
 * @param S A pointer to the LoadState object from which the byte is to be loaded.
 * @return The byte (lu_byte) that was loaded from the LoadState.
 */
static lu_byte LoadByte (LoadState *S) {
  lu_byte x;
  LoadVar(S, x);
  return x;
}


/**
 * Loads an integer value from the provided LoadState object.
 *
 * This function reads an integer value from the LoadState object `S` and stores it in the variable `x`.
 * The actual loading of the integer is handled by the `LoadVar` function, which is expected to correctly
 * deserialize the integer from the state. The loaded integer value is then returned to the caller.
 *
 * @param S A pointer to the LoadState object from which the integer is to be loaded.
 * @return The integer value loaded from the LoadState object.
 */
static int LoadInt (LoadState *S) {
  int x;
  LoadVar(S, x);
  return x;
}


/**
 * Loads a `lua_Number` from the provided `LoadState` object.
 * 
 * This function reads a numeric value of type `lua_Number` from the `LoadState` object `S`
 * using the `LoadVar` function and returns the loaded value. It is typically used in the
 * context of deserializing Lua data, where `LoadState` represents the state of the data
 * being loaded.
 * 
 * @param S A pointer to the `LoadState` object from which the number is loaded.
 * @return The `lua_Number` value loaded from the `LoadState`.
 */
static lua_Number LoadNumber (LoadState *S) {
  lua_Number x;
  LoadVar(S, x);
  return x;
}


/**
 * Loads a `lua_Integer` value from the provided `LoadState` object.
 *
 * This method reads a `lua_Integer` value from the given `LoadState` instance
 * using the `LoadVar` function and returns the loaded value. It is typically
 * used during the deserialization of Lua data structures to restore integer
 * values from a serialized state.
 *
 * @param S A pointer to the `LoadState` object from which the integer is loaded.
 * @return The `lua_Integer` value read from the `LoadState`.
 */
static lua_Integer LoadInteger (LoadState *S) {
  lua_Integer x;
  LoadVar(S, x);
  return x;
}


/**
 * Loads a string from a given LoadState object.
 *
 * This function reads a string from the provided LoadState `S`. The string's size is first read
 * as a single byte. If the size byte is `0xFF`, it indicates that the size is larger than what
 * can be represented by a single byte, and the actual size is then read using `LoadVar`.
 *
 * If the size is `0`, the function returns `NULL`, indicating an empty string. For non-empty strings,
 * the function distinguishes between short strings and long strings based on the size:
 * - If the size (after decrementing by 1) is less than or equal to `LUAI_MAXSHORTLEN`, the string
 *   is considered a short string. It is loaded into a temporary buffer and then used to create a
 *   new short string object via `luaS_newlstr`.
 * - If the size is larger than `LUAI_MAXSHORTLEN`, the string is considered a long string. A long
 *   string object is created via `luaS_createlngstrobj`, and the string data is loaded directly into
 *   the allocated memory.
 *
 * @param S A pointer to the LoadState object from which the string is loaded.
 * @return A pointer to the loaded TString object, or `NULL` if the string is empty.
 */
static TString *LoadString (LoadState *S) {
  size_t size = LoadByte(S);
  if (size == 0xFF)
    LoadVar(S, size);
  if (size == 0)
    return NULL;
  else if (--size <= LUAI_MAXSHORTLEN) {  /* short string? */
    char buff[LUAI_MAXSHORTLEN];
    LoadVector(S, buff, size);
    return luaS_newlstr(S->L, buff, size);
  }
  else {  /* long string */
    TString *ts = luaS_createlngstrobj(S->L, size);
    LoadVector(S, getstr(ts), size);  /* load directly in final place */
    return ts;
  }
}


/**
 * Loads the bytecode instructions for a given Lua function prototype from a LoadState.
 *
 * This method reads the number of instructions (n) from the LoadState, allocates memory
 * for the instructions using `luaM_newvector`, sets the size of the code in the function
 * prototype (f->sizecode), and then loads the actual bytecode instructions into the
 * allocated memory using `LoadVector`.
 *
 * @param S The LoadState from which the bytecode instructions are read.
 * @param f The Proto structure representing the Lua function prototype where the
 *          loaded bytecode instructions will be stored.
 */
static void LoadCode (LoadState *S, Proto *f) {
  int n = LoadInt(S);
  f->code = luaM_newvector(S->L, n, Instruction);
  f->sizecode = n;
  LoadVector(S, f->code, n);
}


static void LoadFunction(LoadState *S, Proto *f, TString *psource);


/**
 * Loads constants into the given Proto structure from the provided LoadState.
 *
 * This function reads a sequence of constants from the LoadState and stores them
 * in the `k` array of the Proto structure. The constants are initialized based
 * on their type, which is read from the LoadState. The types supported include
 * `LUA_TNIL`, `LUA_TBOOLEAN`, `LUA_TNUMFLT`, `LUA_TNUMINT`, `LUA_TSHRSTR`, and
 * `LUA_TLNGSTR`. Each constant is stored in the `k` array of the Proto structure,
 * and the `sizek` field is updated to reflect the number of constants loaded.
 *
 * @param S The LoadState from which the constants are read.
 * @param f The Proto structure where the constants will be stored.
 */
static void LoadConstants (LoadState *S, Proto *f) {
  int i;
  int n = LoadInt(S);
  f->k = luaM_newvector(S->L, n, TValue);
  f->sizek = n;
  for (i = 0; i < n; i++)
    setnilvalue(&f->k[i]);
  for (i = 0; i < n; i++) {
    TValue *o = &f->k[i];
    int t = LoadByte(S);
    switch (t) {
    case LUA_TNIL:
      setnilvalue(o);
      break;
    case LUA_TBOOLEAN:
      setbvalue(o, LoadByte(S));
      break;
    case LUA_TNUMFLT:
      setfltvalue(o, LoadNumber(S));
      break;
    case LUA_TNUMINT:
      setivalue(o, LoadInteger(S));
      break;
    case LUA_TSHRSTR:
    case LUA_TLNGSTR:
      setsvalue2n(S->L, o, LoadString(S));
      break;
    default:
      lua_assert(0);
    }
  }
}


/**
 * Loads a set of prototypes (Protos) into the given Proto structure from the provided LoadState.
 * 
 * This function reads an integer `n` from the LoadState, which represents the number of prototypes
 * to be loaded. It then allocates memory for an array of `n` Proto pointers within the given Proto
 * structure `f`. Each element in the array is initialized to `NULL`. Subsequently, for each index
 * in the array, a new Proto is created using `luaF_newproto`, and the corresponding function is
 * loaded from the LoadState using `LoadFunction`. The source of the Proto is also set during this
 * process.
 *
 * @param S The LoadState from which the prototypes are loaded.
 * @param f The Proto structure where the loaded prototypes will be stored.
 */
static void LoadProtos (LoadState *S, Proto *f) {
  int i;
  int n = LoadInt(S);
  f->p = luaM_newvector(S->L, n, Proto *);
  f->sizep = n;
  for (i = 0; i < n; i++)
    f->p[i] = NULL;
  for (i = 0; i < n; i++) {
    f->p[i] = luaF_newproto(S->L);
    LoadFunction(S, f->p[i], f->source);
  }
}


/**
 * Loads upvalues into a given function prototype from a LoadState.
 *
 * This function reads the number of upvalues from the LoadState, allocates memory
 * for the upvalues in the function prototype, and initializes each upvalue's
 * properties. The upvalues are loaded in two steps:
 * 1. The number of upvalues is read from the LoadState, and memory is allocated
 *    for the upvalues in the function prototype.
 * 2. For each upvalue, the `instack` and `idx` properties are read from the LoadState
 *    and assigned to the corresponding upvalue in the function prototype.
 *
 * @param S The LoadState from which the upvalues are read.
 * @param f The function prototype into which the upvalues are loaded.
 */
static void LoadUpvalues (LoadState *S, Proto *f) {
  int i, n;
  n = LoadInt(S);
  f->upvalues = luaM_newvector(S->L, n, Upvaldesc);
  f->sizeupvalues = n;
  for (i = 0; i < n; i++)
    f->upvalues[i].name = NULL;
  for (i = 0; i < n; i++) {
    f->upvalues[i].instack = LoadByte(S);
    f->upvalues[i].idx = LoadByte(S);
  }
}


/**
 * Loads debug information into a Proto structure from a LoadState.
 *
 * This function reads debug-related data from the provided LoadState and populates
 * the corresponding fields in the Proto structure. The debug information includes:
 * - Line information: An array of integers representing line numbers for the bytecode.
 * - Local variables: An array of LocVar structures containing variable names and their
 *   program counter (PC) ranges (startpc and endpc).
 * - Upvalues: An array of upvalue names.
 *
 * The function performs the following steps:
 * 1. Reads the size of the line information array, allocates memory for it, and loads
 *    the line information from the LoadState.
 * 2. Reads the size of the local variables array, allocates memory for it, initializes
 *    the variable names to NULL, and then loads the variable names and their PC ranges.
 * 3. Reads the size of the upvalues array and loads the upvalue names.
 *
 * @param S The LoadState from which the debug information is read.
 * @param f The Proto structure into which the debug information is loaded.
 */
static void LoadDebug (LoadState *S, Proto *f) {
  int i, n;
  n = LoadInt(S);
  f->lineinfo = luaM_newvector(S->L, n, int);
  f->sizelineinfo = n;
  LoadVector(S, f->lineinfo, n);
  n = LoadInt(S);
  f->locvars = luaM_newvector(S->L, n, LocVar);
  f->sizelocvars = n;
  for (i = 0; i < n; i++)
    f->locvars[i].varname = NULL;
  for (i = 0; i < n; i++) {
    f->locvars[i].varname = LoadString(S);
    f->locvars[i].startpc = LoadInt(S);
    f->locvars[i].endpc = LoadInt(S);
  }
  n = LoadInt(S);
  for (i = 0; i < n; i++)
    f->upvalues[i].name = LoadString(S);
}


/**
 * Loads a function prototype (`Proto`) from a serialized state (`LoadState`).
 * 
 * This method deserializes various components of a function prototype, including its source code,
 * line definitions, parameter count, variable argument flag, maximum stack size, bytecode, constants,
 * upvalues, nested prototypes, and debugging information. If the source code is not present in the
 * serialized state, it reuses the parent's source code (`psource`).
 *
 * @param S The `LoadState` object representing the serialized state from which to load the function.
 * @param f The `Proto` object representing the function prototype to populate with loaded data.
 * @param psource The `TString` object representing the parent's source code, used as a fallback if
 *                the source code is not present in the serialized state.
 */
static void LoadFunction (LoadState *S, Proto *f, TString *psource) {
  f->source = LoadString(S);
  if (f->source == NULL)  /* no source in dump? */
    f->source = psource;  /* reuse parent's source */
  f->linedefined = LoadInt(S);
  f->lastlinedefined = LoadInt(S);
  f->numparams = LoadByte(S);
  f->is_vararg = LoadByte(S);
  f->maxstacksize = LoadByte(S);
  LoadCode(S, f);
  LoadConstants(S, f);
  LoadUpvalues(S, f);
  LoadProtos(S, f);
  LoadDebug(S, f);
}


/**
 * Checks if the next sequence of bytes in the LoadState matches the given literal string.
 * 
 * This function reads a sequence of bytes from the LoadState `S` and compares it against the
 * provided string `s`. If the read bytes do not match the string, an error is raised using the
 * provided error message `msg`.
 *
 * @param S Pointer to the LoadState from which the bytes are read.
 * @param s The literal string to compare against the read bytes.
 * @param msg The error message to be used if the comparison fails.
 *
 * @note The buffer `buff` is sized to be larger than both `LUA_SIGNATURE` and `LUAC_DATA` to
 *       ensure it can accommodate the largest possible literal being checked.
 */
static void checkliteral (LoadState *S, const char *s, const char *msg) {
  char buff[sizeof(LUA_SIGNATURE) + sizeof(LUAC_DATA)]; /* larger than both */
  size_t len = strlen(s);
  LoadVector(S, buff, len);
  if (memcmp(s, buff, len) != 0)
    error(S, msg);
}


/**
 * Checks if the size of the data being loaded matches the expected size.
 * 
 * This function reads a byte from the LoadState `S` and compares it to the expected `size`.
 * If the sizes do not match, it triggers an error with a message indicating a size mismatch
 * for the given type name `tname`. The error message is formatted using `luaO_pushfstring`
 * and includes the type name in the message.
 *
 * @param S Pointer to the LoadState from which the data is being loaded.
 * @param size The expected size of the data.
 * @param tname The name of the type being checked, used in the error message.
 */
static void fchecksize (LoadState *S, size_t size, const char *tname) {
  if (LoadByte(S) != size)
    error(S, luaO_pushfstring(S->L, "%s size mismatch in", tname));
}


#define checksize(S,t)	fchecksize(S,sizeof(t),#t)

/**
 * @brief Verifies the header of a Lua binary chunk to ensure it matches the expected format.
 *
 * This function performs a series of checks on the header of a Lua binary chunk loaded via the `LoadState` structure.
 * It ensures that the header matches the expected Lua signature, version, format, and other critical properties.
 * If any check fails, the function triggers an error indicating the specific mismatch or corruption.
 *
 * The checks include:
 * - Verifying the Lua signature (excluding the first character, which is assumed to be already checked).
 * - Ensuring the Lua version matches the expected `LUAC_VERSION`.
 * - Ensuring the Lua format matches the expected `LUAC_FORMAT`.
 * - Verifying the presence of the expected `LUAC_DATA` literal.
 * - Checking the sizes of fundamental types (`int`, `size_t`, `Instruction`, `lua_Integer`, `lua_Number`).
 * - Verifying the endianness of the binary chunk matches the expected `LUAC_INT`.
 * - Verifying the floating-point format matches the expected `LUAC_NUM`.
 *
 * @param S A pointer to the `LoadState` structure containing the binary chunk to be verified.
 */
static void checkHeader (LoadState *S) {
  checkliteral(S, LUA_SIGNATURE + 1, "not a");  /* 1st char already checked */
  if (LoadByte(S) != LUAC_VERSION)
    error(S, "version mismatch in");
  if (LoadByte(S) != LUAC_FORMAT)
    error(S, "format mismatch in");
  checkliteral(S, LUAC_DATA, "corrupted");
  checksize(S, int);
  checksize(S, size_t);
  checksize(S, Instruction);
  checksize(S, lua_Integer);
  checksize(S, lua_Number);
  if (LoadInteger(S) != LUAC_INT)
    error(S, "endianness mismatch in");
  if (LoadNumber(S) != LUAC_NUM)
    error(S, "float format mismatch in");
}


/*
** load precompiled chunk
*/
LClosure *luaU_undump(lua_State *L, ZIO *Z, const char *name) {
  LoadState S;
  LClosure *cl;
  if (*name == '@' || *name == '=')
    S.name = name + 1;
  else if (*name == LUA_SIGNATURE[0])
    S.name = "binary string";
  else
    S.name = name;
  S.L = L;
  S.Z = Z;
  checkHeader(&S);
  cl = luaF_newLclosure(L, LoadByte(&S));
  setclLvalue(L, L->top, cl);
  luaD_inctop(L);
  cl->p = luaF_newproto(L);
  LoadFunction(&S, cl->p, NULL);
  lua_assert(cl->nupvalues == cl->p->sizeupvalues);
  luai_verifycode(L, buff, cl->p);
  return cl;
}

