/*
** $Id: ldebug.c,v 2.121 2016/10/19 12:32:10 roberto Exp $
** Debug Interface
** See Copyright Notice in lua.h
*/

#define ldebug_c
#define LUA_CORE

#include "lprefix.h"


#include <stdarg.h>
#include <stddef.h>
#include <string.h>

#include "lua.h"

#include "lapi.h"
#include "lcode.h"
#include "ldebug.h"
#include "ldo.h"
#include "lfunc.h"
#include "lobject.h"
#include "lopcodes.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "ltm.h"
#include "lvm.h"



#define noLuaClosure(f)		((f) == NULL || (f)->c.tt == LUA_TCCL)


/* Active Lua function (given call info) */
#define ci_func(ci)		(clLvalue((ci)->func))


static const char *funcnamefromcode (lua_State *L, CallInfo *ci,
                                    const char **name);


/**
 * Computes the current program counter (PC) relative to the start of the function.
 * 
 * This function asserts that the provided CallInfo (`ci`) corresponds to a Lua function.
 * It then calculates the relative PC by determining the difference between the saved PC
 * in the CallInfo and the starting PC of the function's prototype.
 *
 * @param ci A pointer to the CallInfo structure representing the current execution context.
 * @return The relative program counter (PC) as an integer.
 */
static int currentpc (CallInfo *ci) {
  lua_assert(isLua(ci));
  return pcRel(ci->u.l.savedpc, ci_func(ci)->p);
}


/**
 * Retrieves the current line number in the source code being executed by the given CallInfo.
 * 
 * This function calculates the current line number by first obtaining the function prototype
 * associated with the CallInfo, then using the current program counter (PC) to determine
 * the corresponding line number in the source code.
 *
 * @param ci A pointer to the CallInfo structure representing the current execution context.
 * @return The current line number in the source code, as determined by the function's
 *         prototype and the current program counter.
 */
static int currentline (CallInfo *ci) {
  return getfuncline(ci_func(ci)->p, currentpc(ci));
}


/*
** If function yielded, its 'func' can be in the 'extra' field. The
** next function restores 'func' to its correct value for debugging
** purposes. (It exchanges 'func' and 'extra'; so, when called again,
** after debugging, it also "re-restores" ** 'func' to its altered value.
*/
static void swapextra (lua_State *L) {
  if (L->status == LUA_YIELD) {
    CallInfo *ci = L->ci;  /* get function that yielded */
    StkId temp = ci->func;  /* exchange its 'func' and 'extra' values */
    ci->func = restorestack(L, ci->extra);
    ci->extra = savestack(L, temp);
  }
}


/*
** This function can be called asynchronously (e.g. during a signal).
** Fields 'oldpc', 'basehookcount', and 'hookcount' (set by
** 'resethookcount') are for debug only, and it is no problem if they
** get arbitrary values (causes at most one wrong hook call). 'hookmask'
** is an atomic value. We assume that pointers are atomic too (e.g., gcc
** ensures that for all platforms where it runs). Moreover, 'hook' is
** always checked before being called (see 'luaD_hook').
*/
LUA_API void lua_sethook (lua_State *L, lua_Hook func, int mask, int count) {
  if (func == NULL || mask == 0) {  /* turn off hooks? */
    mask = 0;
    func = NULL;
  }
  if (isLua(L->ci))
    L->oldpc = L->ci->u.l.savedpc;
  L->hook = func;
  L->basehookcount = count;
  resethookcount(L);
  L->hookmask = cast_byte(mask);
}


/**
 * Retrieves the current hook function associated with the Lua state.
 *
 * This function returns the hook function that is currently set in the Lua state `L`.
 * The hook function is used for debugging purposes, allowing the interception of
 * specific events during the execution of Lua code, such as function calls, returns,
 * or line execution.
 *
 * @param L A pointer to the Lua state from which to retrieve the hook function.
 * @return The current hook function associated with the Lua state, or `NULL` if no
 *         hook function is set.
 */
LUA_API lua_Hook lua_gethook (lua_State *L) {
  return L->hook;
}


/**
 * Returns the current hook mask for the Lua state.
 *
 * The hook mask is a bitwise combination of flags that determine which events
 * trigger the debug hook. The possible flags are:
 * - `LUA_MASKCALL`: Trigger the hook on function calls.
 * - `LUA_MASKRET`: Trigger the hook on function returns.
 * - `LUA_MASKLINE`: Trigger the hook on line changes.
 * - `LUA_MASKCOUNT`: Trigger the hook after a specified number of instructions.
 *
 * @param L A pointer to the Lua state.
 * @return The current hook mask as an integer.
 */
LUA_API int lua_gethookmask (lua_State *L) {
  return L->hookmask;
}


/**
 * Retrieves the base hook count for the Lua state.
 *
 * This function returns the base hook count associated with the Lua state `L`.
 * The base hook count is the number of instructions between hook calls when the hook
 * mechanism is active. This value is set by `lua_sethookcount` and can be used to
 * control the frequency of hook calls during Lua execution.
 *
 * @param L A pointer to the Lua state.
 * @return The base hook count for the given Lua state.
 */
LUA_API int lua_gethookcount (lua_State *L) {
  return L->basehookcount;
}


/**
 * Retrieves information about the execution stack at a specified level.
 *
 * This function traverses the call stack of the Lua state `L` to find the stack
 * frame corresponding to the given `level`. If the level is valid and a frame
 * is found, the function populates the `lua_Debug` structure `ar` with
 * information about the frame and returns 1. If the level is invalid (negative)
 * or no frame exists at the specified level, the function returns 0.
 *
 * @param L The Lua state.
 * @param level The stack level to retrieve. Level 0 is the current running
 *              function, level 1 is the function that called the current
 *              function, and so on.
 * @param ar A pointer to a `lua_Debug` structure that will be filled with
 *           information about the stack frame if the level is valid.
 * @return 1 if the specified level is valid and a frame is found, 0 otherwise.
 */
LUA_API int lua_getstack (lua_State *L, int level, lua_Debug *ar) {
  int status;
  CallInfo *ci;
  if (level < 0) return 0;  /* invalid (negative) level */
  lua_lock(L);
  for (ci = L->ci; level > 0 && ci != &L->base_ci; ci = ci->previous)
    level--;
  if (level == 0 && ci != &L->base_ci) {  /* level found? */
    status = 1;
    ar->i_ci = ci;
  }
  else status = 0;  /* no such level */
  lua_unlock(L);
  return status;
}


/**
 * Retrieves the name of an upvalue from a given Proto structure based on the provided index.
 *
 * This function takes a Proto structure `p` and an integer `uv` representing the index of the upvalue.
 * It checks if the index is within the valid range of upvalues in the Proto structure. If the index is valid,
 * it retrieves the name of the upvalue. If the name is NULL, it returns "?" as a placeholder. Otherwise, it returns
 * the actual name of the upvalue as a C string.
 *
 * @param p Pointer to the Proto structure containing the upvalues.
 * @param uv The index of the upvalue whose name is to be retrieved.
 * @return A C string representing the name of the upvalue, or "?" if the name is NULL.
 */
static const char *upvalname (Proto *p, int uv) {
  TString *s = check_exp(uv < p->sizeupvalues, p->upvalues[uv].name);
  if (s == NULL) return "?";
  else return getstr(s);
}


/**
 * Finds the nth vararg in the given CallInfo and sets the position pointer.
 *
 * This function calculates the position of the nth vararg in the call stack
 * based on the provided CallInfo (`ci`). It first determines the number of
 * fixed parameters (`nparams`) for the function associated with the CallInfo.
 * If `n` is greater than or equal to the number of varargs available (i.e., the
 * difference between the base of the local stack and the function pointer, minus
 * the number of fixed parameters), the function returns `NULL` to indicate that
 * no such vararg exists. Otherwise, it sets the position pointer (`pos`) to the
 * location of the nth vararg in the stack and returns a generic name for the
 * vararg, "(*vararg)".
 *
 * @param ci Pointer to the CallInfo structure representing the current call.
 * @param n The index of the vararg to find (0-based).
 * @param pos Pointer to a StkId where the position of the nth vararg will be stored.
 * @return A generic name for the vararg ("(*vararg)") if found, or `NULL` if the
 *         nth vararg does not exist.
 */
static const char *findvararg (CallInfo *ci, int n, StkId *pos) {
  int nparams = clLvalue(ci->func)->p->numparams;
  if (n >= cast_int(ci->u.l.base - ci->func) - nparams)
    return NULL;  /* no such vararg */
  else {
    *pos = ci->func + nparams + n;
    return "(*vararg)";  /* generic name for any vararg */
  }
}


/**
 * Finds the name and position of a local variable or temporary value in the Lua stack.
 *
 * This function searches for the nth local variable or temporary value in the call frame
 * represented by `ci`. It retrieves the name of the variable and its position on the stack.
 *
 * @param L The Lua state.
 * @param ci The call info structure representing the current call frame.
 * @param n The index of the local variable or temporary value to find. If `n` is negative,
 *          it represents the nth vararg value.
 * @param pos A pointer to store the position of the variable on the stack.
 *
 * @return The name of the local variable or temporary value. If the variable is unnamed,
 *         a generic name "(*temporary)" is returned. If the index `n` is invalid or the
 *         variable does not exist, NULL is returned.
 */
static const char *findlocal (lua_State *L, CallInfo *ci, int n,
                              StkId *pos) {
  const char *name = NULL;
  StkId base;
  if (isLua(ci)) {
    if (n < 0)  /* access to vararg values? */
      return findvararg(ci, -n, pos);
    else {
      base = ci->u.l.base;
      name = luaF_getlocalname(ci_func(ci)->p, n, currentpc(ci));
    }
  }
  else
    base = ci->func + 1;
  if (name == NULL) {  /* no 'standard' name? */
    StkId limit = (ci == L->ci) ? L->top : ci->next->func;
    if (limit - base >= n && n > 0)  /* is 'n' inside 'ci' stack? */
      name = "(*temporary)";  /* generic name for any valid slot */
    else
      return NULL;  /* no name */
  }
  *pos = base + (n - 1);
  return name;
}


/**
 * Retrieves the name and value of a local variable in a given function or activation record.
 *
 * This function is used to inspect local variables of a Lua function, either in the current
 * activation record or in a non-active function. The function takes a Lua state (`L`), a debug
 * information structure (`ar`), and an integer (`n`) representing the index of the local variable.
 *
 * If `ar` is `NULL`, the function inspects the local variables of the function at the top of the
 * Lua stack. This is typically used for non-active functions (e.g., functions not currently being
 * executed). The function checks if the top of the stack is a Lua function and retrieves the name
 * of the `n`-th local variable (usually a parameter) at the start of the function.
 *
 * If `ar` is not `NULL`, the function inspects the local variables of the active function
 * corresponding to the activation record provided by `ar`. It retrieves the name and value of the
 * `n`-th local variable and pushes the value onto the Lua stack if the variable is found.
 *
 * @param L The Lua state.
 * @param ar A pointer to a `lua_Debug` structure containing debug information about the function.
 *           If `NULL`, the function inspects the top of the stack.
 * @param n The index of the local variable to retrieve (1-based).
 * @return The name of the local variable if found, or `NULL` if the variable does not exist or
 *         the function is not a Lua function.
 */
LUA_API const char *lua_getlocal (lua_State *L, const lua_Debug *ar, int n) {
  const char *name;
  lua_lock(L);
  swapextra(L);
  if (ar == NULL) {  /* information about non-active function? */
    if (!isLfunction(L->top - 1))  /* not a Lua function? */
      name = NULL;
    else  /* consider live variables at function start (parameters) */
      name = luaF_getlocalname(clLvalue(L->top - 1)->p, n, 0);
  }
  else {  /* active function; get information through 'ar' */
    StkId pos = NULL;  /* to avoid warnings */
    name = findlocal(L, ar->i_ci, n, &pos);
    if (name) {
      setobj2s(L, L->top, pos);
      api_incr_top(L);
    }
  }
  swapextra(L);
  lua_unlock(L);
  return name;
}


/**
 * Sets the value of a local variable in a Lua function.
 *
 * This function sets the value of the nth local variable in the active function
 * associated with the given call information (`ar`). The new value is taken from
 * the top of the Lua stack. If the local variable exists, its value is updated,
 * and the value is popped from the stack. If the local variable does not exist,
 * the function does nothing and the stack remains unchanged.
 *
 * @param L The Lua state.
 * @param ar Pointer to a `lua_Debug` structure containing call information.
 * @param n The index of the local variable to set (1-based).
 * @return The name of the local variable if it exists and was successfully set,
 *         or NULL if the local variable does not exist.
 */
LUA_API const char *lua_setlocal (lua_State *L, const lua_Debug *ar, int n) {
  StkId pos = NULL;  /* to avoid warnings */
  const char *name;
  lua_lock(L);
  swapextra(L);
  name = findlocal(L, ar->i_ci, n, &pos);
  if (name) {
    setobjs2s(L, pos, L->top - 1);
    L->top--;  /* pop value */
  }
  swapextra(L);
  lua_unlock(L);
  return name;
}


/**
 * Populates a `lua_Debug` structure with information about a given Lua closure.
 *
 * This function extracts and sets various fields in the `lua_Debug` structure (`ar`) 
 * based on the provided closure (`cl`). If the closure is a C closure (not a Lua closure), 
 * the function sets the `source` field to `"=[C]"`, the `linedefined` and `lastlinedefined` 
 * fields to `-1`, and the `what` field to `"C"`. 
 *
 * If the closure is a Lua closure, the function retrieves the associated `Proto` structure 
 * and sets the `source`, `linedefined`, `lastlinedefined`, and `what` fields based on the 
 * information in the `Proto` structure. The `what` field is set to `"main"` if the function 
 * is the main chunk (i.e., `linedefined` is 0), otherwise it is set to `"Lua"`.
 *
 * Finally, the function generates a shortened version of the source path and stores it 
 * in the `short_src` field of the `lua_Debug` structure using `luaO_chunkid`.
 *
 * @param ar Pointer to the `lua_Debug` structure to be populated.
 * @param cl Pointer to the closure (`Closure`) from which to extract information.
 */
static void funcinfo (lua_Debug *ar, Closure *cl) {
  if (noLuaClosure(cl)) {
    ar->source = "=[C]";
    ar->linedefined = -1;
    ar->lastlinedefined = -1;
    ar->what = "C";
  }
  else {
    Proto *p = cl->l.p;
    ar->source = p->source ? getstr(p->source) : "=?";
    ar->linedefined = p->linedefined;
    ar->lastlinedefined = p->lastlinedefined;
    ar->what = (ar->linedefined == 0) ? "main" : "Lua";
  }
  luaO_chunkid(ar->short_src, ar->source, LUA_IDSIZE);
}


/**
 * Collects valid lines from a Lua closure and stores them in a new table.
 *
 * This function processes the given Lua closure `f` to identify and collect all
 * valid lines of code. If the closure is not a Lua closure (i.e., it is a C closure),
 * the function pushes `nil` onto the Lua stack. Otherwise, it creates a new table
 * and populates it with all valid lines of code from the closure's line information.
 * Each line number is used as a key in the table, and the corresponding value is
 * set to `true`. The resulting table is then pushed onto the Lua stack.
 *
 * @param L The Lua state.
 * @param f The closure to process. If it is not a Lua closure, `nil` is pushed
 *          onto the stack.
 */
static void collectvalidlines (lua_State *L, Closure *f) {
  if (noLuaClosure(f)) {
    setnilvalue(L->top);
    api_incr_top(L);
  }
  else {
    int i;
    TValue v;
    int *lineinfo = f->l.p->lineinfo;
    Table *t = luaH_new(L);  /* new table to store active lines */
    sethvalue(L, L->top, t);  /* push it on stack */
    api_incr_top(L);
    setbvalue(&v, 1);  /* boolean 'true' to be the value of all indices */
    for (i = 0; i < f->l.p->sizelineinfo; i++)  /* for all lines with code */
      luaH_setint(L, t, lineinfo[i], &v);  /* table[line] = true */
  }
}


/**
 * Retrieves the name of the function associated with the given call info (`ci`) in the Lua state (`L`).
 * 
 * This function determines the name of the function based on the call status and the type of the function.
 * If the call info (`ci`) is `NULL`, the function returns `NULL` indicating no information is available.
 * If the call info represents a finalizer (indicated by `CIST_FIN`), the function sets the `name` parameter to `"__gc"`
 * and returns `"metamethod"` to indicate that the function is a metamethod.
 * If the calling function is a known Lua function and not a tail call (indicated by `CIST_TAIL`), the function retrieves
 * the name from the previous call info using `funcnamefromcode`.
 * If none of the above conditions are met, the function returns `NULL`, indicating that the function name could not be determined.
 *
 * @param L The Lua state.
 * @param ci The call info structure containing information about the current call.
 * @param name A pointer to store the retrieved function name.
 * @return A string describing the type of function (e.g., `"metamethod"`) or `NULL` if the function name could not be determined.
 */
static const char *getfuncname (lua_State *L, CallInfo *ci, const char **name) {
  if (ci == NULL)  /* no 'ci'? */
    return NULL;  /* no info */
  else if (ci->callstatus & CIST_FIN) {  /* is this a finalizer? */
    *name = "__gc";
    return "metamethod";  /* report it as such */
  }
  /* calling function is a known Lua function? */
  else if (!(ci->callstatus & CIST_TAIL) && isLua(ci->previous))
    return funcnamefromcode(L, ci->previous, name);
  else return NULL;  /* no way to find a name */
}


/**
 * @brief Retrieves debugging information for a function or call frame.
 *
 * This method populates a `lua_Debug` structure with information about a function or call frame
 * based on the specified query string `what`. The query string consists of one or more characters,
 * each representing a specific piece of information to retrieve. The method processes each character
 * in the query string and updates the `lua_Debug` structure accordingly.
 *
 * @param L The Lua state.
 * @param what A string specifying the information to retrieve. Each character in the string
 *             corresponds to a specific piece of information:
 *             - 'S': Retrieves function information (e.g., source file, line defined).
 *             - 'l': Retrieves the current line number of the function or call frame.
 *             - 'u': Retrieves the number of upvalues, whether the function is vararg, and the number of parameters.
 *             - 't': Retrieves whether the function is in a tail call.
 *             - 'n': Retrieves the name of the function and a description of what the name represents.
 *             - 'L' and 'f': These options are handled by `lua_getinfo` and are ignored here.
 * @param ar A pointer to a `lua_Debug` structure to be filled with the requested information.
 * @param f A pointer to the closure (function) being inspected.
 * @param ci A pointer to the call info structure representing the call frame being inspected.
 * @return Returns 1 if all query characters were valid and processed successfully, otherwise returns 0
 *         if an invalid query character was encountered.
 */
static int auxgetinfo (lua_State *L, const char *what, lua_Debug *ar,
                       Closure *f, CallInfo *ci) {
  int status = 1;
  for (; *what; what++) {
    switch (*what) {
      case 'S': {
        funcinfo(ar, f);
        break;
      }
      case 'l': {
        ar->currentline = (ci && isLua(ci)) ? currentline(ci) : -1;
        break;
      }
      case 'u': {
        ar->nups = (f == NULL) ? 0 : f->c.nupvalues;
        if (noLuaClosure(f)) {
          ar->isvararg = 1;
          ar->nparams = 0;
        }
        else {
          ar->isvararg = f->l.p->is_vararg;
          ar->nparams = f->l.p->numparams;
        }
        break;
      }
      case 't': {
        ar->istailcall = (ci) ? ci->callstatus & CIST_TAIL : 0;
        break;
      }
      case 'n': {
        ar->namewhat = getfuncname(L, ci, &ar->name);
        if (ar->namewhat == NULL) {
          ar->namewhat = "";  /* not found */
          ar->name = NULL;
        }
        break;
      }
      case 'L':
      case 'f':  /* handled by lua_getinfo */
        break;
      default: status = 0;  /* invalid option */
    }
  }
  return status;
}


/**
 * Retrieves information about a function or activation record and stores it in a `lua_Debug` structure.
 *
 * This function is used to inspect the state of a Lua function or call stack. The `what` string specifies
 * what information to retrieve, and the results are stored in the `ar` structure. The `what` string can
 * contain the following characters:
 * - 'n': fills the `name` and `namewhat` fields in `ar`.
 * - 'f': pushes the function itself onto the stack.
 * - 'S': fills the `source`, `linedefined`, `lastlinedefined`, and `what` fields in `ar`.
 * - 'l': fills the `currentline` field in `ar`.
 * - 'u': fills the `nups`, `nparams`, and `isvararg` fields in `ar`.
 * - 't': fills the `istailcall` field in `ar`.
 * - 'L': fills the `activelines` field in `ar`.
 *
 * If the `what` string starts with '>', the function expects the function to be inspected to be on the top
 * of the stack. Otherwise, it uses the activation record provided in `ar`.
 *
 * @param L The Lua state.
 * @param what A string specifying what information to retrieve.
 * @param ar A pointer to a `lua_Debug` structure where the information will be stored.
 * @return 1 if successful, 0 otherwise.
 */
LUA_API int lua_getinfo (lua_State *L, const char *what, lua_Debug *ar) {
  int status;
  Closure *cl;
  CallInfo *ci;
  StkId func;
  lua_lock(L);
  swapextra(L);
  if (*what == '>') {
    ci = NULL;
    func = L->top - 1;
    api_check(L, ttisfunction(func), "function expected");
    what++;  /* skip the '>' */
    L->top--;  /* pop function */
  }
  else {
    ci = ar->i_ci;
    func = ci->func;
    lua_assert(ttisfunction(ci->func));
  }
  cl = ttisclosure(func) ? clvalue(func) : NULL;
  status = auxgetinfo(L, what, ar, cl, ci);
  if (strchr(what, 'f')) {
    setobjs2s(L, L->top, func);
    api_incr_top(L);
  }
  swapextra(L);  /* correct before option 'L', which can raise a mem. error */
  if (strchr(what, 'L'))
    collectvalidlines(L, cl);
  lua_unlock(L);
  return status;
}


/*
** {======================================================
** Symbolic Execution
** =======================================================
*/

static const char *getobjname (Proto *p, int lastpc, int reg,
                               const char **name);


/*
** find a "name" for the RK value 'c'
*/
static void kname (Proto *p, int pc, int c, const char **name) {
  if (ISK(c)) {  /* is 'c' a constant? */
    TValue *kvalue = &p->k[INDEXK(c)];
    if (ttisstring(kvalue)) {  /* literal constant? */
      *name = svalue(kvalue);  /* it is its own name */
      return;
    }
    /* else no reasonable name found */
  }
  else {  /* 'c' is a register */
    const char *what = getobjname(p, pc, c, name); /* search for 'c' */
    if (what && *what == 'c') {  /* found a constant name? */
      return;  /* 'name' already filled */
    }
    /* else no reasonable name found */
  }
  *name = "?";  /* no reasonable name found */
}


/**
 * Filters the program counter (pc) based on the jump target (jmptarget).
 * 
 * This method determines whether the current program counter (pc) is inside a conditional jump 
 * by comparing it with the jump target (jmptarget). If the pc is less than the jmptarget, 
 * it indicates that the code is inside a conditional jump, and the method returns -1, 
 * signifying that it cannot determine which instruction sets the register. 
 * Otherwise, it returns the current pc, indicating that the current position sets the register.
 *
 * @param pc The current program counter value.
 * @param jmptarget The target address of the jump instruction.
 * @return -1 if the pc is inside a conditional jump, otherwise returns the current pc.
 */
static int filterpc (int pc, int jmptarget) {
  if (pc < jmptarget)  /* is code conditional (inside a jump)? */
    return -1;  /* cannot know who sets that register */
  else return pc;  /* current position sets that register */
}


/*
** try to find last instruction before 'lastpc' that modified register 'reg'
*/
static int findsetreg (Proto *p, int lastpc, int reg) {
  int pc;
  int setreg = -1;  /* keep last instruction that changed 'reg' */
  int jmptarget = 0;  /* any code before this address is conditional */
  for (pc = 0; pc < lastpc; pc++) {
    Instruction i = p->code[pc];
    OpCode op = GET_OPCODE(i);
    int a = GETARG_A(i);
    switch (op) {
      case OP_LOADNIL: {
        int b = GETARG_B(i);
        if (a <= reg && reg <= a + b)  /* set registers from 'a' to 'a+b' */
          setreg = filterpc(pc, jmptarget);
        break;
      }
      case OP_TFORCALL: {
        if (reg >= a + 2)  /* affect all regs above its base */
          setreg = filterpc(pc, jmptarget);
        break;
      }
      case OP_CALL:
      case OP_TAILCALL: {
        if (reg >= a)  /* affect all registers above base */
          setreg = filterpc(pc, jmptarget);
        break;
      }
      case OP_JMP: {
        int b = GETARG_sBx(i);
        int dest = pc + 1 + b;
        /* jump is forward and do not skip 'lastpc'? */
        if (pc < dest && dest <= lastpc) {
          if (dest > jmptarget)
            jmptarget = dest;  /* update 'jmptarget' */
        }
        break;
      }
      default:
        if (testAMode(op) && reg == a)  /* any instruction that set A */
          setreg = filterpc(pc, jmptarget);
        break;
    }
  }
  return setreg;
}


/**
 * Retrieves the name and type of an object referenced by a register in a Lua function's prototype.
 *
 * This function attempts to determine the name and type of an object referenced by a given register
 * in a Lua function's bytecode. It first checks if the register corresponds to a local variable. If not,
 * it performs symbolic execution to trace the register's value back to its source, such as a global
 * variable, upvalue, constant, or field in a table.
 *
 * @param p The Proto structure representing the Lua function's prototype.
 * @param lastpc The program counter (PC) of the last instruction executed in the function.
 * @param reg The register number for which to retrieve the object's name and type.
 * @param name A pointer to a string that will be set to the name of the object, if found.
 * @return A string describing the type of the object ("local", "global", "upvalue", "constant", "field", "method"),
 *         or NULL if the object's name and type could not be determined.
 */
static const char *getobjname (Proto *p, int lastpc, int reg,
                               const char **name) {
  int pc;
  *name = luaF_getlocalname(p, reg + 1, lastpc);
  if (*name)  /* is a local? */
    return "local";
  /* else try symbolic execution */
  pc = findsetreg(p, lastpc, reg);
  if (pc != -1) {  /* could find instruction? */
    Instruction i = p->code[pc];
    OpCode op = GET_OPCODE(i);
    switch (op) {
      case OP_MOVE: {
        int b = GETARG_B(i);  /* move from 'b' to 'a' */
        if (b < GETARG_A(i))
          return getobjname(p, pc, b, name);  /* get name for 'b' */
        break;
      }
      case OP_GETTABUP:
      case OP_GETTABLE: {
        int k = GETARG_C(i);  /* key index */
        int t = GETARG_B(i);  /* table index */
        const char *vn = (op == OP_GETTABLE)  /* name of indexed variable */
                         ? luaF_getlocalname(p, t + 1, pc)
                         : upvalname(p, t);
        kname(p, pc, k, name);
        return (vn && strcmp(vn, LUA_ENV) == 0) ? "global" : "field";
      }
      case OP_GETUPVAL: {
        *name = upvalname(p, GETARG_B(i));
        return "upvalue";
      }
      case OP_LOADK:
      case OP_LOADKX: {
        int b = (op == OP_LOADK) ? GETARG_Bx(i)
                                 : GETARG_Ax(p->code[pc + 1]);
        if (ttisstring(&p->k[b])) {
          *name = svalue(&p->k[b]);
          return "constant";
        }
        break;
      }
      case OP_SELF: {
        int k = GETARG_C(i);  /* key index */
        kname(p, pc, k, name);
        return "method";
      }
      default: break;  /* go through to return NULL */
    }
  }
  return NULL;  /* could not find reasonable name */
}


/*
** Try to find a name for a function based on the code that called it.
** (Only works when function was called by a Lua function.)
** Returns what the name is (e.g., "for iterator", "method",
** "metamethod") and sets '*name' to point to the name.
*/
static const char *funcnamefromcode (lua_State *L, CallInfo *ci,
                                     const char **name) {
  TMS tm = (TMS)0;  /* (initial value avoids warnings) */
  Proto *p = ci_func(ci)->p;  /* calling function */
  int pc = currentpc(ci);  /* calling instruction index */
  Instruction i = p->code[pc];  /* calling instruction */
  if (ci->callstatus & CIST_HOOKED) {  /* was it called inside a hook? */
    *name = "?";
    return "hook";
  }
  switch (GET_OPCODE(i)) {
    case OP_CALL:
    case OP_TAILCALL:
      return getobjname(p, pc, GETARG_A(i), name);  /* get function name */
    case OP_TFORCALL: {  /* for iterator */
      *name = "for iterator";
       return "for iterator";
    }
    /* other instructions can do calls through metamethods */
    case OP_SELF: case OP_GETTABUP: case OP_GETTABLE:
      tm = TM_INDEX;
      break;
    case OP_SETTABUP: case OP_SETTABLE:
      tm = TM_NEWINDEX;
      break;
    case OP_ADD: case OP_SUB: case OP_MUL: case OP_MOD:
    case OP_POW: case OP_DIV: case OP_IDIV: case OP_BAND:
    case OP_BOR: case OP_BXOR: case OP_SHL: case OP_SHR: {
      int offset = cast_int(GET_OPCODE(i)) - cast_int(OP_ADD);  /* ORDER OP */
      tm = cast(TMS, offset + cast_int(TM_ADD));  /* ORDER TM */
      break;
    }
    case OP_UNM: tm = TM_UNM; break;
    case OP_BNOT: tm = TM_BNOT; break;
    case OP_LEN: tm = TM_LEN; break;
    case OP_CONCAT: tm = TM_CONCAT; break;
    case OP_EQ: tm = TM_EQ; break;
    case OP_LT: tm = TM_LT; break;
    case OP_LE: tm = TM_LE; break;
    default:
      return NULL;  /* cannot find a reasonable name */
  }
  *name = getstr(G(L)->tmname[tm]);
  return "metamethod";
}

/* }====================================================== */



/*
** The subtraction of two potentially unrelated pointers is
** not ISO C, but it should not crash a program; the subsequent
** checks are ISO C and ensure a correct result.
*/
static int isinstack (CallInfo *ci, const TValue *o) {
  ptrdiff_t i = o - ci->u.l.base;
  return (0 <= i && i < (ci->top - ci->u.l.base) && ci->u.l.base + i == o);
}


/*
** Checks whether value 'o' came from an upvalue. (That can only happen
** with instructions OP_GETTABUP/OP_SETTABUP, which operate directly on
** upvalues.)
*/
static const char *getupvalname (CallInfo *ci, const TValue *o,
                                 const char **name) {
  LClosure *c = ci_func(ci);
  int i;
  for (i = 0; i < c->nupvalues; i++) {
    if (c->upvals[i]->v == o) {
      *name = upvalname(c->p, i);
      return "upvalue";
    }
  }
  return NULL;
}


/**
 * Retrieves and formats information about a given Lua value (`o`) in the context of the current Lua state (`L`).
 * The method determines the kind and name of the value, which can be an upvalue or a register in the current Lua function.
 * 
 * @param L The Lua state in which the value is being inspected.
 * @param o A pointer to the Lua value (`TValue`) for which information is being retrieved.
 * 
 * @return A formatted string describing the value, including its kind and name (if available). 
 *         The string is formatted as " (kind 'name')". If no information is found, an empty string is returned.
 * 
 * The method first checks if the value is an upvalue by calling `getupvalname`. If it is not an upvalue, 
 * it then checks if the value is a register in the current stack frame by calling `getobjname`. 
 * The result is formatted using `luaO_pushfstring` and returned as a string.
 */
static const char *varinfo (lua_State *L, const TValue *o) {
  const char *name = NULL;  /* to avoid warnings */
  CallInfo *ci = L->ci;
  const char *kind = NULL;
  if (isLua(ci)) {
    kind = getupvalname(ci, o, &name);  /* check whether 'o' is an upvalue */
    if (!kind && isinstack(ci, o))  /* no? try a register */
      kind = getobjname(ci_func(ci)->p, currentpc(ci),
                        cast_int(o - ci->u.l.base), &name);
  }
  return (kind) ? luaO_pushfstring(L, " (%s '%s')", kind, name) : "";
}


/**
 * Raises a type error in the Lua interpreter.
 *
 * This function is used to generate and throw a type error when an operation is attempted
 * on a value of an inappropriate type. It constructs an error message indicating the
 * attempted operation (`op`) and the type of the value (`o`), and then raises the error
 * using `luaG_runerror`.
 *
 * @param L The Lua state.
 * @param o The value that caused the type error.
 * @param op The operation that was attempted (e.g., "add", "concatenate").
 *
 * @note The function does not return; it raises an error and terminates the current
 *       execution context.
 */
l_noret luaG_typeerror (lua_State *L, const TValue *o, const char *op) {
  const char *t = luaT_objtypename(L, o);
  luaG_runerror(L, "attempt to %s a %s value%s", op, t, varinfo(L, o));
}


/**
 * Generates a type error for concatenation operations in Lua.
 *
 * This function is called when an attempt is made to concatenate two values
 * (p1 and p2) that are not compatible for concatenation. The function first
 * checks if the first value (p1) is a string or can be converted to a string.
 * If so, it sets p1 to the second value (p2). Then, it calls `luaG_typeerror`
 * to raise a type error indicating that the value in p1 cannot be concatenated.
 *
 * @param L The Lua state.
 * @param p1 The first value involved in the concatenation.
 * @param p2 The second value involved in the concatenation.
 * @return This function does not return (noreturn) as it raises an error.
 */
l_noret luaG_concaterror (lua_State *L, const TValue *p1, const TValue *p2) {
  if (ttisstring(p1) || cvt2str(p1)) p1 = p2;
  luaG_typeerror(L, p1, "concatenate");
}


/**
 * Raises a type error when an operation between two values fails due to incompatible types.
 *
 * This function is used to handle errors in Lua operations where the operands are of incompatible
 * types. It first attempts to convert the first operand `p1` to a number. If this conversion fails,
 * it assumes that the first operand is the one causing the type error and sets `p2` to `p1`. 
 * Otherwise, it assumes the second operand `p2` is the one causing the error. Finally, it raises a 
 * type error using `luaG_typeerror` with the provided error message `msg`.
 *
 * @param L The Lua state.
 * @param p1 The first operand involved in the operation.
 * @param p2 The second operand involved in the operation.
 * @param msg The error message to be displayed when the type error is raised.
 * @return This function does not return (marked with `l_noret`), as it raises an error.
 */
l_noret luaG_opinterror (lua_State *L, const TValue *p1,
                         const TValue *p2, const char *msg) {
  lua_Number temp;
  if (!tonumber(p1, &temp))  /* first operand is wrong? */
    p2 = p1;  /* now second is wrong */
  luaG_typeerror(L, p2, msg);
}


/*
** Error when both values are convertible to numbers, but not to integers
*/
l_noret luaG_tointerror (lua_State *L, const TValue *p1, const TValue *p2) {
  lua_Integer temp;
  if (!tointeger(p1, &temp))
    p2 = p1;
  luaG_runerror(L, "number%s has no integer representation", varinfo(L, p2));
}


/**
 * Raises an error when an attempt is made to compare two values of incompatible types.
 * This function is used to enforce type consistency in comparison operations within Lua.
 * It retrieves the type names of the two values and constructs an appropriate error message
 * based on whether the types are the same or different. If the types are the same, it indicates
 * that the comparison is invalid for that type. If the types are different, it highlights the
 * incompatibility between the two types.
 *
 * @param L The Lua state in which the error is raised.
 * @param p1 The first value involved in the comparison.
 * @param p2 The second value involved in the comparison.
 * @noreturn This function does not return; it raises an error in the Lua state.
 */
l_noret luaG_ordererror (lua_State *L, const TValue *p1, const TValue *p2) {
  const char *t1 = luaT_objtypename(L, p1);
  const char *t2 = luaT_objtypename(L, p2);
  if (strcmp(t1, t2) == 0)
    luaG_runerror(L, "attempt to compare two %s values", t1);
  else
    luaG_runerror(L, "attempt to compare %s with %s", t1, t2);
}


/* add src:line information to 'msg' */
const char *luaG_addinfo (lua_State *L, const char *msg, TString *src,
                                        int line) {
  char buff[LUA_IDSIZE];
  if (src)
    luaO_chunkid(buff, getstr(src), LUA_IDSIZE);
  else {  /* no source available; use "?" instead */
    buff[0] = '?'; buff[1] = '\0';
  }
  return luaO_pushfstring(L, "%s:%d: %s", buff, line, msg);
}


/**
 * Handles and propagates error messages within the Lua state.
 *
 * This function is responsible for managing error handling in the Lua interpreter.
 * If an error handling function (`errfunc`) is registered in the Lua state (`L`),
 * this function will call it with the error message as an argument. The error handling
 * function is expected to be on the stack, and its position is determined by `L->errfunc`.
 *
 * The function performs the following steps:
 * 1. Checks if an error handling function is available (`L->errfunc != 0`).
 * 2. Restores the error handling function from the stack using `restorestack`.
 * 3. Moves the error message (the top element on the stack) to the position of the argument.
 * 4. Pushes the error handling function onto the stack.
 * 5. Calls the error handling function using `luaD_callnoyield`.
 * 6. If no error handling function is available, or after calling it, the function
 *    throws a runtime error using `luaD_throw` with the error code `LUA_ERRRUN`.
 *
 * @param L Pointer to the Lua state.
 * @return This function does not return; it either calls the error handler or throws an error.
 */
l_noret luaG_errormsg (lua_State *L) {
  if (L->errfunc != 0) {  /* is there an error handling function? */
    StkId errfunc = restorestack(L, L->errfunc);
    setobjs2s(L, L->top, L->top - 1);  /* move argument */
    setobjs2s(L, L->top - 1, errfunc);  /* push function */
    L->top++;  /* assume EXTRA_STACK */
    luaD_callnoyield(L, L->top - 2, 1);  /* call it */
  }
  luaD_throw(L, LUA_ERRRUN);
}


/**
 * @brief Raises a runtime error in the Lua interpreter.
 *
 * This function formats an error message using the provided format string `fmt` and
 * additional arguments, similar to `printf`. The formatted message is then pushed onto
 * the Lua stack. If the error occurs within a Lua function, the function appends
 * source file and line number information to the error message. Finally, the function
 * triggers the error handling mechanism in the Lua interpreter, which typically results
 * in the termination of the current execution and the propagation of the error.
 *
 * @param L Pointer to the Lua state.
 * @param fmt Format string for the error message.
 * @param ... Additional arguments to be formatted into the error message.
 * @return This function does not return; it triggers a Lua error.
 */
l_noret luaG_runerror (lua_State *L, const char *fmt, ...) {
  CallInfo *ci = L->ci;
  const char *msg;
  va_list argp;
  va_start(argp, fmt);
  msg = luaO_pushvfstring(L, fmt, argp);  /* format message */
  va_end(argp);
  if (isLua(ci))  /* if Lua function, add source:line information */
    luaG_addinfo(L, msg, ci_func(ci)->p->source, currentline(ci));
  luaG_errormsg(L);
}


/**
 * Executes Lua hook tracing based on the current state of the Lua interpreter.
 * This function is responsible for managing and invoking hooks for line execution
 * and instruction counting. It checks the hook mask and the current call info to
 * determine whether to trigger a hook. The function handles the following scenarios:
 *
 * 1. **Count Hook**: If the hook count reaches zero and the count hook is enabled,
 *    the count hook is invoked, and the hook count is reset.
 *
 * 2. **Line Hook**: If the line hook is enabled, the function checks if the current
 *    execution point has moved to a new line or function. If so, the line hook is invoked.
 *
 * 3. **Hook Yield**: If the Lua interpreter yielded during the last hook execution,
 *    the function marks the state and avoids re-invoking the hook until the next
 *    execution cycle.
 *
 * 4. **Yield Handling**: If the hook causes the Lua interpreter to yield, the function
 *    adjusts the hook count and execution state to ensure proper resumption.
 *
 * @param L Pointer to the Lua state, which contains the current execution context,
 *          hook mask, and hook count.
 */
void luaG_traceexec (lua_State *L) {
  CallInfo *ci = L->ci;
  lu_byte mask = L->hookmask;
  int counthook = (--L->hookcount == 0 && (mask & LUA_MASKCOUNT));
  if (counthook)
    resethookcount(L);  /* reset count */
  else if (!(mask & LUA_MASKLINE))
    return;  /* no line hook and count != 0; nothing to be done */
  if (ci->callstatus & CIST_HOOKYIELD) {  /* called hook last time? */
    ci->callstatus &= ~CIST_HOOKYIELD;  /* erase mark */
    return;  /* do not call hook again (VM yielded, so it did not move) */
  }
  if (counthook)
    luaD_hook(L, LUA_HOOKCOUNT, -1);  /* call count hook */
  if (mask & LUA_MASKLINE) {
    Proto *p = ci_func(ci)->p;
    int npc = pcRel(ci->u.l.savedpc, p);
    int newline = getfuncline(p, npc);
    if (npc == 0 ||  /* call linehook when enter a new function, */
        ci->u.l.savedpc <= L->oldpc ||  /* when jump back (loop), or when */
        newline != getfuncline(p, pcRel(L->oldpc, p)))  /* enter a new line */
      luaD_hook(L, LUA_HOOKLINE, newline);  /* call line hook */
  }
  L->oldpc = ci->u.l.savedpc;
  if (L->status == LUA_YIELD) {  /* did hook yield? */
    if (counthook)
      L->hookcount = 1;  /* undo decrement to zero */
    ci->u.l.savedpc--;  /* undo increment (resume will increment it again) */
    ci->callstatus |= CIST_HOOKYIELD;  /* mark that it yielded */
    ci->func = L->top - 1;  /* protect stack below results */
    luaD_throw(L, LUA_YIELD);
  }
}

