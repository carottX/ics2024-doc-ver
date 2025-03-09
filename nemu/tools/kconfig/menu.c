// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2002 Roman Zippel <zippel@linux-m68k.org>
 */

#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "lkc.h"

static const char nohelp_text[] = "There is no help available for this option.";

struct menu rootmenu;
static struct menu **last_entry_ptr;

struct file *file_list;
struct file *current_file;

/**
 * Prints a warning message associated with a specific menu context.
 *
 * This function formats and prints a warning message to the standard error stream (stderr).
 * The message includes the file name and line number from the provided `menu` context,
 * followed by a custom warning message specified by the format string `fmt` and any
 * additional arguments.
 *
 * @param menu Pointer to the `menu` structure containing the file name and line number
 *             where the warning is being issued.
 * @param fmt  Format string for the warning message, similar to `printf` format.
 * @param ...  Additional arguments to be formatted into the warning message.
 *
 * Example usage:
 *

/**
 * Emits a warning message associated with a property, including the file name and line number
 * where the property is defined. The warning message is formatted using a printf-style format
 * string and variable arguments.
 *
 * @param prop Pointer to the `property` structure containing the file and line number information.
 * @param fmt  Format string for the warning message, similar to `printf`.
 * @param ...  Variable arguments to be formatted according to the format string.
 *
 * The function prints the warning message to the standard error stream (`stderr`) in the format:
 * `<filename>:<lineno>:warning: <formatted message>`, followed by a newline.
 */
static void prop_warn(struct property *prop, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	fprintf(stderr, "%s:%d:warning: ", prop->file->name, prop->lineno);
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");
	va_end(ap);
}

/**
 * Initializes the menu system by setting the current entry and current menu to the root menu.
 * This function also initializes the last_entry_ptr to point to the list of entries in the root menu.
 * It is typically called at the start of the program to set up the initial state of the menu system.
 */
void _menu_init(void)
{
	current_entry = current_menu = &rootmenu;
	last_entry_ptr = &rootmenu.list;
}

/**
 * Adds a new menu entry to the current menu structure.
 *
 * This function allocates memory for a new `struct menu` and initializes it with
 * the provided symbol (`sym`). The new menu entry is linked to the current menu
 * context, including its parent menu, file, and line number. The function also
 * updates the last entry pointer to maintain the linked list of menu entries.
 *
 * If a symbol (`sym`) is provided, it is added to the menu as a symbol entry
 * using `menu_add_symbol`.
 *
 * @param sym Pointer to the `struct symbol` to be associated with the new menu
 *            entry. If `NULL`, no symbol is added to the menu.
 */
void menu_add_entry(struct symbol *sym)
{
	struct menu *menu;

	menu = xmalloc(sizeof(*menu));
	memset(menu, 0, sizeof(*menu));
	menu->sym = sym;
	menu->parent = current_menu;
	menu->file = current_file;
	menu->lineno = zconf_lineno();

	*last_entry_ptr = menu;
	last_entry_ptr = &menu->next;
	current_entry = menu;
	if (sym)
		menu_add_symbol(P_SYMBOL, sym, NULL);
}

/**
 * Adds a new menu to the current menu structure and updates the necessary pointers.
 * 
 * This function sets the `last_entry_ptr` to the address of the list of the current entry,
 * updates the `current_menu` to point to the current entry, and then returns the updated
 * `current_menu`. This is typically used to build a hierarchical menu structure where
 * each menu can contain submenus or entries.
 * 
 * @return A pointer to the updated `current_menu` after adding the new menu.
 */
struct menu *menu_add_menu(void)
{
	last_entry_ptr = &current_entry->list;
	current_menu = current_entry;
	return current_menu;
}

/**
 * Ends the current menu and navigates back to its parent menu.
 * This function updates the `last_entry_ptr` to point to the `next` field of the
 * current menu, effectively marking the end of the current menu's entries. It then
 * sets the `current_menu` to its parent menu, allowing the user to navigate back
 * to the previous menu level. This is typically used in hierarchical menu systems
 * to manage menu traversal and structure.
 */
void menu_end_menu(void)
{
	last_entry_ptr = &current_menu->next;
	current_menu = current_menu->parent;
}

/*
 * Rewrites 'm' to 'm' && MODULES, so that it evaluates to 'n' when running
 * without modules
 */
static struct expr *rewrite_m(struct expr *e)
{
	if (!e)
		return e;

	switch (e->type) {
	case E_NOT:
		e->left.expr = rewrite_m(e->left.expr);
		break;
	case E_OR:
	case E_AND:
		e->left.expr = rewrite_m(e->left.expr);
		e->right.expr = rewrite_m(e->right.expr);
		break;
	case E_SYMBOL:
		/* change 'm' into 'm' && MODULES */
		if (e->left.sym == &symbol_mod)
			return expr_alloc_and(e, expr_alloc_symbol(modules_sym));
		break;
	default:
		break;
	}
	return e;
}

/**
 * Adds a dependency to the current menu entry by performing a logical AND operation
 * between the existing dependency and the new dependency. The result is stored
 * as the new dependency for the current entry.
 *
 * @param dep A pointer to the expression representing the new dependency to be added.
 *            This dependency will be combined with the existing dependency using
 *            a logical AND operation.
 *
 * @note This function modifies the dependency of the current menu entry. The caller
 *       is responsible for ensuring that the `dep` parameter is a valid expression.
 *       The `expr_alloc_and` function is used to allocate and combine the expressions.
 */
void menu_add_dep(struct expr *dep)
{
	current_entry->dep = expr_alloc_and(current_entry->dep, dep);
}

/**
 * Sets the type of the current menu entry's symbol to the specified type.
 * 
 * This function checks the current type of the symbol associated with the menu entry.
 * If the symbol's type is already the same as the specified type, the function returns
 * immediately without making any changes. If the symbol's type is `S_UNKNOWN`, the type
 * is updated to the specified type. If the symbol's type is different from the specified
 * type and is not `S_UNKNOWN`, a warning is issued indicating that the type redefinition
 * is being ignored. The warning includes the symbol's name (if available) and the current
 * and new types.
 * 
 * @param type The type to set for the symbol associated with the current menu entry.
 */
void menu_set_type(int type)
{
	struct symbol *sym = current_entry->sym;

	if (sym->type == type)
		return;
	if (sym->type == S_UNKNOWN) {
		sym->type = type;
		return;
	}
	menu_warn(current_entry,
		"ignoring type redefinition of '%s' from '%s' to '%s'",
		sym->name ? sym->name : "<choice>",
		sym_type_name(sym->type), sym_type_name(type));
}

/**
 * Adds a new property to the current menu entry in the configuration system.
 * The property is initialized with the specified type, expression, and dependency.
 * The property is then appended to the property list of the associated symbol.
 *
 * @param type The type of the property to be added (e.g., a configuration option).
 * @param expr The expression associated with the property, which defines its value or condition.
 * @param dep The dependency expression that determines the visibility of the property.
 *
 * @return A pointer to the newly created and initialized property.
 *
 * @note The property is allocated using `xmalloc` and initialized with zero values.
 *       The property's file, line number, and menu entry are set based on the current context.
 *       If the current menu entry has an associated symbol, the property is appended to the symbol's property list.
 */
static struct property *menu_add_prop(enum prop_type type, struct expr *expr,
				      struct expr *dep)
{
	struct property *prop;

	prop = xmalloc(sizeof(*prop));
	memset(prop, 0, sizeof(*prop));
	prop->type = type;
	prop->file = current_file;
	prop->lineno = zconf_lineno();
	prop->menu = current_entry;
	prop->expr = expr;
	prop->visible.expr = dep;

	/* append property to the prop list of symbol */
	if (current_entry->sym) {
		struct property **propp;

		for (propp = &current_entry->sym->prop;
		     *propp;
		     propp = &(*propp)->next)
			;
		*propp = prop;
	}

	return prop;
}

/**
 * Adds a prompt property to the current menu entry.
 *
 * This function creates a new property of the specified type and associates it
 * with the given prompt text. It handles leading whitespace in the prompt by
 * issuing a warning and skipping it. If the current menu entry already has a
 * prompt, a warning is issued about the redefinition.
 *
 * For properties of type P_PROMPT, the visibility of the prompt is determined
 * by combining the visibility expressions of all parent menus. This ensures
 * that the prompt respects the visibility conditions of its hierarchical
 * context. The visibility expressions of parent menus are copied to avoid
 * unintended side effects from expression reduction functions.
 *
 * @param type The type of the property to add (e.g., P_PROMPT).
 * @param prompt The text of the prompt. Leading whitespace is ignored.
 * @param dep An optional dependency expression for the property.
 *
 * @return A pointer to the newly created property.
 */
struct property *menu_add_prompt(enum prop_type type, char *prompt,
				 struct expr *dep)
{
	struct property *prop = menu_add_prop(type, NULL, dep);

	if (isspace(*prompt)) {
		prop_warn(prop, "leading whitespace ignored");
		while (isspace(*prompt))
			prompt++;
	}
	if (current_entry->prompt)
		prop_warn(prop, "prompt redefined");

	/* Apply all upper menus' visibilities to actual prompts. */
	if (type == P_PROMPT) {
		struct menu *menu = current_entry;

		while ((menu = menu->parent) != NULL) {
			struct expr *dup_expr;

			if (!menu->visibility)
				continue;
			/*
			 * Do not add a reference to the menu's visibility
			 * expression but use a copy of it. Otherwise the
			 * expression reduction functions will modify
			 * expressions that have multiple references which
			 * can cause unwanted side effects.
			 */
			dup_expr = expr_copy(menu->visibility);

			prop->visible.expr = expr_alloc_and(prop->visible.expr,
							    dup_expr);
		}
	}

	current_entry->prompt = prop;
	prop->text = prompt;

	return prop;
}

/**
 * Adds a visibility condition to the current menu entry by combining it with the provided expression.
 * The visibility condition is updated to be the logical AND of the current visibility condition and
 * the given expression. This ensures that the menu entry is only visible when both conditions are true.
 *
 * @param expr Pointer to the expression to be combined with the current visibility condition.
 *             The expression is typically a condition that determines visibility based on certain
 *             configuration options or other criteria.
 */
void menu_add_visibility(struct expr *expr)
{
	current_entry->visibility = expr_alloc_and(current_entry->visibility,
	    expr);
}

/**
 * Adds an expression to the menu based on the specified property type.
 * This function serves as a wrapper for `menu_add_prop`, forwarding the provided
 * arguments to it. It is used to associate an expression (`expr`) with a menu item,
 * optionally specifying a dependency (`dep`) that must be satisfied for the expression
 * to be active.
 *
 * @param type The type of property to add, as defined by the `prop_type` enumeration.
 * @param expr The expression to add to the menu. This represents the condition or
 *             configuration option being added.
 * @param dep  The dependency expression that must be satisfied for `expr` to be
 *             active. If NULL, no dependency is assumed.
 */
void menu_add_expr(enum prop_type type, struct expr *expr, struct expr *dep)
{
	menu_add_prop(type, expr, dep);
}

/**
 * Adds a symbol to the menu with the specified property type and dependency.
 *
 * This function creates a new property expression for the given symbol and adds it to the menu
 * using the `menu_add_prop` function. The property type determines how the symbol is treated
 * within the menu (e.g., as a boolean, tristate, etc.), and the dependency expression specifies
 * the conditions under which the symbol is visible or active.
 *
 * @param type The property type to associate with the symbol (e.g., P_PROMPT, P_DEFAULT).
 * @param sym  The symbol to add to the menu. Must be a valid symbol pointer.
 * @param dep  The dependency expression that controls the visibility or activation of the symbol.
 *             Can be NULL if no dependency is required.
 */
void menu_add_symbol(enum prop_type type, struct symbol *sym, struct expr *dep)
{
	menu_add_prop(type, expr_alloc_symbol(sym), dep);
}

/**
 * Adds the 'modules' option to the menu configuration. This function checks if the 'modules' option
 * has already been defined by another symbol. If it has, an error is raised indicating that the
 * 'modules' option is being redefined by the current symbol. If the 'modules' option has not been
 * defined yet, the current symbol is assigned as the defining symbol for the 'modules' option.
 *
 * This function is typically used in the context of a configuration system where symbols represent
 * configurable options, and the 'modules' option is a special case that can only be defined once.
 *
 * @note This function assumes that `modules_sym` and `current_entry` are properly initialized
 *       before it is called. If `modules_sym` is already set, it indicates that the 'modules'
 *       option has been previously defined.
 *
 * @throws zconf_error If the 'modules' option is being redefined by the current symbol.
 */
void menu_add_option_modules(void)
{
	if (modules_sym)
		zconf_error("symbol '%s' redefines option 'modules' already defined by symbol '%s'",
			    current_entry->sym->name, modules_sym->name);
	modules_sym = current_entry->sym;
}

/**
 * Adds the current symbol to the defconfig list if it is not already defined.
 * If the defconfig list is not yet initialized, the current symbol is set as
 * the defconfig symbol. If the defconfig list is already initialized and the
 * current symbol is different from the existing defconfig symbol, an error is
 * raised to prevent redefinition. Additionally, the defconfig symbol is marked
 * as non-writable to prevent further modifications.
 *
 * @note This function assumes that `sym_defconfig_list` and `current_entry->sym`
 *       are valid pointers to symbol structures.
 *
 * @throws zconf_error If an attempt is made to redefine the defconfig symbol.
 */
void menu_add_option_defconfig_list(void)
{
	if (!sym_defconfig_list)
		sym_defconfig_list = current_entry->sym;
	else if (sym_defconfig_list != current_entry->sym)
		zconf_error("trying to redefine defconfig symbol");
	sym_defconfig_list->flags |= SYMBOL_NO_WRITE;
}

/**
 * Marks the current symbol in the menu configuration with the ALLNOCONFIG_Y flag.
 * This flag indicates that the symbol should be automatically set to 'y' (yes) when
 * the 'allnoconfig' option is used during kernel configuration. This is typically
 * used for essential options that should always be enabled, even in a minimal
 * configuration.
 */
void menu_add_option_allnoconfig_y(void)
{
	current_entry->sym->flags |= SYMBOL_ALLNOCONFIG_Y;
}

/**
 * Validates whether the given symbol `sym2` represents a valid number or can be interpreted as one.
 * 
 * This function checks if `sym2` is of type `S_INT` or `S_HEX`, which are directly recognized as valid numbers.
 * If `sym2` is of type `S_UNKNOWN`, it further validates whether the symbol's name can be interpreted as a valid
 * string representation of a number by calling `sym_string_valid` with `sym` and `sym2->name`.
 *
 * @param sym  A pointer to a `symbol` structure, typically used as context for validation.
 * @param sym2 A pointer to the `symbol` structure to be validated.
 * @return     Returns 1 if `sym2` is a valid number or can be interpreted as one, otherwise returns 0.
 */
static int menu_validate_number(struct symbol *sym, struct symbol *sym2)
{
	return sym2->type == S_INT || sym2->type == S_HEX ||
	       (sym2->type == S_UNKNOWN && sym_string_valid(sym, sym2->name));
}

/**
 * sym_check_prop - Validate the properties associated with a symbol.
 *
 * This function iterates over all properties of the given symbol and performs
 * type-specific validation checks based on the property type. It ensures that
 * the properties conform to the expected rules for the symbol's type. 
 *
 * For example:
 * - For `P_DEFAULT` properties, it checks that the default value is a single
 *   symbol and validates its correctness based on the symbol's type (e.g., 
 *   string, integer, or hexadecimal). It also ensures that choice defaults
 *   are valid and contained within the choice.
 * - For `P_SELECT` and `P_IMPLY` properties, it verifies that the symbol and
 *   the target symbol are of boolean or tristate type.
 * - For `P_RANGE` properties, it ensures that the symbol is of integer or
 *   hexadecimal type and validates the range bounds.
 *
 * If any property fails validation, a warning is issued using `prop_warn`.
 *
 * @sym: The symbol whose properties are to be validated.
 */
static void sym_check_prop(struct symbol *sym)
{
	struct property *prop;
	struct symbol *sym2;
	char *use;

	for (prop = sym->prop; prop; prop = prop->next) {
		switch (prop->type) {
		case P_DEFAULT:
			if ((sym->type == S_STRING || sym->type == S_INT || sym->type == S_HEX) &&
			    prop->expr->type != E_SYMBOL)
				prop_warn(prop,
				    "default for config symbol '%s'"
				    " must be a single symbol", sym->name);
			if (prop->expr->type != E_SYMBOL)
				break;
			sym2 = prop_get_symbol(prop);
			if (sym->type == S_HEX || sym->type == S_INT) {
				if (!menu_validate_number(sym, sym2))
					prop_warn(prop,
					    "'%s': number is invalid",
					    sym->name);
			}
			if (sym_is_choice(sym)) {
				struct property *choice_prop =
					sym_get_choice_prop(sym2);

				if (!choice_prop ||
				    prop_get_symbol(choice_prop) != sym)
					prop_warn(prop,
						  "choice default symbol '%s' is not contained in the choice",
						  sym2->name);
			}
			break;
		case P_SELECT:
		case P_IMPLY:
			use = prop->type == P_SELECT ? "select" : "imply";
			sym2 = prop_get_symbol(prop);
			if (sym->type != S_BOOLEAN && sym->type != S_TRISTATE)
				prop_warn(prop,
				    "config symbol '%s' uses %s, but is "
				    "not bool or tristate", sym->name, use);
			else if (sym2->type != S_UNKNOWN &&
				 sym2->type != S_BOOLEAN &&
				 sym2->type != S_TRISTATE)
				prop_warn(prop,
				    "'%s' has wrong type. '%s' only "
				    "accept arguments of bool and "
				    "tristate type", sym2->name, use);
			break;
		case P_RANGE:
			if (sym->type != S_INT && sym->type != S_HEX)
				prop_warn(prop, "range is only allowed "
						"for int or hex symbols");
			if (!menu_validate_number(sym, prop->expr->left.sym) ||
			    !menu_validate_number(sym, prop->expr->right.sym))
				prop_warn(prop, "range is invalid");
			break;
		default:
			;
		}
	}
}

/**
 * Finalizes the processing of a menu node and its children in a configuration menu tree.
 *
 * This function handles the following tasks:
 * - Recursively processes child menu nodes, propagating parent dependencies and simplifying expressions.
 * - Handles special cases for choice symbols, including setting types and managing dependencies.
 * - Propagates parent dependencies to properties and their conditions, rewriting and simplifying expressions.
 * - Manages `select` and `implies` properties, updating the dependencies of the selected/implied symbols.
 * - Automatically creates submenus for symbols with consecutive dependent items.
 * - Flattens 'if' blocks and undoes automatic submenus created for promptless symbols.
 * - Issues warnings for undefined symbols and choice-related issues.
 * - Adds reverse dependencies for non-optional choices to prevent setting the choice mode to 'n' when visible.
 *
 * @param parent The parent menu node to finalize. This node and its children will be processed.
 */
void menu_finalize(struct menu *parent)
{
	struct menu *menu, *last_menu;
	struct symbol *sym;
	struct property *prop;
	struct expr *parentdep, *basedep, *dep, *dep2, **ep;

	sym = parent->sym;
	if (parent->list) {
		/*
		 * This menu node has children. We (recursively) process them
		 * and propagate parent dependencies before moving on.
		 */

		if (sym && sym_is_choice(sym)) {
			if (sym->type == S_UNKNOWN) {
				/* find the first choice value to find out choice type */
				current_entry = parent;
				for (menu = parent->list; menu; menu = menu->next) {
					if (menu->sym && menu->sym->type != S_UNKNOWN) {
						menu_set_type(menu->sym->type);
						break;
					}
				}
			}
			/* set the type of the remaining choice values */
			for (menu = parent->list; menu; menu = menu->next) {
				current_entry = menu;
				if (menu->sym && menu->sym->type == S_UNKNOWN)
					menu_set_type(sym->type);
			}

			/*
			 * Use the choice itself as the parent dependency of
			 * the contained items. This turns the mode of the
			 * choice into an upper bound on the visibility of the
			 * choice value symbols.
			 */
			parentdep = expr_alloc_symbol(sym);
		} else {
			/* Menu node for 'menu', 'if' */
			parentdep = parent->dep;
		}

		/* For each child menu node... */
		for (menu = parent->list; menu; menu = menu->next) {
			/*
			 * Propagate parent dependencies to the child menu
			 * node, also rewriting and simplifying expressions
			 */
			basedep = rewrite_m(menu->dep);
			basedep = expr_transform(basedep);
			basedep = expr_alloc_and(expr_copy(parentdep), basedep);
			basedep = expr_eliminate_dups(basedep);
			menu->dep = basedep;

			if (menu->sym)
				/*
				 * Note: For symbols, all prompts are included
				 * too in the symbol's own property list
				 */
				prop = menu->sym->prop;
			else
				/*
				 * For non-symbol menu nodes, we just need to
				 * handle the prompt
				 */
				prop = menu->prompt;

			/* For each property... */
			for (; prop; prop = prop->next) {
				if (prop->menu != menu)
					/*
					 * Two possibilities:
					 *
					 * 1. The property lacks dependencies
					 *    and so isn't location-specific,
					 *    e.g. an 'option'
					 *
					 * 2. The property belongs to a symbol
					 *    defined in multiple locations and
					 *    is from some other location. It
					 *    will be handled there in that
					 *    case.
					 *
					 * Skip the property.
					 */
					continue;

				/*
				 * Propagate parent dependencies to the
				 * property's condition, rewriting and
				 * simplifying expressions at the same time
				 */
				dep = rewrite_m(prop->visible.expr);
				dep = expr_transform(dep);
				dep = expr_alloc_and(expr_copy(basedep), dep);
				dep = expr_eliminate_dups(dep);
				if (menu->sym && menu->sym->type != S_TRISTATE)
					dep = expr_trans_bool(dep);
				prop->visible.expr = dep;

				/*
				 * Handle selects and implies, which modify the
				 * dependencies of the selected/implied symbol
				 */
				if (prop->type == P_SELECT) {
					struct symbol *es = prop_get_symbol(prop);
					es->rev_dep.expr = expr_alloc_or(es->rev_dep.expr,
							expr_alloc_and(expr_alloc_symbol(menu->sym), expr_copy(dep)));
				} else if (prop->type == P_IMPLY) {
					struct symbol *es = prop_get_symbol(prop);
					es->implied.expr = expr_alloc_or(es->implied.expr,
							expr_alloc_and(expr_alloc_symbol(menu->sym), expr_copy(dep)));
				}
			}
		}

		if (sym && sym_is_choice(sym))
			expr_free(parentdep);

		/*
		 * Recursively process children in the same fashion before
		 * moving on
		 */
		for (menu = parent->list; menu; menu = menu->next)
			menu_finalize(menu);
	} else if (sym) {
		/*
		 * Automatic submenu creation. If sym is a symbol and A, B, C,
		 * ... are consecutive items (symbols, menus, ifs, etc.) that
		 * all depend on sym, then the following menu structure is
		 * created:
		 *
		 *	sym
		 *	 +-A
		 *	 +-B
		 *	 +-C
		 *	 ...
		 *
		 * This also works recursively, giving the following structure
		 * if A is a symbol and B depends on A:
		 *
		 *	sym
		 *	 +-A
		 *	 | +-B
		 *	 +-C
		 *	 ...
		 */

		basedep = parent->prompt ? parent->prompt->visible.expr : NULL;
		basedep = expr_trans_compare(basedep, E_UNEQUAL, &symbol_no);
		basedep = expr_eliminate_dups(expr_transform(basedep));

		/* Examine consecutive elements after sym */
		last_menu = NULL;
		for (menu = parent->next; menu; menu = menu->next) {
			dep = menu->prompt ? menu->prompt->visible.expr : menu->dep;
			if (!expr_contains_symbol(dep, sym)
				/* No dependency, quit */
				break;
			if (expr_depends_symbol(dep, sym))
				/* Absolute dependency, put in submenu */
				goto next;

			/*
			 * Also consider it a dependency on sym if our
			 * dependencies contain sym and are a "superset" of
			 * sym's dependencies, e.g. '(sym || Q) && R' when sym
			 * depends on R.
			 *
			 * Note that 'R' might be from an enclosing menu or if,
			 * making this a more common case than it might seem.
			 */
			dep = expr_trans_compare(dep, E_UNEQUAL, &symbol_no);
			dep = expr_eliminate_dups(expr_transform(dep));
			dep2 = expr_copy(basedep);
			expr_eliminate_eq(&dep, &dep2);
			expr_free(dep);
			if (!expr_is_yes(dep2)) {
				/* Not superset, quit */
				expr_free(dep2);
				break;
			}
			/* Superset, put in submenu */
			expr_free(dep2);
		next:
			menu_finalize(menu);
			menu->parent = parent;
			last_menu = menu;
		}
		expr_free(basedep);
		if (last_menu) {
			parent->list = parent->next;
			parent->next = last_menu->next;
			last_menu->next = NULL;
		}

		sym->dir_dep.expr = expr_alloc_or(sym->dir_dep.expr, parent->dep);
	}
	for (menu = parent->list; menu; menu = menu->next) {
		if (sym && sym_is_choice(sym) &&
		    menu->sym && !sym_is_choice_value(menu->sym)) {
			current_entry = menu;
			menu->sym->flags |= SYMBOL_CHOICEVAL;
			if (!menu->prompt)
				menu_warn(menu, "choice value must have a prompt");
			for (prop = menu->sym->prop; prop; prop = prop->next) {
				if (prop->type == P_DEFAULT)
					prop_warn(prop, "defaults for choice "
						  "values not supported");
				if (prop->menu == menu)
					continue;
				if (prop->type == P_PROMPT &&
				    prop->menu->parent->sym != sym)
					prop_warn(prop, "choice value used outside its choice group");
			}
			/* Non-tristate choice values of tristate choices must
			 * depend on the choice being set to Y. The choice
			 * values' dependencies were propagated to their
			 * properties above, so the change here must be re-
			 * propagated.
			 */
			if (sym->type == S_TRISTATE && menu->sym->type != S_TRISTATE) {
				basedep = expr_alloc_comp(E_EQUAL, sym, &symbol_yes);
				menu->dep = expr_alloc_and(basedep, menu->dep);
				for (prop = menu->sym->prop; prop; prop = prop->next) {
					if (prop->menu != menu)
						continue;
					prop->visible.expr = expr_alloc_and(expr_copy(basedep),
									    prop->visible.expr);
				}
			}
			menu_add_symbol(P_CHOICE, sym, NULL);
			prop = sym_get_choice_prop(sym);
			for (ep = &prop->expr; *ep; ep = &(*ep)->left.expr)
				;
			*ep = expr_alloc_one(E_LIST, NULL);
			(*ep)->right.sym = menu->sym;
		}

		/*
		 * This code serves two purposes:
		 *
		 * (1) Flattening 'if' blocks, which do not specify a submenu
		 *     and only add dependencies.
		 *
		 *     (Automatic submenu creation might still create a submenu
		 *     from an 'if' before this code runs.)
		 *
		 * (2) "Undoing" any automatic submenus created earlier below
		 *     promptless symbols.
		 *
		 * Before:
		 *
		 *	A
		 *	if ... (or promptless symbol)
		 *	 +-B
		 *	 +-C
		 *	D
		 *
		 * After:
		 *
		 *	A
		 *	if ... (or promptless symbol)
		 *	B
		 *	C
		 *	D
		 */
		if (menu->list && (!menu->prompt || !menu->prompt->text)) {
			for (last_menu = menu->list; ; last_menu = last_menu->next) {
				last_menu->parent = parent;
				if (!last_menu->next)
					break;
			}
			last_menu->next = menu->next;
			menu->next = menu->list;
			menu->list = NULL;
		}
	}

	if (sym && !(sym->flags & SYMBOL_WARNED)) {
		if (sym->type == S_UNKNOWN)
			menu_warn(parent, "config symbol defined without type");

		if (sym_is_choice(sym) && !parent->prompt)
			menu_warn(parent, "choice must have a prompt");

		/* Check properties connected to this symbol */
		sym_check_prop(sym);
		sym->flags |= SYMBOL_WARNED;
	}

	/*
	 * For non-optional choices, add a reverse dependency (corresponding to
	 * a select) of '<visibility> && m'. This prevents the user from
	 * setting the choice mode to 'n' when the choice is visible.
	 *
	 * This would also work for non-choice symbols, but only non-optional
	 * choices clear SYMBOL_OPTIONAL as of writing. Choices are implemented
	 * as a type of symbol.
	 */
	if (sym && !sym_is_optional(sym) && parent->prompt) {
		sym->rev_dep.expr = expr_alloc_or(sym->rev_dep.expr,
				expr_alloc_and(parent->prompt->visible.expr,
					expr_alloc_symbol(&symbol_mod)));
	}
}

/**
 * Checks whether the given menu has a prompt.
 *
 * This function verifies if the provided menu structure contains a valid prompt.
 * A prompt is considered valid if the `prompt` field of the menu is not NULL.
 *
 * @param menu A pointer to the menu structure to be checked.
 * @return `true` if the menu has a valid prompt, `false` otherwise.
 */
bool menu_has_prompt(struct menu *menu)
{
	if (!menu->prompt)
		return false;
	return true;
}

/*
 * Determine if a menu is empty.
 * A menu is considered empty if it contains no or only
 * invisible entries.
 */
bool menu_is_empty(struct menu *menu)
{
	struct menu *child;

	for (child = menu->list; child; child = child->next) {
		if (menu_is_visible(child))
			return(false);
	}
	return(true);
}

/**
 * Determines whether a given menu is visible based on its properties and child menus.
 *
 * This function checks the visibility of a menu by evaluating its prompt, visibility expression,
 * and associated symbol. If the menu has a visibility expression, it is evaluated to determine
 * if the menu should be visible. The function also considers the visibility of child menus.
 * If any child menu is visible, the parent menu is also considered visible.
 *
 * @param menu A pointer to the menu structure to be evaluated.
 * @return true if the menu is visible, false otherwise.
 */
bool menu_is_visible(struct menu *menu)
{
	struct menu *child;
	struct symbol *sym;
	tristate visible;

	if (!menu->prompt)
		return false;

	if (menu->visibility) {
		if (expr_calc_value(menu->visibility) == no)
			return false;
	}

	sym = menu->sym;
	if (sym) {
		sym_calc_value(sym);
		visible = menu->prompt->visible.tri;
	} else
		visible = menu->prompt->visible.tri = expr_calc_value(menu->prompt->visible.expr);

	if (visible != no)
		return true;

	if (!sym || sym_get_tristate_value(menu->sym) == no)
		return false;

	for (child = menu->list; child; child = child->next) {
		if (menu_is_visible(child)) {
			if (sym)
				sym->flags |= SYMBOL_DEF_USER;
			return true;
		}
	}

	return false;
}

/**
 * Retrieves the prompt text associated with the given menu.
 *
 * This function returns the prompt text of the menu if it exists. If the menu does not have a prompt,
 * it attempts to return the name of the associated symbol (if any). If neither a prompt nor a symbol
 * is available, the function returns NULL.
 *
 * @param menu A pointer to the menu structure from which to retrieve the prompt.
 * @return A pointer to the prompt text, the symbol name, or NULL if neither is available.
 */
const char *menu_get_prompt(struct menu *menu)
{
	if (menu->prompt)
		return menu->prompt->text;
	else if (menu->sym)
		return menu->sym->name;
	return NULL;
}

/**
 * @brief Retrieves the root menu structure.
 *
 * This function returns a pointer to the root menu structure, which is the top-level
 * menu in the menu hierarchy. The root menu serves as the starting point for navigating
 * through the menu system.
 *
 * @param menu A pointer to a menu structure. This parameter is not used in the function
 *             but is included for potential future extensibility.
 *
 * @return A pointer to the root menu structure.
 */
struct menu *menu_get_root_menu(struct menu *menu)
{
	return &rootmenu;
}

/**
 * Retrieves the parent menu of the given menu that is of type P_MENU.
 * 
 * This function traverses up the menu hierarchy starting from the specified menu.
 * It checks each parent menu to determine if it is of type P_MENU. The traversal
 * stops when a parent menu of type P_MENU is found or when the root menu is reached.
 * 
 * @param menu A pointer to the menu structure whose parent menu is to be retrieved.
 * @return A pointer to the parent menu of type P_MENU. If no such parent menu is found,
 *         the root menu is returned.
 */
struct menu *menu_get_parent_menu(struct menu *menu)
{
	enum prop_type type;

	for (; menu != &rootmenu; menu = menu->parent) {
		type = menu->prompt ? menu->prompt->type : 0;
		if (type == P_MENU)
			break;
	}
	return menu;
}

/**
 * @brief Checks if a given menu has a help text associated with it.
 *
 * This function examines the `help` field of the provided `menu` structure
 * to determine if it contains a non-NULL pointer. If the `help` field is not NULL,
 * it indicates that the menu has associated help text.
 *
 * @param menu A pointer to the `menu` structure to be checked.
 * @return `true` if the menu has help text (i.e., `menu->help` is not NULL),
 *         `false` otherwise.
 */
bool menu_has_help(struct menu *menu)
{
	return menu->help != NULL;
}

/**
 * Retrieves the help text associated with the given menu.
 *
 * This function checks if the provided menu structure has a non-null `help` field.
 * If the `help` field is not null, it returns the associated help text. Otherwise,
 * it returns an empty string, indicating that no help text is available for the menu.
 *
 * @param menu A pointer to the menu structure whose help text is to be retrieved.
 * @return A pointer to the help text string if available, otherwise an empty string.
 */
const char *menu_get_help(struct menu *menu)
{
	if (menu->help)
		return menu->help;
	else
		return "";
}

/**
 * get_def_str - Appends the definition location of a menu to a string.
 * @r: Pointer to the struct gstr where the formatted string will be appended.
 * @menu: Pointer to the struct menu containing the definition details.
 *
 * This function formats a string that describes the location where a menu
 * is defined, including the file name and line number. The formatted string
 * is appended to the string buffer pointed to by @r.
 *
 * The format of the appended string is: "Defined at <file>:<line>\n"
 * where <file> is the name of the file and <line> is the line number.
 */
static void get_def_str(struct gstr *r, struct menu *menu)
{
	str_printf(r, "Defined at %s:%d\n",
		   menu->file->name, menu->lineno);
}

/**
 * Appends a dependency string representation of the given expression to the provided string buffer.
 * If the expression is not a "yes" (i.e., it represents a dependency), the function appends the
 * specified prefix followed by the string representation of the expression, and then adds a newline.
 *
 * @param r A pointer to the string buffer (`struct gstr`) where the output will be appended.
 * @param expr A pointer to the expression (`struct expr`) to be evaluated and printed.
 * @param prefix A string to be prepended to the expression's representation if it is not a "yes".
 */
static void get_dep_str(struct gstr *r, struct expr *expr, const char *prefix)
{
	if (!expr_is_yes(expr)) {
		str_append(r, prefix);
		expr_gstr_print(expr, r);
		str_append(r, "\n");
	}
}

/**
 * get_prompt_str - Generates a formatted string representation of a prompt and its associated properties.
 *
 * This function constructs a detailed string representation of a prompt, including its text, dependencies,
 * visibility conditions, and location within a menu hierarchy. The generated string is stored in the provided
 * `gstr` structure.
 *
 * The function first prints the prompt text. It then checks and prints the dependencies and visibility conditions
 * if they differ. The function traverses the menu hierarchy to determine the location of the prompt, printing
 * the hierarchy in a structured format. If a `head` list is provided, the function also creates a `jump_key`
 * structure to facilitate navigation within the menu hierarchy.
 *
 * @param r Pointer to the `gstr` structure where the formatted string will be stored.
 * @param prop Pointer to the `property` structure representing the prompt.
 * @param head Pointer to the head of a list of `jump_key` structures. If non-NULL, a new `jump_key` will be
 *             added to this list to represent the prompt's location in the menu hierarchy.
 */
static void get_prompt_str(struct gstr *r, struct property *prop,
			   struct list_head *head)
{
	int i, j;
	struct menu *submenu[8], *menu, *location = NULL;
	struct jump_key *jump = NULL;

	str_printf(r, "  Prompt: %s\n", prop->text);

	get_dep_str(r, prop->menu->dep, "  Depends on: ");
	/*
	 * Most prompts in Linux have visibility that exactly matches their
	 * dependencies. For these, we print only the dependencies to improve
	 * readability. However, prompts with inline "if" expressions and
	 * prompts with a parent that has a "visible if" expression have
	 * differing dependencies and visibility. In these rare cases, we
	 * print both.
	 */
	if (!expr_eq(prop->menu->dep, prop->visible.expr))
		get_dep_str(r, prop->visible.expr, "  Visible if: ");

	menu = prop->menu->parent;
	for (i = 0; menu != &rootmenu && i < 8; menu = menu->parent) {
		bool accessible = menu_is_visible(menu);

		submenu[i++] = menu;
		if (location == NULL && accessible)
			location = menu;
	}
	if (head && location) {
		jump = xmalloc(sizeof(struct jump_key));

		if (menu_is_visible(prop->menu)) {
			/*
			 * There is not enough room to put the hint at the
			 * beginning of the "Prompt" line. Put the hint on the
			 * last "Location" line even when it would belong on
			 * the former.
			 */
			jump->target = prop->menu;
		} else
			jump->target = location;

		if (list_empty(head))
			jump->index = 0;
		else
			jump->index = list_entry(head->prev, struct jump_key,
						 entries)->index + 1;

		list_add_tail(&jump->entries, head);
	}

	if (i > 0) {
		str_printf(r, "  Location:\n");
		for (j = 4; --i >= 0; j += 2) {
			menu = submenu[i];
			if (jump && menu == location)
				jump->offset = strlen(r->s);
			str_printf(r, "%*c-> %s", j, ' ',
				   menu_get_prompt(menu));
			if (menu->sym) {
				str_printf(r, " (%s [=%s])", menu->sym->name ?
					menu->sym->name : "<choice>",
					sym_get_string_value(menu->sym));
			}
			str_append(r, "\n");
		}
	}
}

/**
 * @brief Retrieves and appends the properties of a symbol as a formatted string.
 *
 * This method iterates over all properties of the given symbol that match the specified
 * property type (`tok`). For each matching property, it appends the property's expression
 * to the result string (`r`). The first property is prefixed with the provided `prefix`,
 * and subsequent properties are separated by " && ". If any properties are found, a newline
 * character is appended at the end.
 *
 * @param r Pointer to the `gstr` structure where the result string will be stored.
 * @param sym Pointer to the `symbol` structure whose properties are to be retrieved.
 * @param tok The type of property to filter and retrieve.
 * @param prefix The string to prepend to the first property's expression.
 */
static void get_symbol_props_str(struct gstr *r, struct symbol *sym,
				 enum prop_type tok, const char *prefix)
{
	bool hit = false;
	struct property *prop;

	for_all_properties(sym, prop, tok) {
		if (!hit) {
			str_append(r, prefix);
			hit = true;
		} else
			str_printf(r, " && ");
		expr_gstr_print(prop->expr, r);
	}
	if (hit)
		str_append(r, "\n");
}

/*
 * head is optional and may be NULL
 */
static void get_symbol_str(struct gstr *r, struct symbol *sym,
		    struct list_head *head)
{
	struct property *prop;

	if (sym && sym->name) {
		str_printf(r, "Symbol: %s [=%s]\n", sym->name,
			   sym_get_string_value(sym));
		str_printf(r, "Type  : %s\n", sym_type_name(sym->type));
		if (sym->type == S_INT || sym->type == S_HEX) {
			prop = sym_get_range_prop(sym);
			if (prop) {
				str_printf(r, "Range : ");
				expr_gstr_print(prop->expr, r);
				str_append(r, "\n");
			}
		}
	}

	/* Print the definitions with prompts before the ones without */
	for_all_properties(sym, prop, P_SYMBOL) {
		if (prop->menu->prompt) {
			get_def_str(r, prop->menu);
			get_prompt_str(r, prop->menu->prompt, head);
		}
	}

	for_all_properties(sym, prop, P_SYMBOL) {
		if (!prop->menu->prompt) {
			get_def_str(r, prop->menu);
			get_dep_str(r, prop->menu->dep, "  Depends on: ");
		}
	}

	get_symbol_props_str(r, sym, P_SELECT, "Selects: ");
	if (sym->rev_dep.expr) {
		expr_gstr_print_revdep(sym->rev_dep.expr, r, yes, "Selected by [y]:\n");
		expr_gstr_print_revdep(sym->rev_dep.expr, r, mod, "Selected by [m]:\n");
		expr_gstr_print_revdep(sym->rev_dep.expr, r, no, "Selected by [n]:\n");
	}

	get_symbol_props_str(r, sym, P_IMPLY, "Implies: ");
	if (sym->implied.expr) {
		expr_gstr_print_revdep(sym->implied.expr, r, yes, "Implied by [y]:\n");
		expr_gstr_print_revdep(sym->implied.expr, r, mod, "Implied by [m]:\n");
		expr_gstr_print_revdep(sym->implied.expr, r, no, "Implied by [n]:\n");
	}

	str_append(r, "\n\n");
}

/**
 * get_relations_str - Generates a string representation of the relations between symbols.
 *
 * This function iterates over an array of symbols (`sym_arr`) and appends their string
 * representations to a dynamically allocated string (`res`). The string representation
 * of each symbol is generated using the `get_symbol_str` function. If the symbol array
 * is empty or NULL, the function appends "No matches found." to the result string.
 *
 * @sym_arr: An array of pointers to `struct symbol`. The array is terminated by a NULL pointer.
 * @head: A pointer to the head of a list (likely used in `get_symbol_str` for context).
 *
 * Return: A `struct gstr` containing the concatenated string representations of the symbols
 *         or the "No matches found." message if the array is empty.
 */
struct gstr get_relations_str(struct symbol **sym_arr, struct list_head *head)
{
	struct symbol *sym;
	struct gstr res = str_new();
	int i;

	for (i = 0; sym_arr && (sym = sym_arr[i]); i++)
		get_symbol_str(&res, sym, head);
	if (!i)
		str_append(&res, "No matches found.\n");
	return res;
}


/**
 * menu_get_ext_help - Retrieves and formats extended help information for a menu item.
 *
 * This function gathers help text associated with a given menu item and appends it to a
 * provided string buffer (`help`). If the menu item has a symbol associated with it, the
 * symbol's name is prefixed with `CONFIG_` and added to the help text. The function then
 * retrieves the specific help text for the menu item and appends it to the buffer. If the
 * menu item has a symbol, additional symbol-related information is appended using
 * `get_symbol_str`.
 *
 * @param menu Pointer to the menu structure for which help information is retrieved.
 * @param help Pointer to the string buffer (`gstr`) where the formatted help text is stored.
 */
void menu_get_ext_help(struct menu *menu, struct gstr *help)
{
	struct symbol *sym = menu->sym;
	const char *help_text = nohelp_text;

	if (menu_has_help(menu)) {
		if (sym->name)
			str_printf(help, "%s%s:\n\n", CONFIG_, sym->name);
		help_text = menu_get_help(menu);
	}
	str_printf(help, "%s\n", help_text);
	if (sym)
		get_symbol_str(help, sym, NULL);
}
