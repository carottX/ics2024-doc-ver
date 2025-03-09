/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2002 Roman Zippel <zippel@linux-m68k.org>
 */

#ifndef LKC_H
#define LKC_H

#include "expr.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "lkc_proto.h"

#define SRCTREE "srctree"

#ifndef PACKAGE
#define PACKAGE "linux"
#endif

#ifndef CONFIG_
#define CONFIG_ "CONFIG_"
#endif
/**
 * Retrieves the prefix value for configuration settings.
 *
 * This method attempts to fetch the prefix from the environment variable "CONFIG_".
 * If the environment variable is not set or is empty, it falls back to the default
 * value defined by the macro `CONFIG_`.
 *
 * @return A pointer to a string containing the configuration prefix. This string
 *         is either the value of the "CONFIG_" environment variable or the default
 *         `CONFIG_` value if the environment variable is not set.
 */
static inline const char *CONFIG_prefix(void)
{
    return getenv( "CONFIG_" ) ?: CONFIG_;
}
#undef CONFIG_
#define CONFIG_ CONFIG_prefix()

enum conf_def_mode {
	def_default,
	def_yes,
	def_mod,
	def_y2m,
	def_m2y,
	def_no,
	def_random
};

extern int yylineno;
void zconfdump(FILE *out);
void zconf_starthelp(void);
FILE *zconf_fopen(const char *name);
void zconf_initscan(const char *name);
void zconf_nextfile(const char *name);
int zconf_lineno(void);
const char *zconf_curname(void);

/* confdata.c */
const char *conf_get_configname(void);
void sym_set_change_count(int count);
void sym_add_change_count(int count);
bool conf_set_all_new_symbols(enum conf_def_mode mode);
void conf_rewrite_mod_or_yes(enum conf_def_mode mode);
void set_all_choice_values(struct symbol *csym);

/* confdata.c and expr.c */
static inline void xfwrite(const void *str, size_t len, size_t count, FILE *out)
{
	assert(len != 0);

	if (fwrite(str, len, count, out) != count)
		fprintf(stderr, "Error in writing or end of file.\n");
}

/* util.c */
struct file *file_lookup(const char *name);
void *xmalloc(size_t size);
void *xcalloc(size_t nmemb, size_t size);
void *xrealloc(void *p, size_t size);
char *xstrdup(const char *s);
char *xstrndup(const char *s, size_t n);

/* lexer.l */
int yylex(void);

struct gstr {
	size_t len;
	char  *s;
	/*
	* when max_width is not zero long lines in string s (if any) get
	* wrapped not to exceed the max_width value
	*/
	int max_width;
};
struct gstr str_new(void);
void str_free(struct gstr *gs);
void str_append(struct gstr *gs, const char *s);
void str_printf(struct gstr *gs, const char *fmt, ...);
const char *str_get(struct gstr *gs);

/* menu.c */
void _menu_init(void);
void menu_warn(struct menu *menu, const char *fmt, ...);
struct menu *menu_add_menu(void);
void menu_end_menu(void);
void menu_add_entry(struct symbol *sym);
void menu_add_dep(struct expr *dep);
void menu_add_visibility(struct expr *dep);
struct property *menu_add_prompt(enum prop_type type, char *prompt, struct expr *dep);
void menu_add_expr(enum prop_type type, struct expr *expr, struct expr *dep);
void menu_add_symbol(enum prop_type type, struct symbol *sym, struct expr *dep);
void menu_add_option_modules(void);
void menu_add_option_defconfig_list(void);
void menu_add_option_allnoconfig_y(void);
void menu_finalize(struct menu *parent);
void menu_set_type(int type);

extern struct menu rootmenu;

bool menu_is_empty(struct menu *menu);
bool menu_is_visible(struct menu *menu);
bool menu_has_prompt(struct menu *menu);
const char *menu_get_prompt(struct menu *menu);
struct menu *menu_get_root_menu(struct menu *menu);
struct menu *menu_get_parent_menu(struct menu *menu);
bool menu_has_help(struct menu *menu);
const char *menu_get_help(struct menu *menu);
struct gstr get_relations_str(struct symbol **sym_arr, struct list_head *head);
void menu_get_ext_help(struct menu *menu, struct gstr *help);

/* symbol.c */
void sym_clear_all_valid(void);
struct symbol *sym_choice_default(struct symbol *sym);
struct property *sym_get_range_prop(struct symbol *sym);
const char *sym_get_string_default(struct symbol *sym);
struct symbol *sym_check_deps(struct symbol *sym);
struct symbol *prop_get_symbol(struct property *prop);

/**
 * @brief Retrieves the tristate value of a given symbol.
 *
 * This function returns the current tristate value (e.g., `yes`, `no`, or `mod`)
 * associated with the provided symbol. The tristate value is typically used in
 * configuration contexts to represent the state of a configuration option.
 *
 * @param sym Pointer to the symbol structure whose tristate value is to be retrieved.
 * @return The tristate value of the symbol, which is of type `tristate`.
 */
static inline tristate sym_get_tristate_value(struct symbol *sym)
{
	return sym->curr.tri;
}


/**
 * Retrieves the current value of a choice symbol.
 *
 * This function is used to get the current value of a symbol that represents a choice
 * in a configuration system. The choice symbol typically has a set of possible values,
 * and this function returns the currently selected value from those options.
 *
 * @param sym Pointer to the symbol structure representing the choice.
 * @return Pointer to the symbol structure representing the currently selected value.
 */
static inline struct symbol *sym_get_choice_value(struct symbol *sym)
{
	return (struct symbol *)sym->curr.val;
}

/**
 * @brief Sets the choice value for a given symbol to the specified choice value.
 *
 * This function sets the tristate value of the choice value symbol (`chval`) to `yes`,
 * effectively selecting it as the active choice for the given choice symbol (`ch`).
 *
 * @param ch The choice symbol for which the value is being set.
 * @param chval The choice value symbol to be selected.
 *
 * @return Returns `true` if the operation was successful, otherwise `false`.
 */
static inline bool sym_set_choice_value(struct symbol *ch, struct symbol *chval)
{
	return sym_set_tristate_value(chval, yes);
}

/**
 * @brief Checks if a given symbol represents a choice.
 *
 * This function determines whether the provided symbol is a choice by examining
 * its flags. A symbol is considered a choice if the `SYMBOL_CHOICE` flag is set
 * in its flags field.
 *
 * @param sym Pointer to the symbol to be checked.
 * @return `true` if the symbol is a choice, `false` otherwise.
 */
static inline bool sym_is_choice(struct symbol *sym)
{
	return sym->flags & SYMBOL_CHOICE ? true : false;
}

/**
 * @brief Checks if a given symbol is a choice value.
 *
 * This function determines whether the provided symbol is a choice value by checking
 * if the `SYMBOL_CHOICEVAL` flag is set in its flags. A choice value is typically
 * a selectable option within a configuration choice.
 *
 * @param sym Pointer to the symbol to be checked.
 * @return `true` if the symbol is a choice value, `false` otherwise.
 */
static inline bool sym_is_choice_value(struct symbol *sym)
{
	return sym->flags & SYMBOL_CHOICEVAL ? true : false;
}

/**
 * @brief Checks if a symbol is marked as optional.
 *
 * This function examines the flags of the given symbol to determine if it is
 * marked as optional. A symbol is considered optional if the `SYMBOL_OPTIONAL`
 * flag is set in its `flags` field.
 *
 * @param sym Pointer to the symbol structure to be checked.
 * @return `true` if the symbol is marked as optional, `false` otherwise.
 */
static inline bool sym_is_optional(struct symbol *sym)
{
	return sym->flags & SYMBOL_OPTIONAL ? true : false;
}

/**
 * @brief Checks if a symbol has a user-defined value.
 *
 * This function examines the flags of the given symbol to determine if it has
 * a value that was explicitly set by the user. It checks for the presence of
 * the `SYMBOL_DEF_USER` flag in the symbol's flags field.
 *
 * @param sym Pointer to the symbol structure to be checked.
 * @return `true` if the symbol has a user-defined value, `false` otherwise.
 */
static inline bool sym_has_value(struct symbol *sym)
{
	return sym->flags & SYMBOL_DEF_USER ? true : false;
}

#ifdef __cplusplus
}
#endif

#endif /* LKC_H */
