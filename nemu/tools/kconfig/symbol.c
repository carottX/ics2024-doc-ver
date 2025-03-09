// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2002 Roman Zippel <zippel@linux-m68k.org>
 */

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>
#include <sys/utsname.h>

#include "lkc.h"

struct symbol symbol_yes = {
	.name = "y",
	.curr = { "y", yes },
	.flags = SYMBOL_CONST|SYMBOL_VALID,
};

struct symbol symbol_mod = {
	.name = "m",
	.curr = { "m", mod },
	.flags = SYMBOL_CONST|SYMBOL_VALID,
};

struct symbol symbol_no = {
	.name = "n",
	.curr = { "n", no },
	.flags = SYMBOL_CONST|SYMBOL_VALID,
};

static struct symbol symbol_empty = {
	.name = "",
	.curr = { "", no },
	.flags = SYMBOL_VALID,
};

struct symbol *sym_defconfig_list;
struct symbol *modules_sym;
static tristate modules_val;

/**
 * sym_get_type - Retrieves the type of a given symbol, with special handling for tristate symbols.
 *
 * This function returns the type of the provided symbol. If the symbol's type is `S_TRISTATE`,
 * it performs additional checks to determine the appropriate type:
 * - If the symbol is a choice value and its visibility is `yes`, the type is set to `S_BOOLEAN`.
 * - If the `modules_val` is `no`, the type is also set to `S_BOOLEAN`.
 *
 * @param sym Pointer to the symbol whose type is to be determined.
 * @return The determined type of the symbol, which can be `S_TRISTATE`, `S_BOOLEAN`, or another type.
 */
enum symbol_type sym_get_type(struct symbol *sym)
{
	enum symbol_type type = sym->type;

	if (type == S_TRISTATE) {
		if (sym_is_choice_value(sym) && sym->visible == yes)
			type = S_BOOLEAN;
		else if (modules_val == no)
			type = S_BOOLEAN;
	}
	return type;
}

/**
 * Returns a string representation of the given symbol type.
 *
 * This function takes an enum symbol_type as input and returns a corresponding
 * string that represents the type. The returned string is a human-readable
 * description of the symbol type, such as "bool", "integer", or "string".
 *
 * @param type The symbol type to convert to a string.
 * @return A string representing the symbol type. If the type is not recognized,
 *         the function returns "???".
 */
const char *sym_type_name(enum symbol_type type)
{
	switch (type) {
	case S_BOOLEAN:
		return "bool";
	case S_TRISTATE:
		return "tristate";
	case S_INT:
		return "integer";
	case S_HEX:
		return "hex";
	case S_STRING:
		return "string";
	case S_UNKNOWN:
		return "unknown";
	}
	return "???";
}

/**
 * sym_get_choice_prop - Retrieves the first property associated with a choice symbol.
 * @sym: The symbol representing the choice.
 *
 * This function iterates through all properties associated with the given choice symbol
 * and returns the first property encountered. If no properties are found, it returns NULL.
 *
 * Return: A pointer to the first property associated with the choice symbol, or NULL if
 *         no properties are found.
 */
struct property *sym_get_choice_prop(struct symbol *sym)
{
	struct property *prop;

	for_all_choices(sym, prop)
		return prop;
	return NULL;
}

/**
 * sym_get_default_prop - Retrieves the first visible default property for a given symbol.
 *
 * This function iterates through all default properties associated with the provided symbol.
 * For each property, it evaluates the visibility expression and checks if the property is
 * visible (i.e., the expression does not evaluate to 'no'). If a visible property is found,
 * it is returned immediately. If no visible default properties are found, the function
 * returns NULL.
 *
 * @sym: Pointer to the symbol whose default properties are to be evaluated.
 * @return: Pointer to the first visible default property, or NULL if no visible properties
 *          are found.
 */
static struct property *sym_get_default_prop(struct symbol *sym)
{
	struct property *prop;

	for_all_defaults(sym, prop) {
		prop->visible.tri = expr_calc_value(prop->visible.expr);
		if (prop->visible.tri != no)
			return prop;
	}
	return NULL;
}

/**
 * sym_get_range_prop - Retrieves the first visible range property associated with the given symbol.
 *
 * This function iterates through all properties of the specified symbol that are of type P_RANGE.
 * For each such property, it evaluates the visibility expression and checks if the property is
 * visible (i.e., the visibility expression does not evaluate to 'no'). If a visible range property
 * is found, it is returned immediately. If no visible range properties are found, the function
 * returns NULL.
 *
 * @param sym The symbol whose range properties are to be checked.
 * @return A pointer to the first visible range property, or NULL if no such property exists.
 */
struct property *sym_get_range_prop(struct symbol *sym)
{
	struct property *prop;

	for_all_properties(sym, prop, P_RANGE) {
		prop->visible.tri = expr_calc_value(prop->visible.expr);
		if (prop->visible.tri != no)
			return prop;
	}
	return NULL;
}

/**
 * sym_get_range_val - Retrieves the numerical value of a symbol's current value based on its type.
 *
 * This function calculates the current value of the symbol by calling `sym_calc_value(sym)`. 
 * Depending on the symbol's type, it determines the appropriate base for converting the symbol's 
 * current value to a numerical value:
 * - For integer types (S_INT), the base is set to 10.
 * - For hexadecimal types (S_HEX), the base is set to 16.
 * - For other types, the base remains unchanged.
 *
 * The function then converts the symbol's current value (stored as a string in `sym->curr.val`) 
 * to a long long integer using `strtoll` with the determined base.
 *
 * @param sym  Pointer to the symbol structure whose value is to be retrieved.
 * @param base The base to use for conversion if the symbol's type does not specify one.
 * @return     The numerical value of the symbol's current value as a long long integer.
 */
static long long sym_get_range_val(struct symbol *sym, int base)
{
	sym_calc_value(sym);
	switch (sym->type) {
	case S_INT:
		base = 10;
		break;
	case S_HEX:
		base = 16;
		break;
	default:
		break;
	}
	return strtoll(sym->curr.val, NULL, base);
}

/**
 * sym_validate_range - Validates the current value of a symbol against its defined range.
 *
 * This function checks if the current value of the symbol `sym` falls within its defined range.
 * The range is determined by the symbol's properties. If the value is outside the range, it is
 * adjusted to the nearest boundary of the range.
 *
 * The function handles symbols of type `S_INT` (integer) and `S_HEX` (hexadecimal). For other types,
 * the function returns without performing any validation.
 *
 * @param sym Pointer to the symbol structure whose value is to be validated.
 *
 * The function performs the following steps:
 * 1. Determines the base (10 for integers, 16 for hexadecimal) based on the symbol's type.
 * 2. Retrieves the range property of the symbol.
 * 3. Converts the current value of the symbol to a long long integer using the appropriate base.
 * 4. Retrieves the lower and upper bounds of the range from the symbol's properties.
 * 5. If the current value is within the range, the function returns without making any changes.
 * 6. If the current value is outside the range, it is adjusted to the nearest boundary (either the
 *    lower or upper bound).
 * 7. The adjusted value is then converted back to a string and assigned as the new current value
 *    of the symbol.
 */
static void sym_validate_range(struct symbol *sym)
{
	struct property *prop;
	int base;
	long long val, val2;
	char str[64];

	switch (sym->type) {
	case S_INT:
		base = 10;
		break;
	case S_HEX:
		base = 16;
		break;
	default:
		return;
	}
	prop = sym_get_range_prop(sym);
	if (!prop)
		return;
	val = strtoll(sym->curr.val, NULL, base);
	val2 = sym_get_range_val(prop->expr->left.sym, base);
	if (val >= val2) {
		val2 = sym_get_range_val(prop->expr->right.sym, base);
		if (val <= val2)
			return;
	}
	if (sym->type == S_INT)
		sprintf(str, "%lld", val2);
	else
		sprintf(str, "0x%llx", val2);
	sym->curr.val = xstrdup(str);
}

/**
 * Marks a symbol and its associated properties as changed.
 *
 * This function sets the `SYMBOL_CHANGED` flag on the given symbol `sym` to indicate
 * that the symbol's state has been modified. Additionally, it traverses all properties
 * associated with the symbol and, if a property has an associated menu, sets the
 * `MENU_CHANGED` flag on that menu to indicate that the menu's state has also been
 * modified.
 *
 * @param sym Pointer to the symbol structure to be marked as changed.
 */
static void sym_set_changed(struct symbol *sym)
{
	struct property *prop;

	sym->flags |= SYMBOL_CHANGED;
	for (prop = sym->prop; prop; prop = prop->next) {
		if (prop->menu)
			prop->menu->flags |= MENU_CHANGED;
	}
}

/**
 * sym_set_all_changed - Marks all symbols as changed.
 *
 * This function iterates over all symbols in the symbol table and marks each
 * one as changed by calling `sym_set_changed` on it. This is typically used
 * to force a re-evaluation or update of all symbols, ensuring that any dependent
 * logic or state is refreshed.
 *
 * @note The function assumes that `for_all_symbols` is a macro or function
 * that iterates over all symbols, and `sym_set_changed` is a function that
 * marks a single symbol as changed.
 */
static void sym_set_all_changed(void)
{
	struct symbol *sym;
	int i;

	for_all_symbols(i, sym)
		sym_set_changed(sym);
}

/**
 * sym_calc_visibility - Calculate the visibility of a symbol based on its properties and dependencies.
 *
 * This function evaluates the visibility of a symbol by considering its prompts, dependencies, and
 * implications. It updates the visibility state of the symbol and marks it as changed if necessary.
 *
 * The function performs the following steps:
 * 1. Checks if the symbol is a choice value and retrieves the corresponding choice symbol.
 * 2. Iterates over all prompts associated with the symbol and calculates their visibility.
 * 3. Adjusts the visibility of tristate choice values based on the current state of the choice.
 * 4. Determines the overall visibility of the symbol by combining the visibility of its prompts.
 * 5. Updates the symbol's visibility state and marks it as changed if the visibility has changed.
 * 6. If the symbol is not a choice value, it calculates the visibility based on direct dependencies,
 *    reverse dependencies, and implied dependencies, updating the symbol's state accordingly.
 *
 * @sym: The symbol whose visibility is to be calculated.
 */
static void sym_calc_visibility(struct symbol *sym)
{
	struct property *prop;
	struct symbol *choice_sym = NULL;
	tristate tri;

	/* any prompt visible? */
	tri = no;

	if (sym_is_choice_value(sym))
		choice_sym = prop_get_symbol(sym_get_choice_prop(sym));

	for_all_prompts(sym, prop) {
		prop->visible.tri = expr_calc_value(prop->visible.expr);
		/*
		 * Tristate choice_values with visibility 'mod' are
		 * not visible if the corresponding choice's value is
		 * 'yes'.
		 */
		if (choice_sym && sym->type == S_TRISTATE &&
		    prop->visible.tri == mod && choice_sym->curr.tri == yes)
			prop->visible.tri = no;

		tri = EXPR_OR(tri, prop->visible.tri);
	}
	if (tri == mod && (sym->type != S_TRISTATE || modules_val == no))
		tri = yes;
	if (sym->visible != tri) {
		sym->visible = tri;
		sym_set_changed(sym);
	}
	if (sym_is_choice_value(sym))
		return;
	/* defaulting to "yes" if no explicit "depends on" are given */
	tri = yes;
	if (sym->dir_dep.expr)
		tri = expr_calc_value(sym->dir_dep.expr);
	if (tri == mod && sym_get_type(sym) == S_BOOLEAN)
		tri = yes;
	if (sym->dir_dep.tri != tri) {
		sym->dir_dep.tri = tri;
		sym_set_changed(sym);
	}
	tri = no;
	if (sym->rev_dep.expr)
		tri = expr_calc_value(sym->rev_dep.expr);
	if (tri == mod && sym_get_type(sym) == S_BOOLEAN)
		tri = yes;
	if (sym->rev_dep.tri != tri) {
		sym->rev_dep.tri = tri;
		sym_set_changed(sym);
	}
	tri = no;
	if (sym->implied.expr)
		tri = expr_calc_value(sym->implied.expr);
	if (tri == mod && sym_get_type(sym) == S_BOOLEAN)
		tri = yes;
	if (sym->implied.tri != tri) {
		sym->implied.tri = tri;
		sym_set_changed(sym);
	}
}

/*
 * Find the default symbol for a choice.
 * First try the default values for the choice symbol
 * Next locate the first visible choice value
 * Return NULL if none was found
 */
struct symbol *sym_choice_default(struct symbol *sym)
{
	struct symbol *def_sym;
	struct property *prop;
	struct expr *e;

	/* any of the defaults visible? */
	for_all_defaults(sym, prop) {
		prop->visible.tri = expr_calc_value(prop->visible.expr);
		if (prop->visible.tri == no)
			continue;
		def_sym = prop_get_symbol(prop);
		if (def_sym->visible != no)
			return def_sym;
	}

	/* just get the first visible value */
	prop = sym_get_choice_prop(sym);
	expr_list_for_each_sym(prop->expr, e, def_sym)
		if (def_sym->visible != no)
			return def_sym;

	/* failed to locate any defaults */
	return NULL;
}

/**
 * sym_calc_choice - Calculate the effective symbol for a choice symbol.
 * @sym: The choice symbol to calculate the effective symbol for.
 *
 * This function calculates the effective symbol for a choice symbol by first
 * evaluating the visibility of all choice values. It then determines the
 * default symbol based on the user's choice or the default choice if no user
 * choice is visible. The function also updates the tristate value of the
 * choice symbol if no valid choice is found.
 *
 * The function performs the following steps:
 * 1. Evaluates the visibility of all choice values and calculates the combined
 *    flags based on visible choice values.
 * 2. Updates the flags of the choice symbol, preserving the SYMBOL_DEF_USER
 *    flag if set.
 * 3. Checks if the user's choice is visible and returns it if so.
 * 4. If the user's choice is not visible, determines the default choice symbol
 *    and returns it.
 * 5. If no valid choice is found, resets the tristate value of the choice
 *    symbol to 'no'.
 *
 * Return: The effective symbol for the choice, or NULL if no valid choice is
 *         found.
 */
static struct symbol *sym_calc_choice(struct symbol *sym)
{
	struct symbol *def_sym;
	struct property *prop;
	struct expr *e;
	int flags;

	/* first calculate all choice values' visibilities */
	flags = sym->flags;
	prop = sym_get_choice_prop(sym);
	expr_list_for_each_sym(prop->expr, e, def_sym) {
		sym_calc_visibility(def_sym);
		if (def_sym->visible != no)
			flags &= def_sym->flags;
	}

	sym->flags &= flags | ~SYMBOL_DEF_USER;

	/* is the user choice visible? */
	def_sym = sym->def[S_DEF_USER].val;
	if (def_sym && def_sym->visible != no)
		return def_sym;

	def_sym = sym_choice_default(sym);

	if (def_sym == NULL)
		/* no choice? reset tristate value */
		sym->curr.tri = no;

	return def_sym;
}

/**
 * sym_warn_unmet_dep - Warns about unmet direct dependencies for a given symbol.
 *
 * This function generates a warning message indicating that the specified symbol
 * has unmet direct dependencies. It constructs a detailed message that includes:
 * - The name of the symbol.
 * - The dependencies of the symbol, categorized by their type (module 'm' or built-in 'n').
 * - The reverse dependencies that selected the symbol, categorized by their type
 *   (built-in 'y' or module 'm').
 *
 * The warning message is printed to the standard error stream (stderr).
 *
 * @sym: Pointer to the symbol structure for which the unmet dependencies are being checked.
 */
static void sym_warn_unmet_dep(struct symbol *sym)
{
	struct gstr gs = str_new();

	str_printf(&gs,
		   "\nWARNING: unmet direct dependencies detected for %s\n",
		   sym->name);
	str_printf(&gs,
		   "  Depends on [%c]: ",
		   sym->dir_dep.tri == mod ? 'm' : 'n');
	expr_gstr_print(sym->dir_dep.expr, &gs);
	str_printf(&gs, "\n");

	expr_gstr_print_revdep(sym->rev_dep.expr, &gs, yes,
			       "  Selected by [y]:\n");
	expr_gstr_print_revdep(sym->rev_dep.expr, &gs, mod,
			       "  Selected by [m]:\n");

	fputs(str_get(&gs), stderr);
}

/**
 * sym_calc_value - Calculate the value of a symbol based on its properties and dependencies.
 *
 * This function computes the current value of a symbol (`sym`) by evaluating its type, visibility,
 * default values, and dependencies. It handles various symbol types, including boolean, tristate,
 * integer, hex, and string. The function also manages choice symbols and ensures that dependent
 * symbols are updated accordingly.
 *
 * The function performs the following steps:
 * 1. Checks if the symbol is valid or NULL, returning early if so.
 * 2. Handles choice symbols by recursively calculating their values if necessary.
 * 3. Sets the symbol's `SYMBOL_VALID` flag to mark it as processed.
 * 4. Initializes the symbol's value based on its type (e.g., `symbol_no` for boolean/tristate).
 * 5. Calculates the symbol's visibility and updates its `SYMBOL_WRITE` flag if visible.
 * 6. Computes the symbol's value based on user-defined values, default values, and dependencies.
 * 7. Validates the symbol's range and updates dependent symbols if the value changes.
 * 8. Handles choice symbols by propagating changes to their associated symbols.
 * 9. Clears the `SYMBOL_WRITE` flag if the symbol is marked as `SYMBOL_NO_WRITE`.
 * 10. Sets choice values if the symbol requires it.
 *
 * @param sym Pointer to the symbol whose value is to be calculated. If NULL, the function returns early.
 */
void sym_calc_value(struct symbol *sym)
{
	struct symbol_value newval, oldval;
	struct property *prop;
	struct expr *e;

	if (!sym)
		return;

	if (sym->flags & SYMBOL_VALID)
		return;

	if (sym_is_choice_value(sym) &&
	    sym->flags & SYMBOL_NEED_SET_CHOICE_VALUES) {
		sym->flags &= ~SYMBOL_NEED_SET_CHOICE_VALUES;
		prop = sym_get_choice_prop(sym);
		sym_calc_value(prop_get_symbol(prop));
	}

	sym->flags |= SYMBOL_VALID;

	oldval = sym->curr;

	switch (sym->type) {
	case S_INT:
	case S_HEX:
	case S_STRING:
		newval = symbol_empty.curr;
		break;
	case S_BOOLEAN:
	case S_TRISTATE:
		newval = symbol_no.curr;
		break;
	default:
		sym->curr.val = sym->name;
		sym->curr.tri = no;
		return;
	}
	sym->flags &= ~SYMBOL_WRITE;

	sym_calc_visibility(sym);

	if (sym->visible != no)
		sym->flags |= SYMBOL_WRITE;

	/* set default if recursively called */
	sym->curr = newval;

	switch (sym_get_type(sym)) {
	case S_BOOLEAN:
	case S_TRISTATE:
		if (sym_is_choice_value(sym) && sym->visible == yes) {
			prop = sym_get_choice_prop(sym);
			newval.tri = (prop_get_symbol(prop)->curr.val == sym) ? yes : no;
		} else {
			if (sym->visible != no) {
				/* if the symbol is visible use the user value
				 * if available, otherwise try the default value
				 */
				if (sym_has_value(sym)) {
					newval.tri = EXPR_AND(sym->def[S_DEF_USER].tri,
							      sym->visible);
					goto calc_newval;
				}
			}
			if (sym->rev_dep.tri != no)
				sym->flags |= SYMBOL_WRITE;
			if (!sym_is_choice(sym)) {
				prop = sym_get_default_prop(sym);
				if (prop) {
					newval.tri = EXPR_AND(expr_calc_value(prop->expr),
							      prop->visible.tri);
					if (newval.tri != no)
						sym->flags |= SYMBOL_WRITE;
				}
				if (sym->implied.tri != no) {
					sym->flags |= SYMBOL_WRITE;
					newval.tri = EXPR_OR(newval.tri, sym->implied.tri);
					newval.tri = EXPR_AND(newval.tri,
							      sym->dir_dep.tri);
				}
			}
		calc_newval:
			if (sym->dir_dep.tri < sym->rev_dep.tri)
				sym_warn_unmet_dep(sym);
			newval.tri = EXPR_OR(newval.tri, sym->rev_dep.tri);
		}
		if (newval.tri == mod && sym_get_type(sym) == S_BOOLEAN)
			newval.tri = yes;
		break;
	case S_STRING:
	case S_HEX:
	case S_INT:
		if (sym->visible != no && sym_has_value(sym)) {
			newval.val = sym->def[S_DEF_USER].val;
			break;
		}
		prop = sym_get_default_prop(sym);
		if (prop) {
			struct symbol *ds = prop_get_symbol(prop);
			if (ds) {
				sym->flags |= SYMBOL_WRITE;
				sym_calc_value(ds);
				newval.val = ds->curr.val;
			}
		}
		break;
	default:
		;
	}

	sym->curr = newval;
	if (sym_is_choice(sym) && newval.tri == yes)
		sym->curr.val = sym_calc_choice(sym);
	sym_validate_range(sym);

	if (memcmp(&oldval, &sym->curr, sizeof(oldval))) {
		sym_set_changed(sym);
		if (modules_sym == sym) {
			sym_set_all_changed();
			modules_val = modules_sym->curr.tri;
		}
	}

	if (sym_is_choice(sym)) {
		struct symbol *choice_sym;

		prop = sym_get_choice_prop(sym);
		expr_list_for_each_sym(prop->expr, e, choice_sym) {
			if ((sym->flags & SYMBOL_WRITE) &&
			    choice_sym->visible != no)
				choice_sym->flags |= SYMBOL_WRITE;
			if (sym->flags & SYMBOL_CHANGED)
				sym_set_changed(choice_sym);
		}
	}

	if (sym->flags & SYMBOL_NO_WRITE)
		sym->flags &= ~SYMBOL_WRITE;

	if (sym->flags & SYMBOL_NEED_SET_CHOICE_VALUES)
		set_all_choice_values(sym);
}

/**
 * sym_clear_all_valid - Clears the SYMBOL_VALID flag for all symbols in the symbol table.
 *
 * This function iterates over all symbols in the symbol table and clears the SYMBOL_VALID flag
 * for each symbol. The SYMBOL_VALID flag typically indicates whether a symbol's value is up-to-date.
 * After clearing the flags, the function increments the change count to signal that the symbol
 * table has been modified and then recalculates the value of the 'modules_sym' symbol to ensure
 * consistency.
 *
 * Note: This function is typically used to invalidate all symbols, forcing them to be recalculated
 * when needed.
 */
void sym_clear_all_valid(void)
{
	struct symbol *sym;
	int i;

	for_all_symbols(i, sym)
		sym->flags &= ~SYMBOL_VALID;
	sym_add_change_count(1);
	sym_calc_value(modules_sym);
}

/**
 * Determines whether a given tristate value is within the valid range for a symbol.
 *
 * This function checks if the provided tristate value `val` is a valid configuration option for the
 * given symbol `sym`. The validity is determined based on the symbol's type, visibility, and
 * dependencies. The function returns `true` if the value is within the valid range, otherwise `false`.
 *
 * The function performs the following checks:
 * 1. If the symbol is not visible (`sym->visible == no`), the value is invalid.
 * 2. If the symbol's type is neither `S_BOOLEAN` nor `S_TRISTATE`, the value is invalid.
 * 3. If the symbol is of type `S_BOOLEAN` and the value is `mod`, the value is invalid.
 * 4. If the symbol's visibility is less than or equal to its reverse dependency value, the value is invalid.
 * 5. If the symbol is a choice value and is visible (`sym->visible == yes`), the value is valid only if it is `yes`.
 * 6. Otherwise, the value is valid if it is between the symbol's reverse dependency value and its visibility.
 *
 * @param sym The symbol to check against.
 * @param val The tristate value to validate.
 * @return `true` if the value is within the valid range, `false` otherwise.
 */
bool sym_tristate_within_range(struct symbol *sym, tristate val)
{
	int type = sym_get_type(sym);

	if (sym->visible == no)
		return false;

	if (type != S_BOOLEAN && type != S_TRISTATE)
		return false;

	if (type == S_BOOLEAN && val == mod)
		return false;
	if (sym->visible <= sym->rev_dep.tri)
		return false;
	if (sym_is_choice_value(sym) && sym->visible == yes)
		return val == yes;
	return val >= sym->rev_dep.tri && val <= sym->visible;
}

/**
 * sym_set_tristate_value - Sets the tristate value of a symbol.
 * @sym: Pointer to the symbol whose tristate value is to be set.
 * @val: The new tristate value to be assigned to the symbol.
 *
 * This function sets the tristate value of the given symbol to the specified value.
 * It first checks if the new value is within the valid range for the symbol. If not,
 * the function returns false. If the symbol's value is being changed and the new value
 * is valid, the function updates the symbol's value and marks it as changed.
 *
 * If the symbol is a choice value and the new value is 'yes', the function also resets
 * the new flag of the choice symbol and all other choice values.
 *
 * The function returns true if the value was successfully set, and false otherwise.
 */
bool sym_set_tristate_value(struct symbol *sym, tristate val)
{
	tristate oldval = sym_get_tristate_value(sym);

	if (oldval != val && !sym_tristate_within_range(sym, val))
		return false;

	if (!(sym->flags & SYMBOL_DEF_USER)) {
		sym->flags |= SYMBOL_DEF_USER;
		sym_set_changed(sym);
	}
	/*
	 * setting a choice value also resets the new flag of the choice
	 * symbol and all other choice values.
	 */
	if (sym_is_choice_value(sym) && val == yes) {
		struct symbol *cs = prop_get_symbol(sym_get_choice_prop(sym));
		struct property *prop;
		struct expr *e;

		cs->def[S_DEF_USER].val = sym;
		cs->flags |= SYMBOL_DEF_USER;
		prop = sym_get_choice_prop(cs);
		for (e = prop->expr; e; e = e->left.expr) {
			if (e->right.sym->visible != no)
				e->right.sym->flags |= SYMBOL_DEF_USER;
		}
	}

	sym->def[S_DEF_USER].tri = val;
	if (oldval != val)
		sym_clear_all_valid();

	return true;
}

/**
 * sym_toggle_tristate_value - Toggles the tristate value of a symbol.
 * @sym: Pointer to the symbol whose tristate value is to be toggled.
 *
 * This function cycles through the possible tristate values (no, mod, yes) of the given symbol.
 * It starts with the current value of the symbol and toggles to the next value in the sequence:
 * no -> mod -> yes -> no. The function attempts to set the new value using sym_set_tristate_value.
 * If the setting is successful, the function returns the new value. If the setting fails, it continues
 * to toggle until it either succeeds or returns to the original value.
 *
 * Return: The new tristate value of the symbol after toggling.
 */
tristate sym_toggle_tristate_value(struct symbol *sym)
{
	tristate oldval, newval;

	oldval = newval = sym_get_tristate_value(sym);
	do {
		switch (newval) {
		case no:
			newval = mod;
			break;
		case mod:
			newval = yes;
			break;
		case yes:
			newval = no;
			break;
		}
		if (sym_set_tristate_value(sym, newval))
			break;
	} while (oldval != newval);
	return newval;
}

/**
 * @brief Validates a string based on the type of the given symbol.
 *
 * This function checks if the provided string `str` is valid according to the type
 * of the symbol `sym`. The validation rules depend on the symbol's type:
 * - For `S_STRING`: The string is always considered valid.
 * - For `S_INT`: The string must represent a valid integer. It can optionally start
 *   with a '-' sign, followed by digits. If the first digit is '0', the string must
 *   not contain any additional characters.
 * - For `S_HEX`: The string must represent a valid hexadecimal number. It can
 *   optionally start with '0x' or '0X', followed by hexadecimal digits.
 * - For `S_BOOLEAN` or `S_TRISTATE`: The string must be one of the following:
 *   'y', 'Y', 'm', 'M', 'n', or 'N'.
 * - For any other type: The string is considered invalid.
 *
 * @param sym Pointer to the symbol whose type determines the validation rules.
 * @param str The string to be validated.
 * @return `true` if the string is valid according to the symbol's type, `false` otherwise.
 */
bool sym_string_valid(struct symbol *sym, const char *str)
{
	signed char ch;

	switch (sym->type) {
	case S_STRING:
		return true;
	case S_INT:
		ch = *str++;
		if (ch == '-')
			ch = *str++;
		if (!isdigit(ch))
			return false;
		if (ch == '0' && *str != 0)
			return false;
		while ((ch = *str++)) {
			if (!isdigit(ch))
				return false;
		}
		return true;
	case S_HEX:
		if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X'))
			str += 2;
		ch = *str++;
		do {
			if (!isxdigit(ch))
				return false;
		} while ((ch = *str++));
		return true;
	case S_BOOLEAN:
	case S_TRISTATE:
		switch (str[0]) {
		case 'y': case 'Y':
		case 'm': case 'M':
		case 'n': case 'N':
			return true;
		}
		return false;
	default:
		return false;
	}
}

/**
 * @brief Checks if the given string is valid and within the allowed range for the symbol.
 *
 * This function validates the string based on the symbol's type. For string types (S_STRING),
 * it checks if the string is valid using `sym_string_valid`. For integer types (S_INT), it
 * ensures the string represents a valid integer and checks if it falls within the symbol's
 * defined range. For hexadecimal types (S_HEX), it ensures the string represents a valid
 * hexadecimal number and checks if it falls within the symbol's defined range. For boolean
 * (S_BOOLEAN) and tristate (S_TRISTATE) types, it checks if the string corresponds to a valid
 * state (yes, mod, or no) and if that state is within the symbol's allowed range.
 *
 * @param sym The symbol to validate the string against.
 * @param str The string to validate.
 * @return true if the string is valid and within the symbol's range, false otherwise.
 */
bool sym_string_within_range(struct symbol *sym, const char *str)
{
	struct property *prop;
	long long val;

	switch (sym->type) {
	case S_STRING:
		return sym_string_valid(sym, str);
	case S_INT:
		if (!sym_string_valid(sym, str))
			return false;
		prop = sym_get_range_prop(sym);
		if (!prop)
			return true;
		val = strtoll(str, NULL, 10);
		return val >= sym_get_range_val(prop->expr->left.sym, 10) &&
		       val <= sym_get_range_val(prop->expr->right.sym, 10);
	case S_HEX:
		if (!sym_string_valid(sym, str))
			return false;
		prop = sym_get_range_prop(sym);
		if (!prop)
			return true;
		val = strtoll(str, NULL, 16);
		return val >= sym_get_range_val(prop->expr->left.sym, 16) &&
		       val <= sym_get_range_val(prop->expr->right.sym, 16);
	case S_BOOLEAN:
	case S_TRISTATE:
		switch (str[0]) {
		case 'y': case 'Y':
			return sym_tristate_within_range(sym, yes);
		case 'm': case 'M':
			return sym_tristate_within_range(sym, mod);
		case 'n': case 'N':
			return sym_tristate_within_range(sym, no);
		}
		return false;
	default:
		return false;
	}
}

/**
 * Sets the string value of a symbol and updates its internal state accordingly.
 *
 * This function sets the string value of the given symbol `sym` to `newval`. The behavior of the
 * function depends on the type of the symbol:
 * - For symbols of type `S_BOOLEAN` or `S_TRISTATE`, the function interprets `newval` as a
 *   tristate value ('y', 'm', or 'n') and sets the symbol's value accordingly using
 *   `sym_set_tristate_value`.
 * - For other symbol types, the function checks if `newval` is within the valid range for the
 *   symbol using `sym_string_within_range`. If not, the function returns `false`.
 * - If the symbol's value is changed, the function marks the symbol as user-defined and triggers
 *   a change notification.
 * - For symbols of type `S_HEX`, the function ensures that `newval` is prefixed with '0x' if
 *   necessary.
 * - The function allocates memory for the new value and frees the old value if it exists.
 * - Finally, the function clears all valid flags for the symbol.
 *
 * @param sym     Pointer to the symbol whose value is to be set.
 * @param newval  The new string value to assign to the symbol.
 * @return        `true` if the value was successfully set, `false` otherwise.
 */
bool sym_set_string_value(struct symbol *sym, const char *newval)
{
	const char *oldval;
	char *val;
	int size;

	switch (sym->type) {
	case S_BOOLEAN:
	case S_TRISTATE:
		switch (newval[0]) {
		case 'y': case 'Y':
			return sym_set_tristate_value(sym, yes);
		case 'm': case 'M':
			return sym_set_tristate_value(sym, mod);
		case 'n': case 'N':
			return sym_set_tristate_value(sym, no);
		}
		return false;
	default:
		;
	}

	if (!sym_string_within_range(sym, newval))
		return false;

	if (!(sym->flags & SYMBOL_DEF_USER)) {
		sym->flags |= SYMBOL_DEF_USER;
		sym_set_changed(sym);
	}

	oldval = sym->def[S_DEF_USER].val;
	size = strlen(newval) + 1;
	if (sym->type == S_HEX && (newval[0] != '0' || (newval[1] != 'x' && newval[1] != 'X'))) {
		size += 2;
		sym->def[S_DEF_USER].val = val = xmalloc(size);
		*val++ = '0';
		*val++ = 'x';
	} else if (!oldval || strcmp(oldval, newval))
		sym->def[S_DEF_USER].val = val = xmalloc(size);
	else
		return true;

	strcpy(val, newval);
	free((void *)oldval);
	sym_clear_all_valid();

	return true;
}

/*
 * Find the default value associated to a symbol.
 * For tristate symbol handle the modules=n case
 * in which case "m" becomes "y".
 * If the symbol does not have any default then fallback
 * to the fixed default values.
 */
const char *sym_get_string_default(struct symbol *sym)
{
	struct property *prop;
	struct symbol *ds;
	const char *str;
	tristate val;

	sym_calc_visibility(sym);
	sym_calc_value(modules_sym);
	val = symbol_no.curr.tri;
	str = symbol_empty.curr.val;

	/* If symbol has a default value look it up */
	prop = sym_get_default_prop(sym);
	if (prop != NULL) {
		switch (sym->type) {
		case S_BOOLEAN:
		case S_TRISTATE:
			/* The visibility may limit the value from yes => mod */
			val = EXPR_AND(expr_calc_value(prop->expr), prop->visible.tri);
			break;
		default:
			/*
			 * The following fails to handle the situation
			 * where a default value is further limited by
			 * the valid range.
			 */
			ds = prop_get_symbol(prop);
			if (ds != NULL) {
				sym_calc_value(ds);
				str = (const char *)ds->curr.val;
			}
		}
	}

	/* Handle select statements */
	val = EXPR_OR(val, sym->rev_dep.tri);

	/* transpose mod to yes if modules are not enabled */
	if (val == mod)
		if (!sym_is_choice_value(sym) && modules_sym->curr.tri == no)
			val = yes;

	/* transpose mod to yes if type is bool */
	if (sym->type == S_BOOLEAN && val == mod)
		val = yes;

	/* adjust the default value if this symbol is implied by another */
	if (val < sym->implied.tri)
		val = sym->implied.tri;

	switch (sym->type) {
	case S_BOOLEAN:
	case S_TRISTATE:
		switch (val) {
		case no: return "n";
		case mod: return "m";
		case yes: return "y";
		}
	case S_INT:
	case S_HEX:
		return str;
	case S_STRING:
		return str;
	case S_UNKNOWN:
		break;
	}
	return "";
}

/**
 * Retrieves the string representation of a symbol's current value.
 *
 * This function determines the string value of a given symbol based on its type.
 * For boolean and tristate symbols, it returns "n", "m", or "y" depending on the
 * symbol's tristate value (no, mod, or yes, respectively). For other symbol types,
 * it directly returns the string value stored in the symbol's current value.
 *
 * @param sym Pointer to the symbol structure whose string value is to be retrieved.
 * @return A string representing the symbol's current value. The returned string
 *         is either "n", "m", "y", or the symbol's direct string value.
 */
const char *sym_get_string_value(struct symbol *sym)
{
	tristate val;

	switch (sym->type) {
	case S_BOOLEAN:
	case S_TRISTATE:
		val = sym_get_tristate_value(sym);
		switch (val) {
		case no:
			return "n";
		case mod:
			sym_calc_value(modules_sym);
			return (modules_sym->curr.tri == no) ? "n" : "m";
		case yes:
			return "y";
		}
		break;
	default:
		;
	}
	return (const char *)sym->curr.val;
}

/**
 * Determines if a symbol is changeable based on its visibility and reverse dependency.
 * 
 * A symbol is considered changeable if its visibility level is greater than the 
 * reverse dependency level. The visibility level indicates how accessible the symbol 
 * is, while the reverse dependency level represents the constraints imposed by 
 * other symbols that depend on it.
 *
 * @param sym Pointer to the symbol structure to evaluate.
 * @return `true` if the symbol is changeable (visibility > reverse dependency), 
 *         `false` otherwise.
 */
bool sym_is_changeable(struct symbol *sym)
{
	return sym->visible > sym->rev_dep.tri;
}

/**
 * Computes a 32-bit hash value for the given null-terminated string using the FNV-1a hashing algorithm.
 * The FNV-1a algorithm is a non-cryptographic hash function that produces a hash value by iterating
 * over each character in the string and combining it with the current hash value using XOR and multiplication.
 *
 * @param s A pointer to the null-terminated string to be hashed.
 * @return The computed 32-bit hash value as an unsigned integer.
 *
 * @note The initial hash value is set to the FNV offset basis (2166136261U).
 *       The prime multiplier used in the algorithm is 0x01000193.
 */
static unsigned strhash(const char *s)
{
	/* fnv32 hash */
	unsigned hash = 2166136261U;
	for (; *s; s++)
		hash = (hash ^ *s) * 0x01000193;
	return hash;
}

/**
 * sym_lookup - Lookup or create a symbol with the given name and flags.
 *
 * This function searches for a symbol in the symbol hash table based on the
 * provided name. If a symbol with the given name and matching flags is found,
 * it is returned. If no such symbol exists, a new symbol is created, added to
 * the hash table, and returned.
 *
 * Special handling is provided for single-character names 'y', 'm', and 'n',
 * which return predefined symbols `symbol_yes`, `symbol_mod`, and `symbol_no`
 * respectively.
 *
 * @name: The name of the symbol to lookup or create. If NULL, a new symbol
 *        with a NULL name is created.
 * @flags: The flags to match against the symbol's flags. If flags is non-zero,
 *         the symbol's flags must match the provided flags. If flags is zero,
 *         the symbol's flags must not include SYMBOL_CONST or SYMBOL_CHOICE.
 *
 * Return: A pointer to the found or newly created symbol.
 */
struct symbol *sym_lookup(const char *name, int flags)
{
	struct symbol *symbol;
	char *new_name;
	int hash;

	if (name) {
		if (name[0] && !name[1]) {
			switch (name[0]) {
			case 'y': return &symbol_yes;
			case 'm': return &symbol_mod;
			case 'n': return &symbol_no;
			}
		}
		hash = strhash(name) % SYMBOL_HASHSIZE;

		for (symbol = symbol_hash[hash]; symbol; symbol = symbol->next) {
			if (symbol->name &&
			    !strcmp(symbol->name, name) &&
			    (flags ? symbol->flags & flags
				   : !(symbol->flags & (SYMBOL_CONST|SYMBOL_CHOICE))))
				return symbol;
		}
		new_name = xstrdup(name);
	} else {
		new_name = NULL;
		hash = 0;
	}

	symbol = xmalloc(sizeof(*symbol));
	memset(symbol, 0, sizeof(*symbol));
	symbol->name = new_name;
	symbol->type = S_UNKNOWN;
	symbol->flags = flags;

	symbol->next = symbol_hash[hash];
	symbol_hash[hash] = symbol;

	return symbol;
}

/**
 * sym_find - Searches for a symbol in the symbol table by its name.
 *
 * This function looks up a symbol in the symbol table based on the provided name.
 * It first checks if the name is a single character and matches one of the predefined
 * symbols ('y', 'm', or 'n'), returning the corresponding symbol if found. If the name
 * is longer than one character, it calculates a hash value for the name and searches
 * the symbol table in the corresponding hash bucket. The function returns the first
 * matching symbol that is not marked as constant (SYMBOL_CONST).
 *
 * @name: The name of the symbol to search for. If NULL, the function returns NULL.
 *
 * Return: A pointer to the found symbol, or NULL if no matching symbol is found.
 */
struct symbol *sym_find(const char *name)
{
	struct symbol *symbol = NULL;
	int hash = 0;

	if (!name)
		return NULL;

	if (name[0] && !name[1]) {
		switch (name[0]) {
		case 'y': return &symbol_yes;
		case 'm': return &symbol_mod;
		case 'n': return &symbol_no;
		}
	}
	hash = strhash(name) % SYMBOL_HASHSIZE;

	for (symbol = symbol_hash[hash]; symbol; symbol = symbol->next) {
		if (symbol->name &&
		    !strcmp(symbol->name, name) &&
		    !(symbol->flags & SYMBOL_CONST))
				break;
	}

	return symbol;
}

/**
 * Escapes special characters in a string and wraps it in double quotes.
 *
 * This function takes an input string `in` and processes it to escape any occurrences
 * of the characters `"` (double quote) and `\` (backslash) by prefixing them with a backslash.
 * The resulting string is then wrapped in double quotes, making it suitable for use in contexts
 * where string values need to be safely represented (e.g., JSON, CSV, etc.).
 *
 * The function allocates memory for the resulting string using `xmalloc`, so the caller is
 * responsible for freeing the returned pointer when it is no longer needed.
 *
 * @param in The input string to be escaped and quoted. Must be a null-terminated C string.
 * @return A pointer to the newly allocated, escaped, and quoted string. The caller must free
 *         this memory using `free` when it is no longer needed.
 */
const char *sym_escape_string_value(const char *in)
{
	const char *p;
	size_t reslen;
	char *res;
	size_t l;

	reslen = strlen(in) + strlen("\"\"") + 1;

	p = in;
	for (;;) {
		l = strcspn(p, "\"\\");
		p += l;

		if (p[0] == '\0')
			break;

		reslen++;
		p++;
	}

	res = xmalloc(reslen);
	res[0] = '\0';

	strcat(res, "\"");

	p = in;
	for (;;) {
		l = strcspn(p, "\"\\");
		strncat(res, p, l);
		p += l;

		if (p[0] == '\0')
			break;

		strcat(res, "\\");
		strncat(res, p++, 1);
	}

	strcat(res, "\"");
	return res;
}

struct sym_match {
	struct symbol	*sym;
	off_t		so, eo;
};

/* Compare matched symbols as thus:
 * - first, symbols that match exactly
 * - then, alphabetical sort
 */
static int sym_rel_comp(const void *sym1, const void *sym2)
{
	const struct sym_match *s1 = sym1;
	const struct sym_match *s2 = sym2;
	int exact1, exact2;

	/* Exact match:
	 * - if matched length on symbol s1 is the length of that symbol,
	 *   then this symbol should come first;
	 * - if matched length on symbol s2 is the length of that symbol,
	 *   then this symbol should come first.
	 * Note: since the search can be a regexp, both symbols may match
	 * exactly; if this is the case, we can't decide which comes first,
	 * and we fallback to sorting alphabetically.
	 */
	exact1 = (s1->eo - s1->so) == strlen(s1->sym->name);
	exact2 = (s2->eo - s2->so) == strlen(s2->sym->name);
	if (exact1 && !exact2)
		return -1;
	if (!exact1 && exact2)
		return 1;

	/* As a fallback, sort symbols alphabetically */
	return strcmp(s1->sym->name, s2->sym->name);
}

/**
 * Searches for symbols whose names match the given regular expression pattern.
 *
 * This function compiles the provided regular expression pattern and searches
 * through all available symbols. It matches symbol names against the compiled
 * regular expression, ignoring symbols that are marked as constants or have no
 * name. For each matching symbol, the function records the start and end
 * positions of the match within the symbol's name.
 *
 * The matching symbols are then sorted based on their relevance (using the
 * `sym_rel_comp` function) and returned as an array of pointers to `struct symbol`.
 * The array is NULL-terminated to indicate its end.
 *
 * @param pattern The regular expression pattern to match against symbol names.
 *                The pattern is compiled with `REG_EXTENDED` and `REG_ICASE` flags,
 *                allowing for extended regular expressions and case-insensitive matching.
 *                If the pattern is an empty string, the function returns NULL.
 *
 * @return On success, returns a dynamically allocated array of pointers to `struct symbol`
 *         representing the matching symbols, NULL-terminated. On failure (e.g., invalid
 *         regular expression or memory allocation error), returns NULL. The caller is
 *         responsible for freeing the returned array using `free()`.
 */
struct symbol **sym_re_search(const char *pattern)
{
	struct symbol *sym, **sym_arr = NULL;
	struct sym_match *sym_match_arr = NULL;
	int i, cnt, size;
	regex_t re;
	regmatch_t match[1];

	cnt = size = 0;
	/* Skip if empty */
	if (strlen(pattern) == 0)
		return NULL;
	if (regcomp(&re, pattern, REG_EXTENDED|REG_ICASE))
		return NULL;

	for_all_symbols(i, sym) {
		if (sym->flags & SYMBOL_CONST || !sym->name)
			continue;
		if (regexec(&re, sym->name, 1, match, 0))
			continue;
		if (cnt >= size) {
			void *tmp;
			size += 16;
			tmp = realloc(sym_match_arr, size * sizeof(struct sym_match));
			if (!tmp)
				goto sym_re_search_free;
			sym_match_arr = tmp;
		}
		sym_calc_value(sym);
		/* As regexec returned 0, we know we have a match, so
		 * we can use match[0].rm_[se]o without further checks
		 */
		sym_match_arr[cnt].so = match[0].rm_so;
		sym_match_arr[cnt].eo = match[0].rm_eo;
		sym_match_arr[cnt++].sym = sym;
	}
	if (sym_match_arr) {
		qsort(sym_match_arr, cnt, sizeof(struct sym_match), sym_rel_comp);
		sym_arr = malloc((cnt+1) * sizeof(struct symbol *));
		if (!sym_arr)
			goto sym_re_search_free;
		for (i = 0; i < cnt; i++)
			sym_arr[i] = sym_match_arr[i].sym;
		sym_arr[cnt] = NULL;
	}
sym_re_search_free:
	/* sym_match_arr can be NULL if no match, but free(NULL) is OK */
	free(sym_match_arr);
	regfree(&re);

	return sym_arr;
}

/*
 * When we check for recursive dependencies we use a stack to save
 * current state so we can print out relevant info to user.
 * The entries are located on the call stack so no need to free memory.
 * Note insert() remove() must always match to properly clear the stack.
 */
static struct dep_stack {
	struct dep_stack *prev, *next;
	struct symbol *sym;
	struct property *prop;
	struct expr **expr;
} *check_top;

/**
 * Inserts a new dependency stack node into the linked list of dependency stacks.
 * This function initializes the new stack node by setting its memory to zero,
 * links it to the previous top node in the dependency stack chain, and updates
 * the global `check_top` pointer to point to the newly inserted node.
 *
 * @param stack Pointer to the new dependency stack node to be inserted.
 * @param sym Pointer to the symbol associated with this dependency stack node.
 *
 * The function performs the following steps:
 * 1. Initializes the memory of the new stack node to zero using `memset`.
 * 2. If `check_top` is not NULL, it sets the `next` pointer of the current top
 *    node to point to the new stack node.
 * 3. Sets the `prev` pointer of the new stack node to point to the current top node.
 * 4. Assigns the provided symbol (`sym`) to the `sym` field of the new stack node.
 * 5. Updates the global `check_top` pointer to the new stack node.
 */
static void dep_stack_insert(struct dep_stack *stack, struct symbol *sym)
{
	memset(stack, 0, sizeof(*stack));
	if (check_top)
		check_top->next = stack;
	stack->prev = check_top;
	stack->sym = sym;
	check_top = stack;
}

/**
 * Removes the top element from the dependency stack.
 * 
 * This function updates the `check_top` pointer to point to the previous element
 * in the stack, effectively removing the current top element. If the stack is
 * not empty after removal, it ensures that the new top element's `next` pointer
 * is set to `NULL` to maintain the stack's integrity.
 * 
 * If the stack is empty after removal, no further action is taken.
 */
static void dep_stack_remove(void)
{
	check_top = check_top->prev;
	if (check_top)
		check_top->next = NULL;
}

/*
 * Called when we have detected a recursive dependency.
 * check_top point to the top of the stact so we use
 * the ->prev pointer to locate the bottom of the stack.
 */
static void sym_check_print_recursive(struct symbol *last_sym)
{
	struct dep_stack *stack;
	struct symbol *sym, *next_sym;
	struct menu *menu = NULL;
	struct property *prop;
	struct dep_stack cv_stack;

	if (sym_is_choice_value(last_sym)) {
		dep_stack_insert(&cv_stack, last_sym);
		last_sym = prop_get_symbol(sym_get_choice_prop(last_sym));
	}

	for (stack = check_top; stack != NULL; stack = stack->prev)
		if (stack->sym == last_sym)
			break;
	if (!stack) {
		fprintf(stderr, "unexpected recursive dependency error\n");
		return;
	}

	for (; stack; stack = stack->next) {
		sym = stack->sym;
		next_sym = stack->next ? stack->next->sym : last_sym;
		prop = stack->prop;
		if (prop == NULL)
			prop = stack->sym->prop;

		/* for choice values find the menu entry (used below) */
		if (sym_is_choice(sym) || sym_is_choice_value(sym)) {
			for (prop = sym->prop; prop; prop = prop->next) {
				menu = prop->menu;
				if (prop->menu)
					break;
			}
		}
		if (stack->sym == last_sym)
			fprintf(stderr, "%s:%d:error: recursive dependency detected!\n",
				prop->file->name, prop->lineno);

		if (sym_is_choice(sym)) {
			fprintf(stderr, "%s:%d:\tchoice %s contains symbol %s\n",
				menu->file->name, menu->lineno,
				sym->name ? sym->name : "<choice>",
				next_sym->name ? next_sym->name : "<choice>");
		} else if (sym_is_choice_value(sym)) {
			fprintf(stderr, "%s:%d:\tsymbol %s is part of choice %s\n",
				menu->file->name, menu->lineno,
				sym->name ? sym->name : "<choice>",
				next_sym->name ? next_sym->name : "<choice>");
		} else if (stack->expr == &sym->dir_dep.expr) {
			fprintf(stderr, "%s:%d:\tsymbol %s depends on %s\n",
				prop->file->name, prop->lineno,
				sym->name ? sym->name : "<choice>",
				next_sym->name ? next_sym->name : "<choice>");
		} else if (stack->expr == &sym->rev_dep.expr) {
			fprintf(stderr, "%s:%d:\tsymbol %s is selected by %s\n",
				prop->file->name, prop->lineno,
				sym->name ? sym->name : "<choice>",
				next_sym->name ? next_sym->name : "<choice>");
		} else if (stack->expr == &sym->implied.expr) {
			fprintf(stderr, "%s:%d:\tsymbol %s is implied by %s\n",
				prop->file->name, prop->lineno,
				sym->name ? sym->name : "<choice>",
				next_sym->name ? next_sym->name : "<choice>");
		} else if (stack->expr) {
			fprintf(stderr, "%s:%d:\tsymbol %s %s value contains %s\n",
				prop->file->name, prop->lineno,
				sym->name ? sym->name : "<choice>",
				prop_get_type_name(prop->type),
				next_sym->name ? next_sym->name : "<choice>");
		} else {
			fprintf(stderr, "%s:%d:\tsymbol %s %s is visible depending on %s\n",
				prop->file->name, prop->lineno,
				sym->name ? sym->name : "<choice>",
				prop_get_type_name(prop->type),
				next_sym->name ? next_sym->name : "<choice>");
		}
	}

	fprintf(stderr,
		"For a resolution refer to Documentation/kbuild/kconfig-language.rst\n"
		"subsection \"Kconfig recursive dependency limitations\"\n"
		"\n");

	if (check_top == &cv_stack)
		dep_stack_remove();
}

/**
 * Recursively checks the dependencies of a given expression and returns the first
 * symbol that has unresolved dependencies.
 *
 * This function traverses the expression tree starting from the root node `e`.
 * Depending on the type of the expression node, it recursively checks the dependencies
 * of the left and/or right sub-expressions. For comparison operations (e.g., E_EQUAL,
 * E_GEQ), it checks the dependencies of the symbols involved. For logical operations
 * (e.g., E_OR, E_AND, E_NOT), it continues to traverse the expression tree. If a
 * symbol with unresolved dependencies is found, it is returned immediately.
 *
 * @param e The root expression node to check for dependencies.
 * @return A pointer to the first symbol with unresolved dependencies, or NULL if
 *         no such symbol is found or if the expression is NULL.
 */
static struct symbol *sym_check_expr_deps(struct expr *e)
{
	struct symbol *sym;

	if (!e)
		return NULL;
	switch (e->type) {
	case E_OR:
	case E_AND:
		sym = sym_check_expr_deps(e->left.expr);
		if (sym)
			return sym;
		return sym_check_expr_deps(e->right.expr);
	case E_NOT:
		return sym_check_expr_deps(e->left.expr);
	case E_EQUAL:
	case E_GEQ:
	case E_GTH:
	case E_LEQ:
	case E_LTH:
	case E_UNEQUAL:
		sym = sym_check_deps(e->left.sym);
		if (sym)
			return sym;
		return sym_check_deps(e->right.sym);
	case E_SYMBOL:
		return sym_check_deps(e->left.sym);
	default:
		break;
	}
	fprintf(stderr, "Oops! How to check %d?\n", e->type);
	return NULL;
}

/* return NULL when dependencies are OK */
static struct symbol *sym_check_sym_deps(struct symbol *sym)
{
	struct symbol *sym2;
	struct property *prop;
	struct dep_stack stack;

	dep_stack_insert(&stack, sym);

	stack.expr = &sym->dir_dep.expr;
	sym2 = sym_check_expr_deps(sym->dir_dep.expr);
	if (sym2)
		goto out;

	stack.expr = &sym->rev_dep.expr;
	sym2 = sym_check_expr_deps(sym->rev_dep.expr);
	if (sym2)
		goto out;

	stack.expr = &sym->implied.expr;
	sym2 = sym_check_expr_deps(sym->implied.expr);
	if (sym2)
		goto out;

	stack.expr = NULL;

	for (prop = sym->prop; prop; prop = prop->next) {
		if (prop->type == P_CHOICE || prop->type == P_SELECT ||
		    prop->type == P_IMPLY)
			continue;
		stack.prop = prop;
		sym2 = sym_check_expr_deps(prop->visible.expr);
		if (sym2)
			break;
		if (prop->type != P_DEFAULT || sym_is_choice(sym))
			continue;
		stack.expr = &prop->expr;
		sym2 = sym_check_expr_deps(prop->expr);
		if (sym2)
			break;
		stack.expr = NULL;
	}

out:
	dep_stack_remove();

	return sym2;
}

/**
 * sym_check_choice_deps - Check the dependencies of a choice symbol and its
 *                         associated symbols.
 *
 * This function is responsible for verifying the dependencies of a choice
 * symbol and its constituent symbols. It marks the choice symbol and its
 * associated symbols with the `SYMBOL_CHECK` and `SYMBOL_CHECKED` flags to
 * indicate that they are being processed. The function then recursively checks
 * the dependencies of the choice symbol and its associated symbols using
 * `sym_check_sym_deps`. If a dependency issue is detected, the function
 * returns the problematic symbol. If the problematic symbol is a choice value
 * and its parent choice symbol matches the input choice symbol, the function
 * returns the input choice symbol instead.
 *
 * @choice: The choice symbol whose dependencies are to be checked.
 * @return: Returns the problematic symbol if a dependency issue is found,
 *          otherwise returns NULL.
 */
static struct symbol *sym_check_choice_deps(struct symbol *choice)
{
	struct symbol *sym, *sym2;
	struct property *prop;
	struct expr *e;
	struct dep_stack stack;

	dep_stack_insert(&stack, choice);

	prop = sym_get_choice_prop(choice);
	expr_list_for_each_sym(prop->expr, e, sym)
		sym->flags |= (SYMBOL_CHECK | SYMBOL_CHECKED);

	choice->flags |= (SYMBOL_CHECK | SYMBOL_CHECKED);
	sym2 = sym_check_sym_deps(choice);
	choice->flags &= ~SYMBOL_CHECK;
	if (sym2)
		goto out;

	expr_list_for_each_sym(prop->expr, e, sym) {
		sym2 = sym_check_sym_deps(sym);
		if (sym2)
			break;
	}
out:
	expr_list_for_each_sym(prop->expr, e, sym)
		sym->flags &= ~SYMBOL_CHECK;

	if (sym2 && sym_is_choice_value(sym2) &&
	    prop_get_symbol(sym_get_choice_prop(sym2)) == choice)
		sym2 = choice;

	dep_stack_remove();

	return sym2;
}

/**
 * sym_check_deps - Recursively checks the dependencies of a symbol.
 *
 * This function performs a depth-first search to check the dependencies of the given symbol.
 * It ensures that the symbol and its dependencies are valid and consistent. The function
 * handles different types of symbols, including choice values and choice groups, by
 * delegating to specialized functions (`sym_check_choice_deps` and `sym_check_sym_deps`).
 *
 * The function uses flags (`SYMBOL_CHECK` and `SYMBOL_CHECKED`) to avoid infinite recursion
 * and to track the state of the symbol during the dependency check. If a circular dependency
 * is detected, it calls `sym_check_print_recursive` to report the issue.
 *
 * @param sym The symbol whose dependencies are to be checked.
 * @return Returns the symbol if a circular dependency is detected, otherwise returns NULL
 *         if the symbol and its dependencies are valid. For choice values, it returns the
 *         result of checking the main choice symbol.
 */
struct symbol *sym_check_deps(struct symbol *sym)
{
	struct symbol *sym2;
	struct property *prop;

	if (sym->flags & SYMBOL_CHECK) {
		sym_check_print_recursive(sym);
		return sym;
	}
	if (sym->flags & SYMBOL_CHECKED)
		return NULL;

	if (sym_is_choice_value(sym)) {
		struct dep_stack stack;

		/* for choice groups start the check with main choice symbol */
		dep_stack_insert(&stack, sym);
		prop = sym_get_choice_prop(sym);
		sym2 = sym_check_deps(prop_get_symbol(prop));
		dep_stack_remove();
	} else if (sym_is_choice(sym)) {
		sym2 = sym_check_choice_deps(sym);
	} else {
		sym->flags |= (SYMBOL_CHECK | SYMBOL_CHECKED);
		sym2 = sym_check_sym_deps(sym);
		sym->flags &= ~SYMBOL_CHECK;
	}

	return sym2;
}

/**
 * prop_get_symbol - Retrieves the symbol associated with a property.
 *
 * This function checks if the given property has an expression and if the expression
 * is either of type E_SYMBOL or E_LIST. If so, it returns the symbol associated with
 * the left side of the expression. If the property does not have a valid expression
 * or the expression type is not supported, the function returns NULL.
 *
 * @prop: A pointer to the property structure from which to retrieve the symbol.
 *
 * Return: A pointer to the symbol associated with the property's expression if valid,
 *         otherwise NULL.
 */
struct symbol *prop_get_symbol(struct property *prop)
{
	if (prop->expr && (prop->expr->type == E_SYMBOL ||
			   prop->expr->type == E_LIST))
		return prop->expr->left.sym;
	return NULL;
}

/**
 * @brief Returns the string representation of a given property type.
 *
 * This function takes an enumeration value of type `prop_type` and returns
 * a corresponding string that describes the property type. The returned
 * string is useful for logging, debugging, or displaying the type of a
 * property in a human-readable format.
 *
 * @param type The property type as an enumeration value of type `prop_type`.
 *
 * @return A pointer to a string that represents the property type. If the
 *         provided type is not recognized, the function returns "unknown".
 */
const char *prop_get_type_name(enum prop_type type)
{
	switch (type) {
	case P_PROMPT:
		return "prompt";
	case P_COMMENT:
		return "comment";
	case P_MENU:
		return "menu";
	case P_DEFAULT:
		return "default";
	case P_CHOICE:
		return "choice";
	case P_SELECT:
		return "select";
	case P_IMPLY:
		return "imply";
	case P_RANGE:
		return "range";
	case P_SYMBOL:
		return "symbol";
	case P_UNKNOWN:
		break;
	}
	return "unknown";
}
