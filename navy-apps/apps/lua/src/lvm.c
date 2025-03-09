/*
** $Id: lvm.c,v 2.268 2016/02/05 19:59:14 roberto Exp $
** Lua virtual machine
** See Copyright Notice in lua.h
*/

#define lvm_c
#define LUA_CORE

#include "lprefix.h"

#include <float.h>
#include <limits.h>
#include <mymath.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lua.h"

#include "ldebug.h"
#include "ldo.h"
#include "lfunc.h"
#include "lgc.h"
#include "lobject.h"
#include "lopcodes.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "ltm.h"
#include "lvm.h"


/* limit for table tag-method chains (to avoid loops) */
#define MAXTAGLOOP	2000



/*
** 'l_intfitsf' checks whether a given integer can be converted to a
** float without rounding. Used in comparisons. Left undefined if
** all integers fit in a float precisely.
*/
#if !defined(l_intfitsf)

#if LUA_FLOAT_TYPE != LUA_FLOAT_INT

/* number of bits in the mantissa of a float */
#define NBM		(l_mathlim(MANT_DIG))

/*
** Check whether some integers may not fit in a float, that is, whether
** (maxinteger >> NBM) > 0 (that implies (1 << NBM) <= maxinteger).
** (The shifts are done in parts to avoid shifting by more than the size
** of an integer. In a worst case, NBM == 113 for long double and
** sizeof(integer) == 32.)
*/
#if ((((LUA_MAXINTEGER >> (NBM / 4)) >> (NBM / 4)) >> (NBM / 4)) \
	>> (NBM - (3 * (NBM / 4))))  >  0

#define l_intfitsf(i)  \
  (-((lua_Integer)1 << NBM) <= (i) && (i) <= ((lua_Integer)1 << NBM))

#endif

#endif

#endif


/*
** Try to convert a value to a float. The float case is already handled
** by the macro 'tonumber'.
*/
int luaV_tonumber_ (const TValue *obj, lua_Number *n) {
  TValue v;
  if (ttisinteger(obj)) {
    *n = cast_num(ivalue(obj));
    return 1;
  }
  else if (cvt2num(obj) &&  /* string convertible to number? */
            luaO_str2num(svalue(obj), &v) == vslen(obj) + 1) {
    *n = nvalue(&v);  /* convert result of 'luaO_str2num' to a float */
    return 1;
  }
  else
    return 0;  /* conversion failed */
}


/*
** try to convert a value to an integer, rounding according to 'mode':
** mode == 0: accepts only integral values
** mode == 1: takes the floor of the number
** mode == 2: takes the ceil of the number
*/
int luaV_tointeger (const TValue *obj, lua_Integer *p, int mode) {
  TValue v;
 again:
  if (ttisfloat(obj)) {
    lua_Number n = fltvalue(obj);
    lua_Number f = l_floor(n);
    if (n != f) {  /* not an integral value? */
      if (mode == 0) return 0;  /* fails if mode demands integral value */
      else if (mode > 1)  /* needs ceil? */
        f += 1;  /* convert floor to ceil (remember: n != f) */
    }
    return lua_numbertointeger(f, p);
  }
  else if (ttisinteger(obj)) {
    *p = ivalue(obj);
    return 1;
  }
  else if (cvt2num(obj) &&
            luaO_str2num(svalue(obj), &v) == vslen(obj) + 1) {
    obj = &v;
    goto again;  /* convert result from 'luaO_str2num' to an integer */
  }
  return 0;  /* conversion failed */
}


/*
** Try to convert a 'for' limit to an integer, preserving the
** semantics of the loop.
** (The following explanation assumes a non-negative step; it is valid
** for negative steps mutatis mutandis.)
** If the limit can be converted to an integer, rounding down, that is
** it.
** Otherwise, check whether the limit can be converted to a number.  If
** the number is too large, it is OK to set the limit as LUA_MAXINTEGER,
** which means no limit.  If the number is too negative, the loop
** should not run, because any initial integer value is larger than the
** limit. So, it sets the limit to LUA_MININTEGER. 'stopnow' corrects
** the extreme case when the initial value is LUA_MININTEGER, in which
** case the LUA_MININTEGER limit would still run the loop once.
*/
static int forlimit (const TValue *obj, lua_Integer *p, lua_Integer step,
                     int *stopnow) {
  *stopnow = 0;  /* usually, let loops run */
  if (!luaV_tointeger(obj, p, (step < 0 ? 2 : 1))) {  /* not fit in integer? */
    lua_Number n;  /* try to convert to float */
    if (!tonumber(obj, &n)) /* cannot convert to float? */
      return 0;  /* not a number */
    if (luai_numlt(0, n)) {  /* if true, float is larger than max integer */
      *p = LUA_MAXINTEGER;
      if (step < 0) *stopnow = 1;
    }
    else {  /* float is smaller than min integer */
      *p = LUA_MININTEGER;
      if (step >= 0) *stopnow = 1;
    }
  }
  return 1;
}


/*
** Finish the table access 'val = t[key]'.
** if 'slot' is NULL, 't' is not a table; otherwise, 'slot' points to
** t[k] entry (which must be nil).
*/
void luaV_finishget (lua_State *L, const TValue *t, TValue *key, StkId val,
                      const TValue *slot) {
  int loop;  /* counter to avoid infinite loops */
  const TValue *tm;  /* metamethod */
  for (loop = 0; loop < MAXTAGLOOP; loop++) {
    if (slot == NULL) {  /* 't' is not a table? */
      lua_assert(!ttistable(t));
      tm = luaT_gettmbyobj(L, t, TM_INDEX);
      if (ttisnil(tm))
        luaG_typeerror(L, t, "index");  /* no metamethod */
      /* else will try the metamethod */
    }
    else {  /* 't' is a table */
      lua_assert(ttisnil(slot));
      tm = fasttm(L, hvalue(t)->metatable, TM_INDEX);  /* table's metamethod */
      if (tm == NULL) {  /* no metamethod? */
        setnilvalue(val);  /* result is nil */
        return;
      }
      /* else will try the metamethod */
    }
    if (ttisfunction(tm)) {  /* is metamethod a function? */
      luaT_callTM(L, tm, t, key, val, 1);  /* call it */
      return;
    }
    t = tm;  /* else try to access 'tm[key]' */
    if (luaV_fastget(L,t,key,slot,luaH_get)) {  /* fast track? */
      setobj2s(L, val, slot);  /* done */
      return;
    }
    /* else repeat (tail call 'luaV_finishget') */
  }
  luaG_runerror(L, "'__index' chain too long; possible loop");
}


/*
** Finish a table assignment 't[key] = val'.
** If 'slot' is NULL, 't' is not a table.  Otherwise, 'slot' points
** to the entry 't[key]', or to 'luaO_nilobject' if there is no such
** entry.  (The value at 'slot' must be nil, otherwise 'luaV_fastset'
** would have done the job.)
*/
void luaV_finishset (lua_State *L, const TValue *t, TValue *key,
                     StkId val, const TValue *slot) {
  int loop;  /* counter to avoid infinite loops */
  for (loop = 0; loop < MAXTAGLOOP; loop++) {
    const TValue *tm;  /* '__newindex' metamethod */
    if (slot != NULL) {  /* is 't' a table? */
      Table *h = hvalue(t);  /* save 't' table */
      lua_assert(ttisnil(slot));  /* old value must be nil */
      tm = fasttm(L, h->metatable, TM_NEWINDEX);  /* get metamethod */
      if (tm == NULL) {  /* no metamethod? */
        if (slot == luaO_nilobject)  /* no previous entry? */
          slot = luaH_newkey(L, h, key);  /* create one */
        /* no metamethod and (now) there is an entry with given key */
        setobj2t(L, cast(TValue *, slot), val);  /* set its new value */
        invalidateTMcache(h);
        luaC_barrierback(L, h, val);
        return;
      }
      /* else will try the metamethod */
    }
    else {  /* not a table; check metamethod */
      if (ttisnil(tm = luaT_gettmbyobj(L, t, TM_NEWINDEX)))
        luaG_typeerror(L, t, "index");
    }
    /* try the metamethod */
    if (ttisfunction(tm)) {
      luaT_callTM(L, tm, t, key, val, 0);
      return;
    }
    t = tm;  /* else repeat assignment over 'tm' */
    if (luaV_fastset(L, t, key, slot, luaH_get, val))
      return;  /* done */
    /* else loop */
  }
  luaG_runerror(L, "'__newindex' chain too long; possible loop");
}


/*
** Compare two strings 'ls' x 'rs', returning an integer smaller-equal-
** -larger than zero if 'ls' is smaller-equal-larger than 'rs'.
** The code is a little tricky because it allows '\0' in the strings
** and it uses 'strcoll' (to respect locales) for each segments
** of the strings.
*/
static int l_strcmp (const TString *ls, const TString *rs) {
  const char *l = getstr(ls);
  size_t ll = tsslen(ls);
  const char *r = getstr(rs);
  size_t lr = tsslen(rs);
  for (;;) {  /* for each segment */
    int temp = strcoll(l, r);
    if (temp != 0)  /* not equal? */
      return temp;  /* done */
    else {  /* strings are equal up to a '\0' */
      size_t len = strlen(l);  /* index of first '\0' in both strings */
      if (len == lr)  /* 'rs' is finished? */
        return (len == ll) ? 0 : 1;  /* check 'ls' */
      else if (len == ll)  /* 'ls' is finished? */
        return -1;  /* 'ls' is smaller than 'rs' ('rs' is not finished) */
      /* both strings longer than 'len'; go on comparing after the '\0' */
      len++;
      l += len; ll -= len; r += len; lr -= len;
    }
  }
}


/*
** Check whether integer 'i' is less than float 'f'. If 'i' has an
** exact representation as a float ('l_intfitsf'), compare numbers as
** floats. Otherwise, if 'f' is outside the range for integers, result
** is trivial. Otherwise, compare them as integers. (When 'i' has no
** float representation, either 'f' is "far away" from 'i' or 'f' has
** no precision left for a fractional part; either way, how 'f' is
** truncated is irrelevant.) When 'f' is NaN, comparisons must result
** in false.
*/
static int LTintfloat (lua_Integer i, lua_Number f) {
#if defined(l_intfitsf)
  if (!l_intfitsf(i)) {
    if (f >= -cast_num(LUA_MININTEGER))  /* -minint == maxint + 1 */
      return 1;  /* f >= maxint + 1 > i */
    else if (f > cast_num(LUA_MININTEGER))  /* minint < f <= maxint ? */
      return (i < cast(lua_Integer, f));  /* compare them as integers */
    else  /* f <= minint <= i (or 'f' is NaN)  -->  not(i < f) */
      return 0;
  }
#endif
  return luai_numlt(cast_num(i), f);  /* compare them as floats */
}


/*
** Check whether integer 'i' is less than or equal to float 'f'.
** See comments on previous function.
*/
static int LEintfloat (lua_Integer i, lua_Number f) {
#if defined(l_intfitsf)
  if (!l_intfitsf(i)) {
    if (f >= -cast_num(LUA_MININTEGER))  /* -minint == maxint + 1 */
      return 1;  /* f >= maxint + 1 > i */
    else if (f >= cast_num(LUA_MININTEGER))  /* minint <= f <= maxint ? */
      return (i <= cast(lua_Integer, f));  /* compare them as integers */
    else  /* f < minint <= i (or 'f' is NaN)  -->  not(i <= f) */
      return 0;
  }
#endif
  return luai_numle(cast_num(i), f);  /* compare them as floats */
}


/*
** Return 'l < r', for numbers.
*/
static int LTnum (const TValue *l, const TValue *r) {
  if (ttisinteger(l)) {
    lua_Integer li = ivalue(l);
    if (ttisinteger(r))
      return li < ivalue(r);  /* both are integers */
    else  /* 'l' is int and 'r' is float */
      return LTintfloat(li, fltvalue(r));  /* l < r ? */
  }
  else {
    lua_Number lf = fltvalue(l);  /* 'l' must be float */
    if (ttisfloat(r))
      return luai_numlt(lf, fltvalue(r));  /* both are float */
    else if (luai_numisnan(lf))  /* 'r' is int and 'l' is float */
      return 0;  /* NaN < i is always false */
    else  /* without NaN, (l < r)  <-->  not(r <= l) */
      return !LEintfloat(ivalue(r), lf);  /* not (r <= l) ? */
  }
}


/*
** Return 'l <= r', for numbers.
*/
static int LEnum (const TValue *l, const TValue *r) {
  if (ttisinteger(l)) {
    lua_Integer li = ivalue(l);
    if (ttisinteger(r))
      return li <= ivalue(r);  /* both are integers */
    else  /* 'l' is int and 'r' is float */
      return LEintfloat(li, fltvalue(r));  /* l <= r ? */
  }
  else {
    lua_Number lf = fltvalue(l);  /* 'l' must be float */
    if (ttisfloat(r))
      return luai_numle(lf, fltvalue(r));  /* both are float */
    else if (luai_numisnan(lf))  /* 'r' is int and 'l' is float */
      return 0;  /*  NaN <= i is always false */
    else  /* without NaN, (l <= r)  <-->  not(r < l) */
      return !LTintfloat(ivalue(r), lf);  /* not (r < l) ? */
  }
}


/*
** Main operation less than; return 'l < r'.
*/
int luaV_lessthan (lua_State *L, const TValue *l, const TValue *r) {
  int res;
  if (ttisnumber(l) && ttisnumber(r))  /* both operands are numbers? */
    return LTnum(l, r);
  else if (ttisstring(l) && ttisstring(r))  /* both are strings? */
    return l_strcmp(tsvalue(l), tsvalue(r)) < 0;
  else if ((res = luaT_callorderTM(L, l, r, TM_LT)) < 0)  /* no metamethod? */
    luaG_ordererror(L, l, r);  /* error */
  return res;
}


/*
** Main operation less than or equal to; return 'l <= r'. If it needs
** a metamethod and there is no '__le', try '__lt', based on
** l <= r iff !(r < l) (assuming a total order). If the metamethod
** yields during this substitution, the continuation has to know
** about it (to negate the result of r<l); bit CIST_LEQ in the call
** status keeps that information.
*/
int luaV_lessequal (lua_State *L, const TValue *l, const TValue *r) {
  int res;
  if (ttisnumber(l) && ttisnumber(r))  /* both operands are numbers? */
    return LEnum(l, r);
  else if (ttisstring(l) && ttisstring(r))  /* both are strings? */
    return l_strcmp(tsvalue(l), tsvalue(r)) <= 0;
  else if ((res = luaT_callorderTM(L, l, r, TM_LE)) >= 0)  /* try 'le' */
    return res;
  else {  /* try 'lt': */
    L->ci->callstatus |= CIST_LEQ;  /* mark it is doing 'lt' for 'le' */
    res = luaT_callorderTM(L, r, l, TM_LT);
    L->ci->callstatus ^= CIST_LEQ;  /* clear mark */
    if (res < 0)
      luaG_ordererror(L, l, r);
    return !res;  /* result is negated */
  }
}


/*
** Main operation for equality of Lua values; return 't1 == t2'.
** L == NULL means raw equality (no metamethods)
*/
int luaV_equalobj (lua_State *L, const TValue *t1, const TValue *t2) {
  const TValue *tm;
  if (ttype(t1) != ttype(t2)) {  /* not the same variant? */
    if (ttnov(t1) != ttnov(t2) || ttnov(t1) != LUA_TNUMBER)
      return 0;  /* only numbers can be equal with different variants */
    else {  /* two numbers with different variants */
      lua_Integer i1, i2;  /* compare them as integers */
      return (tointeger(t1, &i1) && tointeger(t2, &i2) && i1 == i2);
    }
  }
  /* values have same type and same variant */
  switch (ttype(t1)) {
    case LUA_TNIL: return 1;
    case LUA_TNUMINT: return (ivalue(t1) == ivalue(t2));
    case LUA_TNUMFLT: return luai_numeq(fltvalue(t1), fltvalue(t2));
    case LUA_TBOOLEAN: return bvalue(t1) == bvalue(t2);  /* true must be 1 !! */
    case LUA_TLIGHTUSERDATA: return pvalue(t1) == pvalue(t2);
    case LUA_TLCF: return fvalue(t1) == fvalue(t2);
    case LUA_TSHRSTR: return eqshrstr(tsvalue(t1), tsvalue(t2));
    case LUA_TLNGSTR: return luaS_eqlngstr(tsvalue(t1), tsvalue(t2));
    case LUA_TUSERDATA: {
      if (uvalue(t1) == uvalue(t2)) return 1;
      else if (L == NULL) return 0;
      tm = fasttm(L, uvalue(t1)->metatable, TM_EQ);
      if (tm == NULL)
        tm = fasttm(L, uvalue(t2)->metatable, TM_EQ);
      break;  /* will try TM */
    }
    case LUA_TTABLE: {
      if (hvalue(t1) == hvalue(t2)) return 1;
      else if (L == NULL) return 0;
      tm = fasttm(L, hvalue(t1)->metatable, TM_EQ);
      if (tm == NULL)
        tm = fasttm(L, hvalue(t2)->metatable, TM_EQ);
      break;  /* will try TM */
    }
    default:
      return gcvalue(t1) == gcvalue(t2);
  }
  if (tm == NULL)  /* no TM? */
    return 0;  /* objects are different */
  luaT_callTM(L, tm, t1, t2, L->top, 1);  /* call TM */
  return !l_isfalse(L->top);
}


/* macro used by 'luaV_concat' to ensure that element at 'o' is a string */
#define tostring(L,o)  \
	(ttisstring(o) || (cvt2str(o) && (luaO_tostring(L, o), 1)))

#define isemptystr(o)	(ttisshrstring(o) && tsvalue(o)->shrlen == 0)

/* copy strings in stack from top - n up to top - 1 to buffer */
static void copy2buff (StkId top, int n, char *buff) {
  size_t tl = 0;  /* size already copied */
  do {
    size_t l = vslen(top - n);  /* length of string being copied */
    memcpy(buff + tl, svalue(top - n), l * sizeof(char));
    tl += l;
  } while (--n > 0);
}


/*
** Main operation for concatenation: concat 'total' values in the stack,
** from 'L->top - total' up to 'L->top - 1'.
*/
void luaV_concat (lua_State *L, int total) {
  lua_assert(total >= 2);
  do {
    StkId top = L->top;
    int n = 2;  /* number of elements handled in this pass (at least 2) */
    if (!(ttisstring(top-2) || cvt2str(top-2)) || !tostring(L, top-1))
      luaT_trybinTM(L, top-2, top-1, top-2, TM_CONCAT);
    else if (isemptystr(top - 1))  /* second operand is empty? */
      cast_void(tostring(L, top - 2));  /* result is first operand */
    else if (isemptystr(top - 2)) {  /* first operand is an empty string? */
      setobjs2s(L, top - 2, top - 1);  /* result is second op. */
    }
    else {
      /* at least two non-empty string values; get as many as possible */
      size_t tl = vslen(top - 1);
      TString *ts;
      /* collect total length and number of strings */
      for (n = 1; n < total && tostring(L, top - n - 1); n++) {
        size_t l = vslen(top - n - 1);
        if (l >= (MAX_SIZE/sizeof(char)) - tl)
          luaG_runerror(L, "string length overflow");
        tl += l;
      }
      if (tl <= LUAI_MAXSHORTLEN) {  /* is result a short string? */
        char buff[LUAI_MAXSHORTLEN];
        copy2buff(top, n, buff);  /* copy strings to buffer */
        ts = luaS_newlstr(L, buff, tl);
      }
      else {  /* long string; copy strings directly to final result */
        ts = luaS_createlngstrobj(L, tl);
        copy2buff(top, n, getstr(ts));
      }
      setsvalue2s(L, top - n, ts);  /* create result */
    }
    total -= n-1;  /* got 'n' strings to create 1 new */
    L->top -= n-1;  /* popped 'n' strings and pushed one */
  } while (total > 1);  /* repeat until only 1 result left */
}


/*
** Main operation 'ra' = #rb'.
*/
void luaV_objlen (lua_State *L, StkId ra, const TValue *rb) {
  const TValue *tm;
  switch (ttype(rb)) {
    case LUA_TTABLE: {
      Table *h = hvalue(rb);
      tm = fasttm(L, h->metatable, TM_LEN);
      if (tm) break;  /* metamethod? break switch to call it */
      setivalue(ra, luaH_getn(h));  /* else primitive len */
      return;
    }
    case LUA_TSHRSTR: {
      setivalue(ra, tsvalue(rb)->shrlen);
      return;
    }
    case LUA_TLNGSTR: {
      setivalue(ra, tsvalue(rb)->u.lnglen);
      return;
    }
    default: {  /* try metamethod */
      tm = luaT_gettmbyobj(L, rb, TM_LEN);
      if (ttisnil(tm))  /* no metamethod? */
        luaG_typeerror(L, rb, "get length of");
      break;
    }
  }
  luaT_callTM(L, tm, rb, rb, ra, 1);
}


/*
** Integer division; return 'm // n', that is, floor(m/n).
** C division truncates its result (rounds towards zero).
** 'floor(q) == trunc(q)' when 'q >= 0' or when 'q' is integer,
** otherwise 'floor(q) == trunc(q) - 1'.
*/
lua_Integer luaV_div (lua_State *L, lua_Integer m, lua_Integer n) {
  if (l_castS2U(n) + 1u <= 1u) {  /* special cases: -1 or 0 */
    if (n == 0)
      luaG_runerror(L, "attempt to divide by zero");
    return intop(-, 0, m);   /* n==-1; avoid overflow with 0x80000...//-1 */
  }
  else {
    lua_Integer q = m / n;  /* perform C division */
    if ((m ^ n) < 0 && m % n != 0)  /* 'm/n' would be negative non-integer? */
      q -= 1;  /* correct result for different rounding */
    return q;
  }
}


/*
** Integer modulus; return 'm % n'. (Assume that C '%' with
** negative operands follows C99 behavior. See previous comment
** about luaV_div.)
*/
lua_Integer luaV_mod (lua_State *L, lua_Integer m, lua_Integer n) {
  if (l_castS2U(n) + 1u <= 1u) {  /* special cases: -1 or 0 */
    if (n == 0)
      luaG_runerror(L, "attempt to perform 'n%%0'");
    return 0;   /* m % -1 == 0; avoid overflow with 0x80000...%-1 */
  }
  else {
    lua_Integer r = m % n;
    if (r != 0 && (m ^ n) < 0)  /* 'm/n' would be non-integer negative? */
      r += n;  /* correct result for different rounding */
    return r;
  }
}


/* number of bits in an integer */
#define NBITS	cast_int(sizeof(lua_Integer) * CHAR_BIT)

/*
** Shift left operation. (Shift right just negates 'y'.)
*/
lua_Integer luaV_shiftl (lua_Integer x, lua_Integer y) {
  if (y < 0) {  /* shift right? */
    if (y <= -NBITS) return 0;
    else return intop(>>, x, -y);
  }
  else {  /* shift left */
    if (y >= NBITS) return 0;
    else return intop(<<, x, y);
  }
}


/*
** check whether cached closure in prototype 'p' may be reused, that is,
** whether there is a cached closure with the same upvalues needed by
** new closure to be created.
*/
static LClosure *getcached (Proto *p, UpVal **encup, StkId base) {
  LClosure *c = p->cache;
  if (c != NULL) {  /* is there a cached closure? */
    int nup = p->sizeupvalues;
    Upvaldesc *uv = p->upvalues;
    int i;
    for (i = 0; i < nup; i++) {  /* check whether it has right upvalues */
      TValue *v = uv[i].instack ? base + uv[i].idx : encup[uv[i].idx]->v;
      if (c->upvals[i]->v != v)
        return NULL;  /* wrong upvalue; cannot reuse closure */
    }
  }
  return c;  /* return cached closure (or NULL if no cached closure) */
}


/*
** create a new Lua closure, push it in the stack, and initialize
** its upvalues. Note that the closure is not cached if prototype is
** already black (which means that 'cache' was already cleared by the
** GC).
*/
static void pushclosure (lua_State *L, Proto *p, UpVal **encup, StkId base,
                         StkId ra) {
  int nup = p->sizeupvalues;
  Upvaldesc *uv = p->upvalues;
  int i;
  LClosure *ncl = luaF_newLclosure(L, nup);
  ncl->p = p;
  setclLvalue(L, ra, ncl);  /* anchor new closure in stack */
  for (i = 0; i < nup; i++) {  /* fill in its upvalues */
    if (uv[i].instack)  /* upvalue refers to local variable? */
      ncl->upvals[i] = luaF_findupval(L, base + uv[i].idx);
    else  /* get upvalue from enclosing function */
      ncl->upvals[i] = encup[uv[i].idx];
    ncl->upvals[i]->refcount++;
    /* new closure is white, so we do not need a barrier here */
  }
  if (!isblack(p))  /* cache will not break GC invariant? */
    p->cache = ncl;  /* save it on cache for reuse */
}


/*
** finish execution of an opcode interrupted by an yield
*/
void luaV_finishOp (lua_State *L) {
  CallInfo *ci = L->ci;
  StkId base = ci->u.l.base;
  Instruction inst = *(ci->u.l.savedpc - 1);  /* interrupted instruction */
  OpCode op = GET_OPCODE(inst);
  switch (op) {  /* finish its execution */
    case OP_ADD: case OP_SUB: case OP_MUL: case OP_DIV: case OP_IDIV:
    case OP_BAND: case OP_BOR: case OP_BXOR: case OP_SHL: case OP_SHR:
    case OP_MOD: case OP_POW:
    case OP_UNM: case OP_BNOT: case OP_LEN:
    case OP_GETTABUP: case OP_GETTABLE: case OP_SELF: {
      setobjs2s(L, base + GETARG_A(inst), --L->top);
      break;
    }
    case OP_LE: case OP_LT: case OP_EQ: {
      int res = !l_isfalse(L->top - 1);
      L->top--;
      if (ci->callstatus & CIST_LEQ) {  /* "<=" using "<" instead? */
        lua_assert(op == OP_LE);
        ci->callstatus ^= CIST_LEQ;  /* clear mark */
        res = !res;  /* negate result */
      }
      lua_assert(GET_OPCODE(*ci->u.l.savedpc) == OP_JMP);
      if (res != GETARG_A(inst))  /* condition failed? */
        ci->u.l.savedpc++;  /* skip jump instruction */
      break;
    }
    case OP_CONCAT: {
      StkId top = L->top - 1;  /* top when 'luaT_trybinTM' was called */
      int b = GETARG_B(inst);      /* first element to concatenate */
      int total = cast_int(top - 1 - (base + b));  /* yet to concatenate */
      setobj2s(L, top - 2, top);  /* put TM result in proper position */
      if (total > 1) {  /* are there elements to concat? */
        L->top = top - 1;  /* top is one after last element (at top-2) */
        luaV_concat(L, total);  /* concat them (may yield again) */
      }
      /* move final result to final position */
      setobj2s(L, ci->u.l.base + GETARG_A(inst), L->top - 1);
      L->top = ci->top;  /* restore top */
      break;
    }
    case OP_TFORCALL: {
      lua_assert(GET_OPCODE(*ci->u.l.savedpc) == OP_TFORLOOP);
      L->top = ci->top;  /* correct top */
      break;
    }
    case OP_CALL: {
      if (GETARG_C(inst) - 1 >= 0)  /* nresults >= 0? */
        L->top = ci->top;  /* adjust results */
      break;
    }
    case OP_TAILCALL: case OP_SETTABUP: case OP_SETTABLE:
      break;
    default: lua_assert(0);
  }
}




/*
** {==================================================================
** Function 'luaV_execute': main interpreter loop
** ===================================================================
*/


/*
** some macros for common tasks in 'luaV_execute'
*/


#define RA(i)	(base+GETARG_A(i))
#define RB(i)	check_exp(getBMode(GET_OPCODE(i)) == OpArgR, base+GETARG_B(i))
#define RC(i)	check_exp(getCMode(GET_OPCODE(i)) == OpArgR, base+GETARG_C(i))
#define RKB(i)	check_exp(getBMode(GET_OPCODE(i)) == OpArgK, \
	ISK(GETARG_B(i)) ? k+INDEXK(GETARG_B(i)) : base+GETARG_B(i))
#define RKC(i)	check_exp(getCMode(GET_OPCODE(i)) == OpArgK, \
	ISK(GETARG_C(i)) ? k+INDEXK(GETARG_C(i)) : base+GETARG_C(i))


/* execute a jump instruction */
#define dojump(ci,i,e) \
  { int a = GETARG_A(i); \
    if (a != 0) luaF_close(L, ci->u.l.base + a - 1); \
    ci->u.l.savedpc += GETARG_sBx(i) + e; }

/* for test instructions, execute the jump instruction that follows it */
#define donextjump(ci)	{ i = *ci->u.l.savedpc; dojump(ci, i, 1); }


#define Protect(x)	{ {x;}; base = ci->u.l.base; }

#define checkGC(L,c)  \
	{ luaC_condGC(L, L->top = (c),  /* limit of live values */ \
                         Protect(L->top = ci->top));  /* restore top */ \
           luai_threadyield(L); }


/* fetch an instruction and prepare its execution */
#define vmfetch()	{ \
  i = *(ci->u.l.savedpc++); \
  if (L->hookmask & (LUA_MASKLINE | LUA_MASKCOUNT)) \
    Protect(luaG_traceexec(L)); \
  ra = RA(i); /* WARNING: any stack reallocation invalidates 'ra' */ \
  lua_assert(base == ci->u.l.base); \
  lua_assert(base <= L->top && L->top < L->stack + L->stacksize); \
}

#define vmdispatch(o)	switch(o)
#define vmcase(l)	case l:
#define vmbreak		break


/*
** copy of 'luaV_gettable', but protecting the call to potential
** metamethod (which can reallocate the stack)
*/
#define gettableProtected(L,t,k,v)  { const TValue *slot; \
  if (luaV_fastget(L,t,k,slot,luaH_get)) { setobj2s(L, v, slot); } \
  else Protect(luaV_finishget(L,t,k,v,slot)); }


/* same for 'luaV_settable' */
#define settableProtected(L,t,k,v) { const TValue *slot; \
  if (!luaV_fastset(L,t,k,slot,luaH_get,v)) \
    Protect(luaV_finishset(L,t,k,v,slot)); }



```c
/**
 * Executes the Lua bytecode instructions for the current function in the Lua state.
 *
 * This function is the core of the Lua virtual machine (VM). It interprets and executes
 * the bytecode instructions of the current Lua function. The function operates on the
 * Lua state `L`, which contains the call stack, execution context, and other runtime
 * information. The VM processes instructions such as arithmetic operations, control
 * flow, function calls, and table manipulation, updating the Lua state accordingly.
 *
 * The function enters a main loop where it fetches and dispatches instructions based
 * on their opcodes. Each instruction is handled by a corresponding case in a large
 * switch statement. The VM ensures proper stack management, error handling, and
 * garbage collection during execution.
 *
 * @param L Pointer to the Lua state, which contains the execution context and stack.
 *
 * The function supports reentry points (e.g., `newframe`) for handling function calls
 * and returns. It also manages tail calls, closures, and vararg handling. The VM
 * ensures that the Lua state remains consistent and valid throughout execution.
 *
 * This function is critical for the execution of Lua scripts and is called repeatedly
 * as the VM processes the bytecode of Lua functions.
 */
void luaV_execute (lua_State *L) {
  CallInfo *ci = L->ci;
  LClosure *cl;
  TValue *k;
  StkId base;
  ci->callstatus |= CIST_FRESH;  /* fresh invocation of 'luaV_execute" */
 newframe:  /* reentry point when frame changes (call/return) */
  lua_assert(ci == L->ci);
  cl = clLvalue(ci->func);  /* local reference to function's closure */
  k = cl->p->k;  /* local reference to function's constant table */
  base = ci->u.l.base;  /* local copy of function's base */
  /* main loop of interpreter */
  for (;;) {
    Instruction i;
    StkId ra;
    vmfetch();
    vmdispatch (GET_OPCODE(i)) {
      /**
       * Executes the OP_MOVE virtual machine instruction.
       * 
       * This method is responsible for moving a value from one register to another within the Lua virtual machine.
       * Specifically, it copies the value from the source register `RB(i)` to the destination register `ra`.
       * 
       * @param L The Lua state, which contains the current execution context.
       * @param ra The destination register where the value will be stored.
       * @param RB(i) The source register from which the value will be copied.
       * 
       * The `vmbreak` statement is used to signal the end of the instruction execution, allowing the virtual
       * machine to proceed to the next instruction.
       */
      vmcase(OP_MOVE) {
              setobjs2s(L, ra, RB(i));
              vmbreak;
            }
      /**
       * Executes the OP_LOADK instruction in the Lua virtual machine.
       * 
       * This method loads a constant value from the constant table into a register.
       * The constant index is extracted from the instruction argument (GETARG_Bx(i)),
       * and the corresponding constant value is fetched from the constant table (k).
       * The value is then assigned to the destination register (ra) in the Lua state (L).
       * 
       * @param L The Lua state, which holds the execution context.
       * @param ra The destination register where the constant value will be stored.
       * @param k The constant table containing the constant values.
       * @param i The current instruction being executed, containing the constant index.
       */
      vmcase(OP_LOADK) {
              TValue *rb = k + GETARG_Bx(i);
              setobj2s(L, ra, rb);
              vmbreak;
            }
      /**
       * Executes the OP_LOADKX instruction in the Lua virtual machine.
       * 
       * This method is responsible for loading an extra constant from the constant table
       * into a register. The constant is identified by an extended argument (Ax) which
       * is fetched from the current instruction pointed to by the saved program counter (savedpc).
       * 
       * The method first asserts that the current instruction is OP_EXTRAARG, ensuring that
       * the extended argument is correctly positioned. It then calculates the address of the
       * constant in the constant table (k) using the extended argument (Ax). The constant
       * is then loaded into the destination register (ra) using the `setobj2s` function.
       * 
       * Finally, the program counter (savedpc) is incremented to point to the next instruction,
       * and the virtual machine breaks to continue execution.
       */
      vmcase(OP_LOADKX) {
              TValue *rb;
              lua_assert(GET_OPCODE(*ci->u.l.savedpc) == OP_EXTRAARG);
              rb = k + GETARG_Ax(*ci->u.l.savedpc++);
              setobj2s(L, ra, rb);
              vmbreak;
            }
      /**
       * Executes the OP_LOADBOOL instruction.
       * 
       * This method sets the boolean value in the register `ra` based on the argument `B` of the instruction.
       * If the argument `C` is non-zero, it increments the saved program counter (`savedpc`) to skip the next instruction.
       * 
       * @param ra The register where the boolean value will be stored.
       * @param i The instruction containing the arguments `B` and `C`.
       * @param ci The current call info structure containing the saved program counter.
       */
      vmcase(OP_LOADBOOL) {
              setbvalue(ra, GETARG_B(i));
              if (GETARG_C(i)) ci->u.l.savedpc++;  /* skip next instruction (if C) */
              vmbreak;
            }
      /**
       * Handles the OP_LOADNIL instruction in the virtual machine.
       * 
       * This method sets a specified number of consecutive registers to nil.
       * The number of registers to set is determined by the argument `b` of the instruction.
       * The method starts by obtaining the value of `b` using the GETARG_B macro.
       * It then iterates `b` times, setting each register in the sequence to nil using the `setnilvalue` function.
       * The `ra` pointer is incremented after each iteration to move to the next register.
       * After all specified registers are set to nil, the method breaks out of the virtual machine loop using `vmbreak`.
       *
       * @param i The instruction containing the OP_LOADNIL opcode and its arguments.
       */
      vmcase(OP_LOADNIL) {
              int b = GETARG_B(i);
              do {
                setnilvalue(ra++);
              } while (b--);
              vmbreak;
            }
      /**
       * Handles the OP_GETUPVAL opcode in the Lua virtual machine.
       * This opcode is used to retrieve the value of an upvalue from the current closure
       * and store it in the specified register.
       *
       * @param L The Lua state.
       * @param ra The destination register where the upvalue will be stored.
       * @param cl The current closure containing the upvalues.
       * @param i The instruction being executed, which encodes the upvalue index.
       *
       * The method performs the following steps:
       * 1. Extracts the upvalue index (b) from the instruction using the GETARG_B macro.
       * 2. Retrieves the value of the upvalue at index (b) from the closure's upvalue array.
       * 3. Stores the retrieved value in the destination register (ra) using the setobj2s function.
       * 4. Executes the vmbreak macro to proceed to the next instruction in the virtual machine.
       */
      vmcase(OP_GETUPVAL) {
              int b = GETARG_B(i);
              setobj2s(L, ra, cl->upvals[b]->v);
              vmbreak;
            }
      /**
       * Executes the OP_GETTABUP bytecode instruction.
       * 
       * This method retrieves a value from an upvalue table and stores it in the
       * specified register. The upvalue is identified by the B argument of the
       * instruction, which is used to index into the closure's upvalue array.
       * The key used to access the table is determined by the C argument of the
       * instruction, which is resolved to a TValue pointer. The `gettableProtected`
       * function is then called to perform the table lookup in a protected manner,
       * ensuring that any errors during the lookup are properly handled.
       * 
       * @param L The Lua state.
       * @param cl The closure containing the upvalues.
       * @param i The instruction being executed.
       * @param ra The register where the result will be stored.
       */
      vmcase(OP_GETTABUP) {
              TValue *upval = cl->upvals[GETARG_B(i)]->v;
              TValue *rc = RKC(i);
              gettableProtected(L, upval, rc, ra);
              vmbreak;
            }
      /**
       * Executes the OP_GETTABLE bytecode instruction.
       *
       * This method retrieves a value from a table using a key and stores the result in a register.
       * It performs the following steps:
       * 1. Retrieves the base register `rb` from the instruction `i`.
       * 2. Retrieves the key `rc` from the instruction `i`.
       * 3. Calls `gettableProtected` to safely access the table value using the base register `rb` and key `rc`.
       * 4. Stores the result in the destination register `ra`.
       * 5. Continues execution to the next bytecode instruction.
       *
       * @param L The Lua state.
       * @param i The current instruction being executed.
       * @param ra The destination register where the result will be stored.
       */
      vmcase(OP_GETTABLE) {
              StkId rb = RB(i);
              TValue *rc = RKC(i);
              gettableProtected(L, rb, rc, ra);
              vmbreak;
            }
      /**
       * Handles the OP_SETTABUP opcode, which sets a value in an upvalue table.
       *
       * This method retrieves the upvalue from the closure's upvalue table using the
       * argument A from the instruction. It then retrieves the key and value from the
       * instruction's B and C arguments, respectively. Finally, it sets the value in
       * the upvalue table using the key, ensuring the operation is protected against
       * errors. The method then proceeds to the next instruction in the virtual machine.
       *
       * @param L The Lua state.
       * @param cl The closure containing the upvalue table.
       * @param i The current instruction being executed.
       */
      vmcase(OP_SETTABUP) {
              TValue *upval = cl->upvals[GETARG_A(i)]->v;
              TValue *rb = RKB(i);
              TValue *rc = RKC(i);
              settableProtected(L, upval, rb, rc);
              vmbreak;
            }
      /**
       * Handles the OP_SETUPVAL instruction in the Lua virtual machine.
       * 
       * This method is responsible for setting the value of an upvalue (a variable
       * captured by a closure from an outer scope) in the current Lua state. It 
       * retrieves the upvalue from the closure's upvalue table using the argument 
       * B of the instruction, then assigns the value from the register 'ra' to 
       * the upvalue. After setting the value, it ensures proper memory barrier 
       * handling for the upvalue to maintain consistency in the garbage collector.
       * 
       * @param L The Lua state in which the operation is performed.
       * @param cl The closure containing the upvalue table.
       * @param ra The register containing the value to be assigned to the upvalue.
       * @param i The instruction being executed, from which argument B is extracted.
       */
      vmcase(OP_SETUPVAL) {
              UpVal *uv = cl->upvals[GETARG_B(i)];
              setobj(L, uv->v, ra);
              luaC_upvalbarrier(L, uv);
              vmbreak;
            }
      /**
       * Executes the OP_SETTABLE operation, which sets a value in a table.
       *
       * This method retrieves the key and value from the instruction's operands using `RKB(i)` and `RKC(i)`,
       * respectively. It then calls `settableProtected` to set the value in the table referenced by `ra` using
       * the key `rb` and value `rc`. The operation is performed in a protected manner to handle potential
       * errors gracefully. Finally, the method breaks out of the virtual machine loop using `vmbreak`.
       *
       * @param L The Lua state.
       * @param ra The table to set the value in.
       * @param rb The key to use for setting the value.
       * @param rc The value to set in the table.
       */
      vmcase(OP_SETTABLE) {
          TValue *rb = RKB(i);
          TValue *rc = RKC(i);
          settableProtected(L, ra, rb, rc);
          vmbreak;
      }
      /**
       * Handles the OP_NEWTABLE instruction in the Lua virtual machine.
       * 
       * This method creates a new Lua table and assigns it to the register `ra`.
       * The size of the table can be optionally specified using the `b` and `c` arguments.
       * If either `b` or `c` is non-zero, the table is resized accordingly using `luaH_resize`.
       * The method also ensures that garbage collection is checked after the operation.
       * 
       * @param L The Lua state.
       * @param ra The register where the new table will be stored.
       * @param i The instruction containing the arguments `b` and `c`.
       */
      vmcase(OP_NEWTABLE) {
              int b = GETARG_B(i);
              int c = GETARG_C(i);
              Table *t = luaH_new(L);
              sethvalue(L, ra, t);
              if (b != 0 || c != 0)
                luaH_resize(L, t, luaO_fb2int(b), luaO_fb2int(c));
              checkGC(L, ra + 1);
              vmbreak;
            }
      /**
       * Handles the OP_SELF instruction in the Lua virtual machine.
       * This instruction is used to implement the 'self' syntax in Lua, which is typically used in object-oriented programming.
       * The method retrieves the method table (metatable) of the object and the method name (key) from the stack.
       * It then sets the object itself (receiver) and the method name in the appropriate stack positions.
       * If the method is found in the method table, it is placed on the stack.
       * If the method is not found, it calls `luaV_finishget` to handle the lookup and potential errors.
       *
       * @param L The Lua state.
       * @param rb The stack index of the receiver (object).
       * @param rc The stack index of the method name (key).
       * @param ra The stack index where the result (method) will be stored.
       * @param aux Auxiliary variable used for temporary storage during lookup.
       */
      vmcase(OP_SELF) {
              const TValue *aux;
              StkId rb = RB(i);
              TValue *rc = RKC(i);
              TString *key = tsvalue(rc);  /* key must be a string */
              setobjs2s(L, ra + 1, rb);
              if (luaV_fastget(L, rb, key, aux, luaH_getstr)) {
                setobj2s(L, ra, aux);
              }
              else Protect(luaV_finishget(L, rb, rc, ra, aux));
              vmbreak;
            }
      /**
       * Handles the OP_ADD virtual machine instruction, which performs addition of two values.
       * 
       * This method retrieves the two operands from the registers specified by the instruction.
       * It first checks if both operands are integers. If they are, it performs integer addition
       * and stores the result in the destination register. If either operand is not an integer,
       * it attempts to convert both operands to floating-point numbers. If successful, it performs
       * floating-point addition and stores the result in the destination register. If neither
       * conversion is successful, it invokes the metamethod for addition to handle the operation.
       * 
       * @param rb Pointer to the first operand (TValue).
       * @param rc Pointer to the second operand (TValue).
       * @param ra Pointer to the destination register where the result will be stored.
       * @param L Pointer to the Lua state.
       * @param i The instruction containing the indices of the operands and the destination register.
       */
      vmcase(OP_ADD) {
              TValue *rb = RKB(i);
              TValue *rc = RKC(i);
              lua_Number nb; lua_Number nc;
              if (ttisinteger(rb) && ttisinteger(rc)) {
                lua_Integer ib = ivalue(rb); lua_Integer ic = ivalue(rc);
                setivalue(ra, intop(+, ib, ic));
              }
              else if (tonumber(rb, &nb) && tonumber(rc, &nc)) {
                setfltvalue(ra, luai_numadd(L, nb, nc));
              }
              else { Protect(luaT_trybinTM(L, rb, rc, ra, TM_ADD)); }
              vmbreak;
            }
      /**
       * Handles the subtraction operation (OP_SUB) in the Lua virtual machine.
       * 
       * This method performs subtraction on two operands `rb` and `rc`, which are retrieved from the 
       * instruction's operands using `RKB(i)` and `RKC(i)`. The result is stored in the register `ra`.
       * 
       * The method first checks if both operands are integers. If so, it performs integer subtraction 
       * using `intop(-, ib, ic)` and stores the result in `ra` as an integer using `setivalue(ra, ...)`.
       * 
       * If the operands are not integers, the method attempts to convert them to Lua numbers (`lua_Number`) 
       * using `tonumber(rb, &nb)` and `tonumber(rc, &nc)`. If successful, it performs floating-point 
       * subtraction using `luai_numsub(L, nb, nc)` and stores the result in `ra` as a float using 
       * `setfltvalue(ra, ...)`.
       * 
       * If neither integer nor floating-point subtraction is possible, the method invokes the metamethod 
       * `TM_SUB` using `luaT_trybinTM(L, rb, rc, ra, TM_SUB)` to handle the operation, which is protected 
       * by the `Protect` macro to ensure proper error handling.
       * 
       * Finally, the method executes `vmbreak` to proceed to the next virtual machine instruction.
       */
      vmcase(OP_SUB) {
              TValue *rb = RKB(i);
              TValue *rc = RKC(i);
              lua_Number nb; lua_Number nc;
              if (ttisinteger(rb) && ttisinteger(rc)) {
                lua_Integer ib = ivalue(rb); lua_Integer ic = ivalue(rc);
                setivalue(ra, intop(-, ib, ic));
              }
              else if (tonumber(rb, &nb) && tonumber(rc, &nc)) {
                setfltvalue(ra, luai_numsub(L, nb, nc));
              }
              else { Protect(luaT_trybinTM(L, rb, rc, ra, TM_SUB)); }
              vmbreak;
            }
      /**
       * Handles the OP_MUL bytecode instruction, which performs multiplication.
       * 
       * This function multiplies two values from the Lua virtual machine's stack 
       * and stores the result in the destination register. The values can be either 
       * integers or floating-point numbers. If both values are integers, the 
       * multiplication is performed using integer arithmetic. If either value is a 
       * floating-point number or can be converted to one, the multiplication is 
       * performed using floating-point arithmetic. If the values are not numbers 
       * and cannot be converted to numbers, the function attempts to invoke the 
       * metamethod for multiplication (__mul) if available.
       *
       * @param rb Pointer to the first operand (TValue).
       * @param rc Pointer to the second operand (TValue).
       * @param ra Pointer to the destination register where the result will be stored.
       * @param L Pointer to the Lua state.
       * @param i The instruction being executed.
       */
      vmcase(OP_MUL) {
              TValue *rb = RKB(i);
              TValue *rc = RKC(i);
              lua_Number nb; lua_Number nc;
              if (ttisinteger(rb) && ttisinteger(rc)) {
                lua_Integer ib = ivalue(rb); lua_Integer ic = ivalue(rc);
                setivalue(ra, intop(*, ib, ic));
              }
              else if (tonumber(rb, &nb) && tonumber(rc, &nc)) {
                setfltvalue(ra, luai_nummul(L, nb, nc));
              }
              else { Protect(luaT_trybinTM(L, rb, rc, ra, TM_MUL)); }
              vmbreak;
            }
      /**
       * Handles the OP_DIV virtual machine operation, which performs floating-point division.
       * This operation always treats the operands as floating-point numbers, regardless of their actual types.
       * 
       * The method retrieves the operands from the virtual machine's registers using RKB(i) and RKC(i).
       * It then attempts to convert these operands to floating-point numbers using the `tonumber` function.
       * If both operands are successfully converted, the division is performed using `luai_numdiv`,
       * and the result is stored in the destination register `ra` using `setfltvalue`.
       * 
       * If either operand cannot be converted to a floating-point number, the method falls back to
       * invoking the appropriate metamethod (TM_DIV) using `luaT_trybinTM` to handle the operation.
       * 
       * The `Protect` macro ensures that any errors during the execution of the metamethod are properly handled.
       * Finally, the method proceeds to the next virtual machine instruction using `vmbreak`.
       */
      vmcase(OP_DIV) {  /* float division (always with floats) */
              TValue *rb = RKB(i);
              TValue *rc = RKC(i);
              lua_Number nb; lua_Number nc;
              if (tonumber(rb, &nb) && tonumber(rc, &nc)) {
                setfltvalue(ra, luai_numdiv(L, nb, nc));
              }
              else { Protect(luaT_trybinTM(L, rb, rc, ra, TM_DIV)); }
              vmbreak;
            }
      /**
       * Performs a bitwise AND operation on two values and stores the result in the 
       * destination register. This method handles the OP_BAND virtual machine instruction.
       *
       * The method first retrieves the operands from the registers specified by the 
       * instruction. It then attempts to convert these operands to integers. If both 
       * operands are successfully converted to integers, a bitwise AND operation is 
       * performed on them, and the result is stored in the destination register. 
       * If either operand cannot be converted to an integer, the method falls back 
       * to calling the appropriate metamethod (TM_BAND) to handle the operation.
       *
       * @param OP_BAND The virtual machine instruction indicating a bitwise AND operation.
       */
      vmcase(OP_BAND) {
              TValue *rb = RKB(i);
              TValue *rc = RKC(i);
              lua_Integer ib; lua_Integer ic;
              if (tointeger(rb, &ib) && tointeger(rc, &ic)) {
                setivalue(ra, intop(&, ib, ic));
              }
              else { Protect(luaT_trybinTM(L, rb, rc, ra, TM_BAND)); }
              vmbreak;
            }
      /**
       * Executes the OP_BOR (bitwise OR) operation in the Lua virtual machine.
       * 
       * This method performs a bitwise OR operation on two integer values. It retrieves the values from the 
       * registers specified by the instruction's operands (RKB and RKC). If both values are integers, it 
       * computes the bitwise OR of the two values and stores the result in the destination register (ra). 
       * If either of the values is not an integer, it attempts to invoke the corresponding metamethod (TM_BOR) 
       * to handle the operation. If the metamethod is not available, an error may be raised.
       * 
       * @param OP_BOR The opcode representing the bitwise OR operation.
       * @param rb Pointer to the first operand (TValue).
       * @param rc Pointer to the second operand (TValue).
       * @param ib The integer value of the first operand if it is an integer.
       * @param ic The integer value of the second operand if it is an integer.
       * @param ra The destination register where the result will be stored.
       * @param L The Lua state.
       * @param Protect A macro to handle potential errors during metamethod invocation.
       * @param vmbreak A macro to break out of the virtual machine loop.
       */
      vmcase(OP_BOR) {
              TValue *rb = RKB(i);
              TValue *rc = RKC(i);
              lua_Integer ib; lua_Integer ic;
              if (tointeger(rb, &ib) && tointeger(rc, &ic)) {
                setivalue(ra, intop(|, ib, ic));
              }
              else { Protect(luaT_trybinTM(L, rb, rc, ra, TM_BOR)); }
              vmbreak;
            }
      /**
       * Executes the bitwise XOR operation for the Lua virtual machine.
       * 
       * This method performs a bitwise XOR operation on two integer values obtained from the
       * Lua stack. It retrieves the values from the registers specified by the instruction
       * operands `RKB(i)` and `RKC(i)`. If both values can be converted to Lua integers,
       * the bitwise XOR operation is performed using the `^` operator, and the result is
       * stored in the register specified by `ra`. If either value cannot be converted to an
       * integer, the method attempts to invoke the corresponding metamethod `TM_BXOR` using
       * `luaT_trybinTM` to handle the operation.
       * 
       * @param OP_BXOR The opcode representing the bitwise XOR operation.
       * @param ra The destination register for the result.
       * @param rb The first operand register.
       * @param rc The second operand register.
       * @param L The Lua state.
       * @param i The instruction being executed.
       */
      vmcase(OP_BXOR) {
              TValue *rb = RKB(i);
              TValue *rc = RKC(i);
              lua_Integer ib; lua_Integer ic;
              if (tointeger(rb, &ib) && tointeger(rc, &ic)) {
                setivalue(ra, intop(^, ib, ic));
              }
              else { Protect(luaT_trybinTM(L, rb, rc, ra, TM_BXOR)); }
              vmbreak;
            }
      /**
       * Implements the OP_SHL (shift left) operation in the Lua virtual machine.
       * 
       * This method retrieves the two operands `rb` and `rc` from the instruction's 
       * operands using `RKB(i)` and `RKC(i)`. It then attempts to convert these 
       * operands to Lua integers `ib` and `ic` using `tointeger()`. If successful, 
       * it performs a left shift operation on `ib` by `ic` bits using `luaV_shiftl()`, 
       * and stores the result in the register `ra` using `setivalue()`. If the 
       * conversion to integers fails, it invokes the metamethod `TM_SHL` using 
       * `luaT_trybinTM()` to handle the operation. Finally, the method breaks 
       * the virtual machine execution flow using `vmbreak`.
       */
      vmcase(OP_SHL) {
              TValue *rb = RKB(i);
              TValue *rc = RKC(i);
              lua_Integer ib; lua_Integer ic;
              if (tointeger(rb, &ib) && tointeger(rc, &ic)) {
                setivalue(ra, luaV_shiftl(ib, ic));
              }
              else { Protect(luaT_trybinTM(L, rb, rc, ra, TM_SHL)); }
              vmbreak;
            }
      /**
       * Handles the OP_SHR (Shift Right) operation in the Lua virtual machine.
       * 
       * This method performs a bitwise right shift operation on two integer values.
       * It retrieves the values of the operands from the registers specified by the
       * instruction. If both operands are integers, it shifts the first operand (rb)
       * to the right by the number of bits specified by the second operand (rc).
       * The result is then stored in the destination register (ra).
       * 
       * If either of the operands is not an integer, the method attempts to invoke
       * the corresponding metamethod (TM_SHR) to handle the operation. If no
       * metamethod is available, an error is raised.
       * 
       * @param OP_SHR The opcode representing the shift right operation.
       * @param rb Pointer to the first operand (value to be shifted).
       * @param rc Pointer to the second operand (number of bits to shift).
       * @param ra Pointer to the destination register where the result is stored.
       * @param ib The integer value of the first operand.
       * @param ic The integer value of the second operand.
       * @param L The Lua state.
       * @param Protect Wrapper macro to handle errors during metamethod invocation.
       * @param vmbreak Macro to break out of the virtual machine loop.
       */
      vmcase(OP_SHR) {
              TValue *rb = RKB(i);
              TValue *rc = RKC(i);
              lua_Integer ib; lua_Integer ic;
              if (tointeger(rb, &ib) && tointeger(rc, &ic)) {
                setivalue(ra, luaV_shiftl(ib, -ic));
              }
              else { Protect(luaT_trybinTM(L, rb, rc, ra, TM_SHR)); }
              vmbreak;
            }
      /**
       * @brief Performs the modulo operation (`%`) on two values and stores the result in the destination register.
       *
       * This method handles the modulo operation for both integer and floating-point operands. 
       * It first checks if both operands are integers. If so, it performs the integer modulo operation 
       * using `luaV_mod` and stores the result in the destination register `ra`. 
       * If the operands are not integers, it attempts to convert them to floating-point numbers 
       * using `tonumber`. If successful, it performs the floating-point modulo operation using 
       * `luai_nummod` and stores the result in `ra`. 
       * If neither of the above cases applies, it invokes the metamethod `TM_MOD` via `luaT_trybinTM` 
       * to handle the operation, ensuring proper behavior for custom types.
       *
       * @param OP_MOD The opcode representing the modulo operation.
       * @param rb Pointer to the first operand (right-hand side).
       * @param rc Pointer to the second operand (right-hand side).
       * @param ra Pointer to the destination register where the result will be stored.
       * @param L The Lua state.
       * @param i The instruction index.
       */
      vmcase(OP_MOD) {
              TValue *rb = RKB(i);
              TValue *rc = RKC(i);
              lua_Number nb; lua_Number nc;
              if (ttisinteger(rb) && ttisinteger(rc)) {
                lua_Integer ib = ivalue(rb); lua_Integer ic = ivalue(rc);
                setivalue(ra, luaV_mod(L, ib, ic));
              }
              else if (tonumber(rb, &nb) && tonumber(rc, &nc)) {
                lua_Number m;
                luai_nummod(L, nb, nc, m);
                setfltvalue(ra, m);
              }
              else { Protect(luaT_trybinTM(L, rb, rc, ra, TM_MOD)); }
              vmbreak;
            }
      /**
       * Implements the floor division operation (IDIV) in the Lua virtual machine.
       * This method performs floor division on two operands, `rb` and `rc`, and stores the result in `ra`.
       * 
       * The method handles both integer and floating-point operands:
       * - If both operands are integers, it performs integer division using `luaV_div` and stores the result as an integer.
       * - If either operand is a floating-point number, it converts both operands to floating-point numbers using `tonumber`
       *   and performs floor division using `luai_numidiv`, storing the result as a floating-point number.
       * - If the operands are not numbers and cannot be converted to numbers, it attempts to invoke the `__idiv` metamethod
       *   using `luaT_trybinTM` to handle the operation.
       * 
       * The method ensures that the operation is protected against errors by using the `Protect` macro.
       * Finally, it breaks the virtual machine execution flow using `vmbreak`.
       */
      vmcase(OP_IDIV) {  /* floor division */
              TValue *rb = RKB(i);
              TValue *rc = RKC(i);
              lua_Number nb; lua_Number nc;
              if (ttisinteger(rb) && ttisinteger(rc)) {
                lua_Integer ib = ivalue(rb); lua_Integer ic = ivalue(rc);
                setivalue(ra, luaV_div(L, ib, ic));
              }
              else if (tonumber(rb, &nb) && tonumber(rc, &nc)) {
                setfltvalue(ra, luai_numidiv(L, nb, nc));
              }
              else { Protect(luaT_trybinTM(L, rb, rc, ra, TM_IDIV)); }
              vmbreak;
            }
      /**
       * Handles the OP_POW virtual machine instruction, which performs exponentiation.
       * 
       * This method retrieves the two operands `rb` and `rc` from the instruction's operands.
       * It attempts to convert both operands to Lua numbers (`lua_Number`). If successful,
       * it computes the result of raising `nb` (the numeric value of `rb`) to the power of
       * `nc` (the numeric value of `rc`) using the `luai_numpow` function. The result is then
       * stored in the register `ra`.
       * 
       * If either operand cannot be converted to a number, the method invokes the `ProtectluaT_trybinTM`
       * function to attempt a metamethod-based operation for exponentiation.
       * 
       * @param L The Lua state.
       * @param ra The register where the result will be stored.
       * @param rb The first operand register.
       * @param rc The second operand register.
       * @param i The instruction containing the operands.
       */
      vmcase(OP_POW) {
              TValue *rb = RKB(i);
              TValue *rc = RKC(i);
              lua_Number nb; lua_Number nc;
              if (tonumber(rb, &nb) && tonumber(rc, &nc)) {
                setfltvalue(ra, luai_numpow(L, nb, nc));
              }
              else { ProtectluaT_trybinTM(L, rb, rc, ra, TM_POW)); }
              vmbreak;
            }
      /**
       * Handles the unary minus operation (OP_UNM) in the Lua virtual machine.
       * 
       * This method performs the unary minus operation on the value stored in the register `rb`
       * and stores the result in the register `ra`. The operation is applied differently based
       * on the type of the value in `rb`:
       * 
       * - If the value is an integer, the method computes the negation by subtracting the value
       *   from 0 using the `intop` macro and stores the result as an integer in `ra`.
       * 
       * - If the value is a number (floating-point), the method computes the negation using the
       *   `luai_numunm` function and stores the result as a floating-point number in `ra`.
       * 
       * - If the value is neither an integer nor a number, the method attempts to invoke the
       *   corresponding metamethod (__unm) using `luaT_trybinTM` to handle the operation. If the
       *   metamethod is not available, an error is raised.
       * 
       * The `Protect` macro ensures that any errors during the metamethod invocation are properly
       * handled. Finally, the `vmbreak` macro is used to proceed to the next virtual machine instruction.
       */
      vmcase(OP_UNM) {
              TValue *rb = RB(i);
              lua_Number nb;
              if (ttisinteger(rb)) {
                lua_Integer ib = ivalue(rb);
                setivalue(ra, intop(-, 0, ib));
              }
              else if (tonumber(rb, &nb)) {
                setfltvalue(ra, luai_numunm(L, nb));
              }
              else {
                Protect(luaT_trybinTM(L, rb, rb, ra, TM_UNM));
              }
              vmbreak;
            }
      /**
       * Handles the bitwise NOT operation (OP_BNOT) in the Lua virtual machine.
       * 
       * This method performs a bitwise NOT operation on the value at the register `RB(i)`.
       * If the value is an integer, it directly computes the bitwise NOT and stores the result
       * in the register `ra`. If the value is not an integer, it attempts to invoke the
       * corresponding metamethod (TM_BNOT) to handle the operation.
       *
       * @param L The Lua state.
       * @param ra The register where the result will be stored.
       * @param rb The register containing the operand.
       * @param i The instruction index.
       * @return void
       */
      vmcase(OP_BNOT) {
              TValue *rb = RB(i);
              lua_Integer ib;
              if (tointeger(rb, &ib)) {
                setivalue(ra, intop(^, ~l_castS2U(0), ib));
              }
              else {
                Protect(luaT_trybinTM(L, rb, rb, ra, TM_BNOT));
              }
              vmbreak;
            }
      /**
       * Handles the OP_NOT bytecode instruction, which performs a logical NOT operation.
       * 
       * This method retrieves the value from the register specified by RB(i) and evaluates
       * whether it is considered "false" in the Lua context (i.e., nil or false). The result
       * of this evaluation is then stored as a boolean value in the register specified by ra.
       * 
       * The logical NOT operation is implemented by checking if the value in rb is "false"
       * using the l_isfalse macro. The result of this check (1 for true, 0 for false) is
       * then stored as a boolean value in ra using the setbvalue macro.
       * 
       * After performing the operation, the method executes a vmbreak to continue with the
       * next bytecode instruction.
       */
      vmcase(OP_NOT) {
              TValue *rb = RB(i);
              int res = l_isfalse(rb);  /* next assignment may change this value */
              setbvalue(ra, res);
              vmbreak;
            }
      /**
       * Handles the OP_LEN opcode in the Lua virtual machine.
       * This opcode is responsible for computing the length of an object, which can be a table, string, or userdata.
       * The length is determined based on the type of the object at register `RB(i)` and stored in register `ra`.
       * The `luaV_objlen` function is used to perform the actual length computation, and `Protect` ensures that
       * any potential errors during the operation are properly handled.
       *
       * @param L The Lua state.
       * @param ra The destination register where the length will be stored.
       * @param RB(i) The source register containing the object whose length is to be computed.
       * @return void
       */
      vmcase(OP_LEN) {
              Protect(luaV_objlen(L, ra, RB(i)));
              vmbreak;
            }
      /**
       * Handles the OP_CONCAT opcode, which concatenates multiple strings or values on the stack.
       * 
       * This method retrieves the range of operands to concatenate using the arguments B and C from the instruction.
       * It adjusts the stack top to mark the end of the concatenation operands. The `luaV_concat` function is then
       * called to perform the actual concatenation, which may invoke metamethods (TMs) and potentially move the stack.
       * After concatenation, the result is stored in the destination register (ra) from the base register (rb).
       * The method ensures proper garbage collection by checking if the result register (ra) is above or equal to the
       * base register (rb) and adjusts the stack top to restore it to its original state.
       *
       * @param L The Lua state.
       * @param base The base of the stack for the current function.
       * @param ci The current call info.
       * @param ra The destination register for the concatenation result.
       * @param i The current instruction being executed.
       */
      vmcase(OP_CONCAT) {
              int b = GETARG_B(i);
              int c = GETARG_C(i);
              StkId rb;
              L->top = base + c + 1;  /* mark the end of concat operands */
              Protect(luaV_concat(L, c - b + 1));
              ra = RA(i);  /* 'luaV_concat' may invoke TMs and move the stack */
              rb = base + b;
              setobjs2s(L, ra, rb);
              checkGC(L, (ra >= rb ? ra + 1 : rb));
              L->top = ci->top;  /* restore top */
              vmbreak;
            }
      /**
       * Executes an unconditional jump operation in the virtual machine.
       * This method is part of the virtual machine's instruction set and is invoked
       * when the OP_JMP opcode is encountered. It performs a jump to a specified
       * instruction address within the current function's bytecode.
       *
       * The jump is performed by calling the `dojump` function, which adjusts the
       * instruction pointer (IP) to the target address. The `vmbreak` macro is then
       * used to signal the end of the current instruction's execution, allowing the
       * virtual machine to proceed to the next instruction.
       *
       * @param ci The current call info structure, which contains context about the
       *           current function being executed.
       * @param i The instruction index from which the jump is being performed.
       * @param 0 Unused parameter, typically reserved for future extensions or
       *          additional jump-related data.
       */
      vmcase(OP_JMP) {
              dojump(ci, i, 0);
              vmbreak;
            }
      /**
       * Handles the OP_EQ (equality) virtual machine instruction.
       * This method compares two Lua values for equality and adjusts the program counter
       * or performs a jump based on the result of the comparison.
       *
       * @param L The Lua state.
       * @param ci The call info structure containing the current execution context.
       * @param i The instruction being executed.
       *
       * The method retrieves the two values to be compared using RKB(i) and RKC(i).
       * It then uses luaV_equalobj to compare the values. If the result of the comparison
       * does not match the expected result (specified by GETARG_A(i)), the program counter
       * is incremented to skip the next instruction. Otherwise, donextjump is called to
       * perform a jump to the next instruction.
       *
       * The Protect macro ensures that the operation is safely executed within the Lua
       * virtual machine's protected environment.
       */
      vmcase(OP_EQ) {
              TValue *rb = RKB(i);
              TValue *rc = RKC(i);
              Protect(
                if (luaV_equalobj(L, rb, rc) != GETARG_A(i))
                  ci->u.l.savedpc++;
                else
                  donextjump(ci);
              )
              vmbreak;
            }
      /**
       * Handles the OP_LT (less than) bytecode operation in the Lua virtual machine.
       * 
       * This method compares two values using the `luaV_lessthan` function. The values are retrieved
       * from the Lua stack using the `RKB` and `RKC` macros, which decode the bytecode instruction
       * to obtain the operands. The result of the comparison is then compared against the expected
       * result specified by `GETARG_A(i)`, which extracts the first argument from the bytecode instruction.
       * 
       * If the comparison result does not match the expected result, the program counter (`savedpc`) is
       * incremented to skip the next instruction. Otherwise, the `donextjump` function is called to
       * handle the next jump in the bytecode.
       * 
       * The `Protect` macro ensures that the operation is performed in a protected environment, catching
       * any errors that may occur during the comparison.
       * 
       * Finally, `vmbreak` is used to signal the end of the current bytecode instruction, allowing the
       * virtual machine to proceed to the next instruction.
       */
      vmcase(OP_LT) {
              Protect(
                if (luaV_lessthan(L, RKB(i), RKC(i)) != GETARG_A(i))
                  ci->u.l.savedpc++;
                else
                  donextjump(ci);
              )
              vmbreak;
            }
      /**
       * Handles the OP_LE (Less Than or Equal) bytecode instruction in the Lua virtual machine.
       * This instruction compares two values from the Lua stack (RKB(i) and RKC(i)) using the
       * `luaV_lessequal` function to determine if the first value is less than or equal to the second.
       * 
       * If the result of the comparison does not match the expected result (GETARG_A(i)), the
       * saved program counter (savedpc) in the current call info (ci) is incremented to skip
       * the next instruction. Otherwise, the `donextjump` function is called to proceed with
       * the next jump in the bytecode.
       * 
       * The `Protect` macro ensures that the operation is safely executed within the Lua
       * virtual machine, handling any potential errors or exceptions.
       * 
       * After the comparison and conditional jump, the `vmbreak` macro is used to exit the
       * current virtual machine case and continue execution.
       */
      vmcase(OP_LE) {
              Protect(
                if (luaV_lessequal(L, RKB(i), RKC(i)) != GETARG_A(i))
                  ci->u.l.savedpc++;
                else
                  donextjump(ci);
              )
              vmbreak;
            }
      /**
       * Handles the OP_TEST virtual machine instruction.
       * 
       * This method evaluates a conditional test based on the argument C of the current instruction.
       * If argument C is non-zero, it checks if the value in register 'ra' is false (using l_isfalse).
       * If argument C is zero, it checks if the value in register 'ra' is not false.
       * 
       * If the condition is met, the saved program counter (savedpc) in the current call info (ci) is incremented
       * to skip the next instruction. Otherwise, the method calls donextjump(ci) to perform a jump to the next
       * instruction as per the control flow.
       * 
       * The method ensures the virtual machine continues execution by issuing a vmbreak after handling the test.
       */
      vmcase(OP_TEST) {
              if (GETARG_C(i) ? l_isfalse(ra) : !l_isfalse(ra))
                  ci->u.l.savedpc++;
                else
                donextjump(ci);
              vmbreak;
            }
      /**
       * Handles the OP_TESTSET operation in the Lua virtual machine.
       *
       * This operation tests the value at the register specified by RB(i) and conditionally
       * sets the value to the register specified by RA(i) based on the test result.
       *
       * The test is performed as follows:
       * - If GETARG_C(i) is true, the test checks if the value at RB(i) is false.
       * - If GETARG_C(i) is false, the test checks if the value at RB(i) is not false.
       *
       * If the test condition is met, the saved program counter (savedpc) is incremented to
       * skip the next instruction. Otherwise, the value at RB(i) is copied to RA(i), and
       * the next jump is executed via donextjump(ci).
       *
       * @param i The instruction being executed, containing the operands for the operation.
       */
      vmcase(OP_TESTSET) {
              TValue *rb = RB(i);
              if (GETARG_C(i) ? l_isfalse(rb) : !l_isfalse(rb))
                ci->u.l.savedpc++;
              else {
                setobjs2s(L, ra, rb);
                donextjump(ci);
              }
              vmbreak;
            }
      /**
       * Handles the execution of the OP_CALL instruction in the Lua virtual machine.
       * This method is responsible for preparing and executing a function call, 
       * either a C function or a Lua function, based on the provided arguments.
       *
       * The method performs the following steps:
       * 1. Retrieves the number of arguments (`b`) and the number of expected results (`nresults`)
       *    from the instruction.
       * 2. Adjusts the top of the stack (`L->top`) to point to the first argument if `b` is not zero.
       * 3. Invokes `luaD_precall` to prepare the function call. If the function is a C function,
       *    it adjusts the top of the stack to the expected results and updates the base pointer.
       * 4. If the function is a Lua function, it updates the current call info (`ci`) and jumps to
       *    `newframe` to restart the execution of the Lua function.
       * 5. Finally, it breaks out of the virtual machine loop using `vmbreak`.
       *
       * @param L The Lua state, which holds the execution context.
       * @param ra The base register for the function call.
       * @param ci The current call info, used to manage function call contexts.
       */
      vmcase(OP_CALL) {
              int b = GETARG_B(i);
              int nresults = GETARG_C(i) - 1;
              if (b != 0) L->top = ra+b;  /* else previous instruction set top */
              if (luaD_precall(L, ra, nresults)) {  /* C function? */
                if (nresults >= 0)
                  L->top = ci->top;  /* adjust results */
                Protect((void)0);  /* update 'base' */
              }
              else {  /* Lua function */
                ci = L->ci;
                goto newframe;  /* restart luaV_execute over new Lua function */
              }
              vmbreak;
            }
      vmcase(OP_TAILCALL) {
        int b = GETARG_B(i);
        if (b != 0) L->top = ra+b;  /* else previous instruction set top */
        lua_assert(GETARG_C(i) - 1 == LUA_MULTRET);
        if (luaD_precall(L, ra, LUA_MULTRET)) {  /* C function? */
          Protect((void)0);  /* update 'base' */
        }
        else {
          /* tail call: put called frame (n) in place of caller one (o) */
          CallInfo *nci = L->ci;  /* called frame */
          Call

/* }================================================================== */

