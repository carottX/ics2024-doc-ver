/*
** $Id: lzio.c,v 1.37 2015/09/08 15:41:05 roberto Exp $
** Buffered streams
** See Copyright Notice in lua.h
*/

#define lzio_c
#define LUA_CORE

#include "lprefix.h"


#include <string.h>

#include "lua.h"

#include "llimits.h"
#include "lmem.h"
#include "lstate.h"
#include "lzio.h"


/**
 * Fills the buffer of the ZIO stream by invoking the associated reader function.
 * This method unlocks the Lua state, calls the reader to fetch data, and then
 * locks the Lua state again. If the reader returns no data (NULL buffer or zero size),
 * the method returns the end-of-stream marker (EOZ). Otherwise, it updates the ZIO
 * stream's buffer pointer and remaining bytes count, and returns the next character
 * from the buffer.
 *
 * @param z Pointer to the ZIO stream structure.
 * @return The next character from the buffer, or EOZ if no data is available.
 */
int luaZ_fill (ZIO *z) {
  size_t size;
  lua_State *L = z->L;
  const char *buff;
  lua_unlock(L);
  buff = z->reader(L, z->data, &size);
  lua_lock(L);
  if (buff == NULL || size == 0)
    return EOZ;
  z->n = size - 1;  /* discount char being returned */
  z->p = buff;
  return cast_uchar(*(z->p++));
}


/**
 * Initializes a ZIO (Zipped I/O) structure for use with Lua's buffered I/O system.
 * 
 * This function sets up the ZIO structure `z` by associating it with the Lua state `L`,
 * assigning the provided `reader` function and `data` pointer, and initializing the
 * buffer state to empty (i.e., `n` is set to 0 and `p` is set to NULL).
 *
 * @param L The Lua state to associate with the ZIO structure.
 * @param z Pointer to the ZIO structure to be initialized.
 * @param reader The reader function to be used for fetching data.
 * @param data User-defined data to be passed to the reader function.
 */
void luaZ_init (lua_State *L, ZIO *z, lua_Reader reader, void *data) {
  z->L = L;
  z->reader = reader;
  z->data = data;
  z->n = 0;
  z->p = NULL;
}


/* --------------------------------------------------------------- read --- */
size_t luaZ_read (ZIO *z, void *b, size_t n) {
  while (n) {
    size_t m;
    if (z->n == 0) {  /* no bytes in buffer? */
      if (luaZ_fill(z) == EOZ)  /* try to read more */
        return n;  /* no more input; return number of missing bytes */
      else {
        z->n++;  /* luaZ_fill consumed first byte; put it back */
        z->p--;
      }
    }
    m = (n <= z->n) ? n : z->n;  /* min. between n and z->n */
    memcpy(b, z->p, m);
    z->n -= m;
    z->p += m;
    b = (char *)b + m;
    n -= m;
  }
  return 0;
}

