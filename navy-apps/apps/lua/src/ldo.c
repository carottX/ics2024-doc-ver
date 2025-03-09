/*
** $Id: ldo.c,v 2.157 2016/12/13 15:52:21 roberto Exp $
** Stack and Call structure of Lua
** See Copyright Notice in lua.h
*/

#define ldo_c
#define LUA_CORE

#include "lprefix.h"


#include <setjmp.h>
#include <stdlib.h>
#include <string.h>

#include "lua.h"

#include "lapi.h"
#include "ldebug.h"
#include "ldo.h"
#include "lfunc.h"
#include "lgc.h"
#include "lmem.h"
#include "lobject.h"
#include "lopcodes.h"
#include "lparser.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "ltm.h"
#include "lundump.h"
#include "lvm.h"
#include "lzio.h"



#define errorstatus(s)	((s) > LUA_YIELD)


/*
** {======================================================
** Error-recovery functions
** =======================================================
*/

/*
** LUAI_THROW/LUAI_TRY define how Lua does exception handling. By
** default, Lua handles errors with exceptions when compiling as
** C++ code, with _longjmp/_setjmp when asked to use them, and with
** longjmp/setjmp otherwise.
*/
#if !defined(LUAI_THROW)				/* { */

#if defined(__cplusplus) && !defined(LUA_USE_LONGJMP)	/* { */

/* C++ exceptions */
#define LUAI_THROW(L,c)		throw(c)
#define LUAI_TRY(L,c,a) \
	try { a } catch(...) { if ((c)->status == 0) (c)->status = -1; }
#define luai_jmpbuf		int  /* dummy variable */

#elif defined(LUA_USE_POSIX)				/* }{ */

/* in POSIX, try _longjmp/_setjmp (more efficient) */
#define LUAI_THROW(L,c)		_longjmp((c)->b, 1)
#define LUAI_TRY(L,c,a)		if (_setjmp((c)->b) == 0) { a }
#define luai_jmpbuf		jmp_buf

#else							/* }{ */

/* ISO C handling with long jumps */
#define LUAI_THROW(L,c)		longjmp((c)->b, 1)
#define LUAI_TRY(L,c,a)		if (setjmp((c)->b) == 0) { a }
#define luai_jmpbuf		jmp_buf

#endif							/* } */

#endif							/* } */



/* chain list of long jump buffers */
struct lua_longjmp {
  struct lua_longjmp *previous;
  luai_jmpbuf b;
  volatile int status;  /* error code */
};


/**
 * Sets the error object on the Lua stack based on the provided error code.
 *
 * This function handles different types of errors by setting an appropriate error message
 * on the Lua stack. The error message is placed at the position specified by `oldtop`.
 *
 * - For `LUA_ERRMEM` (memory error), it reuses a pre-registered memory error message.
 * - For `LUA_ERRERR` (error in error handling), it creates a new string literal with the
 *   corresponding error message.
 * - For all other error codes, it assumes that the error message is already on the top
 *   of the Lua stack and moves it to the position specified by `oldtop`.
 *
 * After setting the error message, the function adjusts the top of the Lua stack to
 * point to the position immediately after the error message.
 *
 * @param L The Lua state.
 * @param errcode The error code indicating the type of error.
 * @param oldtop The stack index where the error message should be placed.
 */
static void seterrorobj (lua_State *L, int errcode, StkId oldtop) {
  switch (errcode) {
    case LUA_ERRMEM: {  /* memory error? */
      setsvalue2s(L, oldtop, G(L)->memerrmsg); /* reuse preregistered msg. */
      break;
    }
    case LUA_ERRERR: {
      setsvalue2s(L, oldtop, luaS_newliteral(L, "error in error handling"));
      break;
    }
    default: {
      setobjs2s(L, oldtop, L->top - 1);  /* error message on current top */
      break;
    }
  }
  L->top = oldtop + 1;
}


/**
 * @brief Throws an error in the Lua state, handling it according to the available error handlers.
 *
 * This function is responsible for propagating errors within the Lua state. It first checks if the
 * current thread (Lua state) has an error handler (`errorJmp`). If an error handler is present, the
 * function sets the error status and jumps to the handler using `LUAI_THROW`. If no error handler
 * is available in the current thread, the function marks the thread as dead by setting its status
 * to the provided error code.
 *
 * If the main thread has an error handler, the error object is copied to the main thread's stack,
 * and the error is re-thrown in the main thread. If no error handler is available in the main thread
 * either, the function checks for a panic function (`g->panic`). If a panic function is present,
 * it prepares the error object, ensures the stack is in a valid state, and calls the panic function
 * as a last resort. If no panic function is available, the function aborts the program.
 *
 * @param L Pointer to the Lua state (`lua_State`) where the error is being thrown.
 * @param errcode The error code to be thrown.
 */
l_noret luaD_throw (lua_State *L, int errcode) {
  if (L->errorJmp) {  /* thread has an error handler? */
    L->errorJmp->status = errcode;  /* set status */
    LUAI_THROW(L, L->errorJmp);  /* jump to it */
  }
  else {  /* thread has no error handler */
    global_State *g = G(L);
    L->status = cast_byte(errcode);  /* mark it as dead */
    if (g->mainthread->errorJmp) {  /* main thread has a handler? */
      setobjs2s(L, g->mainthread->top++, L->top - 1);  /* copy error obj. */
      luaD_throw(g->mainthread, errcode);  /* re-throw in main thread */
    }
    else {  /* no handler at all; abort */
      if (g->panic) {  /* panic function? */
        seterrorobj(L, errcode, L->top);  /* assume EXTRA_STACK */
        if (L->ci->top < L->top)
          L->ci->top = L->top;  /* pushing msg. can break this invariant */
        lua_unlock(L);
        g->panic(L);  /* call panic function (last chance to jump out) */
      }
      abort();
    }
  }
}


/**
 * Executes a function in a protected environment, ensuring that any errors
 * encountered during execution are caught and handled without causing the
 * program to crash. This function sets up a new error handler, runs the
 * provided function, and then restores the previous error handler.
 *
 * @param L Pointer to the Lua state, which holds the execution context.
 * @param f Pointer to the function to be executed in a protected environment.
 * @param ud User-defined data to be passed to the function `f`.
 *
 * @return The status of the execution, which can be one of the following:
 *         - LUA_OK if the function executed successfully.
 *         - An error code if an error occurred during execution.
 *
 * The function temporarily replaces the current error handler with a new one,
 * allowing it to catch any errors that occur during the execution of `f`.
 * After execution, the original error handler is restored, and the status
 * of the execution is returned.
 */
int luaD_rawrunprotected (lua_State *L, Pfunc f, void *ud) {
  unsigned short oldnCcalls = L->nCcalls;
  struct lua_longjmp lj;
  lj.status = LUA_OK;
  lj.previous = L->errorJmp;  /* chain new error handler */
  L->errorJmp = &lj;
  LUAI_TRY(L, &lj,
    (*f)(L, ud);
  );
  L->errorJmp = lj.previous;  /* restore old error handler */
  L->nCcalls = oldnCcalls;
  return lj.status;
}

/* }====================================================== */


/*
** {==================================================================
** Stack reallocation
** ===================================================================
*/
static void correctstack (lua_State *L, TValue *oldstack) {
  CallInfo *ci;
  UpVal *up;
  L->top = (L->top - oldstack) + L->stack;
  for (up = L->openupval; up != NULL; up = up->u.open.next)
    up->v = (up->v - oldstack) + L->stack;
  for (ci = L->ci; ci != NULL; ci = ci->previous) {
    ci->top = (ci->top - oldstack) + L->stack;
    ci->func = (ci->func - oldstack) + L->stack;
    if (isLua(ci))
      ci->u.l.base = (ci->u.l.base - oldstack) + L->stack;
  }
}


/* some space for error handling */
#define ERRORSTACKSIZE	(LUAI_MAXSTACK + 200)


/**
 * Reallocates the stack of the Lua state `L` to a new size `newsize`.
 * 
 * This function adjusts the stack size of the Lua state `L` to `newsize`. It performs the following steps:
 * 1. Saves the current stack pointer `L->stack` in `oldstack`.
 * 2. Ensures that the new size `newsize` is within the allowed limits by asserting that it is either less than or equal to `LUAI_MAXSTACK` or equal to `ERRORSTACKSIZE`.
 * 3. Asserts that the current stack size calculation is correct by comparing `L->stack_last - L->stack` with `L->stacksize - EXTRA_STACK`.
 * 4. Reallocates the stack memory using `luaM_reallocvector`, updating `L->stack` and `L->stacksize` to the new size.
 * 5. Initializes any new stack elements beyond the previous size `lim` to `nil` using `setnilvalue`.
 * 6. Updates `L->stacksize` and `L->stack_last` to reflect the new stack size.
 * 7. Corrects any internal pointers in the Lua state that reference the old stack using `correctstack`.
 *
 * @param L The Lua state whose stack is to be reallocated.
 * @param newsize The new size of the stack.
 */
void luaD_reallocstack (lua_State *L, int newsize) {
  TValue *oldstack = L->stack;
  int lim = L->stacksize;
  lua_assert(newsize <= LUAI_MAXSTACK || newsize == ERRORSTACKSIZE);
  lua_assert(L->stack_last - L->stack == L->stacksize - EXTRA_STACK);
  luaM_reallocvector(L, L->stack, L->stacksize, newsize, TValue);
  for (; lim < newsize; lim++)
    setnilvalue(L->stack + lim); /* erase new segment */
  L->stacksize = newsize;
  L->stack_last = L->stack + newsize - EXTRA_STACK;
  correctstack(L, oldstack);
}


/**
 * Grows the Lua stack to accommodate additional elements.
 *
 * This function ensures that the stack has enough space to store `n` additional
 * elements. If the current stack size is already at or exceeds the maximum
 * allowed size (`LUAI_MAXSTACK`), an error is thrown. Otherwise, the stack is
 * resized to be at least twice its current size or large enough to hold the
 * required number of elements, whichever is larger. If the calculated new size
 * exceeds the maximum allowed size, the stack is resized to a smaller error
 * stack size, and a stack overflow error is raised.
 *
 * @param L Pointer to the Lua state.
 * @param n Number of additional elements to accommodate on the stack.
 */
void luaD_growstack (lua_State *L, int n) {
  int size = L->stacksize;
  if (size > LUAI_MAXSTACK)  /* error after extra size? */
    luaD_throw(L, LUA_ERRERR);
  else {
    int needed = cast_int(L->top - L->stack) + n + EXTRA_STACK;
    int newsize = 2 * size;
    if (newsize > LUAI_MAXSTACK) newsize = LUAI_MAXSTACK;
    if (newsize < needed) newsize = needed;
    if (newsize > LUAI_MAXSTACK) {  /* stack overflow? */
      luaD_reallocstack(L, ERRORSTACKSIZE);
      luaG_runerror(L, "stack overflow");
    }
    else
      luaD_reallocstack(L, newsize);
  }
}


/**
 * Calculates the number of stack slots currently in use in the Lua state.
 *
 * This function iterates through the call information (CallInfo) linked list
 * to determine the highest stack index (`lim`) that is currently in use.
 * It then computes the total number of stack slots in use by calculating the
 * difference between `lim` and the base of the stack (`L->stack`), adding 1
 * to account for 1-based indexing.
 *
 * @param L Pointer to the Lua state.
 * @return The number of stack slots currently in use as an integer.
 */
static int stackinuse (lua_State *L) {
  CallInfo *ci;
  StkId lim = L->top;
  for (ci = L->ci; ci != NULL; ci = ci->previous) {
    if (lim < ci->top) lim = ci->top;
  }
  lua_assert(lim <= L->stack_last);
  return cast_int(lim - L->stack) + 1;  /* part of stack in use */
}


/**
 * Shrinks the stack of the Lua state `L` to a more optimal size if possible.
 * 
 * This function calculates a "good size" for the stack based on the current usage (`inuse`),
 * which is defined as the current stack usage plus a small buffer (1/8th of the current usage)
 * and twice the `EXTRA_STACK` value. The calculated size is capped at `LUAI_MAXSTACK` to
 * respect the maximum stack limit.
 * 
 * If the stack size exceeds `LUAI_MAXSTACK`, it indicates that the stack had been handling
 * an overflow, and all CallInfo (CI) structures are freed. Otherwise, the CI list is shrunk.
 * 
 * The stack is reallocated to the "good size" only if the thread is not currently handling
 * a stack overflow and the calculated size is smaller than the current stack size. If these
 * conditions are not met, the stack remains unchanged.
 * 
 * @param L Pointer to the Lua state whose stack is to be shrunk.
 */
void luaD_shrinkstack (lua_State *L) {
  int inuse = stackinuse(L);
  int goodsize = inuse + (inuse / 8) + 2*EXTRA_STACK;
  if (goodsize > LUAI_MAXSTACK)
    goodsize = LUAI_MAXSTACK;  /* respect stack limit */
  if (L->stacksize > LUAI_MAXSTACK)  /* had been handling stack overflow? */
    luaE_freeCI(L);  /* free all CIs (list grew because of an error) */
  else
    luaE_shrinkCI(L);  /* shrink list */
  /* if thread is currently not handling a stack overflow and its
     good size is smaller than current size, shrink its stack */
  if (inuse <= (LUAI_MAXSTACK - EXTRA_STACK) &&
      goodsize < L->stacksize)
    luaD_reallocstack(L, goodsize);
  else  /* don't change stack */
    condmovestack(L,{},{});  /* (change only for debugging) */
}


/**
 * Increments the top of the Lua stack by one, ensuring there is enough space
 * on the stack to accommodate the new element. This function first checks
 * if the stack has at least one available slot by calling `luaD_checkstack`.
 * If the stack has sufficient space, the `top` pointer of the Lua state `L`
 * is incremented to point to the next available slot.
 *
 * @param L Pointer to the Lua state whose stack top is to be incremented.
 */
void luaD_inctop (lua_State *L) {
  luaD_checkstack(L, 1);
  L->top++;
}

/* }================================================================== */


/*
** Call a hook for the given event. Make sure there is a hook to be
** called. (Both 'L->hook' and 'L->hookmask', which triggers this
** function, can be changed asynchronously by signals.)
*/
void luaD_hook (lua_State *L, int event, int line) {
  lua_Hook hook = L->hook;
  if (hook && L->allowhook) {  /* make sure there is a hook */
    CallInfo *ci = L->ci;
    ptrdiff_t top = savestack(L, L->top);
    ptrdiff_t ci_top = savestack(L, ci->top);
    lua_Debug ar;
    ar.event = event;
    ar.currentline = line;
    ar.i_ci = ci;
    luaD_checkstack(L, LUA_MINSTACK);  /* ensure minimum stack size */
    ci->top = L->top + LUA_MINSTACK;
    lua_assert(ci->top <= L->stack_last);
    L->allowhook = 0;  /* cannot call hooks inside a hook */
    ci->callstatus |= CIST_HOOKED;
    lua_unlock(L);
    (*hook)(L, &ar);
    lua_lock(L);
    lua_assert(!L->allowhook);
    L->allowhook = 1;
    ci->top = restorestack(L, ci_top);
    L->top = restorestack(L, top);
    ci->callstatus &= ~CIST_HOOKED;
  }
}


/**
 * Invokes a hook for a Lua function call or tail call.
 *
 * This function is responsible for triggering a hook when a Lua function is called or when a tail call is detected.
 * It adjusts the saved program counter (pc) to ensure the hook operates on the correct instruction. If the previous
 * call information (ci->previous) is a Lua function and the last executed opcode is OP_TAILCALL, the hook type is
 * set to LUA_HOOKTAILCALL. Otherwise, the hook type is set to LUA_HOOKCALL. After invoking the hook via luaD_hook,
 * the saved pc is restored to its original value.
 *
 * @param L The Lua state.
 * @param ci The call information structure containing details about the current call.
 */
static void callhook (lua_State *L, CallInfo *ci) {
  int hook = LUA_HOOKCALL;
  ci->u.l.savedpc++;  /* hooks assume 'pc' is already incremented */
  if (isLua(ci->previous) &&
      GET_OPCODE(*(ci->previous->u.l.savedpc - 1)) == OP_TAILCALL) {
    ci->callstatus |= CIST_TAIL;
    hook = LUA_HOOKTAILCALL;
  }
  luaD_hook(L, hook, -1);
  ci->u.l.savedpc--;  /* correct 'pc' */
}


/**
 * Adjusts the stack to handle variable arguments in a Lua function call.
 * 
 * This function is used to adjust the stack when a Lua function is called with
 * a variable number of arguments. It ensures that the fixed parameters are
 * moved to their final positions on the stack, and any missing arguments are
 * filled with `nil`. The function also handles the case where the number of
 * actual arguments is less than the number of fixed parameters.
 *
 * @param L The Lua state.
 * @param p The function prototype, which contains information about the number
 *          of fixed parameters (`numparams`).
 * @param actual The number of actual arguments passed to the function.
 *
 * @return The base position on the stack where the adjusted arguments start.
 */
static StkId adjust_varargs (lua_State *L, Proto *p, int actual) {
  int i;
  int nfixargs = p->numparams;
  StkId base, fixed;
  /* move fixed parameters to final position */
  fixed = L->top - actual;  /* first fixed argument */
  base = L->top;  /* final position of first argument */
  for (i = 0; i < nfixargs && i < actual; i++) {
    setobjs2s(L, L->top++, fixed + i);
    setnilvalue(fixed + i);  /* erase original copy (for GC) */
  }
  for (; i < nfixargs; i++)
    setnilvalue(L->top++);  /* complete missing arguments */
  return base;
}


/*
** Check whether __call metafield of 'func' is a function. If so, put
** it in stack below original 'func' so that 'luaD_precall' can call
** it. Raise an error if __call metafield is not a function.
*/
static void tryfuncTM (lua_State *L, StkId func) {
  const TValue *tm = luaT_gettmbyobj(L, func, TM_CALL);
  StkId p;
  if (!ttisfunction(tm))
    luaG_typeerror(L, func, "call");
  /* Open a hole inside the stack at 'func' */
  for (p = L->top; p > func; p--)
    setobjs2s(L, p, p-1);
  L->top++;  /* slot ensured by caller */
  setobj2s(L, func, tm);  /* tag method is the new function to be called */
}


/*
** Given 'nres' results at 'firstResult', move 'wanted' of them to 'res'.
** Handle most typical cases (zero results for commands, one result for
** expressions, multiple results for tail calls/single parameters)
** separated.
*/
static int moveresults (lua_State *L, const TValue *firstResult, StkId res,
                                      int nres, int wanted) {
  switch (wanted) {  /* handle typical cases separately */
    case 0: break;  /* nothing to move */
    case 1: {  /* one result needed */
      if (nres == 0)   /* no results? */
        firstResult = luaO_nilobject;  /* adjust with nil */
      setobjs2s(L, res, firstResult);  /* move it to proper place */
      break;
    }
    case LUA_MULTRET: {
      int i;
      for (i = 0; i < nres; i++)  /* move all results to correct place */
        setobjs2s(L, res + i, firstResult + i);
      L->top = res + nres;
      return 0;  /* wanted == LUA_MULTRET */
    }
    default: {
      int i;
      if (wanted <= nres) {  /* enough results? */
        for (i = 0; i < wanted; i++)  /* move wanted results to correct place */
          setobjs2s(L, res + i, firstResult + i);
      }
      else {  /* not enough results; use all of them plus nils */
        for (i = 0; i < nres; i++)  /* move all results to correct place */
          setobjs2s(L, res + i, firstResult + i);
        for (; i < wanted; i++)  /* complete wanted number of results */
          setnilvalue(res + i);
      }
      break;
    }
  }
  L->top = res + wanted;  /* top points after the last result */
  return 1;
}


/*
** Finishes a function call: calls hook if necessary, removes CallInfo,
** moves current number of results to proper place; returns 0 iff call
** wanted multiple (variable number of) results.
*/
int luaD_poscall (lua_State *L, CallInfo *ci, StkId firstResult, int nres) {
  StkId res;
  int wanted = ci->nresults;
  if (L->hookmask & (LUA_MASKRET | LUA_MASKLINE)) {
    if (L->hookmask & LUA_MASKRET) {
      ptrdiff_t fr = savestack(L, firstResult);  /* hook may change stack */
      luaD_hook(L, LUA_HOOKRET, -1);
      firstResult = restorestack(L, fr);
    }
    L->oldpc = ci->previous->u.l.savedpc;  /* 'oldpc' for caller function */
  }
  res = ci->func;  /* res == final position of 1st result */
  L->ci = ci->previous;  /* back to caller */
  /* move results to proper place */
  return moveresults(L, firstResult, res, nres, wanted);
}



#define next_ci(L) (L->ci = (L->ci->next ? L->ci->next : luaE_extendCI(L)))


/* macro to check stack size, preserving 'p' */
#define checkstackp(L,n,p)  \
  luaD_checkstackaux(L, n, \
    ptrdiff_t t__ = savestack(L, p);  /* save 'p' */ \
    luaC_checkGC(L),  /* stack grow uses memory */ \
    p = restorestack(L, t__))  /* 'pos' part: restore 'p' */


/*
** Prepares a function call: checks the stack, creates a new CallInfo
** entry, fills in the relevant information, calls hook if needed.
** If function is a C function, does the call, too. (Otherwise, leave
** the execution ('luaV_execute') to the caller, to allow stackless
** calls.) Returns true iff function has been executed (C function).
*/
int luaD_precall (lua_State *L, StkId func, int nresults) {
  lua_CFunction f;
  CallInfo *ci;
  switch (ttype(func)) {
    case LUA_TCCL:  /* C closure */
      f = clCvalue(func)->f;
      goto Cfunc;
    case LUA_TLCF:  /* light C function */
      f = fvalue(func);
     Cfunc: {
      int n;  /* number of returns */
      checkstackp(L, LUA_MINSTACK, func);  /* ensure minimum stack size */
      ci = next_ci(L);  /* now 'enter' new function */
      ci->nresults = nresults;
      ci->func = func;
      ci->top = L->top + LUA_MINSTACK;
      lua_assert(ci->top <= L->stack_last);
      ci->callstatus = 0;
      if (L->hookmask & LUA_MASKCALL)
        luaD_hook(L, LUA_HOOKCALL, -1);
      lua_unlock(L);
      n = (*f)(L);  /* do the actual call */
      lua_lock(L);
      api_checknelems(L, n);
      luaD_poscall(L, ci, L->top - n, n);
      return 1;
    }
    case LUA_TLCL: {  /* Lua function: prepare its call */
      StkId base;
      Proto *p = clLvalue(func)->p;
      int n = cast_int(L->top - func) - 1;  /* number of real arguments */
      int fsize = p->maxstacksize;  /* frame size */
      checkstackp(L, fsize, func);
      if (p->is_vararg)
        base = adjust_varargs(L, p, n);
      else {  /* non vararg function */
        for (; n < p->numparams; n++)
          setnilvalue(L->top++);  /* complete missing arguments */
        base = func + 1;
      }
      ci = next_ci(L);  /* now 'enter' new function */
      ci->nresults = nresults;
      ci->func = func;
      ci->u.l.base = base;
      L->top = ci->top = base + fsize;
      lua_assert(ci->top <= L->stack_last);
      ci->u.l.savedpc = p->code;  /* starting point */
      ci->callstatus = CIST_LUA;
      if (L->hookmask & LUA_MASKCALL)
        callhook(L, ci);
      return 0;
    }
    default: {  /* not a function */
      checkstackp(L, 1, func);  /* ensure space for metamethod */
      tryfuncTM(L, func);  /* try to get '__call' metamethod */
      return luaD_precall(L, func, nresults);  /* now it must be a function */
    }
  }
}


/*
** Check appropriate error for stack overflow ("regular" overflow or
** overflow while handling stack overflow). If 'nCalls' is larger than
** LUAI_MAXCCALLS (which means it is handling a "regular" overflow) but
** smaller than 9/8 of LUAI_MAXCCALLS, does not report an error (to
** allow overflow handling to work)
*/
static void stackerror (lua_State *L) {
  if (L->nCcalls == LUAI_MAXCCALLS)
    luaG_runerror(L, "C stack overflow");
  else if (L->nCcalls >= (LUAI_MAXCCALLS + (LUAI_MAXCCALLS>>3)))
    luaD_throw(L, LUA_ERRERR);  /* error while handing stack error */
}


/*
** Call a function (C or Lua). The function to be called is at *func.
** The arguments are on the stack, right after the function.
** When returns, all the results are on the stack, starting at the original
** function position.
*/
void luaD_call (lua_State *L, StkId func, int nResults) {
  if (++L->nCcalls >= LUAI_MAXCCALLS)
    stackerror(L);
  if (!luaD_precall(L, func, nResults))  /* is a Lua function? */
    luaV_execute(L);  /* call it */
  L->nCcalls--;
}


/*
** Similar to 'luaD_call', but does not allow yields during the call
*/
void luaD_callnoyield (lua_State *L, StkId func, int nResults) {
  L->nny++;
  luaD_call(L, func, nResults);
  L->nny--;
}


/*
** Completes the execution of an interrupted C function, calling its
** continuation function.
*/
static void finishCcall (lua_State *L, int status) {
  CallInfo *ci = L->ci;
  int n;
  /* must have a continuation and must be able to call it */
  lua_assert(ci->u.c.k != NULL && L->nny == 0);
  /* error status can only happen in a protected call */
  lua_assert((ci->callstatus & CIST_YPCALL) || status == LUA_YIELD);
  if (ci->callstatus & CIST_YPCALL) {  /* was inside a pcall? */
    ci->callstatus &= ~CIST_YPCALL;  /* continuation is also inside it */
    L->errfunc = ci->u.c.old_errfunc;  /* with the same error function */
  }
  /* finish 'lua_callk'/'lua_pcall'; CIST_YPCALL and 'errfunc' already
     handled */
  adjustresults(L, ci->nresults);
  lua_unlock(L);
  n = (*ci->u.c.k)(L, status, ci->u.c.ctx);  /* call continuation function */
  lua_lock(L);
  api_checknelems(L, n);
  luaD_poscall(L, ci, L->top - n, n);  /* finish 'luaD_precall' */
}


/*
** Executes "full continuation" (everything in the stack) of a
** previously interrupted coroutine until the stack is empty (or another
** interruption long-jumps out of the loop). If the coroutine is
** recovering from an error, 'ud' points to the error status, which must
** be passed to the first continuation function (otherwise the default
** status is LUA_YIELD).
*/
static void unroll (lua_State *L, void *ud) {
  if (ud != NULL)  /* error status? */
    finishCcall(L, *(int *)ud);  /* finish 'lua_pcallk' callee */
  while (L->ci != &L->base_ci) {  /* something in the stack */
    if (!isLua(L->ci))  /* C function? */
      finishCcall(L, LUA_YIELD);  /* complete its execution */
    else {  /* Lua function */
      luaV_finishOp(L);  /* finish interrupted instruction */
      luaV_execute(L);  /* execute down to higher C 'boundary' */
    }
  }
}


/*
** Try to find a suspended protected call (a "recover point") for the
** given thread.
*/
static CallInfo *findpcall (lua_State *L) {
  CallInfo *ci;
  for (ci = L->ci; ci != NULL; ci = ci->previous) {  /* search for a pcall */
    if (ci->callstatus & CIST_YPCALL)
      return ci;
  }
  return NULL;  /* no pending pcall */
}


/*
** Recovers from an error in a coroutine. Finds a recover point (if
** there is one) and completes the execution of the interrupted
** 'luaD_pcall'. If there is no recover point, returns zero.
*/
static int recover (lua_State *L, int status) {
  StkId oldtop;
  CallInfo *ci = findpcall(L);
  if (ci == NULL) return 0;  /* no recovery point */
  /* "finish" luaD_pcall */
  oldtop = restorestack(L, ci->extra);
  luaF_close(L, oldtop);
  seterrorobj(L, status, oldtop);
  L->ci = ci;
  L->allowhook = getoah(ci->callstatus);  /* restore original 'allowhook' */
  L->nny = 0;  /* should be zero to be yieldable */
  luaD_shrinkstack(L);
  L->errfunc = ci->u.c.old_errfunc;
  return 1;  /* continue running the coroutine */
}


/*
** Signal an error in the call to 'lua_resume', not in the execution
** of the coroutine itself. (Such errors should not be handled by any
** coroutine error handler and should not kill the coroutine.)
*/
static int resume_error (lua_State *L, const char *msg, int narg) {
  L->top -= narg;  /* remove args from the stack */
  setsvalue2s(L, L->top, luaS_new(L, msg));  /* push error message */
  api_incr_top(L);
  lua_unlock(L);
  return LUA_ERRRUN;
}


/*
** Do the work for 'lua_resume' in protected mode. Most of the work
** depends on the status of the coroutine: initial state, suspended
** inside a hook, or regularly suspended (optionally with a continuation
** function), plus erroneous cases: non-suspended coroutine or dead
** coroutine.
*/
static void resume (lua_State *L, void *ud) {
  int n = *(cast(int*, ud));  /* number of arguments */
  StkId firstArg = L->top - n;  /* first argument */
  CallInfo *ci = L->ci;
  if (L->status == LUA_OK) {  /* starting a coroutine? */
    if (!luaD_precall(L, firstArg - 1, LUA_MULTRET))  /* Lua function? */
      luaV_execute(L);  /* call it */
  }
  else {  /* resuming from previous yield */
    lua_assert(L->status == LUA_YIELD);
    L->status = LUA_OK;  /* mark that it is running (again) */
    ci->func = restorestack(L, ci->extra);
    if (isLua(ci))  /* yielded inside a hook? */
      luaV_execute(L);  /* just continue running Lua code */
    else {  /* 'common' yield */
      if (ci->u.c.k != NULL) {  /* does it have a continuation function? */
        lua_unlock(L);
        n = (*ci->u.c.k)(L, LUA_YIELD, ci->u.c.ctx); /* call continuation */
        lua_lock(L);
        api_checknelems(L, n);
        firstArg = L->top - n;  /* yield results come from continuation */
      }
      luaD_poscall(L, ci, firstArg, n);  /* finish 'luaD_precall' */
    }
    unroll(L, NULL);  /* run continuation */
  }
}


/**
 * Resumes a coroutine in the given Lua state.
 *
 * This function resumes the execution of a coroutine that was previously suspended
 * via a call to `lua_yield` or started with `lua_newthread`. The coroutine must be
 * in a suspended state (LUA_YIELD) or just created (LUA_OK) to be resumed.
 *
 * @param L The Lua state containing the coroutine to resume.
 * @param from The Lua state from which the coroutine is being resumed, or NULL if
 *             resuming from the main thread.
 * @param nargs The number of arguments passed to the coroutine.
 *
 * @return The status of the coroutine after resuming. Possible return values include:
 *         - LUA_OK: The coroutine completed successfully.
 *         - LUA_YIELD: The coroutine yielded again.
 *         - LUA_ERRRUN: A runtime error occurred during execution.
 *         - Other error codes for unrecoverable errors.
 *
 * @note This function handles recoverable errors by attempting to unroll the stack
 *       and continue execution. If an unrecoverable error occurs, the coroutine is
 *       marked as dead, and an error message is pushed onto the stack.
 *
 * @warning Resuming a coroutine that is not in a suspended or initial state will
 *          result in an error. Additionally, resuming a coroutine that has already
 *          completed (dead coroutine) will also result in an error.
 */
LUA_API int lua_resume (lua_State *L, lua_State *from, int nargs) {
  int status;
  unsigned short oldnny = L->nny;  /* save "number of non-yieldable" calls */
  lua_lock(L);
  if (L->status == LUA_OK) {  /* may be starting a coroutine */
    if (L->ci != &L->base_ci)  /* not in base level? */
      return resume_error(L, "cannot resume non-suspended coroutine", nargs);
  }
  else if (L->status != LUA_YIELD)
    return resume_error(L, "cannot resume dead coroutine", nargs);
  L->nCcalls = (from) ? from->nCcalls + 1 : 1;
  if (L->nCcalls >= LUAI_MAXCCALLS)
    return resume_error(L, "C stack overflow", nargs);
  luai_userstateresume(L, nargs);
  L->nny = 0;  /* allow yields */
  api_checknelems(L, (L->status == LUA_OK) ? nargs + 1 : nargs);
  status = luaD_rawrunprotected(L, resume, &nargs);
  if (status == -1)  /* error calling 'lua_resume'? */
    status = LUA_ERRRUN;
  else {  /* continue running after recoverable errors */
    while (errorstatus(status) && recover(L, status)) {
      /* unroll continuation */
      status = luaD_rawrunprotected(L, unroll, &status);
    }
    if (errorstatus(status)) {  /* unrecoverable error? */
      L->status = cast_byte(status);  /* mark thread as 'dead' */
      seterrorobj(L, status, L->top);  /* push error message */
      L->ci->top = L->top;
    }
    else lua_assert(status == L->status);  /* normal end or yield */
  }
  L->nny = oldnny;  /* restore 'nny' */
  L->nCcalls--;
  lua_assert(L->nCcalls == ((from) ? from->nCcalls : 0));
  lua_unlock(L);
  return status;
}


/**
 * Checks whether the Lua state is yieldable.
 *
 * This function determines if the current Lua state (coroutine) can yield. 
 * A Lua state is yieldable if it is not in a non-yieldable context, which is 
 * indicated by the `nny` (non-yieldable) field in the Lua state structure. 
 * If `nny` is 0, the state is yieldable; otherwise, it is not.
 *
 * @param L Pointer to the Lua state to check.
 * @return Returns 1 if the state is yieldable, otherwise returns 0.
 */
LUA_API int lua_isyieldable (lua_State *L) {
  return (L->nny == 0);
}


/**
 * Yields the current coroutine, allowing it to be resumed later.
 *
 * This function is used to yield the execution of a coroutine, effectively pausing it
 * and returning control to the caller. The coroutine can be resumed later using `lua_resume`.
 * The function also supports continuations, which allow the coroutine to resume execution
 * at a specific point after yielding.
 *
 * @param L The Lua state associated with the coroutine.
 * @param nresults The number of results to leave on the stack when yielding.
 * @param ctx A context value to be passed to the continuation function.
 * @param k The continuation function to be called when the coroutine is resumed.
 *           If NULL, no continuation is used.
 *
 * @return Always returns 0, indicating that the coroutine has yielded successfully.
 *
 * @throws Throws an error if an attempt is made to yield across a C-call boundary
 *         or from outside a coroutine.
 *
 * @note This function is typically used in conjunction with `lua_resume` to implement
 *       cooperative multitasking in Lua.
 */
LUA_API int lua_yieldk (lua_State *L, int nresults, lua_KContext ctx,
                        lua_KFunction k) {
  CallInfo *ci = L->ci;
  luai_userstateyield(L, nresults);
  lua_lock(L);
  api_checknelems(L, nresults);
  if (L->nny > 0) {
    if (L != G(L)->mainthread)
      luaG_runerror(L, "attempt to yield across a C-call boundary");
    else
      luaG_runerror(L, "attempt to yield from outside a coroutine");
  }
  L->status = LUA_YIELD;
  ci->extra = savestack(L, ci->func);  /* save current 'func' */
  if (isLua(ci)) {  /* inside a hook? */
    api_check(L, k == NULL, "hooks cannot continue after yielding");
  }
  else {
    if ((ci->u.c.k = k) != NULL)  /* is there a continuation? */
      ci->u.c.ctx = ctx;  /* save context */
    ci->func = L->top - nresults - 1;  /* protect stack below results */
    luaD_throw(L, LUA_YIELD);
  }
  lua_assert(ci->callstatus & CIST_HOOKED);  /* must be inside a hook */
  lua_unlock(L);
  return 0;  /* return to 'luaD_hook' */
}


/**
 * Executes a protected call to a Lua function, handling errors gracefully.
 *
 * This function sets up a protected environment to call the given Lua function `func` with the
 * user-defined context `u`. It ensures that any errors during execution are caught and handled
 * appropriately, restoring the Lua state to its original condition before returning.
 *
 * @param L Pointer to the Lua state.
 * @param func The function to be called in a protected environment.
 * @param u User-defined context passed to `func`.
 * @param old_top The stack index before the call, used to restore the stack in case of an error.
 * @param ef The error function to be used during the protected call.
 *
 * @return Returns the status of the protected call. If the call succeeds, `LUA_OK` is returned.
 *         If an error occurs, the error status is returned, and the Lua state is restored to its
 *         original state, with the error object placed on the stack.
 *
 * @note This function temporarily modifies the Lua state's error handler, call info, and other
 *       internal fields to ensure proper error handling. These modifications are reverted before
 *       the function returns.
 */
int luaD_pcall (lua_State *L, Pfunc func, void *u,
                ptrdiff_t old_top, ptrdiff_t ef) {
  int status;
  CallInfo *old_ci = L->ci;
  lu_byte old_allowhooks = L->allowhook;
  unsigned short old_nny = L->nny;
  ptrdiff_t old_errfunc = L->errfunc;
  L->errfunc = ef;
  status = luaD_rawrunprotected(L, func, u);
  if (status != LUA_OK) {  /* an error occurred? */
    StkId oldtop = restorestack(L, old_top);
    luaF_close(L, oldtop);  /* close possible pending closures */
    seterrorobj(L, status, oldtop);
    L->ci = old_ci;
    L->allowhook = old_allowhooks;
    L->nny = old_nny;
    luaD_shrinkstack(L);
  }
  L->errfunc = old_errfunc;
  return status;
}



/*
** Execute a protected parser.
*/
struct SParser {  /* data to 'f_parser' */
  ZIO *z;
  Mbuffer buff;  /* dynamic structure used by the scanner */
  Dyndata dyd;  /* dynamic structures used by the parser */
  const char *mode;
  const char *name;
};


/**
 * Checks if the given mode string contains the first character of the string `x`.
 * If the mode string does not contain the character, an error is raised using Lua's
 * error handling mechanism. This function is typically used to validate the mode
 * of a chunk being loaded, ensuring it matches the expected mode.
 *
 * @param L The Lua state, used for error reporting and stack manipulation.
 * @param mode A string specifying the allowed modes. Each character in this string
 *             represents a valid mode. If NULL, no check is performed.
 * @param x A string whose first character is checked against the mode string.
 *          This typically represents the mode of the chunk being loaded.
 *
 * @throws LUA_ERRSYNTAX If the first character of `x` is not found in `mode`,
 *         an error is raised with a descriptive message.
 */
static void checkmode (lua_State *L, const char *mode, const char *x) {
  if (mode && strchr(mode, x[0]) == NULL) {
    luaO_pushfstring(L,
       "attempt to load a %s chunk (mode is '%s')", x, mode);
    luaD_throw(L, LUA_ERRSYNTAX);
  }
}


/**
 * Parses a Lua chunk from the input stream and initializes the resulting closure.
 *
 * This function reads the first character from the input stream to determine whether the chunk
 * is in binary or text format. If the first character matches the Lua binary signature, the
 * function processes the chunk as a binary chunk using `luaU_undump`. Otherwise, it processes
 * the chunk as a text chunk using `luaY_parser`. After parsing, the function initializes the
 * upvalues of the resulting closure.
 *
 * @param L The Lua state.
 * @param ud A pointer to the `SParser` structure containing the input stream and parsing context.
 *
 * @note The function asserts that the number of upvalues in the closure matches the expected
 *       size of upvalues in the prototype.
 */
static void f_parser (lua_State *L, void *ud) {
  LClosure *cl;
  struct SParser *p = cast(struct SParser *, ud);
  int c = zgetc(p->z);  /* read first character */
  if (c == LUA_SIGNATURE[0]) {
    checkmode(L, p->mode, "binary");
    cl = luaU_undump(L, p->z, p->name);
  }
  else {
    checkmode(L, p->mode, "text");
    cl = luaY_parser(L, p->z, &p->buff, &p->dyd, p->name, c);
  }
  lua_assert(cl->nupvalues == cl->p->sizeupvalues);
  luaF_initupvals(L, cl);
}


/**
 * Parses Lua code in a protected environment, ensuring that errors during parsing
 * are caught and handled appropriately. This function initializes a parser state,
 * sets up the necessary buffers and dynamic arrays, and then invokes the parser
 * function within a protected call (`luaD_pcall`). If an error occurs during parsing,
 * it is propagated back to the caller via the return status.
 *
 * @param L The Lua state in which the parsing will occur.
 * @param z The ZIO stream providing the input data to be parsed.
 * @param name The name of the chunk being parsed, used for error messages and debugging.
 * @param mode The mode of parsing, which can influence how the input is interpreted
 *             (e.g., binary vs. text mode).
 * @return An integer status code indicating the result of the parsing operation.
 *         A return value of 0 indicates success, while a non-zero value indicates
 *         an error occurred during parsing.
 *
 * The function temporarily disables yielding during parsing by incrementing `L->nny`.
 * After parsing, it cleans up allocated resources, including buffers and dynamic arrays,
 * and restores the ability to yield by decrementing `L->nny`.
 */
int luaD_protectedparser (lua_State *L, ZIO *z, const char *name,
                                        const char *mode) {
  struct SParser p;
  int status;
  L->nny++;  /* cannot yield during parsing */
  p.z = z; p.name = name; p.mode = mode;
  p.dyd.actvar.arr = NULL; p.dyd.actvar.size = 0;
  p.dyd.gt.arr = NULL; p.dyd.gt.size = 0;
  p.dyd.label.arr = NULL; p.dyd.label.size = 0;
  luaZ_initbuffer(L, &p.buff);
  status = luaD_pcall(L, f_parser, &p, savestack(L, L->top), L->errfunc);
  luaZ_freebuffer(L, &p.buff);
  luaM_freearray(L, p.dyd.actvar.arr, p.dyd.actvar.size);
  luaM_freearray(L, p.dyd.gt.arr, p.dyd.gt.size);
  luaM_freearray(L, p.dyd.label.arr, p.dyd.label.size);
  L->nny--;
  return status;
}


