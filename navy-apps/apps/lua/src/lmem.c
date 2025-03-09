/*
** $Id: lmem.c,v 1.91 2015/03/06 19:45:54 roberto Exp $
** Interface to Memory Manager
** See Copyright Notice in lua.h
*/

#define lmem_c
#define LUA_CORE

#include "lprefix.h"


#include <stddef.h>

#include "lua.h"

#include "ldebug.h"
#include "ldo.h"
#include "lgc.h"
#include "lmem.h"
#include "lobject.h"
#include "lstate.h"



/*
** About the realloc function:
** void * frealloc (void *ud, void *ptr, size_t osize, size_t nsize);
** ('osize' is the old size, 'nsize' is the new size)
**
** * frealloc(ud, NULL, x, s) creates a new block of size 's' (no
** matter 'x').
**
** * frealloc(ud, p, x, 0) frees the block 'p'
** (in this specific case, frealloc must return NULL);
** particularly, frealloc(ud, NULL, 0, 0) does nothing
** (which is equivalent to free(NULL) in ISO C)
**
** frealloc returns NULL if it cannot create or reallocate the area
** (any reallocation to an equal or smaller size cannot fail!)
*/



#define MINSIZEARRAY	4


/**
 * Grows an auxiliary array or block of memory in a controlled manner.
 *
 * This function is used to dynamically increase the size of a memory block
 * while ensuring that the size does not exceed a specified limit. The function
 * doubles the size of the block if possible, but if doubling would exceed the
 * limit, it grows the block to the limit instead. If the block is already at
 * or beyond the limit, an error is raised.
 *
 * @param L The Lua state, used for error reporting and memory allocation.
 * @param block The current memory block to be resized. Can be NULL if the block
 *              is being allocated for the first time.
 * @param size A pointer to the current size of the block. This value is updated
 *             to the new size upon successful resizing.
 * @param size_elems The size of each element in the block, in bytes.
 * @param limit The maximum allowed size for the block.
 * @param what A string describing the type of the block, used in error messages.
 *
 * @return A pointer to the resized memory block. If the resizing fails, the
 *         function raises an error and does not return.
 *
 * @note The function ensures that the new size is at least MINSIZEARRAY if
 *       doubling the size would result in a smaller value. This prevents
 *       excessive reallocations for very small blocks.
 */
void *luaM_growaux_ (lua_State *L, void *block, int *size, size_t size_elems,
                     int limit, const char *what) {
  void *newblock;
  int newsize;
  if (*size >= limit/2) {  /* cannot double it? */
    if (*size >= limit)  /* cannot grow even a little? */
      luaG_runerror(L, "too many %s (limit is %d)", what, limit);
    newsize = limit;  /* still have at least one free place */
  }
  else {
    newsize = (*size)*2;
    if (newsize < MINSIZEARRAY)
      newsize = MINSIZEARRAY;  /* minimum size */
  }
  newblock = luaM_reallocv(L, block, *size, newsize, size_elems);
  *size = newsize;  /* update only when everything else is OK */
  return newblock;
}


/**
 * @brief Triggers a runtime error indicating a memory allocation failure due to an excessively large block.
 *
 * This function is called when an attempt to allocate memory fails because the requested block size
 * exceeds the permissible limit. It raises a runtime error in the Lua state `L` with the message
 * "memory allocation error: block too big".
 *
 * @param L Pointer to the Lua state where the error will be raised.
 * @noreturn This function does not return; it terminates the current execution flow by raising an error.
 */
l_noret luaM_toobig (lua_State *L) {
  luaG_runerror(L, "memory allocation error: block too big");
}



/*
** generic allocation routine.
*/
void *luaM_realloc_ (lua_State *L, void *block, size_t osize, size_t nsize) {
  void *newblock;
  global_State *g = G(L);
  size_t realosize = (block) ? osize : 0;
  lua_assert((realosize == 0) == (block == NULL));
#if defined(HARDMEMTESTS)
  if (nsize > realosize && g->gcrunning)
    luaC_fullgc(L, 1);  /* force a GC whenever possible */
#endif
  newblock = (*g->frealloc)(g->ud, block, osize, nsize);
  if (newblock == NULL && nsize > 0) {
    lua_assert(nsize > realosize);  /* cannot fail when shrinking a block */
    if (g->version) {  /* is state fully built? */
      luaC_fullgc(L, 1);  /* try to free some memory... */
      newblock = (*g->frealloc)(g->ud, block, osize, nsize);  /* try again */
    }
    if (newblock == NULL)
      luaD_throw(L, LUA_ERRMEM);
  }
  lua_assert((nsize == 0) == (newblock == NULL));
  g->GCdebt = (g->GCdebt + nsize) - realosize;
  return newblock;
}

