// SPDX-License-Identifier: GPL-2.0
//
// Copyright (C) 2018 Masahiro Yamada <yamada.masahiro@socionext.com>

#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "list.h"
#include "lkc.h"

#define ARRAY_SIZE(arr)		(sizeof(arr) / sizeof((arr)[0]))

static char *expand_string_with_args(const char *in, int argc, char *argv[]);
static char *expand_string(const char *in);

/**
 * @brief Prints an error message to the standard error stream and terminates the program.
 *
 * This function formats and prints an error message to the standard error stream (stderr).
 * The message includes the name of the current file and the current line number (yylineno)
 * from the global context. The function then appends a custom error message provided by
 * the caller using a format string and variable arguments (similar to printf). After
 * printing the error message, the function terminates the program with an exit status of 1.
 *
 * @param format A format string specifying the error message, similar to printf.
 * @param ... Variable arguments to be formatted and included in the error message.
 *
 * @note This function is marked with the `noreturn` attribute, indicating that it does not
 * return to the caller. Instead, it terminates the program after printing the error message.
 */
static void __attribute__((noreturn)) pperror(const char *format, ...)
{
	va_list ap;

	fprintf(stderr, "%s:%d: ", current_file->name, yylineno);
	va_start(ap, format);
	vfprintf(stderr, format, ap);
	va_end(ap);
	fprintf(stderr, "\n");

	exit(1);
}

/*
 * Environment variables
 */
static LIST_HEAD(env_list);

struct env {
	char *name;
	char *value;
	struct list_head node;
};

/**
 * Adds a new environment variable to the global environment list.
 *
 * This function allocates memory for a new environment variable structure,
 * duplicates the provided `name` and `value` strings, and adds the new
 * environment variable to the end of the global environment list (`env_list`).
 *
 * @param name  The name of the environment variable to add. Must be a
 *              null-terminated string.
 * @param value The value of the environment variable to add. Must be a
 *              null-terminated string.
 *
 * @note The function assumes that `xmalloc` and `xstrdup` are available for
 *       memory allocation and string duplication, respectively. It also
 *       assumes that `env_list` is a valid global list initialized elsewhere.
 */
static void env_add(const char *name, const char *value)
{
	struct env *e;

	e = xmalloc(sizeof(*e));
	e->name = xstrdup(name);
	e->value = xstrdup(value);

	list_add_tail(&e->node, &env_list);
}

/**
 * Deletes an environment variable entry from the environment list and frees its associated memory.
 * 
 * This function removes the environment variable entry `e` from the linked list by calling `list_del`
 * on its node. It then frees the memory allocated for the environment variable's name and value,
 * and finally frees the memory allocated for the environment variable structure itself.
 * 
 * @param e Pointer to the environment variable entry to be deleted. Must not be NULL.
 */
static void env_del(struct env *e)
{
	list_del(&e->node);
	free(e->name);
	free(e->value);
	free(e);
}

/* The returned pointer must be freed when done */
static char *env_expand(const char *name)
{
	struct env *e;
	const char *value;

	if (!*name)
		return NULL;

	list_for_each_entry(e, &env_list, node) {
		if (!strcmp(name, e->name))
			return xstrdup(e->value);
	}

	value = getenv(name);
	if (!value)
		return NULL;

	/*
	 * We need to remember all referenced environment variables.
	 * They will be written out to include/config/auto.conf.cmd
	 */
	env_add(name, value);

	return xstrdup(value);
}

/**
 * Writes environment variable dependencies to a specified file in a Makefile format.
 * 
 * This function iterates over a list of environment variables and writes a conditional
 * block for each variable to the provided file. The block checks if the environment
 * variable's value differs from its current value in the environment. If it does, the
 * specified `autoconfig_name` target is marked as dependent on the `FORCE` target,
 * ensuring it is rebuilt when the environment variable changes. After writing the
 * dependency, the environment variable is removed from the list.
 *
 * @param f The file pointer to which the dependencies are written.
 * @param autoconfig_name The name of the target in the Makefile that depends on the
 *                        environment variables.
 */
void env_write_dep(FILE *f, const char *autoconfig_name)
{
	struct env *e, *tmp;

	list_for_each_entry_safe(e, tmp, &env_list, node) {
		fprintf(f, "ifneq \"$(%s)\" \"%s\"\n", e->name, e->value);
		fprintf(f, "%s: FORCE\n", autoconfig_name);
		fprintf(f, "endif\n");
		env_del(e);
	}
}

/*
 * Built-in functions
 */
struct function {
	const char *name;
	unsigned int min_args;
	unsigned int max_args;
	char *(*func)(int argc, char *argv[]);
};

/**
 * @brief Checks the first argument of the command line input and prints an error message if it matches "y".
 *
 * This function compares the first argument (`argv[0]`) with the string "y". If they match, it prints
 * an error message using `pperror` with the second argument (`argv[1]`) as the message. Regardless of
 * the comparison result, the function returns a dynamically allocated duplicate of an empty string.
 *
 * @param argc The number of command line arguments.
 * @param argv The array of command line arguments.
 * @return A dynamically allocated duplicate of an empty string. The caller is responsible for freeing
 *         the returned memory.
 */
static char *do_error_if(int argc, char *argv[])
{
	if (!strcmp(argv[0], "y"))
		pperror("%s", argv[1]);

	return xstrdup("");
}

/**
 * Retrieves the name of the current file being processed and returns a duplicate of it.
 * This function is typically used to extract the filename from the command-line arguments
 * or the current context, ensuring that the returned string is a dynamically allocated copy.
 *
 * @param argc The number of command-line arguments passed to the program.
 * @param argv An array of command-line argument strings.
 * @return A dynamically allocated copy of the current file's name. The caller is responsible
 *         for freeing the returned string to avoid memory leaks.
 */
static char *do_filename(int argc, char *argv[])
{
	return xstrdup(current_file->name);
}

/**
 * @brief Prints the first argument from the provided argument vector and returns an empty string.
 *
 * This function takes an argument count (`argc`) and an argument vector (`argv`) as input. 
 * It prints the first element of the argument vector (`argv[0]`) to the standard output.
 * The function then returns a dynamically allocated duplicate of an empty string.
 *
 * @param argc The number of arguments in the argument vector.
 * @param argv The argument vector containing the arguments.
 * @return A dynamically allocated duplicate of an empty string. The caller is responsible for freeing the returned memory.
 */
static char *do_info(int argc, char *argv[])
{
	printf("%s\n", argv[0]);

	return xstrdup("");
}

/**
 * Generates a string representation of the current line number (`yylineno`) and returns a dynamically allocated copy of it.
 *
 * This function formats the integer value of `yylineno` into a string using a buffer of fixed size (16 bytes).
 * The formatted string is then duplicated using `xstrdup`, which allocates memory for the string and returns a pointer to it.
 *
 * @param argc The number of arguments passed to the function (unused in this implementation).
 * @param argv The array of argument strings passed to the function (unused in this implementation).
 * @return A pointer to a dynamically allocated string containing the current line number. The caller is responsible for freeing this memory.
 */
static char *do_lineno(int argc, char *argv[])
{
	char buf[16];

	sprintf(buf, "%d", yylineno);

	return xstrdup(buf);
}

/**
 * Executes a shell command and processes its output.
 *
 * This function takes a command and its arguments, executes the command using `popen`,
 * reads the output, and processes it to remove trailing newlines and replace internal
 * newlines with spaces. The processed output is then returned as a dynamically allocated
 * string.
 *
 * @param argc The number of arguments in the `argv` array.
 * @param argv An array of strings where the first element is the command to execute,
 *             and the remaining elements are the arguments to the command.
 *
 * @return A dynamically allocated string containing the processed output of the command.
 *         The caller is responsible for freeing this memory.
 *
 * @note If `popen` or `pclose` fails, the function will print an error message to `stderr`
 *       and terminate the program with an exit code of 1.
 */
static char *do_shell(int argc, char *argv[])
{
	FILE *p;
	char buf[256];
	char *cmd;
	size_t nread;
	int i;

	cmd = argv[0];

	p = popen(cmd, "r");
	if (!p) {
		perror(cmd);
		exit(1);
	}

	nread = fread(buf, 1, sizeof(buf), p);
	if (nread == sizeof(buf))
		nread--;

	/* remove trailing new lines */
	while (nread > 0 && buf[nread - 1] == '\n')
		nread--;

	buf[nread] = 0;

	/* replace a new line with a space */
	for (i = 0; i < nread; i++) {
		if (buf[i] == '\n')
			buf[i] = ' ';
	}

	if (pclose(p) == -1) {
		perror(cmd);
		exit(1);
	}

	return xstrdup(buf);
}

/**
 * @brief Issues a warning message if the first argument is "y".
 *
 * This function checks if the first element of the `argv` array is equal to "y".
 * If it is, a warning message is printed to the standard error stream. The message
 * includes the name of the current file, the current line number, and the second
 * element of the `argv` array as the warning text.
 *
 * @param argc The number of arguments in the `argv` array.
 * @param argv An array of strings where the first element is checked for "y" and
 *             the second element is used as the warning message.
 *
 * @return A dynamically allocated empty string. The caller is responsible for
 *         freeing the returned memory.
 */
static char *do_warning_if(int argc, char *argv[])
{
	if (!strcmp(argv[0], "y"))
		fprintf(stderr, "%s:%d: %s\n",
			current_file->name, yylineno, argv[1]);

	return xstrdup("");
}

static const struct function function_table[] = {
	/* Name		MIN	MAX	Function */
	{ "error-if",	2,	2,	do_error_if },
	{ "filename",	0,	0,	do_filename },
	{ "info",	1,	1,	do_info },
	{ "lineno",	0,	0,	do_lineno },
	{ "shell",	1,	1,	do_shell },
	{ "warning-if",	2,	2,	do_warning_if },
};

#define FUNCTION_MAX_ARGS		16

/**
 * Expands a function call by looking up the function by name in the function table
 * and invoking it with the provided arguments.
 *
 * This function searches the `function_table` for a function whose name matches
 * the `name` parameter. If a match is found, it validates the number of arguments
 * (`argc`) against the function's minimum (`min_args`) and maximum (`max_args`)
 * argument requirements. If the argument count is invalid, an error is logged
 * using `pperror`. If the argument count is valid, the function is invoked with
 * the provided arguments (`argv`), and the result is returned.
 *
 * @param name The name of the function to expand.
 * @param argc The number of arguments passed to the function.
 * @param argv An array of argument strings passed to the function.
 *
 * @return A pointer to the result of the function invocation if the function
 *         is found and the argument count is valid. Returns `NULL` if the
 *         function is not found in the function table.
 */
static char *function_expand(const char *name, int argc, char *argv[])
{
	const struct function *f;
	int i;

	for (i = 0; i < ARRAY_SIZE(function_table); i++) {
		f = &function_table[i];
		if (strcmp(f->name, name))
			continue;

		if (argc < f->min_args)
			pperror("too few function arguments passed to '%s'",
				name);

		if (argc > f->max_args)
			pperror("too many function arguments passed to '%s'",
				name);

		return f->func(argc, argv);
	}

	return NULL;
}

/*
 * Variables (and user-defined functions)
 */
static LIST_HEAD(variable_list);

struct variable {
	char *name;
	char *value;
	enum variable_flavor flavor;
	int exp_count;
	struct list_head node;
};

/**
 * Searches for a variable with the specified name in the global variable list.
 *
 * This function iterates through the `variable_list` to find a variable whose name
 * matches the given `name` parameter. The comparison is case-sensitive and uses
 * `strcmp` to check for equality. If a matching variable is found, a pointer to
 * the corresponding `struct variable` is returned. If no match is found, the function
 * returns NULL.
 *
 * @param name The name of the variable to search for. Must be a null-terminated string.
 * @return A pointer to the `struct variable` if found, otherwise NULL.
 */
static struct variable *variable_lookup(const char *name)
{
	struct variable *v;

	list_for_each_entry(v, &variable_list, node) {
		if (!strcmp(name, v->name))
			return v;
	}

	return NULL;
}

/**
 * Expands a variable by looking up its value and handling recursive expansion.
 *
 * This function looks up a variable by its name and expands its value based on the
 * variable's flavor. If the variable is recursive, it expands the value with the
 * provided arguments. If the variable is not recursive, it simply duplicates the value.
 * The function also handles recursive expansion checks to prevent infinite loops or
 * excessive recursion depth.
 *
 * @param name The name of the variable to expand.
 * @param argc The number of arguments provided for expansion.
 * @param argv The array of arguments for expansion.
 *
 * @return A pointer to the expanded value of the variable. The caller is responsible
 *         for freeing this memory. Returns NULL if the variable is not found.
 *
 * @note This function increments and decrements the `exp_count` field of the variable
 *       to track recursion depth. If the recursion depth exceeds 1000, an error is
 *       logged. If a recursive variable references itself without arguments, an error
 *       is also logged.
 */
static char *variable_expand(const char *name, int argc, char *argv[])
{
	struct variable *v;
	char *res;

	v = variable_lookup(name);
	if (!v)
		return NULL;

	if (argc == 0 && v->exp_count)
		pperror("Recursive variable '%s' references itself (eventually)",
			name);

	if (v->exp_count > 1000)
		pperror("Too deep recursive expansion");

	v->exp_count++;

	if (v->flavor == VAR_RECURSIVE)
		res = expand_string_with_args(v->value, argc, argv);
	else
		res = xstrdup(v->value);

	v->exp_count--;

	return res;
}

/**
 * Adds or updates a variable in the variable list with the specified name, value, and flavor.
 * 
 * If a variable with the given name already exists, its value is updated based on the provided flavor.
 * If the flavor is VAR_APPEND, the new value is appended to the existing value, separated by a space.
 * Otherwise, the existing value is replaced with the new value.
 * 
 * If the variable does not exist, it is created with the specified name and flavor. If the flavor is
 * VAR_APPEND, it defaults to VAR_RECURSIVE for new variables.
 * 
 * The value is processed differently based on the flavor:
 * - For VAR_SIMPLE, the value is expanded using `expand_string`.
 * - For other flavors, the value is duplicated as-is.
 * 
 * @param name   The name of the variable to add or update. Must not be NULL.
 * @param value  The value to assign to the variable. Must not be NULL.
 * @param flavor The flavor of the variable, which determines how the value is processed and stored.
 *               Possible values are VAR_SIMPLE, VAR_RECURSIVE, and VAR_APPEND.
 */
void variable_add(const char *name, const char *value,
		  enum variable_flavor flavor)
{
	struct variable *v;
	char *new_value;
	bool append = false;

	v = variable_lookup(name);
	if (v) {
		/* For defined variables, += inherits the existing flavor */
		if (flavor == VAR_APPEND) {
			flavor = v->flavor;
			append = true;
		} else {
			free(v->value);
		}
	} else {
		/* For undefined variables, += assumes the recursive flavor */
		if (flavor == VAR_APPEND)
			flavor = VAR_RECURSIVE;

		v = xmalloc(sizeof(*v));
		v->name = xstrdup(name);
		v->exp_count = 0;
		list_add_tail(&v->node, &variable_list);
	}

	v->flavor = flavor;

	if (flavor == VAR_SIMPLE)
		new_value = expand_string(value);
	else
		new_value = xstrdup(value);

	if (append) {
		v->value = xrealloc(v->value,
				    strlen(v->value) + strlen(new_value) + 2);
		strcat(v->value, " ");
		strcat(v->value, new_value);
		free(new_value);
	} else {
		v->value = new_value;
	}
}

/**
 * @brief Deletes a variable structure and its associated resources.
 *
 * This function removes the variable from its containing list, frees the memory
 * allocated for the variable's name and value, and finally frees the memory
 * allocated for the variable structure itself. It ensures that all resources
 * associated with the variable are properly released to avoid memory leaks.
 *
 * @param v Pointer to the variable structure to be deleted. The structure must
 *          have been dynamically allocated and must not be NULL.
 */
static void variable_del(struct variable *v)
{
	list_del(&v->node);
	free(v->name);
	free(v->value);
	free(v);
}

/**
 * Deletes all variables in the variable list.
 *
 * This function iterates over the `variable_list` and deletes each variable
 * by calling `variable_del(v)` on it. The iteration is done safely using
 * `list_for_each_entry_safe`, which allows for the deletion of the current
 * list entry without causing issues during traversal.
 *
 * After this function is called, the `variable_list` will be empty.
 */
void variable_all_del(void)
{
	struct variable *v, *tmp;

	list_for_each_entry_safe(v, tmp, &variable_list, node)
		variable_del(v);
}

/*
 * Evaluate a clause with arguments.  argc/argv are arguments from the upper
 * function call.
 *
 * Returned string must be freed when done
 */
static char *eval_clause(const char *str, size_t len, int argc, char *argv[])
{
	char *tmp, *name, *res, *endptr, *prev, *p;
	int new_argc = 0;
	char *new_argv[FUNCTION_MAX_ARGS];
	int nest = 0;
	int i;
	unsigned long n;

	tmp = xstrndup(str, len);

	/*
	 * If variable name is '1', '2', etc.  It is generally an argument
	 * from a user-function call (i.e. local-scope variable).  If not
	 * available, then look-up global-scope variables.
	 */
	n = strtoul(tmp, &endptr, 10);
	if (!*endptr && n > 0 && n <= argc) {
		res = xstrdup(argv[n - 1]);
		goto free_tmp;
	}

	prev = p = tmp;

	/*
	 * Split into tokens
	 * The function name and arguments are separated by a comma.
	 * For example, if the function call is like this:
	 *   $(foo,$(x),$(y))
	 *
	 * The input string for this helper should be:
	 *   foo,$(x),$(y)
	 *
	 * and split into:
	 *   new_argv[0] = 'foo'
	 *   new_argv[1] = '$(x)'
	 *   new_argv[2] = '$(y)'
	 */
	while (*p) {
		if (nest == 0 && *p == ',') {
			*p = 0;
			if (new_argc >= FUNCTION_MAX_ARGS)
				pperror("too many function arguments");
			new_argv[new_argc++] = prev;
			prev = p + 1;
		} else if (*p == '(') {
			nest++;
		} else if (*p == ')') {
			nest--;
		}

		p++;
	}
	new_argv[new_argc++] = prev;

	/*
	 * Shift arguments
	 * new_argv[0] represents a function name or a variable name.  Put it
	 * into 'name', then shift the rest of the arguments.  This simplifies
	 * 'const' handling.
	 */
	name = expand_string_with_args(new_argv[0], argc, argv);
	new_argc--;
	for (i = 0; i < new_argc; i++)
		new_argv[i] = expand_string_with_args(new_argv[i + 1],
						      argc, argv);

	/* Search for variables */
	res = variable_expand(name, new_argc, new_argv);
	if (res)
		goto free;

	/* Look for built-in functions */
	res = function_expand(name, new_argc, new_argv);
	if (res)
		goto free;

	/* Last, try environment variable */
	if (new_argc == 0) {
		res = env_expand(name);
		if (res)
			goto free;
	}

	res = xstrdup("");
free:
	for (i = 0; i < new_argc; i++)
		free(new_argv[i]);
	free(name);
free_tmp:
	free(tmp);

	return res;
}

/*
 * Expand a string that follows '$'
 *
 * For example, if the input string is
 *     ($(FOO)$($(BAR)))$(BAZ)
 * this helper evaluates
 *     $($(FOO)$($(BAR)))
 * and returns a new string containing the expansion (note that the string is
 * recursively expanded), also advancing 'str' to point to the next character
 * after the corresponding closing parenthesis, in this case, *str will be
 *     $(BAR)
 */
static char *expand_dollar_with_args(const char **str, int argc, char *argv[])
{
	const char *p = *str;
	const char *q;
	int nest = 0;

	/*
	 * In Kconfig, variable/function references always start with "$(".
	 * Neither single-letter variables as in $A nor curly braces as in ${CC}
	 * are supported.  '$' not followed by '(' loses its special meaning.
	 */
	if (*p != '(') {
		*str = p;
		return xstrdup("$");
	}

	p++;
	q = p;
	while (*q) {
		if (*q == '(') {
			nest++;
		} else if (*q == ')') {
			if (nest-- == 0)
				break;
		}
		q++;
	}

	if (!*q)
		pperror("unterminated reference to '%s': missing ')'", p);

	/* Advance 'str' to after the expanded initial portion of the string */
	*str = q + 1;

	return eval_clause(p, q - p, argc, argv);
}

/**
 * Expands any dollar sign ($) followed by a variable name in the input string.
 * This function is a convenience wrapper for `expand_dollar_with_args` that
 * does not pass any additional arguments or argument count. It is used to
 * expand environment variables or special shell variables in the input string.
 *
 * @param str A pointer to the input string containing dollar sign variables
 *            to be expanded. The pointer is updated to point to the character
 *            after the last processed character in the input string.
 * @return A dynamically allocated string containing the expanded result.
 *         The caller is responsible for freeing the returned string.
 */
char *expand_dollar(const char **str)
{
    return expand_dollar_with_args(str, 0, NULL);
}

/**
 * Expands a string by replacing occurrences of '$' followed by a valid argument
 * placeholder with the corresponding argument from the provided array. The
 * expansion process continues until a character that satisfies the `is_end`
 * condition is encountered.
 *
 * @param str      Pointer to the input string. The pointer is advanced to the
 *                 character that caused the expansion to end.
 * @param is_end   Function pointer to a predicate that determines the end of
 *                 the expansion. It takes a single character as input and
 *                 returns true if the character signifies the end of the
 *                 expansion.
 * @param argc     The number of arguments in the `argv` array.
 * @param argv     Array of argument strings used for expansion.
 *
 * @return         A newly allocated string containing the expanded result. The
 *                 caller is responsible for freeing the returned string.
 *
 * @note           The function dynamically allocates memory for the expanded
 *                 string. If the input string contains no '$' characters, the
 *                 function returns a copy of the input string up to the end
 *                 character.
 */
static char *__expand_string(const char **str, bool (*is_end)(char c),
			     int argc, char *argv[])
{
	const char *in, *p;
	char *expansion, *out;
	size_t in_len, out_len;

	out = xmalloc(1);
	*out = 0;
	out_len = 1;

	p = in = *str;

	while (1) {
		if (*p == '$') {
			in_len = p - in;
			p++;
			expansion = expand_dollar_with_args(&p, argc, argv);
			out_len += in_len + strlen(expansion);
			out = xrealloc(out, out_len);
			strncat(out, in, in_len);
			strcat(out, expansion);
			free(expansion);
			in = p;
			continue;
		}

		if (is_end(*p))
			break;

		p++;
	}

	in_len = p - in;
	out_len += in_len;
	out = xrealloc(out, out_len);
	strncat(out, in, in_len);

	/* Advance 'str' to the end character */
	*str = p;

	return out;
}

/**
 * Checks if the given character is the null terminator, indicating the end of a string.
 * 
 * @param c The character to check.
 * @return `true` if the character is the null terminator (`'\0'`), otherwise `false`.
 */
static bool is_end_of_str(char c)
{
	return !c;
}

/*
 * Expand variables and functions in the given string.  Undefined variables
 * expand to an empty string.
 * The returned string must be freed when done.
 */
static char *expand_string_with_args(const char *in, int argc, char *argv[])
{
	return __expand_string(&in, is_end_of_str, argc, argv);
}

/**
 * Expands the given input string by processing any special characters or placeholders.
 * This function is a simplified wrapper for `expand_string_with_args` that does not
 * require additional arguments or flags. It is useful for basic string expansion
 * where no additional context or arguments are needed.
 *
 * @param in The input string to be expanded. This string may contain special
 *           characters or placeholders that will be processed during expansion.
 * @return A pointer to the expanded string. The caller is responsible for freeing
 *         the allocated memory for the expanded string when it is no longer needed.
 *         Returns NULL if the expansion fails or if the input string is NULL.
 */
static char *expand_string(const char *in)
{
	return expand_string_with_args(in, 0, NULL);
}

/**
 * Determines if the given character marks the end of a token.
 * 
 * A token is considered to be a sequence of alphanumeric characters,
 * underscores ('_'), or hyphens ('-'). This method checks if the
 * provided character does not belong to any of these categories,
 * thereby indicating the end of a token.
 * 
 * @param c The character to be checked.
 * @return `true` if the character marks the end of a token (i.e., it is not
 *         an alphanumeric character, underscore, or hyphen), otherwise `false`.
 */
static bool is_end_of_token(char c)
{
    return !(isalnum(c) || c == '_' || c == '-');
}

/*
 * Expand variables in a token.  The parsing stops when a token separater
 * (in most cases, it is a whitespace) is encountered.  'str' is updated to
 * point to the next character.
 *
 * The returned string must be freed when done.
 */
char *expand_one_token(const char **str)
{
	return __expand_string(str, is_end_of_token, 0, NULL);
}
