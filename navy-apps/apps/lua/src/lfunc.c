/*
** $Id: lfunc.c,v 2.45 2014/11/02 19:19:04 roberto Exp $
** Auxiliary functions to manipulate prototypes and closures
** See Copyright Notice in lua.h
*/

#define lfunc_c
#define LUA_CORE

#include "lprefix.h"


#include <stddef.h>

#include "lua.h"

#include "lfunc.h"
#include "lgc.h"
#include "lmem.h"
#include "lobject.h"
#include "lstate.h"



/**
 * Creates and initializes a new C closure object.
 *
 * This function allocates memory for a new C closure object, initializes it with the specified number of upvalues,
 * and returns a pointer to the newly created C closure. The closure is created as a garbage-collectable object
 * of type `LUA_TCCL`.
 *
 * @param L Pointer to the Lua state.
 * @param n The number of upvalues to be associated with the C closure.
 * @return A pointer to the newly created C closure object.
 */
CClosure *luaF_newCclosure (lua_State *L, int n) {
  GCObject *o = luaC_newobj(L, LUA_TCCL, sizeCclosure(n));
  CClosure *c = gco2ccl(o);
  c->nupvalues = cast_byte(n);
  return c;
}


/**
 * Creates and initializes a new Lua closure (LClosure) with a specified number of upvalues.
 * 
 * This function allocates memory for a new Lua closure object using the Lua garbage collector
 * (via `luaC_newobj`). The closure is initialized with the given number of upvalues, all of which
 * are set to `NULL`. The closure's prototype (`p`) is also initialized to `NULL`.
 *
 * @param L The Lua state in which the closure is created.
 * @param n The number of upvalues the closure will have.
 * @return A pointer to the newly created and initialized LClosure object.
 */
LClosure *luaF_newLclosure (lua_State *L, int n) {
  GCObject *o = luaC_newobj(L, LUA_TLCL, sizeLclosure(n));
  LClosure *c = gco2lcl(o);
  c->p = NULL;
  c->nupvalues = cast_byte(n);
  while (n--) c->upvals[n] = NULL;
  return c;
}

/*
** fill a closure with new closed upvalues
*/
void luaF_initupvals (lua_State *L, LClosure *cl) {
  int i;
  for (i = 0; i < cl->nupvalues; i++) {
    UpVal *uv = luaM_new(L, UpVal);
    uv->refcount = 1;
    uv->v = &uv->u.value;  /* make it closed */
    setnilvalue(uv->v);
    cl->upvals[i] = uv;
  }
}


/**
 * Finds or creates an upvalue for the given stack level in the Lua state.
 *
 * This function searches for an existing upvalue associated with the specified
 * stack level (`level`) in the Lua state `L`. If an upvalue is found, it is
 * returned. If no upvalue is found, a new upvalue is created, linked to the list
 * of open upvalues, and returned.
 *
 * The function ensures that the Lua state is properly linked to the list of
 * threads with upvalues if it isn't already. The created upvalue is initialized
 * with a reference count of 0 and marked as touched.
 *
 * @param L The Lua state in which to find or create the upvalue.
 * @param level The stack level for which to find or create the upvalue.
 * @return A pointer to the found or newly created upvalue.
 */
UpVal *luaF_findupval (lua_State *L, StkId level) {
  UpVal **pp = &L->openupval;
  UpVal *p;
  UpVal *uv;
  lua_assert(isintwups(L) || L->openupval == NULL);
  while (*pp != NULL && (p = *pp)->v >= level) {
    lua_assert(upisopen(p));
    if (p->v == level)  /* found a corresponding upvalue? */
      return p;  /* return it */
    pp = &p->u.open.next;
  }
  /* not found: create a new upvalue */
  uv = luaM_new(L, UpVal);
  uv->refcount = 0;
  uv->u.open.next = *pp;  /* link it to list of open upvalues */
  uv->u.open.touched = 1;
  *pp = uv;
  uv->v = level;  /* current value lives in the stack */
  if (!isintwups(L)) {  /* thread not in list of threads with upvalues? */
    L->twups = G(L)->twups;  /* link it to the list */
    G(L)->twups = L;
  }
  return uv;
}


/**
 * Closes all open upvalues in the Lua state `L` that are at or above the stack level `level`.
 * An upvalue is considered open if it is still referencing a value on the stack. This function
 * ensures that any open upvalues above the specified stack level are properly closed, either
 * by freeing them if they have no references or by moving their values to their respective
 * upvalue slots if they are still in use.
 *
 * @param L The Lua state in which the upvalues are being closed.
 * @param level The stack level above which all open upvalues will be closed. Upvalues at or
 *              above this level will be processed.
 *
 * The function iterates through the list of open upvalues, starting from the top of the stack.
 * For each upvalue, it checks if the upvalue is still open and if it is located at or above the
 * specified `level`. If so, the upvalue is removed from the 'open' list. If the upvalue has no
 * references, it is freed. Otherwise, the value referenced by the upvalue is moved to the
 * upvalue's slot, and the upvalue is marked as closed. The function also triggers the garbage
 * collector's upvalue barrier to ensure proper handling of the upvalue's state.
 */
void luaF_close (lua_State *L, StkId level) {
  UpVal *uv;
  while (L->openupval != NULL && (uv = L->openupval)->v >= level) {
    lua_assert(upisopen(uv));
    L->openupval = uv->u.open.next;  /* remove from 'open' list */
    if (uv->refcount == 0)  /* no references? */
      luaM_free(L, uv);  /* free upvalue */
    else {
      setobj(L, &uv->u.value, uv->v);  /* move value to upvalue slot */
      uv->v = &uv->u.value;  /* now current value lives here */
      luaC_upvalbarrier(L, uv);
    }
  }
}


/**
 * Creates and initializes a new `Proto` structure, which represents a Lua function prototype.
 * This function allocates memory for the prototype and sets all its fields to their default values.
 * 
 * @param L The Lua state in which the prototype is created.
 * @return A pointer to the newly created and initialized `Proto` structure.
 * 
 * The `Proto` structure fields are initialized as follows:
 * - `k`, `p`, `code`, `cache`, `lineinfo`, `upvalues`, `locvars`, and `source` are set to `NULL`.
 * - `sizek`, `sizep`, `sizecode`, `sizelineinfo`, `sizeupvalues`, `sizelocvars`, `numparams`, 
 *   `is_vararg`, `maxstacksize`, `linedefined`, and `lastlinedefined` are set to `0`.
 */
Proto *luaF_newproto (lua_State *L) {
  GCObject *o = luaC_newobj(L, LUA_TPROTO, sizeof(Proto));
  Proto *f = gco2p(o);
  f->k = NULL;
  f->sizek = 0;
  f->p = NULL;
  f->sizep = 0;
  f->code = NULL;
  f->cache = NULL;
  f->sizecode = 0;
  f->lineinfo = NULL;
  f->sizelineinfo = 0;
  f->upvalues = NULL;
  f->sizeupvalues = 0;
  f->numparams = 0;
  f->is_vararg = 0;
  f->maxstacksize = 0;
  f->locvars = NULL;
  f->sizelocvars = 0;
  f->linedefined = 0;
  f->lastlinedefined = 0;
  f->source = NULL;
  return f;
}


/**
 * Frees the memory allocated for a Lua function prototype (`Proto`).
 * This function deallocates all dynamically allocated arrays within the `Proto` structure,
 * including the bytecode (`code`), the array of nested function prototypes (`p`), the array
 * of constants (`k`), the line information (`lineinfo`), the local variable information
 * (`locvars`), and the upvalue information (`upvalues`). After freeing these arrays, it
 * deallocates the `Proto` structure itself.
 *
 * @param L Pointer to the Lua state, used for memory management.
 * @param f Pointer to the `Proto` structure to be freed.
 */
void luaF_freeproto (lua_State *L, Proto *f) {
  luaM_freearray(L, f->code, f->sizecode);
  luaM_freearray(L, f->p, f->sizep);
  luaM_freearray(L, f->k, f->sizek);
  luaM_freearray(L, f->lineinfo, f->sizelineinfo);
  luaM_freearray(L, f->locvars, f->sizelocvars);
  luaM_freearray(L, f->upvalues, f->sizeupvalues);
  luaM_free(L, f);
}


/*
** Look for n-th local variable at line 'line' in function 'func'.
** Returns NULL if not found.
*/
const char *luaF_getlocalname (const Proto *f, int local_number, int pc) {
  int i;
  for (i = 0; i<f->sizelocvars && f->locvars[i].startpc <= pc; i++) {
    if (pc < f->locvars[i].endpc) {  /* is variable active? */
      local_number--;
      if (local_number == 0)
        return getstr(f->locvars[i].varname);
    }
  }
  return NULL;  /* not found */
}

