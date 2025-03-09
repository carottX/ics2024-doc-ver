/*
** $Id: ldump.c,v 2.37 2015/10/08 15:53:49 roberto Exp $
** save precompiled Lua chunks
** See Copyright Notice in lua.h
*/

#define ldump_c
#define LUA_CORE

#include "lprefix.h"


#include <stddef.h>

#include "lua.h"

#include "lobject.h"
#include "lstate.h"
#include "lundump.h"


typedef struct {
  lua_State *L;
  lua_Writer writer;
  void *data;
  int strip;
  int status;
} DumpState;


/*
** All high-level dumps go through DumpVector; you can change it to
** change the endianness of the result
*/
#define DumpVector(v,n,D)	DumpBlock(v,(n)*sizeof((v)[0]),D)

#define DumpLiteral(s,D)	DumpBlock(s, sizeof(s) - sizeof(char), D)


/**
 * Dumps a block of data to the specified writer function in the DumpState.
 * 
 * This function writes the given block of data to the writer function stored in the DumpState.
 * The writer function is invoked only if the DumpState's status is 0 (indicating no errors)
 * and the size of the block is greater than 0. Before invoking the writer function, the Lua
 * state is unlocked to allow for potential reentrant operations, and it is locked again
 * immediately after the writer function completes. The status of the DumpState is updated
 * with the return value of the writer function.
 *
 * @param b     Pointer to the block of data to be dumped.
 * @param size  Size of the block of data in bytes.
 * @param D     Pointer to the DumpState structure containing the writer function and other
 *              relevant data. The DumpState's status is updated based on the result of the
 *              writer function.
 */
static void DumpBlock (const void *b, size_t size, DumpState *D) {
  if (D->status == 0 && size > 0) {
    lua_unlock(D->L);
    D->status = (*D->writer)(D->L, b, size, D->data);
    lua_lock(D->L);
  }
}


#define DumpVar(x,D)		DumpVector(&x,1,D)


/**
 * @brief Dumps a single byte to the provided DumpState.
 *
 * This method takes an integer `y`, casts it to a `lu_byte` (unsigned byte),
 * and then dumps the resulting byte to the `DumpState` object `D` using the
 * `DumpVar` function. This is typically used to serialize or write a single
 * byte of data to a specific state or output stream.
 *
 * @param y The integer value to be cast to a byte and dumped.
 * @param D Pointer to the DumpState object where the byte will be dumped.
 */
static void DumpByte (int y, DumpState *D) {
  lu_byte x = (lu_byte)y;
  DumpVar(x, D);
}


/**
 * @brief Dumps an integer value into the provided DumpState.
 *
 * This method serializes the integer `x` and writes it into the `DumpState` structure
 * pointed to by `D`. It internally uses the `DumpVar` function to handle the actual
 * serialization process. This function is typically used in scenarios where data
 * needs to be saved or transmitted in a structured format.
 *
 * @param x The integer value to be dumped.
 * @param D Pointer to the DumpState structure where the integer will be dumped.
 */
static void DumpInt (int x, DumpState *D) {
  DumpVar(x, D);
}


/**
 * Dumps a Lua number (`lua_Number`) into the provided `DumpState`.
 *
 * This function serializes the given Lua number `x` and writes it into the
 * dump state `D` using the `DumpVar` function. It is typically used during
 * the serialization process of Lua data structures to prepare them for
 * storage or transmission.
 *
 * @param x The Lua number to be dumped.
 * @param D Pointer to the `DumpState` where the number will be stored.
 */
static void DumpNumber(lua_Number x, DumpState *D) {
  DumpVar(x, D);
}


/**
 * Dumps a Lua integer value into the provided DumpState.
 *
 * This method serializes the given Lua integer `x` and writes it into the
 * DumpState `D`. The actual serialization is handled by the `DumpVar` function,
 * which is responsible for encoding the integer in a format suitable for
 * storage or transmission.
 *
 * @param x The Lua integer value to be dumped.
 * @param D A pointer to the DumpState where the integer will be written.
 */
static void DumpInteger (lua_Integer x, DumpState *D) {
  DumpVar(x, D);
}


/**
 * Dumps a string to the provided DumpState.
 *
 * This function serializes a string (`s`) into the given DumpState (`D`). If the string is `NULL`,
 * it writes a single byte with the value `0` to indicate a null string. Otherwise, it writes the
 * length of the string followed by the string content itself. The length is written as a single byte
 * if it is less than `0xFF`; otherwise, it writes `0xFF` followed by the actual length as a variable-sized
 * integer. The string content is written without the trailing null terminator.
 *
 * @param s The string to be dumped. If `NULL`, a null string is indicated in the dump.
 * @param D The DumpState where the string will be serialized.
 */
static void DumpString (const TString *s, DumpState *D) {
  if (s == NULL)
    DumpByte(0, D);
  else {
    size_t size = tsslen(s) + 1;  /* include trailing '\0' */
    const char *str = getstr(s);
    if (size < 0xFF)
      DumpByte(cast_int(size), D);
    else {
      DumpByte(0xFF, D);
      DumpVar(size, D);
    }
    DumpVector(str, size - 1, D);  /* no need to save '\0' */
  }
}


/**
 * Dumps the bytecode of a Lua function prototype to the provided DumpState.
 *
 * This function serializes the bytecode of the given Lua function prototype (`f`)
 * by writing its size and the actual bytecode array to the DumpState (`D`). The
 * size of the bytecode is first written using `DumpInt`, followed by the bytecode
 * array itself using `DumpVector`.
 *
 * @param f Pointer to the Proto structure containing the Lua function prototype.
 * @param D Pointer to the DumpState structure used for serialization.
 */
static void DumpCode (const Proto *f, DumpState *D) {
  DumpInt(f->sizecode, D);
  DumpVector(f->code, f->sizecode, D);
}


static void DumpFunction(const Proto *f, TString *psource, DumpState *D);

/**
 * Dumps the constants from a given Proto structure into a DumpState.
 *
 * This function iterates over the constants stored in the `k` array of the Proto structure `f`.
 * It writes the number of constants followed by each constant's type and value to the DumpState `D`.
 * The constants can be of various Lua types, including `LUA_TNIL`, `LUA_TBOOLEAN`, `LUA_TNUMFLT`,
 * `LUA_TNUMINT`, `LUA_TSHRSTR`, and `LUA_TLNGSTR`. The function handles each type appropriately:
 * - For `LUA_TNIL`, no additional data is written.
 * - For `LUA_TBOOLEAN`, the boolean value is written.
 * - For `LUA_TNUMFLT`, the floating-point number is written.
 * - For `LUA_TNUMINT`, the integer value is written.
 * - For `LUA_TSHRSTR` and `LUA_TLNGSTR`, the string value is written.
 * If an unexpected type is encountered, the function asserts and terminates the program.
 *
 * @param f Pointer to the Proto structure containing the constants to be dumped.
 * @param D Pointer to the DumpState where the constants will be written.
 */
static void DumpConstants (const Proto *f, DumpState *D) {
  int i;
  int n = f->sizek;
  DumpInt(n, D);
  for (i = 0; i < n; i++) {
    const TValue *o = &f->k[i];
    DumpByte(ttype(o), D);
    switch (ttype(o)) {
    case LUA_TNIL:
      break;
    case LUA_TBOOLEAN:
      DumpByte(bvalue(o), D);
      break;
    case LUA_TNUMFLT:
      DumpNumber(fltvalue(o), D);
      break;
    case LUA_TNUMINT:
      DumpInteger(ivalue(o), D);
      break;
    case LUA_TSHRSTR:
    case LUA_TLNGSTR:
      DumpString(tsvalue(o), D);
      break;
    default:
      lua_assert(0);
    }
  }
}


/**
 * Dumps the prototypes (Protos) contained within the given Proto structure to the provided DumpState.
 * This function iterates over the array of prototypes (`f->p`) and writes each one to the DumpState
 * using the `DumpFunction` function. The number of prototypes is first written to the DumpState using
 * `DumpInt`. This function is typically used during serialization or debugging to output the contents
 * of a Proto structure.
 *
 * @param f Pointer to the Proto structure containing the prototypes to be dumped.
 * @param D Pointer to the DumpState structure where the prototypes will be written.
 */
static void DumpProtos (const Proto *f, DumpState *D) {
  int i;
  int n = f->sizep;
  DumpInt(n, D);
  for (i = 0; i < n; i++)
    DumpFunction(f->p[i], f->source, D);
}


/**
 * Dumps the upvalues of a given Lua function prototype to the provided DumpState.
 *
 * This function iterates over the upvalues of the function prototype `f` and writes
 * their properties (instack and idx) to the DumpState `D`. The number of upvalues
 * is first written to the DumpState, followed by the properties of each upvalue.
 *
 * @param f The function prototype whose upvalues are to be dumped.
 * @param D The DumpState where the upvalue information is written.
 */
static void DumpUpvalues (const Proto *f, DumpState *D) {
  int i, n = f->sizeupvalues;
  DumpInt(n, D);
  for (i = 0; i < n; i++) {
    DumpByte(f->upvalues[i].instack, D);
    DumpByte(f->upvalues[i].idx, D);
  }
}


/**
 * Dumps debugging information from the given Proto structure into the provided DumpState.
 * This function serializes various debugging-related data, such as line information, local
 * variables, and upvalues, into the DumpState for further processing or storage.
 * 
 * If the DumpState's `strip` flag is set, the function skips dumping certain debugging
 * information to reduce the output size.
 * 
 * @param f Pointer to the Proto structure containing the debugging information to be dumped.
 * @param D Pointer to the DumpState structure where the debugging information will be serialized.
 * 
 * The function performs the following steps:
 * 1. Dumps the size of the line information array, unless `strip` is set.
 * 2. Dumps the line information array itself, if applicable.
 * 3. Dumps the number of local variables, unless `strip` is set.
 * 4. For each local variable, dumps its name, start program counter (PC), and end PC.
 * 5. Dumps the number of upvalues, unless `strip` is set.
 * 6. For each upvalue, dumps its name.
 */
static void DumpDebug (const Proto *f, DumpState *D) {
  int i, n;
  n = (D->strip) ? 0 : f->sizelineinfo;
  DumpInt(n, D);
  DumpVector(f->lineinfo, n, D);
  n = (D->strip) ? 0 : f->sizelocvars;
  DumpInt(n, D);
  for (i = 0; i < n; i++) {
    DumpString(f->locvars[i].varname, D);
    DumpInt(f->locvars[i].startpc, D);
    DumpInt(f->locvars[i].endpc, D);
  }
  n = (D->strip) ? 0 : f->sizeupvalues;
  DumpInt(n, D);
  for (i = 0; i < n; i++)
    DumpString(f->upvalues[i].name, D);
}


/**
 * Dumps the contents of a Proto object to the provided DumpState.
 *
 * This function serializes the contents of a Proto object, which represents a Lua function,
 * into a binary format using the provided DumpState. The serialized data includes the function's
 * source information, line numbers, parameters, variable arguments, stack size, bytecode,
 * constants, upvalues, nested functions, and debug information.
 *
 * @param f The Proto object representing the Lua function to be dumped.
 * @param psource The source string of the parent function. If the function's source is the same
 *                as the parent's or if the DumpState is configured to strip debug information,
 *                the source string is not dumped.
 * @param D The DumpState object used to manage the dumping process and store the serialized data.
 *
 * The function performs the following steps:
 * 1. Dumps the source string of the function, unless it matches the parent's source or debug
 *    information is stripped.
 * 2. Dumps the line numbers where the function is defined and ends.
 * 3. Dumps the number of parameters, variable arguments flag, and maximum stack size.
 * 4. Dumps the function's bytecode.
 * 5. Dumps the constants used by the function.
 * 6. Dumps the upvalues referenced by the function.
 * 7. Dumps any nested functions (Protos) within the function.
 * 8. Dumps debug information associated with the function.
 */
static void DumpFunction (const Proto *f, TString *psource, DumpState *D) {
  if (D->strip || f->source == psource)
    DumpString(NULL, D);  /* no debug info or same source as its parent */
  else
    DumpString(f->source, D);
  DumpInt(f->linedefined, D);
  DumpInt(f->lastlinedefined, D);
  DumpByte(f->numparams, D);
  DumpByte(f->is_vararg, D);
  DumpByte(f->maxstacksize, D);
  DumpCode(f, D);
  DumpConstants(f, D);
  DumpUpvalues(f, D);
  DumpProtos(f, D);
  DumpDebug(f, D);
}


/**
 * @brief Dumps the header information into the provided DumpState.
 *
 * This function writes the header information required for a Lua bytecode file
 * into the specified DumpState. The header includes:
 * - The Lua signature (LUA_SIGNATURE) to identify the file as a Lua bytecode file.
 * - The Lua version (LUAC_VERSION) and format (LUAC_FORMAT) to ensure compatibility.
 * - Additional data (LUAC_DATA) that may be used for identification or other purposes.
 * - The sizes of various data types (int, size_t, Instruction, lua_Integer, lua_Number)
 *   to ensure the bytecode is correctly interpreted on different platforms.
 * - Specific integer (LUAC_INT) and number (LUAC_NUM) values to verify the bytecode's integrity.
 *
 * @param D Pointer to the DumpState where the header information will be written.
 */
static void DumpHeader (DumpState *D) {
  DumpLiteral(LUA_SIGNATURE, D);
  DumpByte(LUAC_VERSION, D);
  DumpByte(LUAC_FORMAT, D);
  DumpLiteral(LUAC_DATA, D);
  DumpByte(sizeof(int), D);
  DumpByte(sizeof(size_t), D);
  DumpByte(sizeof(Instruction), D);
  DumpByte(sizeof(lua_Integer), D);
  DumpByte(sizeof(lua_Number), D);
  DumpInteger(LUAC_INT, D);
  DumpNumber(LUAC_NUM, D);
}


/*
** dump Lua function as precompiled chunk
*/
int luaU_dump(lua_State *L, const Proto *f, lua_Writer w, void *data,
              int strip) {
  DumpState D;
  D.L = L;
  D.writer = w;
  D.data = data;
  D.strip = strip;
  D.status = 0;
  DumpHeader(&D);
  DumpByte(f->sizeupvalues, &D);
  DumpFunction(f, NULL, &D);
  return D.status;
}

