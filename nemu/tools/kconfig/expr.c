// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2002 Roman Zippel <zippel@linux-m68k.org>
 */

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lkc.h"

#define DEBUG_EXPR	0

static struct expr *expr_eliminate_yn(struct expr *e);

/**
 * Allocates and initializes a new expression of type E_SYMBOL.
 *
 * This function creates a new expression structure and sets its type to E_SYMBOL.
 * The left symbol field of the expression is initialized with the provided symbol
 * pointer. The memory for the expression is allocated using xcalloc, ensuring that
 * the structure is zero-initialized.
 *
 * @param sym Pointer to the symbol to be associated with the expression.
 * @return A pointer to the newly allocated and initialized expression structure.
 */
struct expr *expr_alloc_symbol(struct symbol *sym)
{
	struct expr *e = xcalloc(1, sizeof(*e));
	e->type = E_SYMBOL;
	e->left.sym = sym;
	return e;
}

/**
 * Allocates and initializes a new expression node with a single child expression.
 *
 * This function creates a new expression node of the specified type and assigns
 * the given child expression to its left child. The newly created node is
 * initialized with zeroed memory using `xcalloc`.
 *
 * @param type The type of the expression node to be created.
 * @param ce The child expression to be assigned to the left child of the new node.
 *
 * @return A pointer to the newly allocated and initialized expression node.
 */
struct expr *expr_alloc_one(enum expr_type type, struct expr *ce)
{
	struct expr *e = xcalloc(1, sizeof(*e));
	e->type = type;
	e->left.expr = ce;
	return e;
}

/**
 * Allocates and initializes a new expression node with two child expressions.
 *
 * This function creates a new expression node of the specified type and assigns
 * the provided expressions `e1` and `e2` as its left and right children, respectively.
 * The node is allocated using `xcalloc` to ensure it is zero-initialized.
 *
 * @param type The type of the expression node to create.
 * @param e1   The left child expression. Can be NULL if not applicable.
 * @param e2   The right child expression. Can be NULL if not applicable.
 *
 * @return A pointer to the newly allocated and initialized expression node.
 */
struct expr *expr_alloc_two(enum expr_type type, struct expr *e1, struct expr *e2)
{
	struct expr *e = xcalloc(1, sizeof(*e));
	e->type = type;
	e->left.expr = e1;
	e->right.expr = e2;
	return e;
}

/**
 * Allocates and initializes a new comparison expression node.
 *
 * This function creates a new expression node of the specified type and
 * associates it with the given symbols. The node is allocated using xcalloc
 * to ensure it is zero-initialized. The type of the expression is set to the
 * provided `type`, and the left and right symbols are set to `s1` and `s2`,
 * respectively.
 *
 * @param type The type of the comparison expression (e.g., EXPR_EQUAL, EXPR_LESS).
 * @param s1 The symbol to be used as the left operand of the comparison.
 * @param s2 The symbol to be used as the right operand of the comparison.
 * @return A pointer to the newly allocated and initialized expression node.
 */
struct expr *expr_alloc_comp(enum expr_type type, struct symbol *s1, struct symbol *s2)
{
	struct expr *e = xcalloc(1, sizeof(*e));
	e->type = type;
	e->left.sym = s1;
	e->right.sym = s2;
	return e;
}

/**
 * Allocates and returns a new expression representing the logical AND of two expressions.
 * If either of the input expressions is NULL, the other expression is returned directly.
 * If both expressions are non-NULL, a new expression of type E_AND is allocated, 
 * combining the two input expressions as its operands.
 *
 * @param e1 The first expression to be combined with AND. Can be NULL.
 * @param e2 The second expression to be combined with AND. Can be NULL.
 * @return A pointer to the resulting expression. If both inputs are NULL, returns NULL.
 */
struct expr *expr_alloc_and(struct expr *e1, struct expr *e2)
{
	if (!e1)
		return e2;
	return e2 ? expr_alloc_two(E_AND, e1, e2) : e1;
}

/**
 * Allocates and returns a new logical OR expression node.
 *
 * This function constructs a logical OR expression node by combining two existing
 * expression nodes, `e1` and `e2`. If `e1` is NULL, the function returns `e2` directly.
 * If `e2` is NULL, the function returns `e1` directly. If both `e1` and `e2` are non-NULL,
 * the function creates a new expression node of type `E_OR` with `e1` and `e2` as its
 * left and right operands, respectively.
 *
 * @param e1 The first expression node (left operand).
 * @param e2 The second expression node (right operand).
 * @return A pointer to the resulting expression node. If either `e1` or `e2` is NULL,
 *         the non-NULL operand is returned. If both are NULL, the behavior is undefined.
 */
struct expr *expr_alloc_or(struct expr *e1, struct expr *e2)
{
	if (!e1)
		return e2;
	return e2 ? expr_alloc_two(E_OR, e1, e2) : e1;
}

/**
 * Creates a deep copy of the given expression structure.
 *
 * This function allocates memory for a new expression structure and copies the
 * contents of the original expression into it. Depending on the type of the
 * expression, it recursively copies the left and right sub-expressions or
 * directly copies the symbol references. If the original expression is NULL,
 * the function returns NULL. If the expression type is unsupported, an error
 * message is printed, and NULL is returned.
 *
 * @param org Pointer to the original expression structure to be copied.
 * @return Pointer to the newly allocated and copied expression structure, or
 *         NULL if the original expression is NULL or if the type is unsupported.
 */
struct expr *expr_copy(const struct expr *org)
{
	struct expr *e;

	if (!org)
		return NULL;

	e = xmalloc(sizeof(*org));
	memcpy(e, org, sizeof(*org));
	switch (org->type) {
	case E_SYMBOL:
		e->left = org->left;
		break;
	case E_NOT:
		e->left.expr = expr_copy(org->left.expr);
		break;
	case E_EQUAL:
	case E_GEQ:
	case E_GTH:
	case E_LEQ:
	case E_LTH:
	case E_UNEQUAL:
		e->left.sym = org->left.sym;
		e->right.sym = org->right.sym;
		break;
	case E_AND:
	case E_OR:
	case E_LIST:
		e->left.expr = expr_copy(org->left.expr);
		e->right.expr = expr_copy(org->right.expr);
		break;
	default:
		fprintf(stderr, "can't copy type %d\n", e->type);
		free(e);
		e = NULL;
		break;
	}

	return e;
}

/**
 * Frees the memory allocated for an expression tree.
 *
 * This function recursively traverses the expression tree rooted at `e` and
 * frees all associated memory. The behavior depends on the type of the expression:
 * - For `E_SYMBOL`, no additional memory needs to be freed.
 * - For `E_NOT`, the left sub-expression is recursively freed.
 * - For `E_EQUAL`, `E_GEQ`, `E_GTH`, `E_LEQ`, `E_LTH`, and `E_UNEQUAL`, no additional
 *   memory needs to be freed.
 * - For `E_OR` and `E_AND`, both the left and right sub-expressions are recursively freed.
 * - For any unknown type, an error message is printed to `stderr`.
 *
 * After handling the sub-expressions, the memory allocated for the current expression
 * node `e` is freed.
 *
 * @param e Pointer to the root of the expression tree to be freed. If `e` is `NULL`,
 *          the function returns immediately.
 */
void expr_free(struct expr *e)
{
	if (!e)
		return;

	switch (e->type) {
	case E_SYMBOL:
		break;
	case E_NOT:
		expr_free(e->left.expr);
		break;
	case E_EQUAL:
	case E_GEQ:
	case E_GTH:
	case E_LEQ:
	case E_LTH:
	case E_UNEQUAL:
		break;
	case E_OR:
	case E_AND:
		expr_free(e->left.expr);
		expr_free(e->right.expr);
		break;
	default:
		fprintf(stderr, "how to free type %d?\n", e->type);
		break;
	}
	free(e);
}

static int trans_count;

#define e1 (*ep1)
#define e2 (*ep2)

/*
 * expr_eliminate_eq() helper.
 *
 * Walks the two expression trees given in 'ep1' and 'ep2'. Any node that does
 * not have type 'type' (E_OR/E_AND) is considered a leaf, and is compared
 * against all other leaves. Two equal leaves are both replaced with either 'y'
 * or 'n' as appropriate for 'type', to be eliminated later.
 */
static void __expr_eliminate_eq(enum expr_type type, struct expr **ep1, struct expr **ep2)
{
	/* Recurse down to leaves */

	if (e1->type == type) {
		__expr_eliminate_eq(type, &e1->left.expr, &e2);
		__expr_eliminate_eq(type, &e1->right.expr, &e2);
		return;
	}
	if (e2->type == type) {
		__expr_eliminate_eq(type, &e1, &e2->left.expr);
		__expr_eliminate_eq(type, &e1, &e2->right.expr);
		return;
	}

	/* e1 and e2 are leaves. Compare them. */

	if (e1->type == E_SYMBOL && e2->type == E_SYMBOL &&
	    e1->left.sym == e2->left.sym &&
	    (e1->left.sym == &symbol_yes || e1->left.sym == &symbol_no))
		return;
	if (!expr_eq(e1, e2))
		return;

	/* e1 and e2 are equal leaves. Prepare them for elimination. */

	trans_count++;
	expr_free(e1); expr_free(e2);
	switch (type) {
	case E_OR:
		e1 = expr_alloc_symbol(&symbol_no);
		e2 = expr_alloc_symbol(&symbol_no);
		break;
	case E_AND:
		e1 = expr_alloc_symbol(&symbol_yes);
		e2 = expr_alloc_symbol(&symbol_yes);
		break;
	default:
		;
	}
}

/*
 * Rewrites the expressions 'ep1' and 'ep2' to remove operands common to both.
 * Example reductions:
 *
 *	ep1: A && B           ->  ep1: y
 *	ep2: A && B && C      ->  ep2: C
 *
 *	ep1: A || B           ->  ep1: n
 *	ep2: A || B || C      ->  ep2: C
 *
 *	ep1: A && (B && FOO)  ->  ep1: FOO
 *	ep2: (BAR && B) && A  ->  ep2: BAR
 *
 *	ep1: A && (B || C)    ->  ep1: y
 *	ep2: (C || B) && A    ->  ep2: y
 *
 * Comparisons are done between all operands at the same "level" of && or ||.
 * For example, in the expression 'e1 && (e2 || e3) && (e4 || e5)', the
 * following operands will be compared:
 *
 *	- 'e1', 'e2 || e3', and 'e4 || e5', against each other
 *	- e2 against e3
 *	- e4 against e5
 *
 * Parentheses are irrelevant within a single level. 'e1 && (e2 && e3)' and
 * '(e1 && e2) && e3' are both a single level.
 *
 * See __expr_eliminate_eq() as well.
 */
void expr_eliminate_eq(struct expr **ep1, struct expr **ep2)
{
	if (!e1 || !e2)
		return;
	switch (e1->type) {
	case E_OR:
	case E_AND:
		__expr_eliminate_eq(e1->type, ep1, ep2);
	default:
		;
	}
	if (e1->type != e2->type) switch (e2->type) {
	case E_OR:
	case E_AND:
		__expr_eliminate_eq(e2->type, ep1, ep2);
	default:
		;
	}
	e1 = expr_eliminate_yn(e1);
	e2 = expr_eliminate_yn(e2);
}

#undef e1
#undef e2

/*
 * Returns true if 'e1' and 'e2' are equal, after minor simplification. Two
 * &&/|| expressions are considered equal if every operand in one expression
 * equals some operand in the other (operands do not need to appear in the same
 * order), recursively.
 */
int expr_eq(struct expr *e1, struct expr *e2)
{
	int res, old_count;

	/*
	 * A NULL expr is taken to be yes, but there's also a different way to
	 * represent yes. expr_is_yes() checks for either representation.
	 */
	if (!e1 || !e2)
		return expr_is_yes(e1) && expr_is_yes(e2);

	if (e1->type != e2->type)
		return 0;
	switch (e1->type) {
	case E_EQUAL:
	case E_GEQ:
	case E_GTH:
	case E_LEQ:
	case E_LTH:
	case E_UNEQUAL:
		return e1->left.sym == e2->left.sym && e1->right.sym == e2->right.sym;
	case E_SYMBOL:
		return e1->left.sym == e2->left.sym;
	case E_NOT:
		return expr_eq(e1->left.expr, e2->left.expr);
	case E_AND:
	case E_OR:
		e1 = expr_copy(e1);
		e2 = expr_copy(e2);
		old_count = trans_count;
		expr_eliminate_eq(&e1, &e2);
		res = (e1->type == E_SYMBOL && e2->type == E_SYMBOL &&
		       e1->left.sym == e2->left.sym);
		expr_free(e1);
		expr_free(e2);
		trans_count = old_count;
		return res;
	case E_LIST:
	case E_RANGE:
	case E_NONE:
		/* panic */;
	}

	if (DEBUG_EXPR) {
		expr_fprint(e1, stdout);
		printf(" = ");
		expr_fprint(e2, stdout);
		printf(" ?\n");
	}

	return 0;
}

/*
 * Recursively performs the following simplifications in-place (as well as the
 * corresponding simplifications with swapped operands):
 *
 *	expr && n  ->  n
 *	expr && y  ->  expr
 *	expr || n  ->  expr
 *	expr || y  ->  y
 *
 * Returns the optimized expression.
 */
static struct expr *expr_eliminate_yn(struct expr *e)
{
	struct expr *tmp;

	if (e) switch (e->type) {
	case E_AND:
		e->left.expr = expr_eliminate_yn(e->left.expr);
		e->right.expr = expr_eliminate_yn(e->right.expr);
		if (e->left.expr->type == E_SYMBOL) {
			if (e->left.expr->left.sym == &symbol_no) {
				expr_free(e->left.expr);
				expr_free(e->right.expr);
				e->type = E_SYMBOL;
				e->left.sym = &symbol_no;
				e->right.expr = NULL;
				return e;
			} else if (e->left.expr->left.sym == &symbol_yes) {
				free(e->left.expr);
				tmp = e->right.expr;
				*e = *(e->right.expr);
				free(tmp);
				return e;
			}
		}
		if (e->right.expr->type == E_SYMBOL) {
			if (e->right.expr->left.sym == &symbol_no) {
				expr_free(e->left.expr);
				expr_free(e->right.expr);
				e->type = E_SYMBOL;
				e->left.sym = &symbol_no;
				e->right.expr = NULL;
				return e;
			} else if (e->right.expr->left.sym == &symbol_yes) {
				free(e->right.expr);
				tmp = e->left.expr;
				*e = *(e->left.expr);
				free(tmp);
				return e;
			}
		}
		break;
	case E_OR:
		e->left.expr = expr_eliminate_yn(e->left.expr);
		e->right.expr = expr_eliminate_yn(e->right.expr);
		if (e->left.expr->type == E_SYMBOL) {
			if (e->left.expr->left.sym == &symbol_no) {
				free(e->left.expr);
				tmp = e->right.expr;
				*e = *(e->right.expr);
				free(tmp);
				return e;
			} else if (e->left.expr->left.sym == &symbol_yes) {
				expr_free(e->left.expr);
				expr_free(e->right.expr);
				e->type = E_SYMBOL;
				e->left.sym = &symbol_yes;
				e->right.expr = NULL;
				return e;
			}
		}
		if (e->right.expr->type == E_SYMBOL) {
			if (e->right.expr->left.sym == &symbol_no) {
				free(e->right.expr);
				tmp = e->left.expr;
				*e = *(e->left.expr);
				free(tmp);
				return e;
			} else if (e->right.expr->left.sym == &symbol_yes) {
				expr_free(e->left.expr);
				expr_free(e->right.expr);
				e->type = E_SYMBOL;
				e->left.sym = &symbol_yes;
				e->right.expr = NULL;
				return e;
			}
		}
		break;
	default:
		;
	}
	return e;
}

/*
 * bool FOO!=n => FOO
 */
struct expr *expr_trans_bool(struct expr *e)
{
	if (!e)
		return NULL;
	switch (e->type) {
	case E_AND:
	case E_OR:
	case E_NOT:
		e->left.expr = expr_trans_bool(e->left.expr);
		e->right.expr = expr_trans_bool(e->right.expr);
		break;
	case E_UNEQUAL:
		// FOO!=n -> FOO
		if (e->left.sym->type == S_TRISTATE) {
			if (e->right.sym == &symbol_no) {
				e->type = E_SYMBOL;
				e->right.sym = NULL;
			}
		}
		break;
	default:
		;
	}
	return e;
}

/*
 * e1 || e2 -> ?
 */
static struct expr *expr_join_or(struct expr *e1, struct expr *e2)
{
	struct expr *tmp;
	struct symbol *sym1, *sym2;

	if (expr_eq(e1, e2))
		return expr_copy(e1);
	if (e1->type != E_EQUAL && e1->type != E_UNEQUAL && e1->type != E_SYMBOL && e1->type != E_NOT)
		return NULL;
	if (e2->type != E_EQUAL && e2->type != E_UNEQUAL && e2->type != E_SYMBOL && e2->type != E_NOT)
		return NULL;
	if (e1->type == E_NOT) {
		tmp = e1->left.expr;
		if (tmp->type != E_EQUAL && tmp->type != E_UNEQUAL && tmp->type != E_SYMBOL)
			return NULL;
		sym1 = tmp->left.sym;
	} else
		sym1 = e1->left.sym;
	if (e2->type == E_NOT) {
		if (e2->left.expr->type != E_SYMBOL)
			return NULL;
		sym2 = e2->left.expr->left.sym;
	} else
		sym2 = e2->left.sym;
	if (sym1 != sym2)
		return NULL;
	if (sym1->type != S_BOOLEAN && sym1->type != S_TRISTATE)
		return NULL;
	if (sym1->type == S_TRISTATE) {
		if (e1->type == E_EQUAL && e2->type == E_EQUAL &&
		    ((e1->right.sym == &symbol_yes && e2->right.sym == &symbol_mod) ||
		     (e1->right.sym == &symbol_mod && e2->right.sym == &symbol_yes))) {
			// (a='y') || (a='m') -> (a!='n')
			return expr_alloc_comp(E_UNEQUAL, sym1, &symbol_no);
		}
		if (e1->type == E_EQUAL && e2->type == E_EQUAL &&
		    ((e1->right.sym == &symbol_yes && e2->right.sym == &symbol_no) ||
		     (e1->right.sym == &symbol_no && e2->right.sym == &symbol_yes))) {
			// (a='y') || (a='n') -> (a!='m')
			return expr_alloc_comp(E_UNEQUAL, sym1, &symbol_mod);
		}
		if (e1->type == E_EQUAL && e2->type == E_EQUAL &&
		    ((e1->right.sym == &symbol_mod && e2->right.sym == &symbol_no) ||
		     (e1->right.sym == &symbol_no && e2->right.sym == &symbol_mod))) {
			// (a='m') || (a='n') -> (a!='y')
			return expr_alloc_comp(E_UNEQUAL, sym1, &symbol_yes);
		}
	}
	if (sym1->type == S_BOOLEAN && sym1 == sym2) {
		if ((e1->type == E_NOT && e1->left.expr->type == E_SYMBOL && e2->type == E_SYMBOL) ||
		    (e2->type == E_NOT && e2->left.expr->type == E_SYMBOL && e1->type == E_SYMBOL))
			return expr_alloc_symbol(&symbol_yes);
	}

	if (DEBUG_EXPR) {
		printf("optimize (");
		expr_fprint(e1, stdout);
		printf(") || (");
		expr_fprint(e2, stdout);
		printf(")?\n");
	}
	return NULL;
}

/**
 * @brief Joins two expressions using a logical AND operation.
 *
 * This function combines two expressions `e1` and `e2` into a single expression
 * representing their logical AND. The function handles various types of expressions,
 * including equality (`E_EQUAL`), inequality (`E_UNEQUAL`), symbols (`E_SYMBOL`),
 * and negations (`E_NOT`). It performs optimizations based on the types and values
 * of the expressions to simplify the resulting expression.
 *
 * The function returns a new expression representing the logical AND of `e1` and `e2`.
 * If the expressions are identical, it returns a copy of one of them. If the expressions
 * are not compatible for logical AND (e.g., they refer to different symbols or have
 * unsupported types), the function returns `NULL`.
 *
 * @param e1 The first expression to join.
 * @param e2 The second expression to join.
 * @return A new expression representing the logical AND of `e1` and `e2`, or `NULL`
 *         if the expressions cannot be joined.
 */
static struct expr *expr_join_and(struct expr *e1, struct expr *e2)
{
	struct expr *tmp;
	struct symbol *sym1, *sym2;

	if (expr_eq(e1, e2))
		return expr_copy(e1);
	if (e1->type != E_EQUAL && e1->type != E_UNEQUAL && e1->type != E_SYMBOL && e1->type != E_NOT)
		return NULL;
	if (e2->type != E_EQUAL && e2->type != E_UNEQUAL && e2->type != E_SYMBOL && e2->type != E_NOT)
		return NULL;
	if (e1->type == E_NOT) {
		tmp = e1->left.expr;
		if (tmp->type != E_EQUAL && tmp->type != E_UNEQUAL && tmp->type != E_SYMBOL)
			return NULL;
		sym1 = tmp->left.sym;
	} else
		sym1 = e1->left.sym;
	if (e2->type == E_NOT) {
		if (e2->left.expr->type != E_SYMBOL)
			return NULL;
		sym2 = e2->left.expr->left.sym;
	} else
		sym2 = e2->left.sym;
	if (sym1 != sym2)
		return NULL;
	if (sym1->type != S_BOOLEAN && sym1->type != S_TRISTATE)
		return NULL;

	if ((e1->type == E_SYMBOL && e2->type == E_EQUAL && e2->right.sym == &symbol_yes) ||
	    (e2->type == E_SYMBOL && e1->type == E_EQUAL && e1->right.sym == &symbol_yes))
		// (a) && (a='y') -> (a='y')
		return expr_alloc_comp(E_EQUAL, sym1, &symbol_yes);

	if ((e1->type == E_SYMBOL && e2->type == E_UNEQUAL && e2->right.sym == &symbol_no) ||
	    (e2->type == E_SYMBOL && e1->type == E_UNEQUAL && e1->right.sym == &symbol_no))
		// (a) && (a!='n') -> (a)
		return expr_alloc_symbol(sym1);

	if ((e1->type == E_SYMBOL && e2->type == E_UNEQUAL && e2->right.sym == &symbol_mod) ||
	    (e2->type == E_SYMBOL && e1->type == E_UNEQUAL && e1->right.sym == &symbol_mod))
		// (a) && (a!='m') -> (a='y')
		return expr_alloc_comp(E_EQUAL, sym1, &symbol_yes);

	if (sym1->type == S_TRISTATE) {
		if (e1->type == E_EQUAL && e2->type == E_UNEQUAL) {
			// (a='b') && (a!='c') -> 'b'='c' ? 'n' : a='b'
			sym2 = e1->right.sym;
			if ((e2->right.sym->flags & SYMBOL_CONST) && (sym2->flags & SYMBOL_CONST))
				return sym2 != e2->right.sym ? expr_alloc_comp(E_EQUAL, sym1, sym2)
							     : expr_alloc_symbol(&symbol_no);
		}
		if (e1->type == E_UNEQUAL && e2->type == E_EQUAL) {
			// (a='b') && (a!='c') -> 'b'='c' ? 'n' : a='b'
			sym2 = e2->right.sym;
			if ((e1->right.sym->flags & SYMBOL_CONST) && (sym2->flags & SYMBOL_CONST))
				return sym2 != e1->right.sym ? expr_alloc_comp(E_EQUAL, sym1, sym2)
							     : expr_alloc_symbol(&symbol_no);
		}
		if (e1->type == E_UNEQUAL && e2->type == E_UNEQUAL &&
			   ((e1->right.sym == &symbol_yes && e2->right.sym == &symbol_no) ||
			    (e1->right.sym == &symbol_no && e2->right.sym == &symbol_yes)))
			// (a!='y') && (a!='n') -> (a='m')
			return expr_alloc_comp(E_EQUAL, sym1, &symbol_mod);

		if (e1->type == E_UNEQUAL && e2->type == E_UNEQUAL &&
			   ((e1->right.sym == &symbol_yes && e2->right.sym == &symbol_mod) ||
			    (e1->right.sym == &symbol_mod && e2->right.sym == &symbol_yes)))
			// (a!='y') && (a!='m') -> (a='n')
			return expr_alloc_comp(E_EQUAL, sym1, &symbol_no);

		if (e1->type == E_UNEQUAL && e2->type == E_UNEQUAL &&
			   ((e1->right.sym == &symbol_mod && e2->right.sym == &symbol_no) ||
			    (e1->right.sym == &symbol_no && e2->right.sym == &symbol_mod)))
			// (a!='m') && (a!='n') -> (a='m')
			return expr_alloc_comp(E_EQUAL, sym1, &symbol_yes);

		if ((e1->type == E_SYMBOL && e2->type == E_EQUAL && e2->right.sym == &symbol_mod) ||
		    (e2->type == E_SYMBOL && e1->type == E_EQUAL && e1->right.sym == &symbol_mod) ||
		    (e1->type == E_SYMBOL && e2->type == E_UNEQUAL && e2->right.sym == &symbol_yes) ||
		    (e2->type == E_SYMBOL && e1->type == E_UNEQUAL && e1->right.sym == &symbol_yes))
			return NULL;
	}

	if (DEBUG_EXPR) {
		printf("optimize (");
		expr_fprint(e1, stdout);
		printf(") && (");
		expr_fprint(e2, stdout);
		printf(")?\n");
	}
	return NULL;
}

/*
 * expr_eliminate_dups() helper.
 *
 * Walks the two expression trees given in 'ep1' and 'ep2'. Any node that does
 * not have type 'type' (E_OR/E_AND) is considered a leaf, and is compared
 * against all other leaves to look for simplifications.
 */
static void expr_eliminate_dups1(enum expr_type type, struct expr **ep1, struct expr **ep2)
{
#define e1 (*ep1)
#define e2 (*ep2)
	struct expr *tmp;

	/* Recurse down to leaves */

	if (e1->type == type) {
		expr_eliminate_dups1(type, &e1->left.expr, &e2);
		expr_eliminate_dups1(type, &e1->right.expr, &e2);
		return;
	}
	if (e2->type == type) {
		expr_eliminate_dups1(type, &e1, &e2->left.expr);
		expr_eliminate_dups1(type, &e1, &e2->right.expr);
		return;
	}

	/* e1 and e2 are leaves. Compare and process them. */

	if (e1 == e2)
		return;

	switch (e1->type) {
	case E_OR: case E_AND:
		expr_eliminate_dups1(e1->type, &e1, &e1);
	default:
		;
	}

	switch (type) {
	case E_OR:
		tmp = expr_join_or(e1, e2);
		if (tmp) {
			expr_free(e1); expr_free(e2);
			e1 = expr_alloc_symbol(&symbol_no);
			e2 = tmp;
			trans_count++;
		}
		break;
	case E_AND:
		tmp = expr_join_and(e1, e2);
		if (tmp) {
			expr_free(e1); expr_free(e2);
			e1 = expr_alloc_symbol(&symbol_yes);
			e2 = tmp;
			trans_count++;
		}
		break;
	default:
		;
	}
#undef e1
#undef e2
}

/*
 * Rewrites 'e' in-place to remove ("join") duplicate and other redundant
 * operands.
 *
 * Example simplifications:
 *
 *	A || B || A    ->  A || B
 *	A && B && A=y  ->  A=y && B
 *
 * Returns the deduplicated expression.
 */
struct expr *expr_eliminate_dups(struct expr *e)
{
	int oldcount;
	if (!e)
		return e;

	oldcount = trans_count;
	while (1) {
		trans_count = 0;
		switch (e->type) {
		case E_OR: case E_AND:
			expr_eliminate_dups1(e->type, &e, &e);
		default:
			;
		}
		if (!trans_count)
			/* No simplifications done in this pass. We're done */
			break;
		e = expr_eliminate_yn(e);
	}
	trans_count = oldcount;
	return e;
}

/*
 * Performs various simplifications involving logical operators and
 * comparisons.
 *
 * Allocates and returns a new expression.
 */
struct expr *expr_transform(struct expr *e)
{
	struct expr *tmp;

	if (!e)
		return NULL;
	switch (e->type) {
	case E_EQUAL:
	case E_GEQ:
	case E_GTH:
	case E_LEQ:
	case E_LTH:
	case E_UNEQUAL:
	case E_SYMBOL:
	case E_LIST:
		break;
	default:
		e->left.expr = expr_transform(e->left.expr);
		e->right.expr = expr_transform(e->right.expr);
	}

	switch (e->type) {
	case E_EQUAL:
		if (e->left.sym->type != S_BOOLEAN)
			break;
		if (e->right.sym == &symbol_no) {
			e->type = E_NOT;
			e->left.expr = expr_alloc_symbol(e->left.sym);
			e->right.sym = NULL;
			break;
		}
		if (e->right.sym == &symbol_mod) {
			printf("boolean symbol %s tested for 'm'? test forced to 'n'\n", e->left.sym->name);
			e->type = E_SYMBOL;
			e->left.sym = &symbol_no;
			e->right.sym = NULL;
			break;
		}
		if (e->right.sym == &symbol_yes) {
			e->type = E_SYMBOL;
			e->right.sym = NULL;
			break;
		}
		break;
	case E_UNEQUAL:
		if (e->left.sym->type != S_BOOLEAN)
			break;
		if (e->right.sym == &symbol_no) {
			e->type = E_SYMBOL;
			e->right.sym = NULL;
			break;
		}
		if (e->right.sym == &symbol_mod) {
			printf("boolean symbol %s tested for 'm'? test forced to 'y'\n", e->left.sym->name);
			e->type = E_SYMBOL;
			e->left.sym = &symbol_yes;
			e->right.sym = NULL;
			break;
		}
		if (e->right.sym == &symbol_yes) {
			e->type = E_NOT;
			e->left.expr = expr_alloc_symbol(e->left.sym);
			e->right.sym = NULL;
			break;
		}
		break;
	case E_NOT:
		switch (e->left.expr->type) {
		case E_NOT:
			// !!a -> a
			tmp = e->left.expr->left.expr;
			free(e->left.expr);
			free(e);
			e = tmp;
			e = expr_transform(e);
			break;
		case E_EQUAL:
		case E_UNEQUAL:
			// !a='x' -> a!='x'
			tmp = e->left.expr;
			free(e);
			e = tmp;
			e->type = e->type == E_EQUAL ? E_UNEQUAL : E_EQUAL;
			break;
		case E_LEQ:
		case E_GEQ:
			// !a<='x' -> a>'x'
			tmp = e->left.expr;
			free(e);
			e = tmp;
			e->type = e->type == E_LEQ ? E_GTH : E_LTH;
			break;
		case E_LTH:
		case E_GTH:
			// !a<'x' -> a>='x'
			tmp = e->left.expr;
			free(e);
			e = tmp;
			e->type = e->type == E_LTH ? E_GEQ : E_LEQ;
			break;
		case E_OR:
			// !(a || b) -> !a && !b
			tmp = e->left.expr;
			e->type = E_AND;
			e->right.expr = expr_alloc_one(E_NOT, tmp->right.expr);
			tmp->type = E_NOT;
			tmp->right.expr = NULL;
			e = expr_transform(e);
			break;
		case E_AND:
			// !(a && b) -> !a || !b
			tmp = e->left.expr;
			e->type = E_OR;
			e->right.expr = expr_alloc_one(E_NOT, tmp->right.expr);
			tmp->type = E_NOT;
			tmp->right.expr = NULL;
			e = expr_transform(e);
			break;
		case E_SYMBOL:
			if (e->left.expr->left.sym == &symbol_yes) {
				// !'y' -> 'n'
				tmp = e->left.expr;
				free(e);
				e = tmp;
				e->type = E_SYMBOL;
				e->left.sym = &symbol_no;
				break;
			}
			if (e->left.expr->left.sym == &symbol_mod) {
				// !'m' -> 'm'
				tmp = e->left.expr;
				free(e);
				e = tmp;
				e->type = E_SYMBOL;
				e->left.sym = &symbol_mod;
				break;
			}
			if (e->left.expr->left.sym == &symbol_no) {
				// !'n' -> 'y'
				tmp = e->left.expr;
				free(e);
				e = tmp;
				e->type = E_SYMBOL;
				e->left.sym = &symbol_yes;
				break;
			}
			break;
		default:
			;
		}
		break;
	default:
		;
	}
	return e;
}

/**
 * Determines whether a given symbol is present in an expression tree.
 *
 * This function recursively traverses the expression tree rooted at `dep` and checks
 * if the symbol `sym` is present in any of the nodes. The expression tree can consist
 * of logical operators (AND, OR, NOT), relational operators (EQUAL, GEQ, GTH, LEQ, LTH, UNEQUAL),
 * or direct symbol references (E_SYMBOL).
 *
 * @param dep The root of the expression tree to search. If NULL, the function returns 0.
 * @param sym The symbol to search for in the expression tree.
 * @return 1 if the symbol `sym` is found in the expression tree, 0 otherwise.
 */
int expr_contains_symbol(struct expr *dep, struct symbol *sym)
{
	if (!dep)
		return 0;

	switch (dep->type) {
	case E_AND:
	case E_OR:
		return expr_contains_symbol(dep->left.expr, sym) ||
		       expr_contains_symbol(dep->right.expr, sym);
	case E_SYMBOL:
		return dep->left.sym == sym;
	case E_EQUAL:
	case E_GEQ:
	case E_GTH:
	case E_LEQ:
	case E_LTH:
	case E_UNEQUAL:
		return dep->left.sym == sym ||
		       dep->right.sym == sym;
	case E_NOT:
		return expr_contains_symbol(dep->left.expr, sym);
	default:
		;
	}
	return 0;
}

/**
 * Determines whether a given expression depends on a specific symbol.
 *
 * This function recursively checks if the provided expression `dep` depends on the symbol `sym`.
 * The function handles different types of expressions:
 * - E_AND: Checks if either the left or right sub-expression depends on `sym`.
 * - E_SYMBOL: Checks if the symbol in the expression matches `sym`.
 * - E_EQUAL: Checks if the left symbol matches `sym` and the right symbol is either `symbol_yes` or `symbol_mod`.
 * - E_UNEQUAL: Checks if the left symbol matches `sym` and the right symbol is `symbol_no`.
 *
 * @param dep The expression to be checked for dependency on `sym`.
 * @param sym The symbol to check for in the expression `dep`.
 * @return `true` if the expression depends on the symbol, `false` otherwise.
 */
bool expr_depends_symbol(struct expr *dep, struct symbol *sym)
{
	if (!dep)
		return false;

	switch (dep->type) {
	case E_AND:
		return expr_depends_symbol(dep->left.expr, sym) ||
		       expr_depends_symbol(dep->right.expr, sym);
	case E_SYMBOL:
		return dep->left.sym == sym;
	case E_EQUAL:
		if (dep->left.sym == sym) {
			if (dep->right.sym == &symbol_yes || dep->right.sym == &symbol_mod)
				return true;
		}
		break;
	case E_UNEQUAL:
		if (dep->left.sym == sym) {
			if (dep->right.sym == &symbol_no)
				return true;
		}
		break;
	default:
		;
	}
 	return false;
}

/*
 * Inserts explicit comparisons of type 'type' to symbol 'sym' into the
 * expression 'e'.
 *
 * Examples transformations for type == E_UNEQUAL, sym == &symbol_no:
 *
 *	A              ->  A!=n
 *	!A             ->  A=n
 *	A && B         ->  !(A=n || B=n)
 *	A || B         ->  !(A=n && B=n)
 *	A && (B || C)  ->  !(A=n || (B=n && C=n))
 *
 * Allocates and returns a new expression.
 */
struct expr *expr_trans_compare(struct expr *e, enum expr_type type, struct symbol *sym)
{
	struct expr *e1, *e2;

	if (!e) {
		e = expr_alloc_symbol(sym);
		if (type == E_UNEQUAL)
			e = expr_alloc_one(E_NOT, e);
		return e;
	}
	switch (e->type) {
	case E_AND:
		e1 = expr_trans_compare(e->left.expr, E_EQUAL, sym);
		e2 = expr_trans_compare(e->right.expr, E_EQUAL, sym);
		if (sym == &symbol_yes)
			e = expr_alloc_two(E_AND, e1, e2);
		if (sym == &symbol_no)
			e = expr_alloc_two(E_OR, e1, e2);
		if (type == E_UNEQUAL)
			e = expr_alloc_one(E_NOT, e);
		return e;
	case E_OR:
		e1 = expr_trans_compare(e->left.expr, E_EQUAL, sym);
		e2 = expr_trans_compare(e->right.expr, E_EQUAL, sym);
		if (sym == &symbol_yes)
			e = expr_alloc_two(E_OR, e1, e2);
		if (sym == &symbol_no)
			e = expr_alloc_two(E_AND, e1, e2);
		if (type == E_UNEQUAL)
			e = expr_alloc_one(E_NOT, e);
		return e;
	case E_NOT:
		return expr_trans_compare(e->left.expr, type == E_EQUAL ? E_UNEQUAL : E_EQUAL, sym);
	case E_UNEQUAL:
	case E_LTH:
	case E_LEQ:
	case E_GTH:
	case E_GEQ:
	case E_EQUAL:
		if (type == E_EQUAL) {
			if (sym == &symbol_yes)
				return expr_copy(e);
			if (sym == &symbol_mod)
				return expr_alloc_symbol(&symbol_no);
			if (sym == &symbol_no)
				return expr_alloc_one(E_NOT, expr_copy(e));
		} else {
			if (sym == &symbol_yes)
				return expr_alloc_one(E_NOT, expr_copy(e));
			if (sym == &symbol_mod)
				return expr_alloc_symbol(&symbol_yes);
			if (sym == &symbol_no)
				return expr_copy(e);
		}
		break;
	case E_SYMBOL:
		return expr_alloc_comp(type, e->left.sym, sym);
	case E_LIST:
	case E_RANGE:
	case E_NONE:
		/* panic */;
	}
	return NULL;
}

enum string_value_kind {
	k_string,
	k_signed,
	k_unsigned,
};

union string_value {
	unsigned long long u;
	signed long long s;
};

/**
 * @brief Parses a string into a value based on the specified symbol type.
 *
 * This function interprets the input string `str` according to the `type` parameter
 * and stores the parsed value in the `val` union. The function supports parsing
 * boolean/tristate values, integers, hexadecimal values, and generic strings.
 *
 * @param str The input string to be parsed.
 * @param type The symbol type that determines how the string should be parsed.
 *             Supported types include `S_BOOLEAN`, `S_TRISTATE`, `S_INT`, and `S_HEX`.
 * @param val A union where the parsed value will be stored. The union can hold
 *            signed, unsigned, or string values depending on the parsing result.
 *
 * @return Returns an `enum string_value_kind` indicating the type of the parsed value:
 *         - `k_signed` if the string was successfully parsed as a signed integer.
 *         - `k_unsigned` if the string was successfully parsed as an unsigned integer.
 *         - `k_string` if the string could not be parsed as a numeric value or if an error occurred.
 */
static enum string_value_kind expr_parse_string(const char *str,
						enum symbol_type type,
						union string_value *val)
{
	char *tail;
	enum string_value_kind kind;

	errno = 0;
	switch (type) {
	case S_BOOLEAN:
	case S_TRISTATE:
		val->s = !strcmp(str, "n") ? 0 :
			 !strcmp(str, "m") ? 1 :
			 !strcmp(str, "y") ? 2 : -1;
		return k_signed;
	case S_INT:
		val->s = strtoll(str, &tail, 10);
		kind = k_signed;
		break;
	case S_HEX:
		val->u = strtoull(str, &tail, 16);
		kind = k_unsigned;
		break;
	default:
		val->s = strtoll(str, &tail, 0);
		kind = k_signed;
		break;
	}
	return !errno && !*tail && tail > str && isxdigit(tail[-1])
	       ? kind : k_string;
}

/**
 * @brief Calculates the tristate value of a given expression.
 *
 * This function evaluates the value of an expression represented by the `struct expr` pointer `e`.
 * The expression can be of various types, including logical operations (AND, OR, NOT), relational
 * operations (EQUAL, GEQ, GTH, LEQ, LTH, UNEQUAL), and symbolic references. The function recursively
 * evaluates sub-expressions and applies the appropriate operation to determine the final tristate
 * value (yes, no, or a computed value based on the operation).
 *
 * @param e Pointer to the expression structure to be evaluated. If `e` is NULL, the function returns `yes`.
 *
 * @return The tristate value resulting from the evaluation of the expression. The possible return values are:
 *         - `yes`: The expression evaluates to true or a positive condition.
 *         - `no`: The expression evaluates to false or a negative condition.
 *         - A computed value based on the specific operation applied to the expression.
 */
tristate expr_calc_value(struct expr *e)
{
	tristate val1, val2;
	const char *str1, *str2;
	enum string_value_kind k1 = k_string, k2 = k_string;
	union string_value lval = {}, rval = {};
	int res;

	if (!e)
		return yes;

	switch (e->type) {
	case E_SYMBOL:
		sym_calc_value(e->left.sym);
		return e->left.sym->curr.tri;
	case E_AND:
		val1 = expr_calc_value(e->left.expr);
		val2 = expr_calc_value(e->right.expr);
		return EXPR_AND(val1, val2);
	case E_OR:
		val1 = expr_calc_value(e->left.expr);
		val2 = expr_calc_value(e->right.expr);
		return EXPR_OR(val1, val2);
	case E_NOT:
		val1 = expr_calc_value(e->left.expr);
		return EXPR_NOT(val1);
	case E_EQUAL:
	case E_GEQ:
	case E_GTH:
	case E_LEQ:
	case E_LTH:
	case E_UNEQUAL:
		break;
	default:
		printf("expr_calc_value: %d?\n", e->type);
		return no;
	}

	sym_calc_value(e->left.sym);
	sym_calc_value(e->right.sym);
	str1 = sym_get_string_value(e->left.sym);
	str2 = sym_get_string_value(e->right.sym);

	if (e->left.sym->type != S_STRING || e->right.sym->type != S_STRING) {
		k1 = expr_parse_string(str1, e->left.sym->type, &lval);
		k2 = expr_parse_string(str2, e->right.sym->type, &rval);
	}

	if (k1 == k_string || k2 == k_string)
		res = strcmp(str1, str2);
	else if (k1 == k_unsigned || k2 == k_unsigned)
		res = (lval.u > rval.u) - (lval.u < rval.u);
	else /* if (k1 == k_signed && k2 == k_signed) */
		res = (lval.s > rval.s) - (lval.s < rval.s);

	switch(e->type) {
	case E_EQUAL:
		return res ? no : yes;
	case E_GEQ:
		return res >= 0 ? yes : no;
	case E_GTH:
		return res > 0 ? yes : no;
	case E_LEQ:
		return res <= 0 ? yes : no;
	case E_LTH:
		return res < 0 ? yes : no;
	case E_UNEQUAL:
		return res ? yes : no;
	default:
		printf("expr_calc_value: relation %d?\n", e->type);
		return no;
	}
}

/**
 * Compares two expression types (`t1` and `t2`) and returns an integer indicating their relative precedence.
 * 
 * The comparison is based on a predefined hierarchy of expression types:
 * - If `t1` and `t2` are the same, the function returns `0`.
 * - If `t1` has a higher precedence than `t2`, the function returns `1`.
 * - If `t1` has a lower precedence than `t2`, the function returns `-1`.
 * 
 * The precedence order is as follows:
 * 1. E_LEQ, E_LTH, E_GEQ, E_GTH
 * 2. E_EQUAL, E_UNEQUAL
 * 3. E_NOT
 * 4. E_AND
 * 5. E_OR
 * 6. E_LIST
 * 7. 0 (used as a sentinel value)
 * 
 * @param t1 The first expression type to compare.
 * @param t2 The second expression type to compare.
 * @return `0` if `t1` and `t2` are equal, `1` if `t1` has higher precedence, or `-1` if `t1` has lower precedence.
 */
static int expr_compare_type(enum expr_type t1, enum expr_type t2)
{
	if (t1 == t2)
		return 0;
	switch (t1) {
	case E_LEQ:
	case E_LTH:
	case E_GEQ:
	case E_GTH:
		if (t2 == E_EQUAL || t2 == E_UNEQUAL)
			return 1;
	case E_EQUAL:
	case E_UNEQUAL:
		if (t2 == E_NOT)
			return 1;
	case E_NOT:
		if (t2 == E_AND)
			return 1;
	case E_AND:
		if (t2 == E_OR)
			return 1;
	case E_OR:
		if (t2 == E_LIST)
			return 1;
	case E_LIST:
		if (t2 == 0)
			return 1;
	default:
		return -1;
	}
	printf("[%dgt%d?]", t1, t2);
	return 0;
}

/**
 * Prints an expression tree in a human-readable format using a provided callback function.
 *
 * This function traverses the expression tree rooted at `e` and prints its contents
 * in a structured and readable format. The actual printing is delegated to the
 * callback function `fn`, which is invoked with the necessary data and symbols.
 *
 * @param e Pointer to the root of the expression tree to be printed. If `e` is NULL,
 *          the function will print a placeholder value ("y") and return immediately.
 * @param fn Callback function used to perform the actual printing. It takes three arguments:
 *           - `data`: A pointer to user-defined data passed through to the callback.
 *           - `sym`: A pointer to a symbol associated with the current node (may be NULL).
 *           - `str`: A string representation of the current node or operator.
 * @param data User-defined data passed to the callback function `fn`.
 * @param prevtoken The type of the previous token in the expression, used to determine
 *                  whether parentheses are needed around the current expression.
 *
 * The function handles various types of expressions, including symbols, logical operators
 * (NOT, OR, AND), comparison operators (EQUAL, LEQ, LTH, GEQ, GTH, UNEQUAL), lists, and ranges.
 * It ensures proper formatting by adding parentheses when necessary to preserve the
 * precedence of operations.
 */
void expr_print(struct expr *e,
		void (*fn)(void *, struct symbol *, const char *),
		void *data, int prevtoken)
{
	if (!e) {
		fn(data, NULL, "y");
		return;
	}

	if (expr_compare_type(prevtoken, e->type) > 0)
		fn(data, NULL, "(");
	switch (e->type) {
	case E_SYMBOL:
		if (e->left.sym->name)
			fn(data, e->left.sym, e->left.sym->name);
		else
			fn(data, NULL, "<choice>");
		break;
	case E_NOT:
		fn(data, NULL, "!");
		expr_print(e->left.expr, fn, data, E_NOT);
		break;
	case E_EQUAL:
		if (e->left.sym->name)
			fn(data, e->left.sym, e->left.sym->name);
		else
			fn(data, NULL, "<choice>");
		fn(data, NULL, "=");
		fn(data, e->right.sym, e->right.sym->name);
		break;
	case E_LEQ:
	case E_LTH:
		if (e->left.sym->name)
			fn(data, e->left.sym, e->left.sym->name);
		else
			fn(data, NULL, "<choice>");
		fn(data, NULL, e->type == E_LEQ ? "<=" : "<");
		fn(data, e->right.sym, e->right.sym->name);
		break;
	case E_GEQ:
	case E_GTH:
		if (e->left.sym->name)
			fn(data, e->left.sym, e->left.sym->name);
		else
			fn(data, NULL, "<choice>");
		fn(data, NULL, e->type == E_GEQ ? ">=" : ">");
		fn(data, e->right.sym, e->right.sym->name);
		break;
	case E_UNEQUAL:
		if (e->left.sym->name)
			fn(data, e->left.sym, e->left.sym->name);
		else
			fn(data, NULL, "<choice>");
		fn(data, NULL, "!=");
		fn(data, e->right.sym, e->right.sym->name);
		break;
	case E_OR:
		expr_print(e->left.expr, fn, data, E_OR);
		fn(data, NULL, " || ");
		expr_print(e->right.expr, fn, data, E_OR);
		break;
	case E_AND:
		expr_print(e->left.expr, fn, data, E_AND);
		fn(data, NULL, " && ");
		expr_print(e->right.expr, fn, data, E_AND);
		break;
	case E_LIST:
		fn(data, e->right.sym, e->right.sym->name);
		if (e->left.expr) {
			fn(data, NULL, " ^ ");
			expr_print(e->left.expr, fn, data, E_LIST);
		}
		break;
	case E_RANGE:
		fn(data, NULL, "[");
		fn(data, e->left.sym, e->left.sym->name);
		fn(data, NULL, " ");
		fn(data, e->right.sym, e->right.sym->name);
		fn(data, NULL, "]");
		break;
	default:
	  {
		char buf[32];
		sprintf(buf, "<unknown type %d>", e->type);
		fn(data, NULL, buf);
		break;
	  }
	}
	if (expr_compare_type(prevtoken, e->type) > 0)
		fn(data, NULL, ")");
}

/**
 * @brief Helper function to print a string to a file.
 *
 * This function writes the given string `str` to the file represented by `data`.
 * It uses the `xfwrite` function to perform the write operation. The function is
 * typically used in conjunction with symbol-related operations where `sym` may
 * represent a symbol in the context of the operation, though it is not directly
 * used in this function.
 *
 * @param data Pointer to the file where the string will be written. This is
 *             typically a file handle or FILE* cast to void*.
 * @param sym  Pointer to a symbol structure. This parameter is not used in the
 *             function but may be relevant in the broader context of its usage.
 * @param str  The string to be written to the file. The string is assumed to be
 *             null-terminated.
 */
static void expr_print_file_helper(void *data, struct symbol *sym, const char *str)
{
	xfwrite(str, strlen(str), 1, data);
}

/**
 * Prints the given expression to the specified output file.
 *
 * This function formats and outputs the expression `e` to the file `out` using
 * the `expr_print` function. The printing is customized by the helper function
 * `expr_print_file_helper`, which handles the actual writing to the file. The
 * `E_NONE` flag indicates that no special formatting options are applied.
 *
 * @param e    Pointer to the expression to be printed.
 * @param out  Pointer to the FILE stream where the expression will be printed.
 */
void expr_fprint(struct expr *e, FILE *out)
{
	expr_print(e, expr_print_file_helper, out, E_NONE);
}

/**
 * expr_print_gstr_helper - Appends a string to a gstr buffer, optionally including
 *                          a symbol's string value, while ensuring the output does
 *                          not exceed the maximum width constraint.
 *
 * @data: A pointer to the gstr buffer where the output will be appended.
 * @sym:  A pointer to a symbol structure. If non-NULL, the symbol's string value
 *        will be appended to the output in the format "[=<symbol_value>]".
 * @str:  The string to append to the gstr buffer.
 *
 * This function appends the provided string to the gstr buffer. If a symbol is
 * provided and its type is not S_UNKNOWN, the symbol's string value is appended
 * in a formatted manner. Additionally, if the gstr buffer has a maximum width
 * constraint (gs->max_width), the function ensures that the output does not
 * exceed this width by inserting a line continuation ("\\\n") when necessary.
 *
 * The function calculates the length of the current line in the buffer and checks
 * if appending the new string (and optionally the symbol's value) would exceed
 * the maximum width. If so, it inserts a line continuation before appending the
 * new content.
 */
static void expr_print_gstr_helper(void *data, struct symbol *sym, const char *str)
{
	struct gstr *gs = (struct gstr*)data;
	const char *sym_str = NULL;

	if (sym)
		sym_str = sym_get_string_value(sym);

	if (gs->max_width) {
		unsigned extra_length = strlen(str);
		const char *last_cr = strrchr(gs->s, '\n');
		unsigned last_line_length;

		if (sym_str)
			extra_length += 4 + strlen(sym_str);

		if (!last_cr)
			last_cr = gs->s;

		last_line_length = strlen(gs->s) - (last_cr - gs->s);

		if ((last_line_length + extra_length) > gs->max_width)
			str_append(gs, "\\\n");
	}

	str_append(gs, str);
	if (sym && sym->type != S_UNKNOWN)
		str_printf(gs, " [=%s]", sym_str);
}

/**
 * Prints the expression tree to a string buffer using a helper function.
 *
 * This function traverses the expression tree rooted at `e` and prints its
 * structure to the string buffer `gs`. The printing is performed using the
 * `expr_print_gstr_helper` function, which formats the expression nodes
 * appropriately. The `E_NONE` flag indicates that no specific printing mode
 * is enabled, so the default behavior is used.
 *
 * @param e Pointer to the root of the expression tree to be printed.
 * @param gs Pointer to the string buffer where the output will be stored.
 */
void expr_gstr_print(struct expr *e, struct gstr *gs)
{
	expr_print(e, expr_print_gstr_helper, gs, E_NONE);
}

/*
 * Transform the top level "||" tokens into newlines and prepend each
 * line with a minus. This makes expressions much easier to read.
 * Suitable for reverse dependency expressions.
 */
static void expr_print_revdep(struct expr *e,
			      void (*fn)(void *, struct symbol *, const char *),
			      void *data, tristate pr_type, const char **title)
{
	if (e->type == E_OR) {
		expr_print_revdep(e->left.expr, fn, data, pr_type, title);
		expr_print_revdep(e->right.expr, fn, data, pr_type, title);
	} else if (expr_calc_value(e) == pr_type) {
		if (*title) {
			fn(data, NULL, *title);
			*title = NULL;
		}

		fn(data, NULL, "  - ");
		expr_print(e, fn, data, E_NONE);
		fn(data, NULL, "\n");
	}
}

/**
 * @brief Prints the reverse dependencies of a given expression in a formatted string.
 *
 * This function prints the reverse dependencies of the expression `e` into the string
 * buffer `gs`. The output is formatted based on the provided `pr_type` and includes
 * a title specified by `title`. The function internally uses `expr_print_revdep` to
 * handle the actual printing, with `expr_print_gstr_helper` as the helper function
 * to append the output to the string buffer.
 *
 * @param e The expression whose reverse dependencies are to be printed.
 * @param gs The string buffer where the formatted output will be stored.
 * @param pr_type The type of printing to be performed (e.g., `yes`, `no`, `mod`).
 * @param title The title to be included in the output.
 */
void expr_gstr_print_revdep(struct expr *e, struct gstr *gs,
			    tristate pr_type, const char *title)
{
	expr_print_revdep(e, expr_print_gstr_helper, gs, pr_type, &title);
}
