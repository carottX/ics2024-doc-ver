/*
** $Id: lparser.c,v 2.155 2016/08/01 19:51:24 roberto Exp $
** Lua Parser
** See Copyright Notice in lua.h
*/

#define lparser_c
#define LUA_CORE

#include "lprefix.h"


#include <string.h>

#include "lua.h"

#include "lcode.h"
#include "ldebug.h"
#include "ldo.h"
#include "lfunc.h"
#include "llex.h"
#include "lmem.h"
#include "lobject.h"
#include "lopcodes.h"
#include "lparser.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"



/* maximum number of local variables per function (must be smaller
   than 250, due to the bytecode format) */
#define MAXVARS		200


#define hasmultret(k)		((k) == VCALL || (k) == VVARARG)


/* because all strings are unified by the scanner, the parser
   can use pointer equality for string equality */
#define eqstr(a,b)	((a) == (b))


/*
** nodes for block list (list of active blocks)
*/
typedef struct BlockCnt {
  struct BlockCnt *previous;  /* chain */
  int firstlabel;  /* index of first label in this block */
  int firstgoto;  /* index of first pending goto in this block */
  lu_byte nactvar;  /* # active locals outside the block */
  lu_byte upval;  /* true if some variable in the block is an upvalue */
  lu_byte isloop;  /* true if 'block' is a loop */
} BlockCnt;



/*
** prototypes for recursive non-terminal functions
*/
static void statement (LexState *ls);
static void expr (LexState *ls, expdesc *v);


/* semantic error */
static l_noret semerror (LexState *ls, const char *msg) {
  ls->t.token = 0;  /* remove "near <token>" from final message */
  luaX_syntaxerror(ls, msg);
}


/**
 * Reports a syntax error indicating that a specific token was expected.
 * This function is used to generate and throw a syntax error when the parser
 * encounters an unexpected token. The error message includes the expected token
 * as a string representation.
 *
 * @param ls Pointer to the LexState structure, which holds the current state
 *           of the lexer and parser.
 * @param token The token that was expected but not found. This token is
 *              converted to a string representation for the error message.
 *
 * @note This function does not return (marked as `l_noret`), as it terminates
 *       the program flow by throwing a syntax error.
 */
static l_noret error_expected (LexState *ls, int token) {
  luaX_syntaxerror(ls,
      luaO_pushfstring(ls->L, "%s expected", luaX_token2str(ls, token)));
}


/**
 * @brief Reports an error when a specified limit is exceeded in a function.
 *
 * This method generates and throws a syntax error when a given limit is exceeded
 * in a function. The error message includes details about the limit, the type of
 * entity that exceeded the limit, and the location (line number) where the error
 * occurred. If the function is the main function (i.e., no specific line number),
 * it is identified as such in the error message.
 *
 * @param fs Pointer to the FuncState structure representing the current function state.
 * @param limit The maximum allowed value that was exceeded.
 * @param what A string describing the type of entity that exceeded the limit (e.g., "locals", "upvalues").
 *
 * @note This function does not return; it throws a syntax error using luaX_syntaxerror.
 */
static l_noret errorlimit (FuncState *fs, int limit, const char *what) {
  lua_State *L = fs->ls->L;
  const char *msg;
  int line = fs->f->linedefined;
  const char *where = (line == 0)
                      ? "main function"
                      : luaO_pushfstring(L, "function at line %d", line);
  msg = luaO_pushfstring(L, "too many %s (limit is %d) in %s",
                             what, limit, where);
  luaX_syntaxerror(fs->ls, msg);
}


/**
 * Checks if a given value exceeds a specified limit and triggers an error if it does.
 *
 * This function compares the value `v` against the limit `l`. If `v` is greater than `l`,
 * it calls the `errorlimit` function to report the error, providing the limit `l` and
 * a description `what` to indicate what the limit pertains to.
 *
 * @param fs Pointer to the function state, typically used for error reporting context.
 * @param v The value to check against the limit.
 * @param l The limit value to compare against.
 * @param what A string describing what the limit pertains to (e.g., "local variables").
 */
static void checklimit (FuncState *fs, int v, int l, const char *what) {
  if (v > l) errorlimit(fs, l, what);
}


/**
 * Checks if the current token in the lexer state matches the specified character.
 * If the token matches, the lexer advances to the next token and returns 1.
 * If the token does not match, the lexer does not advance and returns 0.
 *
 * @param ls Pointer to the LexState structure representing the lexer state.
 * @param c The character to compare against the current token.
 * @return 1 if the current token matches the specified character, 0 otherwise.
 */
static int testnext (LexState *ls, int c) {
  if (ls->t.token == c) {
    luaX_next(ls);
    return 1;
  }
  else return 0;
}


/**
 * Checks if the current token in the LexState matches the expected token.
 * If the current token does not match the expected token, an error is raised
 * indicating that the expected token was not found.
 *
 * @param ls Pointer to the LexState structure containing the current lexical state.
 * @param c The expected token value to compare against the current token.
 */
static void check (LexState *ls, int c) {
  if (ls->t.token != c)
    error_expected(ls, c);
}


/**
 * Checks if the next token in the LexState matches the expected character `c`,
 * and then advances the lexer to the next token.
 *
 * This function first verifies that the current token in the LexState `ls` matches
 * the specified character `c` by calling the `check` function. If the check passes,
 * it proceeds to advance the lexer to the next token by calling `luaX_next`.
 *
 * @param ls Pointer to the LexState structure, which maintains the state of the lexer.
 * @param c The expected character to be matched against the current token.
 */
static void checknext (LexState *ls, int c) {
  check(ls, c);
  luaX_next(ls);
}


#define check_condition(ls,c,msg)	{ if (!(c)) luaX_syntaxerror(ls, msg); }



/**
 * Checks if the next token in the lexer state matches the expected token.
 * If the next token does not match, an error is raised. The error message
 * is tailored based on the context provided by the `where` parameter.
 *
 * @param ls The lexer state containing the current parsing context.
 * @param what The expected token to match.
 * @param who The token that needs to be closed (used in the error message).
 * @param where The line number where the token `who` was opened (used in the error message).
 *
 * @details This function first checks if the next token matches `what` using `testnext`.
 * If it does not match, an error is raised. If the current line number matches `where`,
 * a simple "expected" error is raised. Otherwise, a more detailed syntax error is raised,
 * indicating that `what` was expected to close `who` at the specified line number.
 */
static void check_match (LexState *ls, int what, int who, int where) {
  if (!testnext(ls, what)) {
    if (where == ls->linenumber)
      error_expected(ls, what);
    else {
      luaX_syntaxerror(ls, luaO_pushfstring(ls->L,
             "%s expected (to close %s at line %d)",
              luaX_token2str(ls, what), luaX_token2str(ls, who), where));
    }
  }
}


/**
 * Checks if the current token in the lexer state is a name token (TK_NAME).
 * If it is, retrieves the associated TString from the semantic information
 * and advances the lexer to the next token. Returns the retrieved TString.
 *
 * @param ls The lexer state containing the current token and semantic information.
 * @return The TString associated with the name token.
 */
static TString *str_checkname (LexState *ls) {
  TString *ts;
  check(ls, TK_NAME);
  ts = ls->t.seminfo.ts;
  luaX_next(ls);
  return ts;
}


/**
 * Initializes an `expdesc` structure with the specified values.
 *
 * This function sets the `f` and `t` fields of the `expdesc` structure to `NO_JUMP`,
 * indicating that there are no jump targets associated with the expression. It then
 * assigns the provided `expkind` value to the `k` field and the integer value to the
 * `u.info` field of the structure.
 *
 * @param e Pointer to the `expdesc` structure to be initialized.
 * @param k The `expkind` value to assign to the `k` field of the `expdesc` structure.
 * @param i The integer value to assign to the `u.info` field of the `expdesc` structure.
 */
static void init_exp (expdesc *e, expkind k, int i) {
  e->f = e->t = NO_JUMP;
  e->k = k;
  e->u.info = i;
}


/**
 * Generates a code string and initializes an expression descriptor with it.
 * This function is used in the context of a lexical state to create a constant
 * string expression and store it in the function's constant pool. The resulting
 * expression descriptor is initialized to represent a constant value of type VK
 * (a constant string).
 *
 * @param ls Pointer to the LexState structure, which holds the current lexical state.
 * @param e Pointer to the expdesc structure, which will be initialized to represent
 *          the constant string expression.
 * @param s Pointer to the TString structure representing the string to be coded as
 *          a constant.
 */
static void codestring (LexState *ls, expdesc *e, TString *s) {
  init_exp(e, VK, luaK_stringK(ls->fs, s));
}


/**
 * Checks the validity of a name in the current lexical state and encodes it as a string.
 * 
 * This function retrieves the name from the lexical state using `str_checkname`, which
 * ensures the name is valid according to the language's lexical rules. It then encodes
 * the name as a string in the expression descriptor `e` using `codestring`.
 * 
 * @param ls Pointer to the LexState structure representing the current lexical state.
 * @param e  Pointer to the expdesc structure where the encoded string will be stored.
 */
static void checkname (LexState *ls, expdesc *e) {
  codestring(ls, e, str_checkname(ls));
}


/**
 * Registers a local variable in the current function's local variable table.
 *
 * This method is responsible for adding a new local variable to the function's
 * local variable table. It ensures that the table has enough space to accommodate
 * the new variable by growing the table if necessary. The variable's name is
 * stored in the table, and a barrier is set to ensure proper memory management.
 *
 * @param ls Pointer to the LexState structure, which contains the current lexical state.
 * @param varname Pointer to the TString representing the name of the local variable.
 * @return The index of the newly registered local variable in the local variable table.
 */
static int registerlocalvar (LexState *ls, TString *varname) {
  FuncState *fs = ls->fs;
  Proto *f = fs->f;
  int oldsize = f->sizelocvars;
  luaM_growvector(ls->L, f->locvars, fs->nlocvars, f->sizelocvars,
                  LocVar, SHRT_MAX, "local variables");
  while (oldsize < f->sizelocvars)
    f->locvars[oldsize++].varname = NULL;
  f->locvars[fs->nlocvars].varname = varname;
  luaC_objbarrier(ls->L, f, varname);
  return fs->nlocvars++;
}


/**
 * Creates a new local variable in the current function's scope.
 *
 * This function registers a new local variable with the given name in the current
 * function's scope. It ensures that the number of local variables does not exceed
 * the maximum allowed limit (`MAXVARS`). The variable is assigned a register index
 * and added to the dynamic data structure (`dyd->actvar`) that tracks active variables.
 *
 * @param ls The lexical state containing the current parsing context.
 * @param name The name of the local variable to be created.
 *
 * @note This function may trigger memory allocation to grow the `actvar` array
 *       if the current capacity is insufficient to accommodate the new variable.
 */
static void new_localvar (LexState *ls, TString *name) {
  FuncState *fs = ls->fs;
  Dyndata *dyd = ls->dyd;
  int reg = registerlocalvar(ls, name);
  checklimit(fs, dyd->actvar.n + 1 - fs->firstlocal,
                  MAXVARS, "local variables");
  luaM_growvector(ls->L, dyd->actvar.arr, dyd->actvar.n + 1,
                  dyd->actvar.size, Vardesc, MAX_INT, "local variables");
  dyd->actvar.arr[dyd->actvar.n++].idx = cast(short, reg);
}


/**
 * @brief Creates a new local variable in the current lexical scope using a string literal.
 *
 * This function is a convenience wrapper around `new_localvar` that simplifies the creation
 * of a local variable from a string literal. It first creates a new string object using
 * `luaX_newstring` with the provided name and size, and then calls `new_localvar` to add
 * the variable to the current lexical scope.
 *
 * @param ls Pointer to the LexState structure, which maintains the state of the lexical analyzer.
 * @param name The name of the local variable as a string literal.
 * @param sz The size of the string literal, typically obtained using `strlen` or similar.
 *
 * @note The string literal is converted into a Lua string object internally, which is then
 * used to create the local variable.
 */
static void new_localvarliteral_ (LexState *ls, const char *name, size_t sz) {
  new_localvar(ls, luaX_newstring(ls, name, sz));
}

#define new_localvarliteral(ls,v) \
	new_localvarliteral_(ls, "" v, (sizeof(v)/sizeof(char))-1)


/**
 * Retrieves the local variable at the specified index from the function state.
 *
 * This method accesses the local variable by calculating its actual index in the function's
 * local variable array. It first retrieves the index from the active variable array, which
 * is stored in the function state's lexical state. The method then asserts that the retrieved
 * index is within the valid range of local variables. Finally, it returns a pointer to the
 * corresponding local variable in the function's local variable array.
 *
 * @param fs The function state containing the local variables and related information.
 * @param i The index of the local variable to retrieve, relative to the first local variable.
 * @return A pointer to the LocVar structure representing the requested local variable.
 */
static LocVar *getlocvar (FuncState *fs, int i) {
  int idx = fs->ls->dyd->actvar.arr[fs->firstlocal + i].idx;
  lua_assert(idx < fs->nlocvars);
  return &fs->f->locvars[idx];
}


/**
 * Adjusts the local variables in the current function state based on the given number of variables.
 * 
 * This function updates the count of active local variables (`nactvar`) in the function state (`fs`)
 * by incrementing it by `nvars`. It then sets the starting program counter (`startpc`) for each of the
 * newly added local variables to the current program counter (`pc`) in the function state.
 * 
 * @param ls The LexState object containing the current lexical state and function state.
 * @param nvars The number of local variables to be added or adjusted.
 */
static void adjustlocalvars (LexState *ls, int nvars) {
  FuncState *fs = ls->fs;
  fs->nactvar = cast_byte(fs->nactvar + nvars);
  for (; nvars; nvars--) {
    getlocvar(fs, fs->nactvar - nvars)->startpc = fs->pc;
  }
}


/**
 * Removes local variables from the function state up to the specified level.
 * This function adjusts the active variable count and marks the end program counter
 * (pc) for each removed local variable.
 *
 * @param fs The function state from which local variables are to be removed.
 * @param tolevel The target level of active variables to which the function state
 *                should be reduced. All active variables above this level will be
 *                removed.
 *
 * The function performs the following operations:
 * 1. Decreases the count of active variables in the function state by the difference
 *    between the current number of active variables (`fs->nactvar`) and the target
 *    level (`tolevel`).
 * 2. Iterates through the active variables above the target level, marking the end
 *    program counter (`endpc`) for each removed variable with the current program
 *    counter (`fs->pc`).
 */
static void removevars (FuncState *fs, int tolevel) {
  fs->ls->dyd->actvar.n -= (fs->nactvar - tolevel);
  while (fs->nactvar > tolevel)
    getlocvar(fs, --fs->nactvar)->endpc = fs->pc;
}


/**
 * Searches for an upvalue by name in the given function state.
 *
 * This function iterates through the upvalues of the function associated with
 * the provided `FuncState` (`fs`). It compares the name of each upvalue with
 * the given `name` using the `eqstr` function. If a match is found, the index
 * of the upvalue is returned. If no match is found, the function returns -1.
 *
 * @param fs Pointer to the function state containing the upvalues to search.
 * @param name Pointer to the string representing the name of the upvalue to search for.
 * @return The index of the upvalue if found, otherwise -1.
 */
static int searchupvalue (FuncState *fs, TString *name) {
  int i;
  Upvaldesc *up = fs->f->upvalues;
  for (i = 0; i < fs->nups; i++) {
    if (eqstr(up[i].name, name)) return i;
  }
  return -1;  /* not found */
}


/**
 * Adds a new upvalue to the function's upvalue table and returns its index.
 *
 * This function is responsible for managing upvalues in the context of a Lua function's
 * prototype. It ensures that the upvalue table has enough space to accommodate the new
 * upvalue by growing the table if necessary. The upvalue is initialized with the provided
 * name and its properties are set based on the given expression descriptor. The function
 * also performs a memory barrier to ensure proper garbage collection handling.
 *
 * @param fs Pointer to the FuncState structure representing the current function state.
 * @param name Pointer to the TString representing the name of the upvalue.
 * @param v Pointer to the expdesc structure describing the upvalue's properties.
 * @return The index of the newly added upvalue in the function's upvalue table.
 */
static int newupvalue (FuncState *fs, TString *name, expdesc *v) {
  Proto *f = fs->f;
  int oldsize = f->sizeupvalues;
  checklimit(fs, fs->nups + 1, MAXUPVAL, "upvalues");
  luaM_growvector(fs->ls->L, f->upvalues, fs->nups, f->sizeupvalues,
                  Upvaldesc, MAXUPVAL, "upvalues");
  while (oldsize < f->sizeupvalues)
    f->upvalues[oldsize++].name = NULL;
  f->upvalues[fs->nups].instack = (v->k == VLOCAL);
  f->upvalues[fs->nups].idx = cast_byte(v->u.info);
  f->upvalues[fs->nups].name = name;
  luaC_objbarrier(fs->ls->L, f, name);
  return fs->nups++;
}


/**
 * Searches for a local variable by name within the current function state.
 *
 * This function iterates through the active local variables of the function state
 * in reverse order, starting from the most recently added variable. It compares
 * the provided variable name `n` with the names of the local variables using
 * string equality. If a match is found, the index of the matching variable is
 * returned. If no match is found, the function returns -1.
 *
 * @param fs Pointer to the FuncState structure representing the current function state.
 * @param n  Pointer to the TString representing the name of the variable to search for.
 * @return   The index of the matching local variable if found, otherwise -1.
 */
static int searchvar (FuncState *fs, TString *n) {
  int i;
  for (i = cast_int(fs->nactvar) - 1; i >= 0; i--) {
    if (eqstr(n, getlocvar(fs, i)->varname))
      return i;
  }
  return -1;  /* not found */
}


/*
  Mark block where variable at given level was defined
  (to emit close instructions later).
*/
static void markupval (FuncState *fs, int level) {
  BlockCnt *bl = fs->bl;
  while (bl->nactvar > level)
    bl = bl->previous;
  bl->upval = 1;
}


/*
  Find variable with given name 'n'. If it is an upvalue, add this
  upvalue into all intermediate functions.
*/
static void singlevaraux (FuncState *fs, TString *n, expdesc *var, int base) {
  if (fs == NULL)  /* no more levels? */
    init_exp(var, VVOID, 0);  /* default is global */
  else {
    int v = searchvar(fs, n);  /* look up locals at current level */
    if (v >= 0) {  /* found? */
      init_exp(var, VLOCAL, v);  /* variable is local */
      if (!base)
        markupval(fs, v);  /* local will be used as an upval */
    }
    else {  /* not found as local at current level; try upvalues */
      int idx = searchupvalue(fs, n);  /* try existing upvalues */
      if (idx < 0) {  /* not found? */
        singlevaraux(fs->prev, n, var, 0);  /* try upper levels */
        if (var->k == VVOID)  /* not found? */
          return;  /* it is a global */
        /* else was LOCAL or UPVAL */
        idx  = newupvalue(fs, n, var);  /* will be a new upvalue */
      }
      init_exp(var, VUPVAL, idx);  /* new or old upvalue */
    }
  }
}


/**
 * Handles the resolution of a single variable in the lexical state.
 * 
 * This function processes a single variable by first checking its name using `str_checkname`.
 * It then attempts to resolve the variable in the current function's scope using `singlevaraux`.
 * If the variable is not found in the local scope (i.e., its kind is `VVOID`), it is assumed to be a global variable.
 * In this case, the function retrieves the environment variable and constructs an indexed access expression
 * representing `env[varname]`, where `env` is the environment table and `varname` is the variable name.
 * 
 * @param ls Pointer to the LexState structure, representing the lexical state of the parser.
 * @param var Pointer to an expdesc structure, which will be updated to describe the resolved variable.
 */
static void singlevar (LexState *ls, expdesc *var) {
  TString *varname = str_checkname(ls);
  FuncState *fs = ls->fs;
  singlevaraux(fs, varname, var, 1);
  if (var->k == VVOID) {  /* global name? */
    expdesc key;
    singlevaraux(fs, ls->envn, var, 1);  /* get environment variable */
    lua_assert(var->k != VVOID);  /* this one must exist */
    codestring(ls, &key, varname);  /* key is variable name */
    luaK_indexed(fs, var, &key);  /* env[varname] */
  }
}


/**
 * Adjusts the assignment of values to variables in the current lexical state.
 *
 * This function ensures that the number of values being assigned matches the number
 * of variables. If there are more values than variables, the extra values are discarded.
 * If there are more variables than values, the extra variables are initialized to `nil`.
 *
 * @param ls Pointer to the LexState structure representing the current lexical state.
 * @param nvars The number of variables to which values are being assigned.
 * @param nexps The number of expressions (values) being assigned.
 * @param e Pointer to the expdesc structure representing the last expression in the assignment.
 *
 * The function handles two main cases:
 * 1. If the last expression has multiple return values (e.g., a function call), it adjusts
 *    the number of return values to match the number of variables. If there are more
 *    variables than return values, it reserves additional registers and sets them to `nil`.
 * 2. If the last expression does not have multiple return values, it ensures the last
 *    expression is closed and assigns `nil` to any extra variables.
 *
 * Finally, if there are more values than variables, the extra values are removed from
 * the register stack.
 */
static void adjust_assign (LexState *ls, int nvars, int nexps, expdesc *e) {
  FuncState *fs = ls->fs;
  int extra = nvars - nexps;
  if (hasmultret(e->k)) {
    extra++;  /* includes call itself */
    if (extra < 0) extra = 0;
    luaK_setreturns(fs, e, extra);  /* last exp. provides the difference */
    if (extra > 1) luaK_reserveregs(fs, extra-1);
  }
  else {
    if (e->k != VVOID) luaK_exp2nextreg(fs, e);  /* close last expression */
    if (extra > 0) {
      int reg = fs->freereg;
      luaK_reserveregs(fs, extra);
      luaK_nil(fs, reg, extra);
    }
  }
  if (nexps > nvars)
    ls->fs->freereg -= nexps - nvars;  /* remove extra values */
}


/**
 * Increments the C call counter for the Lua state and checks if the maximum
 * number of C calls has been exceeded. This function is typically used to
 * manage the call stack depth and prevent stack overflow when entering a new
 * level of C function calls within the Lua interpreter.
 *
 * @param ls Pointer to the LexState structure, which contains the Lua state
 *           and other lexical analysis information.
 *
 * @note This function increments the `nCcalls` field of the Lua state (`L`)
 *       and then checks if the new value exceeds the predefined limit
 *       `LUAI_MAXCCALLS`. If the limit is exceeded, an error is raised
 *       indicating that the maximum number of C levels has been reached.
 */
static void enterlevel (LexState *ls) {
  lua_State *L = ls->L;
  ++L->nCcalls;
  checklimit(ls->fs, L->nCcalls, LUAI_MAXCCALLS, "C levels");
}


#define leavelevel(ls)	((ls)->L->nCcalls--)


/**
 * Closes a pending goto statement by resolving it to the specified label.
 * This function ensures that the goto statement does not jump into the scope
 * of a local variable, which would be illegal. If such a jump is detected,
 * an error is raised. Otherwise, the goto statement is patched to jump to
 * the label's program counter (pc), and the goto statement is removed from
 * the list of pending goto statements.
 *
 * @param ls The LexState structure representing the current lexical state.
 * @param g The index of the goto statement in the pending goto list.
 * @param label The Labeldesc structure representing the target label.
 * @throws Semerror if the goto statement jumps into the scope of a local variable.
 */
static void closegoto (LexState *ls, int g, Labeldesc *label) {
  int i;
  FuncState *fs = ls->fs;
  Labellist *gl = &ls->dyd->gt;
  Labeldesc *gt = &gl->arr[g];
  lua_assert(eqstr(gt->name, label->name));
  if (gt->nactvar < label->nactvar) {
    TString *vname = getlocvar(fs, gt->nactvar)->varname;
    const char *msg = luaO_pushfstring(ls->L,
      "<goto %s> at line %d jumps into the scope of local '%s'",
      getstr(gt->name), gt->line, getstr(vname));
    semerror(ls, msg);
  }
  luaK_patchlist(fs, gt->pc, label->pc);
  /* remove goto from pending list */
  for (i = g; i < gl->n - 1; i++)
    gl->arr[i] = gl->arr[i + 1];
  gl->n--;
}


/*
** try to close a goto with existing labels; this solves backward jumps
*/
static int findlabel (LexState *ls, int g) {
  int i;
  BlockCnt *bl = ls->fs->bl;
  Dyndata *dyd = ls->dyd;
  Labeldesc *gt = &dyd->gt.arr[g];
  /* check labels in current block for a match */
  for (i = bl->firstlabel; i < dyd->label.n; i++) {
    Labeldesc *lb = &dyd->label.arr[i];
    if (eqstr(lb->name, gt->name)) {  /* correct label? */
      if (gt->nactvar > lb->nactvar &&
          (bl->upval || dyd->label.n > bl->firstlabel))
        luaK_patchclose(ls->fs, gt->pc, lb->nactvar);
      closegoto(ls, g, lb);  /* close it */
      return 1;
    }
  }
  return 0;  /* label not found; cannot close goto */
}


/**
 * Adds a new label entry to the label list.
 *
 * This function appends a new label descriptor to the given label list `l`. The label
 * is associated with the provided name, line number, and program counter (pc). The
 * function ensures that the label list has sufficient capacity by growing the internal
 * array if necessary. The new label entry is initialized with the provided values and
 * the current number of active variables from the function state (`ls->fs->nactvar`).
 *
 * @param ls The lexer state, which contains the function state and Lua state.
 * @param l The label list to which the new label entry will be added.
 * @param name The name of the label, represented as a `TString`.
 * @param line The line number where the label is defined.
 * @param pc The program counter (bytecode offset) associated with the label.
 * @return The index of the newly added label entry in the label list.
 */
static int newlabelentry (LexState *ls, Labellist *l, TString *name,
                          int line, int pc) {
  int n = l->n;
  luaM_growvector(ls->L, l->arr, n, l->size,
                  Labeldesc, SHRT_MAX, "labels/gotos");
  l->arr[n].name = name;
  l->arr[n].line = line;
  l->arr[n].nactvar = ls->fs->nactvar;
  l->arr[n].pc = pc;
  l->n = n + 1;
  return n;
}


/*
** check whether new label 'lb' matches any pending gotos in current
** block; solves forward jumps
*/
static void findgotos (LexState *ls, Labeldesc *lb) {
  Labellist *gl = &ls->dyd->gt;
  int i = ls->fs->bl->firstgoto;
  while (i < gl->n) {
    if (eqstr(gl->arr[i].name, lb->name))
      closegoto(ls, i, lb);
    else
      i++;
  }
}


/*
** export pending gotos to outer level, to check them against
** outer labels; if the block being exited has upvalues, and
** the goto exits the scope of any variable (which can be the
** upvalue), close those variables being exited.
*/
static void movegotosout (FuncState *fs, BlockCnt *bl) {
  int i = bl->firstgoto;
  Labellist *gl = &fs->ls->dyd->gt;
  /* correct pending gotos to current block and try to close it
     with visible labels */
  while (i < gl->n) {
    Labeldesc *gt = &gl->arr[i];
    if (gt->nactvar > bl->nactvar) {
      if (bl->upval)
        luaK_patchclose(fs, gt->pc, bl->nactvar);
      gt->nactvar = bl->nactvar;
    }
    if (!findlabel(fs->ls, i))
      i++;  /* move to next one */
  }
}


/**
 * Enters a new block scope within the function state.
 * 
 * This function sets up a new block scope by initializing the provided BlockCnt structure
 * with the current state of the function. It updates the block's properties such as whether
 * it is a loop block, the number of active variables, the first label and goto indices, and
 * links the new block to the previous block in the function state. The function also asserts
 * that the number of free registers matches the number of active variables.
 *
 * @param fs Pointer to the FuncState structure representing the current function state.
 * @param bl Pointer to the BlockCnt structure to be initialized for the new block.
 * @param isloop A flag indicating whether the new block is a loop block (1 for loop, 0 otherwise).
 */
static void enterblock (FuncState *fs, BlockCnt *bl, lu_byte isloop) {
  bl->isloop = isloop;
  bl->nactvar = fs->nactvar;
  bl->firstlabel = fs->ls->dyd->label.n;
  bl->firstgoto = fs->ls->dyd->gt.n;
  bl->upval = 0;
  bl->previous = fs->bl;
  fs->bl = bl;
  lua_assert(fs->freereg == fs->nactvar);
}


/*
** create a label named 'break' to resolve break statements
*/
static void breaklabel (LexState *ls) {
  TString *n = luaS_new(ls->L, "break");
  int l = newlabelentry(ls, &ls->dyd->label, n, 0, ls->fs->pc);
  findgotos(ls, &ls->dyd->label.arr[l]);
}

/*
** generates an error for an undefined 'goto'; choose appropriate
** message when label name is a reserved word (which can only be 'break')
*/
static l_noret undefgoto (LexState *ls, Labeldesc *gt) {
  const char *msg = isreserved(gt->name)
                    ? "<%s> at line %d not inside a loop"
                    : "no visible label '%s' for <goto> at line %d";
  msg = luaO_pushfstring(ls->L, msg, getstr(gt->name), gt->line);
  semerror(ls, msg);
}


/**
 * Closes the current block in the function state, performing necessary cleanup and adjustments.
 * This includes handling upvalues, loop breaks, and pending gotos. The method ensures that
 * all local variables and labels within the block are properly removed, and any pending
 * control flow adjustments (e.g., jumps or gotos) are resolved. Additionally, it updates the
 * function state to reflect the closure of the block, such as freeing registers and restoring
 * the previous block context.
 *
 * @param fs Pointer to the FuncState structure representing the current function state.
 *           This structure contains information about the current block, local variables,
 *           and control flow.
 */
static void leaveblock (FuncState *fs) {
  BlockCnt *bl = fs->bl;
  LexState *ls = fs->ls;
  if (bl->previous && bl->upval) {
    /* create a 'jump to here' to close upvalues */
    int j = luaK_jump(fs);
    luaK_patchclose(fs, j, bl->nactvar);
    luaK_patchtohere(fs, j);
  }
  if (bl->isloop)
    breaklabel(ls);  /* close pending breaks */
  fs->bl = bl->previous;
  removevars(fs, bl->nactvar);
  lua_assert(bl->nactvar == fs->nactvar);
  fs->freereg = fs->nactvar;  /* free registers */
  ls->dyd->label.n = bl->firstlabel;  /* remove local labels */
  if (bl->previous)  /* inner block? */
    movegotosout(fs, bl);  /* update pending gotos to outer block */
  else if (bl->firstgoto < ls->dyd->gt.n)  /* pending gotos in outer block? */
    undefgoto(ls, &ls->dyd->gt.arr[bl->firstgoto]);  /* error */
}


/*
** adds a new prototype into list of prototypes
*/
static Proto *addprototype (LexState *ls) {
  Proto *clp;
  lua_State *L = ls->L;
  FuncState *fs = ls->fs;
  Proto *f = fs->f;  /* prototype of current function */
  if (fs->np >= f->sizep) {
    int oldsize = f->sizep;
    luaM_growvector(L, f->p, fs->np, f->sizep, Proto *, MAXARG_Bx, "functions");
    while (oldsize < f->sizep)
      f->p[oldsize++] = NULL;
  }
  f->p[fs->np++] = clp = luaF_newproto(L);
  luaC_objbarrier(L, f, clp);
  return clp;
}


/*
** codes instruction to create new closure in parent function.
** The OP_CLOSURE instruction must use the last available register,
** so that, if it invokes the GC, the GC knows which registers
** are in use at that time.
*/
static void codeclosure (LexState *ls, expdesc *v) {
  FuncState *fs = ls->fs->prev;
  init_exp(v, VRELOCABLE, luaK_codeABx(fs, OP_CLOSURE, 0, fs->np - 1));
  luaK_exp2nextreg(fs, v);  /* fix it at the last register */
}


/**
 * Initializes a new function state (`FuncState`) and associates it with the current lexical state (`LexState`).
 * This function sets up the necessary context for compiling a new function, including initializing various
 * function-specific fields such as program counter (`pc`), jump list (`jpc`), register usage (`freereg`), 
 * and local variable counts (`nlocvars`, `nactvar`). It also links the new function state to the previous
 * function state (if any) to maintain a stack of active functions during compilation.
 *
 * The function also initializes the associated `Proto` structure, setting the source reference and the
 * maximum stack size. Finally, it enters a new block scope by calling `enterblock`, which sets up the
 * block control structure (`BlockCnt`) for the function.
 *
 * @param ls Pointer to the current lexical state (`LexState`), which contains the context for lexical analysis.
 * @param fs Pointer to the function state (`FuncState`) to be initialized.
 * @param bl Pointer to the block control structure (`BlockCnt`) for the new function scope.
 */
static void open_func (LexState *ls, FuncState *fs, BlockCnt *bl) {
  Proto *f;
  fs->prev = ls->fs;  /* linked list of funcstates */
  fs->ls = ls;
  ls->fs = fs;
  fs->pc = 0;
  fs->lasttarget = 0;
  fs->jpc = NO_JUMP;
  fs->freereg = 0;
  fs->nk = 0;
  fs->np = 0;
  fs->nups = 0;
  fs->nlocvars = 0;
  fs->nactvar = 0;
  fs->firstlocal = ls->dyd->actvar.n;
  fs->bl = NULL;
  f = fs->f;
  f->source = ls->source;
  f->maxstacksize = 2;  /* registers 0/1 are always valid */
  enterblock(fs, bl, 0);
}


/**
 * Closes the current function being compiled in the lexical state `ls`.
 * This function performs the following operations:
 * 1. Inserts a final return instruction into the function's bytecode.
 * 2. Exits the current block scope.
 * 3. Resizes the function's code, line information, constants, prototypes, local variables, and upvalues arrays to match the actual sizes used during compilation.
 * 4. Updates the sizes of these arrays in the function's `Proto` structure.
 * 5. Asserts that there are no remaining open blocks in the function state.
 * 6. Restores the previous function state from the lexical state.
 * 7. Triggers a garbage collection check to ensure memory is managed properly.
 *
 * @param ls Pointer to the lexical state containing the function state to be closed.
 */
static void close_func (LexState *ls) {
  lua_State *L = ls->L;
  FuncState *fs = ls->fs;
  Proto *f = fs->f;
  luaK_ret(fs, 0, 0);  /* final return */
  leaveblock(fs);
  luaM_reallocvector(L, f->code, f->sizecode, fs->pc, Instruction);
  f->sizecode = fs->pc;
  luaM_reallocvector(L, f->lineinfo, f->sizelineinfo, fs->pc, int);
  f->sizelineinfo = fs->pc;
  luaM_reallocvector(L, f->k, f->sizek, fs->nk, TValue);
  f->sizek = fs->nk;
  luaM_reallocvector(L, f->p, f->sizep, fs->np, Proto *);
  f->sizep = fs->np;
  luaM_reallocvector(L, f->locvars, f->sizelocvars, fs->nlocvars, LocVar);
  f->sizelocvars = fs->nlocvars;
  luaM_reallocvector(L, f->upvalues, f->sizeupvalues, fs->nups, Upvaldesc);
  f->sizeupvalues = fs->nups;
  lua_assert(fs->bl == NULL);
  ls->fs = fs->prev;
  luaC_checkGC(L);
}



/*============================================================*/
/* GRAMMAR RULES */
/*============================================================*/


/*
** check whether current token is in the follow set of a block.
** 'until' closes syntactical blocks, but do not close scope,
** so it is handled in separate.
*/
static int block_follow (LexState *ls, int withuntil) {
  switch (ls->t.token) {
    case TK_ELSE: case TK_ELSEIF:
    case TK_END: case TK_EOS:
      return 1;
    case TK_UNTIL: return withuntil;
    default: return 0;
  }
}


/**
 * Processes a list of statements in the current lexical state.
 *
 * This method iterates through a sequence of statements until a block-following token is encountered.
 * It handles the special case where a 'return' statement is encountered, ensuring it is the last statement
 * in the list. Each statement is processed by calling the `statement` method. The loop continues until
 * `block_follow` indicates the end of the statement list.
 *
 * @param ls Pointer to the LexState structure representing the current lexical state.
 */
static void statlist (LexState *ls) {
  /* statlist -> { stat [';'] } */
  while (!block_follow(ls, 1)) {
    if (ls->t.token == TK_RETURN) {
      statement(ls);
      return;  /* 'return' must be last statement */
    }
    statement(ls);
  }
}


/**
 * Processes a field selection in the lexer state.
 * 
 * This method handles the selection of a field from a table, which can be either a dot ('.') 
 * or colon (':') followed by a name. It ensures that the expression representing the table 
 * is in a register and then processes the field name. The method updates the expression 
 * descriptor to reflect the indexed access to the field.
 *
 * @param ls Pointer to the LexState structure, representing the current lexer state.
 * @param v Pointer to the expdesc structure, representing the expression to be indexed.
 *
 * The method performs the following steps:
 * 1. Ensures the expression in `v` is in a register using `luaK_exp2anyregup`.
 * 2. Advances the lexer to skip the dot or colon using `luaX_next`.
 * 3. Checks and retrieves the field name using `checkname`.
 * 4. Updates the expression descriptor `v` to represent the indexed access using `luaK_indexed`.
 */
static void fieldsel (LexState *ls, expdesc *v) {
  /* fieldsel -> ['.' | ':'] NAME */
  FuncState *fs = ls->fs;
  expdesc key;
  luaK_exp2anyregup(fs, v);
  luaX_next(ls);  /* skip the dot or colon */
  checkname(ls, &key);
  luaK_indexed(fs, v, &key);
}


/**
 * Processes an index expression in the form of '[' expr ']'.
 * 
 * This method is responsible for handling index expressions within the lexer state.
 * It skips the opening '[' character, evaluates the expression within the brackets,
 * ensures the expression is converted to a value, and then checks for the closing ']'.
 * 
 * @param ls Pointer to the LexState structure representing the current lexer state.
 * @param v Pointer to the expdesc structure where the resulting expression descriptor
 *          will be stored after evaluating the index expression.
 */
static void yindex (LexState *ls, expdesc *v) {
  /* index -> '[' expr ']' */
  luaX_next(ls);  /* skip the '[' */
  expr(ls, v);
  luaK_exp2val(ls->fs, v);
  checknext(ls, ']');
}


/*
** {======================================================================
** Rules for Constructors
** =======================================================================
*/


struct ConsControl {
  expdesc v;  /* last list item read */
  expdesc *t;  /* table descriptor */
  int nh;  /* total number of 'record' elements */
  int na;  /* total number of array elements */
  int tostore;  /* number of array elements pending to be stored */
};


/**
 * Processes a record field in a table constructor.
 *
 * This function handles the parsing and code generation for a record field
 * in a table constructor. A record field can be either a named field (using
 * an identifier) or an indexed field (using an expression within square brackets).
 * The field is followed by an assignment operator '=' and an expression that
 * provides the value for the field.
 *
 * The function performs the following steps:
 * 1. Checks if the current token is a name (TK_NAME) or an opening square bracket ('[').
 * 2. If it is a name, it checks the limit of items in the constructor and validates the name.
 * 3. If it is an opening square bracket, it processes the index expression.
 * 4. Increments the count of items in the constructor.
 * 5. Ensures the presence of the assignment operator '='.
 * 6. Converts the key and value expressions to register indices.
 * 7. Generates the bytecode for setting the table field using the OP_SETTABLE instruction.
 * 8. Restores the free register count to its original state.
 *
 * @param ls Pointer to the LexState structure, which holds the lexical analyzer state.
 * @param cc Pointer to the ConsControl structure, which manages the table constructor state.
 */
static void recfield (LexState *ls, struct ConsControl *cc) {
  /* recfield -> (NAME | '['exp1']') = exp1 */
  FuncState *fs = ls->fs;
  int reg = ls->fs->freereg;
  expdesc key, val;
  int rkkey;
  if (ls->t.token == TK_NAME) {
    checklimit(fs, cc->nh, MAX_INT, "items in a constructor");
    checkname(ls, &key);
  }
  else  /* ls->t.token == '[' */
    yindex(ls, &key);
  cc->nh++;
  checknext(ls, '=');
  rkkey = luaK_exp2RK(fs, &key);
  expr(ls, &val);
  luaK_codeABC(fs, OP_SETTABLE, cc->t->u.info, rkkey, luaK_exp2RK(fs, &val));
  fs->freereg = reg;  /* free registers */
}


/**
 * Closes the current list field in the function state, handling any pending items.
 * 
 * This method is responsible for finalizing the current list field being constructed.
 * If the current list item is void (i.e., no item is present), the method returns immediately.
 * Otherwise, it converts the current expression to the next available register and marks
 * the current item as void. If the number of items to store reaches the flush threshold
 * (LFIELDS_PER_FLUSH), the method flushes the pending items to the list and resets the
 * item counter.
 *
 * @param fs Pointer to the FuncState structure representing the current function state.
 * @param cc Pointer to the ConsControl structure managing the list construction.
 */
static void closelistfield (FuncState *fs, struct ConsControl *cc) {
  if (cc->v.k == VVOID) return;  /* there is no list item */
  luaK_exp2nextreg(fs, &cc->v);
  cc->v.k = VVOID;
  if (cc->tostore == LFIELDS_PER_FLUSH) {
    luaK_setlist(fs, cc->t->u.info, cc->na, cc->tostore);  /* flush */
    cc->tostore = 0;  /* no more items pending */
  }
}


/**
 * Processes the last field in a list construction and updates the function state accordingly.
 * This method handles two scenarios based on the properties of the last field:
 * 1. If the last field has multiple return values (as determined by `hasmultret`), it sets up
 *    the function state to handle multiple return values and adjusts the list construction
 *    to account for the unknown number of elements.
 * 2. If the last field does not have multiple return values, it ensures the field is stored
 *    in the next available register (if not void) and updates the list construction with
 *    the known number of elements.
 *
 * @param fs Pointer to the function state, which tracks the state of the current function
 *           being compiled.
 * @param cc Pointer to the ConsControl structure, which manages the state of the list
 *           construction, including the number of elements to store and the current
 *           expression being processed.
 */
static void lastlistfield (FuncState *fs, struct ConsControl *cc) {
  if (cc->tostore == 0) return;
  if (hasmultret(cc->v.k)) {
    luaK_setmultret(fs, &cc->v);
    luaK_setlist(fs, cc->t->u.info, cc->na, LUA_MULTRET);
    cc->na--;  /* do not count last expression (unknown number of elements) */
  }
  else {
    if (cc->v.k != VVOID)
      luaK_exp2nextreg(fs, &cc->v);
    luaK_setlist(fs, cc->t->u.info, cc->na, cc->tostore);
  }
}


/**
 * Processes a list field within a constructor by parsing an expression and updating the constructor control state.
 * 
 * This function is responsible for handling a single field in a list-style constructor. It parses the expression 
 * associated with the field using the `expr` function and updates the constructor control structure (`ConsControl`) 
 * to reflect the addition of a new item. The function also ensures that the number of items in the constructor 
 * does not exceed the maximum allowed limit (`MAX_INT`), raising an error if the limit is exceeded.
 * 
 * @param ls Pointer to the LexState structure, which maintains the lexical analyzer's state.
 * @param cc Pointer to the ConsControl structure, which tracks the state of the constructor being built.
 * 
 * @note The `ConsControl` structure is updated by incrementing the `na` (number of array items) and `tostore` 
 * (number of items to store) fields to reflect the addition of a new item to the constructor.
 */
static void listfield (LexState *ls, struct ConsControl *cc) {
  /* listfield -> exp */
  expr(ls, &cc->v);
  checklimit(ls->fs, cc->na, MAX_INT, "items in a constructor");
  cc->na++;
  cc->tostore++;
}


/**
 * Processes a field in a table constructor, determining whether it is a list field or a record field.
 * 
 * A field in a table constructor can be either a list field or a record field. A list field is an expression
 * that is added to the table in sequence, while a record field is a key-value pair where the key is either an
 * identifier or an expression in square brackets.
 * 
 * This method examines the current token in the lexer state (`ls->t.token`) to determine the type of field:
 * - If the token is `TK_NAME`, it checks the lookahead token to determine if it is followed by an '='. If not,
 *   it processes the field as a list field; otherwise, it processes it as a record field.
 * - If the token is '[', it processes the field as a record field.
 * - For all other tokens, it processes the field as a list field.
 * 
 * @param ls Pointer to the lexer state, which contains the current token and other parsing context.
 * @param cc Pointer to the constructor control structure, which manages the construction of the table.
 */
static void field (LexState *ls, struct ConsControl *cc) {
  /* field -> listfield | recfield */
  switch(ls->t.token) {
    case TK_NAME: {  /* may be 'listfield' or 'recfield' */
      if (luaX_lookahead(ls) != '=')  /* expression? */
        listfield(ls, cc);
      else
        recfield(ls, cc);
      break;
    }
    case '[': {
      recfield(ls, cc);
      break;
    }
    default: {
      listfield(ls, cc);
      break;
    }
  }
}


/**
 * Parses and constructs a table constructor in the Lua source code.
 * 
 * This method handles the syntax for table constructors, which are enclosed in curly braces `{}`.
 * It supports optional separators (comma `,` or semicolon `;`) between fields. The method initializes
 * a new table in the Lua virtual machine, processes each field in the constructor, and finalizes the
 * table's array and hash sizes based on the number of elements added.
 *
 * @param ls Pointer to the LexState structure, which holds the current lexical state and token stream.
 * @param t Pointer to an expdesc structure, which represents the resulting table expression.
 *
 * The method performs the following steps:
 * 1. Generates a new table using the `OP_NEWTABLE` opcode.
 * 2. Initializes a ConsControl structure to manage table construction.
 * 3. Processes each field in the constructor, updating the table's array and hash counts.
 * 4. Ensures the constructor is properly closed with a matching `}`.
 * 5. Sets the initial array and hash sizes in the table's opcode arguments.
 */
static void constructor (LexState *ls, expdesc *t) {
  /* constructor -> '{' [ field { sep field } [sep] ] '}'
     sep -> ',' | ';' */
  FuncState *fs = ls->fs;
  int line = ls->linenumber;
  int pc = luaK_codeABC(fs, OP_NEWTABLE, 0, 0, 0);
  struct ConsControl cc;
  cc.na = cc.nh = cc.tostore = 0;
  cc.t = t;
  init_exp(t, VRELOCABLE, pc);
  init_exp(&cc.v, VVOID, 0);  /* no value (yet) */
  luaK_exp2nextreg(ls->fs, t);  /* fix it at stack top */
  checknext(ls, '{');
  do {
    lua_assert(cc.v.k == VVOID || cc.tostore > 0);
    if (ls->t.token == '}') break;
    closelistfield(fs, &cc);
    field(ls, &cc);
  } while (testnext(ls, ',') || testnext(ls, ';'));
  check_match(ls, '}', '{', line);
  lastlistfield(fs, &cc);
  SETARG_B(fs->f->code[pc], luaO_int2fb(cc.na)); /* set initial array size */
  SETARG_C(fs->f->code[pc], luaO_int2fb(cc.nh));  /* set initial table size */
}

/* }====================================================================== */



static void parlist (LexState *ls) {
  /* parlist -> [ param { ',' param } ] */
  FuncState *fs = ls->fs;
  Proto *f = fs->f;
  int nparams = 0;
  f->is_vararg = 0;
  if (ls->t.token != ')') {  /* is 'parlist' not empty? */
    do {
      switch (ls->t.token) {
        case TK_NAME: {  /* param -> NAME */
          new_localvar(ls, str_checkname(ls));
          nparams++;
          break;
        }
        case TK_DOTS: {  /* param -> '...' */
          luaX_next(ls);
          f->is_vararg = 1;  /* declared vararg */
          break;
        }
        default: luaX_syntaxerror(ls, "<name> or '...' expected");
      }
    } while (!f->is_vararg && testnext(ls, ','));
  }
  adjustlocalvars(ls, nparams);
  f->numparams = cast_byte(fs->nactvar);
  luaK_reserveregs(fs, fs->nactvar);  /* reserve register for parameters */
}


/**
 * Parses and compiles a function body in the context of the given LexState.
 * 
 * This method handles the syntax and semantics of a function body, which includes:
 * - The function prototype and its definition line.
 * - The parameter list, including the special 'self' parameter for methods.
 * - The block of statements that make up the function body.
 * - The closing 'end' keyword to signify the end of the function body.
 * 
 * The method also manages the function's lexical scope and generates the corresponding
 * bytecode for the function closure.
 * 
 * @param ls The LexState object representing the current lexical analyzer state.
 * @param e The expdesc object where the resulting function closure will be stored.
 * @param ismethod A flag indicating whether the function is a method (1) or a regular function (0).
 * @param line The line number where the function definition starts.
 */
static void body (LexState *ls, expdesc *e, int ismethod, int line) {
  /* body ->  '(' parlist ')' block END */
  FuncState new_fs;
  BlockCnt bl;
  new_fs.f = addprototype(ls);
  new_fs.f->linedefined = line;
  open_func(ls, &new_fs, &bl);
  checknext(ls, '(');
  if (ismethod) {
    new_localvarliteral(ls, "self");  /* create 'self' parameter */
    adjustlocalvars(ls, 1);
  }
  parlist(ls);
  checknext(ls, ')');
  statlist(ls);
  new_fs.f->lastlinedefined = ls->linenumber;
  check_match(ls, TK_END, TK_FUNCTION, line);
  codeclosure(ls, e);
  close_func(ls);
}


/**
 * Parses a list of expressions separated by commas in the given lexical state.
 * 
 * This function processes a sequence of expressions, where each expression is separated by a comma.
 * It starts by parsing the first expression and then iteratively parses subsequent expressions as long
 * as a comma is encountered. Each parsed expression is processed to ensure it is stored in the next
 * available register in the function's stack frame. The function returns the total number of expressions
 * parsed in the list.
 *
 * @param ls Pointer to the LexState structure representing the current lexical state.
 * @param v Pointer to an expdesc structure where the parsed expression will be stored.
 * @return The total number of expressions parsed in the list.
 */
static int explist (LexState *ls, expdesc *v) {
  /* explist -> expr { ',' expr } */
  int n = 1;  /* at least one expression */
  expr(ls, v);
  while (testnext(ls, ',')) {
    luaK_exp2nextreg(ls->fs, v);
    expr(ls, v);
    n++;
  }
  return n;
}


/**
 * Processes function arguments in the context of a LexState and generates the corresponding
 * bytecode for the function call. This method handles different types of function arguments,
 * including:
 * - Parenthesized argument lists (e.g., `func()` or `func(a, b, c)`).
 * - Table constructors (e.g., `func{...}`).
 * - String literals (e.g., `func"string"`).
 *
 * The method ensures that the arguments are properly parsed and prepared for the function call.
 * It updates the function state (`FuncState`) and the expression descriptor (`expdesc`) to reflect
 * the processed arguments. The resulting bytecode includes the necessary instructions to
 * execute the function call with the provided arguments.
 *
 * @param ls The LexState containing the lexical analyzer state and token stream.
 * @param f The expression descriptor representing the function being called.
 * @param line The line number in the source code where the function call occurs.
 */
static void funcargs (LexState *ls, expdesc *f, int line) {
  FuncState *fs = ls->fs;
  expdesc args;
  int base, nparams;
  switch (ls->t.token) {
    case '(': {  /* funcargs -> '(' [ explist ] ')' */
      luaX_next(ls);
      if (ls->t.token == ')')  /* arg list is empty? */
        args.k = VVOID;
      else {
        explist(ls, &args);
        luaK_setmultret(fs, &args);
      }
      check_match(ls, ')', '(', line);
      break;
    }
    case '{': {  /* funcargs -> constructor */
      constructor(ls, &args);
      break;
    }
    case TK_STRING: {  /* funcargs -> STRING */
      codestring(ls, &args, ls->t.seminfo.ts);
      luaX_next(ls);  /* must use 'seminfo' before 'next' */
      break;
    }
    default: {
      luaX_syntaxerror(ls, "function arguments expected");
    }
  }
  lua_assert(f->k == VNONRELOC);
  base = f->u.info;  /* base register for call */
  if (hasmultret(args.k))
    nparams = LUA_MULTRET;  /* open call */
  else {
    if (args.k != VVOID)
      luaK_exp2nextreg(fs, &args);  /* close last argument */
    nparams = fs->freereg - (base+1);
  }
  init_exp(f, VCALL, luaK_codeABC(fs, OP_CALL, base, nparams+1, 2));
  luaK_fixline(fs, line);
  fs->freereg = base+1;  /* call remove function and arguments and leaves
                            (unless changed) one result */
}




/*
** {======================================================================
** Expression parsing
** =======================================================================
*/


static void primaryexp (LexState *ls, expdesc *v) {
  /* primaryexp -> NAME | '(' expr ')' */
  switch (ls->t.token) {
    case '(': {
      int line = ls->linenumber;
      luaX_next(ls);
      expr(ls, v);
      check_match(ls, ')', '(', line);
      luaK_dischargevars(ls->fs, v);
      return;
    }
    case TK_NAME: {
      singlevar(ls, v);
      return;
    }
    default: {
      luaX_syntaxerror(ls, "unexpected symbol");
    }
  }
}


/**
 * Parses and evaluates a suffixed expression in the Lua source code.
 * 
 * A suffixed expression starts with a primary expression and can be followed by
 * one or more suffix operations. These suffix operations include:
 * - Field selection (e.g., `obj.field`),
 * - Indexing (e.g., `obj[index]`),
 * - Method calls with the colon syntax (e.g., `obj:method(args)`),
 * - Function calls (e.g., `func(args)`).
 * 
 * The method processes the primary expression first and then iteratively
 * handles any suffix operations until no more are found. It updates the
 * `expdesc` structure `v` to reflect the current state of the expression being
 * parsed.
 * 
 * @param ls Pointer to the LexState structure, which holds the current state
 *           of the lexical analyzer.
 * @param v  Pointer to the expdesc structure, which describes the expression
 *           being parsed and evaluated.
 */
static void suffixedexp (LexState *ls, expdesc *v) {
  /* suffixedexp ->
       primaryexp { '.' NAME | '[' exp ']' | ':' NAME funcargs | funcargs } */
  FuncState *fs = ls->fs;
  int line = ls->linenumber;
  primaryexp(ls, v);
  for (;;) {
    switch (ls->t.token) {
      case '.': {  /* fieldsel */
        fieldsel(ls, v);
        break;
      }
      case '[': {  /* '[' exp1 ']' */
        expdesc key;
        luaK_exp2anyregup(fs, v);
        yindex(ls, &key);
        luaK_indexed(fs, v, &key);
        break;
      }
      case ':': {  /* ':' NAME funcargs */
        expdesc key;
        luaX_next(ls);
        checkname(ls, &key);
        luaK_self(fs, v, &key);
        funcargs(ls, v, line);
        break;
      }
      case '(': case TK_STRING: case '{': {  /* funcargs */
        luaK_exp2nextreg(fs, v);
        funcargs(ls, v, line);
        break;
      }
      default: return;
    }
  }
}


/**
 * Parses a simple expression in the lexer state and initializes the expression descriptor.
 *
 * This method handles various types of simple expressions, including:
 * - Floating-point literals (FLT)
 * - Integer literals (INT)
 * - String literals (STRING)
 * - Nil (NIL)
 * - Boolean literals (TRUE, FALSE)
 * - Vararg expressions (TK_DOTS)
 * - Constructors (enclosed in '{' and '}')
 * - Function definitions (TK_FUNCTION)
 * - Suffixed expressions (e.g., variable access, function calls)
 *
 * The method processes the current token in the lexer state (ls) and initializes
 * the expression descriptor (v) based on the token type. It also advances the lexer
 * to the next token after processing the current one.
 *
 * @param ls Pointer to the lexer state, which contains the current token and other parsing context.
 * @param v Pointer to the expression descriptor to be initialized based on the parsed expression.
 */
static void simpleexp (LexState *ls, expdesc *v) {
  /* simpleexp -> FLT | INT | STRING | NIL | TRUE | FALSE | ... |
                  constructor | FUNCTION body | suffixedexp */
  switch (ls->t.token) {
    case TK_FLT: {
      init_exp(v, VKFLT, 0);
      v->u.nval = ls->t.seminfo.r;
      break;
    }
    case TK_INT: {
      init_exp(v, VKINT, 0);
      v->u.ival = ls->t.seminfo.i;
      break;
    }
    case TK_STRING: {
      codestring(ls, v, ls->t.seminfo.ts);
      break;
    }
    case TK_NIL: {
      init_exp(v, VNIL, 0);
      break;
    }
    case TK_TRUE: {
      init_exp(v, VTRUE, 0);
      break;
    }
    case TK_FALSE: {
      init_exp(v, VFALSE, 0);
      break;
    }
    case TK_DOTS: {  /* vararg */
      FuncState *fs = ls->fs;
      check_condition(ls, fs->f->is_vararg,
                      "cannot use '...' outside a vararg function");
      init_exp(v, VVARARG, luaK_codeABC(fs, OP_VARARG, 0, 1, 0));
      break;
    }
    case '{': {  /* constructor */
      constructor(ls, v);
      return;
    }
    case TK_FUNCTION: {
      luaX_next(ls);
      body(ls, v, 0, ls->linenumber);
      return;
    }
    default: {
      suffixedexp(ls, v);
      return;
    }
  }
  luaX_next(ls);
}


/**
 * Retrieves the unary operator corresponding to the given token.
 *
 * This method maps a token representing a unary operator to its corresponding
 * unary operator enumeration value. The token is typically a character or a
 * predefined token constant (e.g., TK_NOT). If the token does not correspond
 * to any known unary operator, the method returns OPR_NOUNOPR.
 *
 * @param op The token or character representing the unary operator.
 * @return The unary operator enumeration value corresponding to the token.
 *         Returns OPR_NOUNOPR if the token does not match any known unary operator.
 */
static UnOpr getunopr(int op) {
  switch (op) {
    case TK_NOT: return OPR_NOT;
    case '-': return OPR_MINUS;
    case '~': return OPR_BNOT;
    case '#': return OPR_LEN;
    default: return OPR_NOUNOPR;
  }
}


/**
 * Converts a given operator token or character into its corresponding binary operator representation.
 * 
 * This function takes an integer `op` representing an operator token or character and maps it to
 * the appropriate binary operator defined by the `BinOpr` enumeration. The function supports a wide
 * range of operators, including arithmetic (e.g., '+', '-', '*', '/'), bitwise (e.g., '&', '|', '~'),
 * comparison (e.g., '<', '>', TK_EQ, TK_NE), and logical operators (e.g., TK_AND, TK_OR). If the input
 * `op` does not match any known operator, the function returns `OPR_NOBINOPR` to indicate an invalid
 * or unsupported operator.
 * 
 * @param op The operator token or character to be converted.
 * @return The corresponding `BinOpr` enumeration value for the given operator, or `OPR_NOBINOPR` if
 *         the operator is not recognized.
 */
static BinOpr getbinopr (int op) {
  switch (op) {
    case '+': return OPR_ADD;
    case '-': return OPR_SUB;
    case '*': return OPR_MUL;
    case '%': return OPR_MOD;
    case '^': return OPR_POW;
    case '/': return OPR_DIV;
    case TK_IDIV: return OPR_IDIV;
    case '&': return OPR_BAND;
    case '|': return OPR_BOR;
    case '~': return OPR_BXOR;
    case TK_SHL: return OPR_SHL;
    case TK_SHR: return OPR_SHR;
    case TK_CONCAT: return OPR_CONCAT;
    case TK_NE: return OPR_NE;
    case TK_EQ: return OPR_EQ;
    case '<': return OPR_LT;
    case TK_LE: return OPR_LE;
    case '>': return OPR_GT;
    case TK_GE: return OPR_GE;
    case TK_AND: return OPR_AND;
    case TK_OR: return OPR_OR;
    default: return OPR_NOBINOPR;
  }
}


static const struct {
  lu_byte left;  /* left priority for each binary operator */
  lu_byte right; /* right priority */
} priority[] = {  /* ORDER OPR */
   {10, 10}, {10, 10},           /* '+' '-' */
   {11, 11}, {11, 11},           /* '*' '%' */
   {14, 13},                  /* '^' (right associative) */
   {11, 11}, {11, 11},           /* '/' '//' */
   {6, 6}, {4, 4}, {5, 5},   /* '&' '|' '~' */
   {7, 7}, {7, 7},           /* '<<' '>>' */
   {9, 8},                   /* '..' (right associative) */
   {3, 3}, {3, 3}, {3, 3},   /* ==, <, <= */
   {3, 3}, {3, 3}, {3, 3},   /* ~=, >, >= */
   {2, 2}, {1, 1}            /* and, or */
};

#define UNARY_PRIORITY	12  /* priority for unary operators */


/*
** subexpr -> (simpleexp | unop subexpr) { binop subexpr }
** where 'binop' is any binary operator with a priority higher than 'limit'
*/
static BinOpr subexpr (LexState *ls, expdesc *v, int limit) {
  BinOpr op;
  UnOpr uop;
  enterlevel(ls);
  uop = getunopr(ls->t.token);
  if (uop != OPR_NOUNOPR) {
    int line = ls->linenumber;
    luaX_next(ls);
    subexpr(ls, v, UNARY_PRIORITY);
    luaK_prefix(ls->fs, uop, v, line);
  }
  else simpleexp(ls, v);
  /* expand while operators have priorities higher than 'limit' */
  op = getbinopr(ls->t.token);
  while (op != OPR_NOBINOPR && priority[op].left > limit) {
    expdesc v2;
    BinOpr nextop;
    int line = ls->linenumber;
    luaX_next(ls);
    luaK_infix(ls->fs, op, v);
    /* read sub-expression with higher priority */
    nextop = subexpr(ls, &v2, priority[op].right);
    luaK_posfix(ls->fs, op, v, &v2, line);
    op = nextop;
  }
  leavelevel(ls);
  return op;  /* return first untreated operator */
}


/**
 * Parses an expression in the current lexical state and stores the result in the provided expression descriptor.
 * This function serves as a wrapper around the `subexpr` function, initiating the parsing of an expression
 * with the lowest precedence level (0). It is typically used to parse a complete expression in the context
 * of the current lexical state.
 *
 * @param ls Pointer to the LexState structure representing the current lexical state.
 * @param v Pointer to the expdesc structure where the parsed expression's descriptor will be stored.
 */
static void expr (LexState *ls, expdesc *v) {
  subexpr(ls, v, 0);
}

/* }==================================================================== */



/*
** {======================================================================
** Rules for Statements
** =======================================================================
*/


static void block (LexState *ls) {
  /* block -> statlist */
  FuncState *fs = ls->fs;
  BlockCnt bl;
  enterblock(fs, &bl, 0);
  statlist(ls);
  leaveblock(fs);
}


/*
** structure to chain all variables in the left-hand side of an
** assignment
*/
struct LHS_assign {
  struct LHS_assign *prev;
  expdesc v;  /* variable (global, local, upvalue, or indexed) */
};


/*
** check whether, in an assignment to an upvalue/local variable, the
** upvalue/local variable is begin used in a previous assignment to a
** table. If so, save original upvalue/local value in a safe place and
** use this safe copy in the previous assignment.
*/
static void check_conflict (LexState *ls, struct LHS_assign *lh, expdesc *v) {
  FuncState *fs = ls->fs;
  int extra = fs->freereg;  /* eventual position to save local variable */
  int conflict = 0;
  for (; lh; lh = lh->prev) {  /* check all previous assignments */
    if (lh->v.k == VINDEXED) {  /* assigning to a table? */
      /* table is the upvalue/local being assigned now? */
      if (lh->v.u.ind.vt == v->k && lh->v.u.ind.t == v->u.info) {
        conflict = 1;
        lh->v.u.ind.vt = VLOCAL;
        lh->v.u.ind.t = extra;  /* previous assignment will use safe copy */
      }
      /* index is the local being assigned? (index cannot be upvalue) */
      if (v->k == VLOCAL && lh->v.u.ind.idx == v->u.info) {
        conflict = 1;
        lh->v.u.ind.idx = extra;  /* previous assignment will use safe copy */
      }
    }
  }
  if (conflict) {
    /* copy upvalue/local value to a temporary (in position 'extra') */
    OpCode op = (v->k == VLOCAL) ? OP_MOVE : OP_GETUPVAL;
    luaK_codeABC(fs, op, extra, v->u.info, 0);
    luaK_reserveregs(fs, 1);
  }
}


/**
 * Handles the assignment operation in the parser.
 *
 * This method processes an assignment statement, which can be either a single assignment
 * or a multiple assignment separated by commas. It ensures that the left-hand side (LHS)
 * of the assignment is a valid variable or indexed expression. For multiple assignments,
 * it recursively processes each LHS expression and checks for conflicts between variables.
 * For single assignments, it processes the right-hand side (RHS) expression list and adjusts
 * the assignment if the number of expressions does not match the number of variables.
 * Finally, it stores the result of the RHS expression into the LHS variable.
 *
 * @param ls The LexState structure representing the current lexical state.
 * @param lh The LHS_assign structure representing the left-hand side of the assignment.
 * @param nvars The number of variables in the current assignment.
 */
static void assignment (LexState *ls, struct LHS_assign *lh, int nvars) {
  expdesc e;
  check_condition(ls, vkisvar(lh->v.k), "syntax error");
  if (testnext(ls, ',')) {  /* assignment -> ',' suffixedexp assignment */
    struct LHS_assign nv;
    nv.prev = lh;
    suffixedexp(ls, &nv.v);
    if (nv.v.k != VINDEXED)
      check_conflict(ls, lh, &nv.v);
    checklimit(ls->fs, nvars + ls->L->nCcalls, LUAI_MAXCCALLS,
                    "C levels");
    assignment(ls, &nv, nvars+1);
  }
  else {  /* assignment -> '=' explist */
    int nexps;
    checknext(ls, '=');
    nexps = explist(ls, &e);
    if (nexps != nvars)
      adjust_assign(ls, nvars, nexps, &e);
    else {
      luaK_setoneret(ls->fs, &e);  /* close last expression */
      luaK_storevar(ls->fs, &lh->v, &e);
      return;  /* avoid default */
    }
  }
  init_exp(&e, VNONRELOC, ls->fs->freereg-1);  /* default assignment */
  luaK_storevar(ls->fs, &lh->v, &e);
}


/**
 * Evaluates a condition in the current lexical state.
 * 
 * This method reads and evaluates an expression from the lexical state `ls` and stores the result in `v`.
 * If the evaluated expression is `VNIL`, it is treated as `VFALSE` since all 'falses' are considered equal in this context.
 * The method then generates code to jump to a label if the condition is true using `luaK_goiftrue`.
 * 
 * @param ls Pointer to the lexical state containing the condition to be evaluated.
 * @return The false label associated with the evaluated condition.
 */
static int cond (LexState *ls) {
  /* cond -> exp */
  expdesc v;
  expr(ls, &v);  /* read condition */
  if (v.k == VNIL) v.k = VFALSE;  /* 'falses' are all equal here */
  luaK_goiftrue(ls->fs, &v);
  return v.f;
}


/**
 * Processes a goto statement or a break statement within the lexer state.
 * 
 * This function handles the parsing and semantic analysis of goto and break statements.
 * It first checks if the current token is a `TK_GOTO` token. If so, it retrieves the label name
 * using `str_checkname`. If not, it assumes a break statement, skips the `TK_BREAK` token,
 * and creates a label named "break". It then creates a new label entry in the global label table
 * using `newlabelentry`, associating the label with the current line number and the program counter (pc).
 * Finally, it checks if the label is already defined using `findlabel` and closes it if necessary.
 * 
 * @param ls The lexer state containing the current parsing context.
 * @param pc The program counter indicating the current position in the bytecode.
 */
static void gotostat (LexState *ls, int pc) {
  int line = ls->linenumber;
  TString *label;
  int g;
  if (testnext(ls, TK_GOTO))
    label = str_checkname(ls);
  else {
    luaX_next(ls);  /* skip break */
    label = luaS_new(ls->L, "break");
  }
  g = newlabelentry(ls, &ls->dyd->gt, label, line, pc);
  findlabel(ls, g);  /* close it if label already defined */
}


/* check for repeated labels on the same block */
static void checkrepeated (FuncState *fs, Labellist *ll, TString *label) {
  int i;
  for (i = fs->bl->firstlabel; i < ll->n; i++) {
    if (eqstr(label, ll->arr[i].name)) {
      const char *msg = luaO_pushfstring(fs->ls->L,
                          "label '%s' already defined on line %d",
                          getstr(label), ll->arr[i].line);
      semerror(fs->ls, msg);
    }
  }
}


/* skip no-op statements */
static void skipnoopstat (LexState *ls) {
  while (ls->t.token == ';' || ls->t.token == TK_DBCOLON)
    statement(ls);
}


/**
 * Processes a label statement in the lexer state.
 *
 * This function handles the parsing and registration of a label in the current lexical scope.
 * It ensures that the label is not a duplicate, skips the double colon token that marks the label,
 * and creates a new entry for the label in the label list. Additionally, it skips any no-op statements
 * that follow the label and adjusts the scope of local variables if the label is the last statement
 * in a block. Finally, it resolves any goto statements that reference this label.
 *
 * @param ls The lexer state containing the current parsing context.
 * @param label The string representing the label name.
 * @param line The line number where the label is defined.
 */
static void labelstat (LexState *ls, TString *label, int line) {
  /* label -> '::' NAME '::' */
  FuncState *fs = ls->fs;
  Labellist *ll = &ls->dyd->label;
  int l;  /* index of new label being created */
  checkrepeated(fs, ll, label);  /* check for repeated labels */
  checknext(ls, TK_DBCOLON);  /* skip double colon */
  /* create new entry for this label */
  l = newlabelentry(ls, ll, label, line, luaK_getlabel(fs));
  skipnoopstat(ls);  /* skip other no-op statements */
  if (block_follow(ls, 0)) {  /* label is last no-op statement in the block? */
    /* assume that locals are already out of scope */
    ll->arr[l].nactvar = fs->bl->nactvar;
  }
  findgotos(ls, &ll->arr[l]);
}


/**
 * Processes a `while` statement in the Lua parser.
 *
 * This method handles the parsing and code generation for a `while` loop. It performs the following steps:
 * 1. Skips the `WHILE` token.
 * 2. Generates a label for the start of the loop (`whileinit`).
 * 3. Evaluates the loop condition using the `cond` function and stores the exit point for false conditions (`condexit`).
 * 4. Enters a new block scope for the loop body.
 * 5. Skips the `DO` token.
 * 6. Parses the loop body using the `block` function.
 * 7. Generates a jump back to the start of the loop (`whileinit`) to continue the loop.
 * 8. Ensures the `END` token matches the `WHILE` token.
 * 9. Leaves the block scope.
 * 10. Patches the false condition exit point to the current position to handle loop termination.
 *
 * @param ls The lexer state, which contains the current parsing context.
 * @param line The line number where the `while` statement begins, used for error reporting.
 */
static void whilestat (LexState *ls, int line) {
  /* whilestat -> WHILE cond DO block END */
  FuncState *fs = ls->fs;
  int whileinit;
  int condexit;
  BlockCnt bl;
  luaX_next(ls);  /* skip WHILE */
  whileinit = luaK_getlabel(fs);
  condexit = cond(ls);
  enterblock(fs, &bl, 1);
  checknext(ls, TK_DO);
  block(ls);
  luaK_jumpto(fs, whileinit);
  check_match(ls, TK_END, TK_WHILE, line);
  leaveblock(fs);
  luaK_patchtohere(fs, condexit);  /* false conditions finish the loop */
}


/**
 * Parses and compiles a 'repeat...until' loop statement.
 *
 * This method handles the syntax and semantic analysis of a 'repeat...until' loop,
 * which repeatedly executes a block of code until a specified condition is true.
 * The loop consists of a 'REPEAT' keyword, a block of statements, and an 'UNTIL'
 * keyword followed by a condition. The condition is evaluated after each iteration
 * of the loop block.
 *
 * The method performs the following steps:
 * 1. Marks the start of the loop with a label for later jump instructions.
 * 2. Enters a new block scope for the loop body.
 * 3. Enters a nested block scope for the condition evaluation.
 * 4. Skips the 'REPEAT' keyword and parses the block of statements.
 * 5. Ensures the 'UNTIL' keyword is present and matches the 'REPEAT' keyword.
 * 6. Parses and evaluates the condition expression.
 * 7. Handles upvalues in the nested scope, if any, by patching the control flow.
 * 8. Exits the nested scope and patches the loop's control flow to jump back to
 *    the start of the loop if the condition is false.
 * 9. Exits the loop block scope.
 *
 * @param ls Pointer to the LexState structure, which maintains the lexical state
 *           of the parser.
 * @param line The line number where the 'repeat' statement begins, used for error
 *             reporting and debugging.
 */
static void repeatstat (LexState *ls, int line) {
  /* repeatstat -> REPEAT block UNTIL cond */
  int condexit;
  FuncState *fs = ls->fs;
  int repeat_init = luaK_getlabel(fs);
  BlockCnt bl1, bl2;
  enterblock(fs, &bl1, 1);  /* loop block */
  enterblock(fs, &bl2, 0);  /* scope block */
  luaX_next(ls);  /* skip REPEAT */
  statlist(ls);
  check_match(ls, TK_UNTIL, TK_REPEAT, line);
  condexit = cond(ls);  /* read condition (inside scope block) */
  if (bl2.upval)  /* upvalues? */
    luaK_patchclose(fs, condexit, bl2.nactvar);
  leaveblock(fs);  /* finish scope */
  luaK_patchlist(fs, condexit, repeat_init);  /* close the loop */
  leaveblock(fs);  /* finish loop */
}


/**
 * Parses and compiles an expression in the current lexical state, ensuring it is evaluated
 * and stored in a register. This function performs the following steps:
 * 1. Initializes an `expdesc` structure to hold the expression's description.
 * 2. Parses the expression using the `expr` function, storing the result in the `expdesc` structure.
 * 3. Compiles the expression to ensure it is stored in the next available register using `luaK_exp2nextreg`.
 * 4. Asserts that the expression is in a non-relocatable form (VNONRELOC).
 * 5. Retrieves the register number where the expression's value is stored.
 * 6. Returns the register number.
 *
 * @param ls Pointer to the current lexical state.
 * @return The register number where the expression's value is stored.
 */
static int exp1 (LexState *ls) {
  expdesc e;
  int reg;
  expr(ls, &e);
  luaK_exp2nextreg(ls->fs, &e);
  lua_assert(e.k == VNONRELOC);
  reg = e.u.info;
  return reg;
}


/**
 * Handles the body of a 'for' loop in the Lua parser.
 * This function processes the control structure of a 'for' loop, including both numeric
 * and generic (iterator-based) 'for' loops. It sets up the necessary control variables,
 * enters a new block scope for the loop body, and generates the appropriate bytecode
 * instructions for loop execution.
 *
 * @param ls Pointer to the LexState structure, which holds the parser's state.
 * @param base The base register for the loop's control variables.
 * @param line The line number where the 'for' loop is defined in the source code.
 * @param nvars The number of loop variables (1 for numeric loops, 3 for generic loops).
 * @param isnum A flag indicating whether the loop is numeric (1) or generic (0).
 *
 * The function performs the following steps:
 * 1. Adjusts the local variables to accommodate the loop's control variables.
 * 2. Ensures the 'DO' keyword is present and consumes it.
 * 3. Generates the initial loop preparation bytecode (OP_FORPREP for numeric loops or a jump for generic loops).
 * 4. Enters a new block scope for the loop body and adjusts local variables for the loop variables.
 * 5. Reserves registers for the loop variables.
 * 6. Parses the loop body block.
 * 7. Exits the block scope for the loop variables.
 * 8. Patches the loop preparation bytecode to the current position.
 * 9. Generates the loop iteration bytecode (OP_FORLOOP for numeric loops or OP_TFORLOOP for generic loops).
 * 10. Patches the loop end jump to the loop preparation position.
 * 11. Fixes the line number for the generated bytecode.
 */
static void forbody (LexState *ls, int base, int line, int nvars, int isnum) {
  /* forbody -> DO block */
  BlockCnt bl;
  FuncState *fs = ls->fs;
  int prep, endfor;
  adjustlocalvars(ls, 3);  /* control variables */
  checknext(ls, TK_DO);
  prep = isnum ? luaK_codeAsBx(fs, OP_FORPREP, base, NO_JUMP) : luaK_jump(fs);
  enterblock(fs, &bl, 0);  /* scope for declared variables */
  adjustlocalvars(ls, nvars);
  luaK_reserveregs(fs, nvars);
  block(ls);
  leaveblock(fs);  /* end of scope for declared variables */
  luaK_patchtohere(fs, prep);
  if (isnum)  /* numeric for? */
    endfor = luaK_codeAsBx(fs, OP_FORLOOP, base, NO_JUMP);
  else {  /* generic for */
    luaK_codeABC(fs, OP_TFORCALL, base, 0, nvars);
    luaK_fixline(fs, line);
    endfor = luaK_codeAsBx(fs, OP_TFORLOOP, base + 2, NO_JUMP);
  }
  luaK_patchlist(fs, endfor, prep + 1);
  luaK_fixline(fs, line);
}


/**
 * Parses and compiles a numeric 'for' loop statement.
 *
 * This method handles the syntax for a numeric 'for' loop, which has the form:
 *   for varname = exp1, exp1[, exp1] do ... end
 * where:
 *   - `varname` is the loop variable.
 *   - The first `exp1` is the initial value of the loop variable.
 *   - The second `exp1` is the limit value.
 *   - The optional third `exp1` is the step value (defaults to 1 if not provided).
 *
 * The method performs the following steps:
 * 1. Creates local variables for the loop index, limit, and step.
 * 2. Registers the loop variable as a local variable.
 * 3. Parses and compiles the initial value, limit, and optional step expressions.
 * 4. If the step is not provided, defaults it to 1.
 * 5. Compiles the loop body using the `forbody` function.
 *
 * @param ls The lexer state, which holds the current parsing context.
 * @param varname The name of the loop variable.
 * @param line The line number where the 'for' loop starts.
 */
static void fornum (LexState *ls, TString *varname, int line) {
  /* fornum -> NAME = exp1,exp1[,exp1] forbody */
  FuncState *fs = ls->fs;
  int base = fs->freereg;
  new_localvarliteral(ls, "(for index)");
  new_localvarliteral(ls, "(for limit)");
  new_localvarliteral(ls, "(for step)");
  new_localvar(ls, varname);
  checknext(ls, '=');
  exp1(ls);  /* initial value */
  checknext(ls, ',');
  exp1(ls);  /* limit */
  if (testnext(ls, ','))
    exp1(ls);  /* optional step */
  else {  /* default step = 1 */
    luaK_codek(fs, fs->freereg, luaK_intK(fs, 1));
    luaK_reserveregs(fs, 1);
  }
  forbody(ls, base, line, 1, 1);
}


/**
 * Processes a 'for' loop statement with a list of variables in the Lua source code.
 * This method handles the syntax: `for NAME {, NAME} IN explist forbody`.
 * It creates control variables for the loop (generator, state, control) and local 
 * variables for the loop indices. The method then parses the expression list (`explist`) 
 * and generates the loop body (`forbody`).
 *
 * @param ls The LexState object representing the lexical analyzer state.
 * @param indexname The name of the first loop index variable (as a TString).
 *
 * The method performs the following steps:
 * 1. Initializes control variables for the loop: "(for generator)", "(for state)", and "(for control)".
 * 2. Creates a local variable for the loop index specified by `indexname`.
 * 3. If additional loop indices are provided (separated by commas), creates local variables for them.
 * 4. Ensures the 'IN' keyword is present in the source code.
 * 5. Parses the expression list (`explist`) that provides the values for the loop indices.
 * 6. Adjusts the assignment of the expression list to the loop indices.
 * 7. Generates the loop body using `forbody`, ensuring sufficient stack space for the generator function.
 */
static void forlist (LexState *ls, TString *indexname) {
  /* forlist -> NAME {,NAME} IN explist forbody */
  FuncState *fs = ls->fs;
  expdesc e;
  int nvars = 4;  /* gen, state, control, plus at least one declared var */
  int line;
  int base = fs->freereg;
  /* create control variables */
  new_localvarliteral(ls, "(for generator)");
  new_localvarliteral(ls, "(for state)");
  new_localvarliteral(ls, "(for control)");
  /* create declared variables */
  new_localvar(ls, indexname);
  while (testnext(ls, ',')) {
    new_localvar(ls, str_checkname(ls));
    nvars++;
  }
  checknext(ls, TK_IN);
  line = ls->linenumber;
  adjust_assign(ls, 3, explist(ls, &e), &e);
  luaK_checkstack(fs, 3);  /* extra space to call generator */
  forbody(ls, base, line, nvars - 3, 0);
}


/**
 * Parses and compiles a 'for' loop statement in the Lua source code.
 *
 * This method handles both numeric and generic 'for' loops. It first enters a new block scope
 * for the loop and its control variables. It then skips the 'for' keyword and retrieves the
 * first variable name. Depending on the next token encountered, it delegates to either
 * `fornum` (for numeric loops) or `forlist` (for generic loops). After processing the loop
 * body, it ensures the loop is properly closed with the 'end' keyword and leaves the block scope.
 *
 * @param ls Pointer to the LexState structure, which holds the lexical analyzer state.
 * @param line The line number in the source code where the 'for' statement begins.
 *
 * @throws luaX_syntaxerror if the expected '=' or 'in' token is missing.
 */
static void forstat (LexState *ls, int line) {
  /* forstat -> FOR (fornum | forlist) END */
  FuncState *fs = ls->fs;
  TString *varname;
  BlockCnt bl;
  enterblock(fs, &bl, 1);  /* scope for loop and control variables */
  luaX_next(ls);  /* skip 'for' */
  varname = str_checkname(ls);  /* first variable name */
  switch (ls->t.token) {
    case '=': fornum(ls, varname, line); break;
    case ',': case TK_IN: forlist(ls, varname); break;
    default: luaX_syntaxerror(ls, "'=' or 'in' expected");
  }
  check_match(ls, TK_END, TK_FOR, line);
  leaveblock(fs);  /* loop scope ('break' jumps to this point) */
}


/**
 * Processes an 'if' or 'elseif' conditional block in the Lua parser.
 * 
 * This method handles the parsing and code generation for 'if' or 'elseif' statements.
 * It skips the 'if' or 'elseif' token, evaluates the condition, and processes the 'then' block.
 * If the condition is false, it skips the 'then' block. If the condition is true, it executes
 * the 'then' block. Additionally, it handles 'goto' and 'break' statements within the block.
 * 
 * @param ls Pointer to the LexState structure, which holds the current state of the lexer.
 * @param escapelist Pointer to a list of jump instructions used for handling control flow
 *                   (e.g., jumps to 'else' or 'elseif' blocks).
 * 
 * The method performs the following steps:
 * 1. Skips the 'if' or 'elseif' token.
 * 2. Reads and evaluates the condition.
 * 3. Skips the 'then' token.
 * 4. Handles 'goto' or 'break' statements if present.
 * 5. Enters a new block scope.
 * 6. Processes the 'then' block.
 * 7. Leaves the block scope.
 * 8. Handles 'else' or 'elseif' blocks if present.
 */
static void test_then_block (LexState *ls, int *escapelist) {
  /* test_then_block -> [IF | ELSEIF] cond THEN block */
  BlockCnt bl;
  FuncState *fs = ls->fs;
  expdesc v;
  int jf;  /* instruction to skip 'then' code (if condition is false) */
  luaX_next(ls);  /* skip IF or ELSEIF */
  expr(ls, &v);  /* read condition */
  checknext(ls, TK_THEN);
  if (ls->t.token == TK_GOTO || ls->t.token == TK_BREAK) {
    luaK_goiffalse(ls->fs, &v);  /* will jump to label if condition is true */
    enterblock(fs, &bl, 0);  /* must enter block before 'goto' */
    gotostat(ls, v.t);  /* handle goto/break */
    skipnoopstat(ls);  /* skip other no-op statements */
    if (block_follow(ls, 0)) {  /* 'goto' is the entire block? */
      leaveblock(fs);
      return;  /* and that is it */
    }
    else  /* must skip over 'then' part if condition is false */
      jf = luaK_jump(fs);
  }
  else {  /* regular case (not goto/break) */
    luaK_goiftrue(ls->fs, &v);  /* skip over block if condition is false */
    enterblock(fs, &bl, 0);
    jf = v.f;
  }
  statlist(ls);  /* 'then' part */
  leaveblock(fs);
  if (ls->t.token == TK_ELSE ||
      ls->t.token == TK_ELSEIF)  /* followed by 'else'/'elseif'? */
    luaK_concat(fs, escapelist, luaK_jump(fs));  /* must jump over it */
  luaK_patchtohere(fs, jf);
}


/**
 * Parses and compiles an 'if' statement in the Lua source code.
 * 
 * This method handles the syntax of an 'if' statement, which includes:
 * - An initial 'if' condition followed by a 'then' block.
 * - Zero or more 'elseif' conditions, each followed by a 'then' block.
 * - An optional 'else' block.
 * - A mandatory 'end' token to close the 'if' statement.
 * 
 * The method compiles the control flow logic, including jumps for conditions
 * that evaluate to false, and ensures that all escape points are patched
 * correctly to the end of the 'if' statement.
 * 
 * @param ls Pointer to the LexState structure, which holds the lexical state
 *           and current token information.
 * @param line The line number where the 'if' statement starts, used for error
 *             reporting and debugging.
 */
static void ifstat (LexState *ls, int line) {
  /* ifstat -> IF cond THEN block {ELSEIF cond THEN block} [ELSE block] END */
  FuncState *fs = ls->fs;
  int escapelist = NO_JUMP;  /* exit list for finished parts */
  test_then_block(ls, &escapelist);  /* IF cond THEN block */
  while (ls->t.token == TK_ELSEIF)
    test_then_block(ls, &escapelist);  /* ELSEIF cond THEN block */
  if (testnext(ls, TK_ELSE))
    block(ls);  /* 'else' part */
  check_match(ls, TK_END, TK_IF, line);
  luaK_patchtohere(fs, escapelist);  /* patch escape list to 'if' end */
}


/**
 * Processes a local function definition within the current lexical scope.
 *
 * This method handles the creation and initialization of a local function. It performs the following steps:
 * 1. Creates a new local variable in the current lexical scope using the name obtained from the lexer state.
 * 2. Adjusts the local variable scope to account for the newly created variable.
 * 3. Generates the function body, storing the resulting expression descriptor in the provided `expdesc` structure.
 *    The function is created in the next available register.
 * 4. Updates the debug information to associate the local variable with its starting program counter (PC) value.
 *    Note that the debug information will only recognize the variable after this point.
 *
 * @param ls Pointer to the current lexical state, which contains the necessary context for parsing and scope management.
 */
static void localfunc (LexState *ls) {
  expdesc b;
  FuncState *fs = ls->fs;
  new_localvar(ls, str_checkname(ls));  /* new local variable */
  adjustlocalvars(ls, 1);  /* enter its scope */
  body(ls, &b, 0, ls->linenumber);  /* function created in next register */
  /* debug information will only see the variable after this point! */
  getlocvar(fs, b.u.info)->startpc = fs->pc;
}


/**
 * Handles the parsing of a local variable declaration statement in the lexer state.
 * 
 * This method processes a local variable declaration of the form:
 * 
 *   LOCAL NAME {',' NAME} ['=' explist]
 * 
 * Where:
 * - `LOCAL` is the keyword indicating a local variable declaration.
 * - `NAME` is the name of the local variable.
 * - `',' NAME` indicates additional local variables can be declared, separated by commas.
 * - `'=' explist` is an optional assignment of an expression list to the declared variables.
 * 
 * The method performs the following steps:
 * 1. Iterates through the list of variable names, creating new local variables for each name.
 * 2. If an assignment operator ('=') is found, it parses the expression list (`explist`) 
 *    and assigns it to the declared variables.
 * 3. If no assignment is found, the variables are initialized as `VVOID` (no value).
 * 4. Adjusts the assignment and local variable count in the lexer state to reflect the 
 *    newly declared variables.
 * 
 * @param ls The lexer state containing the current parsing context.
 */
static void localstat (LexState *ls) {
  /* stat -> LOCAL NAME {',' NAME} ['=' explist] */
  int nvars = 0;
  int nexps;
  expdesc e;
  do {
    new_localvar(ls, str_checkname(ls));
    nvars++;
  } while (testnext(ls, ','));
  if (testnext(ls, '='))
    nexps = explist(ls, &e);
  else {
    e.k = VVOID;
    nexps = 0;
  }
  adjust_assign(ls, nvars, nexps, &e);
  adjustlocalvars(ls, nvars);
}


/**
 * Parses a function name or method call in the context of a lexical state.
 *
 * This function processes a function name or method call syntax, which consists of a name
 * optionally followed by one or more field selectors (using the '.' operator) and an optional
 * method call (using the ':' operator). The function updates the provided `expdesc` structure
 * to represent the parsed expression.
 *
 * @param ls Pointer to the LexState structure representing the current lexical state.
 * @param v Pointer to the expdesc structure where the parsed expression will be stored.
 * @return Returns 1 if the function name is followed by a method call (i.e., contains a ':'),
 *         otherwise returns 0.
 *
 * The function first parses the initial name using `singlevar`. If the name is followed by
 * one or more '.' tokens, it processes each field selector using `fieldsel`. If the name is
 * followed by a ':' token, it indicates a method call, and the function processes the method
 * name using `fieldsel` and returns 1. Otherwise, it returns 0.
 */
static int funcname (LexState *ls, expdesc *v) {
  /* funcname -> NAME {fieldsel} [':' NAME] */
  int ismethod = 0;
  singlevar(ls, v);
  while (ls->t.token == '.')
    fieldsel(ls, v);
  if (ls->t.token == ':') {
    ismethod = 1;
    fieldsel(ls, v);
  }
  return ismethod;
}


/**
 * Parses and compiles a function statement in the Lua source code.
 *
 * This method processes a function statement, which consists of the 'FUNCTION' keyword,
 * a function name, and a function body. It skips the 'FUNCTION' keyword, parses the function
 * name to determine if it is a method (i.e., whether it uses the ':' syntax), and then
 * compiles the function body. Finally, it stores the compiled function in the appropriate
 * variable and sets the line number for the function definition.
 *
 * @param ls Pointer to the LexState structure, which maintains the state of the lexer.
 * @param line The line number in the source code where the function statement begins.
 *
 * The method performs the following steps:
 * 1. Skips the 'FUNCTION' keyword using `luaX_next`.
 * 2. Parses the function name using `funcname`, which returns whether the function is a method.
 * 3. Compiles the function body using `body`, passing the method flag and the starting line number.
 * 4. Stores the compiled function in the variable specified by the function name using `luaK_storevar`.
 * 5. Sets the line number for the function definition using `luaK_fixline`.
 */
static void funcstat (LexState *ls, int line) {
  /* funcstat -> FUNCTION funcname body */
  int ismethod;
  expdesc v, b;
  luaX_next(ls);  /* skip FUNCTION */
  ismethod = funcname(ls, &v);
  body(ls, &b, ismethod, line);
  luaK_storevar(ls->fs, &v, &b);
  luaK_fixline(ls->fs, line);  /* definition "happens" in the first line */
}


/**
 * Parses and processes an expression statement in the Lua source code.
 * 
 * This function handles two types of expression statements:
 * 1. Function calls: If the expression is a function call, it ensures that the
 *    function call is used as a statement (i.e., its return values are discarded).
 * 2. Assignments: If the expression is followed by an assignment operator ('=' or ','),
 *    it processes the assignment statement.
 * 
 * The function first parses a suffixed expression (which could be a variable, function call, etc.).
 * Based on the current token, it determines whether the expression is part of an assignment
 * or a function call statement. For assignments, it delegates to the `assignment` function.
 * For function calls, it verifies that the expression is indeed a function call and adjusts
 * the instruction to indicate that the call is used as a statement.
 * 
 * @param ls Pointer to the LexState structure, which contains the lexical analyzer state
 *           and other context needed for parsing.
 */
static void exprstat (LexState *ls) {
  /* stat -> func | assignment */
  FuncState *fs = ls->fs;
  struct LHS_assign v;
  suffixedexp(ls, &v.v);
  if (ls->t.token == '=' || ls->t.token == ',') { /* stat -> assignment ? */
    v.prev = NULL;
    assignment(ls, &v, 1);
  }
  else {  /* stat -> func */
    check_condition(ls, v.v.k == VCALL, "syntax error");
    SETARG_C(getinstruction(fs, &v.v), 1);  /* call statement uses no results */
  }
}


/**
 * Handles the `return` statement in the Lua parser.
 *
 * This method processes the `return` statement, which can optionally include a list of expressions
 * to return and an optional semicolon. The method determines the number of values to return and
 * their positions in the register stack. It also handles special cases such as tail calls and
 * multiple return values.
 *
 * @param ls Pointer to the LexState structure, which contains the lexical analyzer state.
 *
 * The method performs the following steps:
 * 1. Checks if the `return` statement is followed by a semicolon or if it is at the end of a block.
 *    If so, it returns no values.
 * 2. If there are return values, it parses the expression list using `explist` and determines the
 *    number of values to return.
 * 3. If the expression list results in multiple return values (e.g., a function call), it sets up
 *    the function state to return all values and handles tail call optimization if applicable.
 * 4. If there is only one return value, it ensures the value is in a register.
 * 5. If there are multiple return values, it ensures the values are on the stack.
 * 6. Finally, it generates the return instruction using `luaK_ret` and skips any optional semicolon.
 */
static void retstat (LexState *ls) {
  /* stat -> RETURN [explist] [';'] */
  FuncState *fs = ls->fs;
  expdesc e;
  int first, nret;  /* registers with returned values */
  if (block_follow(ls, 1) || ls->t.token == ';')
    first = nret = 0;  /* return no values */
  else {
    nret = explist(ls, &e);  /* optional return values */
    if (hasmultret(e.k)) {
      luaK_setmultret(fs, &e);
      if (e.k == VCALL && nret == 1) {  /* tail call? */
        SET_OPCODE(getinstruction(fs,&e), OP_TAILCALL);
        lua_assert(GETARG_A(getinstruction(fs,&e)) == fs->nactvar);
      }
      first = fs->nactvar;
      nret = LUA_MULTRET;  /* return all values */
    }
    else {
      if (nret == 1)  /* only one single value? */
        first = luaK_exp2anyreg(fs, &e);
      else {
        luaK_exp2nextreg(fs, &e);  /* values must go to the stack */
        first = fs->nactvar;  /* return all active values */
        lua_assert(nret == fs->freereg - first);
      }
    }
  }
  luaK_ret(fs, first, nret);
  testnext(ls, ';');  /* skip optional semicolon */
}


/**
 * Parses and executes a statement based on the current token in the LexState.
 * 
 * This function handles various types of statements in the Lua language, including:
 * - Empty statements (;)
 * - Conditional statements (if)
 * - Loops (while, do, for, repeat)
 * - Function declarations (function, local function)
 * - Local variable declarations (local)
 * - Labels (::label::)
 * - Return statements (return)
 * - Control flow statements (break, goto)
 * - Function calls and assignments (default case)
 * 
 * The function first records the current line number for potential error messages,
 * then enters a new lexical level using `enterlevel`. It processes the statement
 * based on the current token using a switch statement. After processing, it ensures
 * the stack size is valid, frees any temporary registers, and leaves the lexical level
 * using `leavelevel`.
 * 
 * @param ls Pointer to the LexState structure containing the lexical analyzer state.
 */
static void statement (LexState *ls) {
  int line = ls->linenumber;  /* may be needed for error messages */
  enterlevel(ls);
  switch (ls->t.token) {
    case ';': {  /* stat -> ';' (empty statement) */
      luaX_next(ls);  /* skip ';' */
      break;
    }
    case TK_IF: {  /* stat -> ifstat */
      ifstat(ls, line);
      break;
    }
    case TK_WHILE: {  /* stat -> whilestat */
      whilestat(ls, line);
      break;
    }
    case TK_DO: {  /* stat -> DO block END */
      luaX_next(ls);  /* skip DO */
      block(ls);
      check_match(ls, TK_END, TK_DO, line);
      break;
    }
    case TK_FOR: {  /* stat -> forstat */
      forstat(ls, line);
      break;
    }
    case TK_REPEAT: {  /* stat -> repeatstat */
      repeatstat(ls, line);
      break;
    }
    case TK_FUNCTION: {  /* stat -> funcstat */
      funcstat(ls, line);
      break;
    }
    case TK_LOCAL: {  /* stat -> localstat */
      luaX_next(ls);  /* skip LOCAL */
      if (testnext(ls, TK_FUNCTION))  /* local function? */
        localfunc(ls);
      else
        localstat(ls);
      break;
    }
    case TK_DBCOLON: {  /* stat -> label */
      luaX_next(ls);  /* skip double colon */
      labelstat(ls, str_checkname(ls), line);
      break;
    }
    case TK_RETURN: {  /* stat -> retstat */
      luaX_next(ls);  /* skip RETURN */
      retstat(ls);
      break;
    }
    case TK_BREAK:   /* stat -> breakstat */
    case TK_GOTO: {  /* stat -> 'goto' NAME */
      gotostat(ls, luaK_jump(ls->fs));
      break;
    }
    default: {  /* stat -> func | assignment */
      exprstat(ls);
      break;
    }
  }
  lua_assert(ls->fs->f->maxstacksize >= ls->fs->freereg &&
             ls->fs->freereg >= ls->fs->nactvar);
  ls->fs->freereg = ls->fs->nactvar;  /* free registers */
  leavelevel(ls);
}

/* }====================================================================== */


/*
** compiles the main function, which is a regular vararg function with an
** upvalue named LUA_ENV
*/
static void mainfunc (LexState *ls, FuncState *fs) {
  BlockCnt bl;
  expdesc v;
  open_func(ls, fs, &bl);
  fs->f->is_vararg = 1;  /* main function is always declared vararg */
  init_exp(&v, VLOCAL, 0);  /* create and... */
  newupvalue(fs, ls->envn, &v);  /* ...set environment upvalue */
  luaX_next(ls);  /* read first token */
  statlist(ls);  /* parse main body */
  check(ls, TK_EOS);
  close_func(ls);
}


/**
 * Parses Lua source code and generates a closure representing the main function.
 *
 * This function initializes the lexical and functional state required for parsing,
 * creates a new closure and prototype for the main function, and sets up the scanner
 * with the provided input. It then invokes the main parsing function to process the
 * source code and generate the corresponding bytecode. After parsing, it ensures
 * that all scopes are correctly finished and cleans up temporary data structures.
 *
 * @param L The Lua state.
 * @param z The ZIO input stream providing the source code.
 * @param buff The buffer used by the scanner.
 * @param dyd The dynamic data structure for tracking variables, goto statements, and labels.
 * @param name The name of the source (e.g., filename or identifier).
 * @param firstchar The first character of the input stream.
 * @return A pointer to the newly created closure representing the main function.
 */
LClosure *luaY_parser (lua_State *L, ZIO *z, Mbuffer *buff,
                       Dyndata *dyd, const char *name, int firstchar) {
  LexState lexstate;
  FuncState funcstate;
  LClosure *cl = luaF_newLclosure(L, 1);  /* create main closure */
  setclLvalue(L, L->top, cl);  /* anchor it (to avoid being collected) */
  luaD_inctop(L);
  lexstate.h = luaH_new(L);  /* create table for scanner */
  sethvalue(L, L->top, lexstate.h);  /* anchor it */
  luaD_inctop(L);
  funcstate.f = cl->p = luaF_newproto(L);
  funcstate.f->source = luaS_new(L, name);  /* create and anchor TString */
  lua_assert(iswhite(funcstate.f));  /* do not need barrier here */
  lexstate.buff = buff;
  lexstate.dyd = dyd;
  dyd->actvar.n = dyd->gt.n = dyd->label.n = 0;
  luaX_setinput(L, &lexstate, z, funcstate.f->source, firstchar);
  mainfunc(&lexstate, &funcstate);
  lua_assert(!funcstate.prev && funcstate.nups == 1 && !lexstate.fs);
  /* all scopes should be correctly finished */
  lua_assert(dyd->actvar.n == 0 && dyd->gt.n == 0 && dyd->label.n == 0);
  L->top--;  /* remove scanner's table */
  return cl;  /* closure is on the stack, too */
}

