// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2002 Roman Zippel <zippel@linux-m68k.org>
 */

#include <sys/mman.h>
#include <sys/stat.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "lkc.h"

/* return true if 'path' exists, false otherwise */
static bool is_present(const char *path)
{
	struct stat st;

	return !stat(path, &st);
}

/* return true if 'path' exists and it is a directory, false otherwise */
static bool is_dir(const char *path)
{
	struct stat st;

	if (stat(path, &st))
		return 0;

	return S_ISDIR(st.st_mode);
}

/* return true if the given two files are the same, false otherwise */
static bool is_same(const char *file1, const char *file2)
{
	int fd1, fd2;
	struct stat st1, st2;
	void *map1, *map2;
	bool ret = false;

	fd1 = open(file1, O_RDONLY);
	if (fd1 < 0)
		return ret;

	fd2 = open(file2, O_RDONLY);
	if (fd2 < 0)
		goto close1;

	ret = fstat(fd1, &st1);
	if (ret)
		goto close2;
	ret = fstat(fd2, &st2);
	if (ret)
		goto close2;

	if (st1.st_size != st2.st_size)
		goto close2;

	map1 = mmap(NULL, st1.st_size, PROT_READ, MAP_PRIVATE, fd1, 0);
	if (map1 == MAP_FAILED)
		goto close2;

	map2 = mmap(NULL, st2.st_size, PROT_READ, MAP_PRIVATE, fd2, 0);
	if (map2 == MAP_FAILED)
		goto close2;

	if (bcmp(map1, map2, st1.st_size))
		goto close2;

	ret = true;
close2:
	close(fd2);
close1:
	close(fd1);

	return ret;
}

/*
 * Create the parent directory of the given path.
 *
 * For example, if 'include/config/auto.conf' is given, create 'include/config'.
 */
static int make_parent_dir(const char *path)
{
	char tmp[PATH_MAX + 1];
	char *p;

	strncpy(tmp, path, sizeof(tmp));
	tmp[sizeof(tmp) - 1] = 0;

	/* Remove the base name. Just return if nothing is left */
	p = strrchr(tmp, '/');
	if (!p)
		return 0;
	*(p + 1) = 0;

	/* Just in case it is an absolute path */
	p = tmp;
	while (*p == '/')
		p++;

	while ((p = strchr(p, '/'))) {
		*p = 0;

		/* skip if the directory exists */
		if (!is_dir(tmp) && mkdir(tmp, 0755))
			return -1;

		*p = '/';
		while (*p == '/')
			p++;
	}

	return 0;
}

static char depfile_path[PATH_MAX];
static size_t depfile_prefix_len;

/* touch depfile for symbol 'name' */
static int conf_touch_dep(const char *name)
{
	int fd, ret;
	const char *s;
	char *d, c;

	/* check overflow: prefix + name + ".h" + '\0' must fit in buffer. */
	if (depfile_prefix_len + strlen(name) + 3 > sizeof(depfile_path))
		return -1;

	d = depfile_path + depfile_prefix_len;
	s = name;

	while ((c = *s++))
		*d++ = (c == '_') ? '/' : tolower(c);
	strcpy(d, ".h");

	/* Assume directory path already exists. */
	fd = open(depfile_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd == -1) {
		if (errno != ENOENT)
			return -1;

		ret = make_parent_dir(depfile_path);
		if (ret)
			return ret;

		/* Try it again. */
		fd = open(depfile_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
		if (fd == -1)
			return -1;
	}
	close(fd);

	return 0;
}

struct conf_printer {
	void (*print_symbol)(FILE *, struct symbol *, const char *, void *);
	void (*print_comment)(FILE *, const char *, void *);
};

static void conf_warning(const char *fmt, ...)
	__attribute__ ((format (printf, 1, 2)));

static void conf_message(const char *fmt, ...)
	__attribute__ ((format (printf, 1, 2)));

static const char *conf_filename;
static int conf_lineno, conf_warnings;

/**
 * Prints a warning message to the standard error stream (stderr) with a formatted prefix.
 * The prefix includes the configuration filename (`conf_filename`) and line number (`conf_lineno`)
 * followed by the string "warning:". The method then appends the provided formatted message
 * (`fmt`) and a newline character. After printing the warning, it increments the global
 * `conf_warnings` counter to track the number of warnings issued.
 *
 * @param fmt A format string specifying the warning message, similar to `printf`.
 * @param ... Additional arguments corresponding to the format specifiers in `fmt`.
 */
static void conf_warning(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	fprintf(stderr, "%s:%d:warning: ", conf_filename, conf_lineno);
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");
	va_end(ap);
	conf_warnings++;
}

/**
 * @brief Prints a formatted message with a predefined prefix and suffix.
 *
 * This method takes a string `s` as input and prints it in a formatted manner.
 * The message is prefixed with "#\n# " and suffixed with "\n#\n", effectively
 * surrounding the input string with a line of "#" characters above and below.
 * This is useful for highlighting or separating messages in console output.
 *
 * @param s The string message to be printed. The string should be null-terminated.
 */
static void conf_default_message_callback(const char *s)
{
	printf("#\n# ");
	printf("%s", s);
	printf("\n#\n");
}

static void (*conf_message_callback)(const char *s) =
	conf_default_message_callback;
/**
 * @brief Sets the message callback function for the configuration system.
 *
 * This function allows the caller to specify a custom callback function that will be invoked
 * whenever the configuration system needs to output a message. The callback function should
 * accept a single parameter, a C-style string (`const char *`), which represents the message
 * to be processed or displayed.
 *
 * @param fn A pointer to the callback function. The function should have the signature
 *           `void (*)(const char *s)`, where `s` is the message string. If `fn` is `NULL`,
 *           the message callback will be disabled.
 */
void conf_set_message_callback(void (*fn)(const char *s))
{
	conf_message_callback = fn;
}

/**
 * @brief Formats a message using a provided format string and variable arguments, 
 *        then passes the formatted message to a callback function.
 *
 * This function takes a format string `fmt` and a variable number of arguments, 
 * formats them into a message using `vsnprintf`, and then calls the `conf_message_callback` 
 * function with the formatted message as its argument. If `conf_message_callback` is NULL, 
 * the function does nothing.
 *
 * @param fmt The format string (similar to printf) that specifies how the message 
 *            should be formatted.
 * @param ... Variable arguments that will be substituted into the format string.
 *
 * @note The formatted message is stored in a buffer of size 4096 bytes. If the 
 *       formatted message exceeds this size, it will be truncated.
 */
static void conf_message(const char *fmt, ...)
{
	va_list ap;
	char buf[4096];

	if (!conf_message_callback)
		return;

	va_start(ap, fmt);

	vsnprintf(buf, sizeof(buf), fmt, ap);
	conf_message_callback(buf);
	va_end(ap);
}

/**
 * @brief Retrieves the configuration file name from the environment or returns a default.
 *
 * This function checks the environment for the variable `KCONFIG_CONFIG`. If the variable
 * is set, it returns the value of the variable as the configuration file name. If the
 * variable is not set, the function returns the default configuration file name ".config".
 *
 * @return A pointer to the configuration file name. This is either the value of the
 *         `KCONFIG_CONFIG` environment variable or the string ".config" if the variable
 *         is not set.
 */
const char *conf_get_configname(void)
{
	char *name = getenv("KCONFIG_CONFIG");

	return name ? name : ".config";
}

/**
 * Retrieves the name of the auto-configuration file to be used.
 * 
 * This method checks for the presence of the environment variable `KCONFIG_AUTOCONFIG`.
 * If the environment variable is set, its value is returned as the name of the auto-configuration file.
 * If the environment variable is not set, the method defaults to returning "include/config/auto.conf".
 *
 * @return A pointer to the name of the auto-configuration file. The returned string is either the value
 *         of the `KCONFIG_AUTOCONFIG` environment variable or the default path "include/config/auto.conf".
 */
static const char *conf_get_autoconfig_name(void)
{
	char *name = getenv("KCONFIG_AUTOCONFIG");

	return name ? name : "include/config/auto.conf";
}

/**
 * conf_set_sym_val - Set the value of a symbol based on its type and input string.
 *
 * @sym: Pointer to the symbol structure whose value is to be set.
 * @def: The definition index to be used within the symbol's definition array.
 * @def_flags: Flags to be applied to the symbol upon successful value assignment.
 * @p: Input string representing the value to be assigned to the symbol.
 *
 * This function processes the input string @p and assigns it to the symbol @sym
 * based on the symbol's type. The behavior varies depending on the symbol type:
 * - For S_TRISTATE: Accepts 'm' (module), 'y' (yes), or 'n' (no) as valid values.
 * - For S_BOOLEAN: Accepts 'y' (yes) or 'n' (no) as valid values.
 * - For S_STRING: Processes the input string, ensuring it is properly quoted and
 *   escapes special characters.
 * - For S_INT and S_HEX: Validates the input string and assigns it if valid.
 *
 * If the input value is invalid for the symbol's type, a warning is issued (unless
 * @def is S_DEF_AUTO), and the function returns 1. On success, the function returns 0.
 *
 * Return: 0 on success, 1 on failure (invalid value for the symbol type).
 */
static int conf_set_sym_val(struct symbol *sym, int def, int def_flags, char *p)
{
	char *p2;

	switch (sym->type) {
	case S_TRISTATE:
		if (p[0] == 'm') {
			sym->def[def].tri = mod;
			sym->flags |= def_flags;
			break;
		}
		/* fall through */
	case S_BOOLEAN:
		if (p[0] == 'y') {
			sym->def[def].tri = yes;
			sym->flags |= def_flags;
			break;
		}
		if (p[0] == 'n') {
			sym->def[def].tri = no;
			sym->flags |= def_flags;
			break;
		}
		if (def != S_DEF_AUTO)
			conf_warning("symbol value '%s' invalid for %s",
				     p, sym->name);
		return 1;
	case S_STRING:
		if (*p++ != '"')
			break;
		for (p2 = p; (p2 = strpbrk(p2, "\"\\")); p2++) {
			if (*p2 == '"') {
				*p2 = 0;
				break;
			}
			memmove(p2, p2 + 1, strlen(p2));
		}
		if (!p2) {
			if (def != S_DEF_AUTO)
				conf_warning("invalid string found");
			return 1;
		}
		/* fall through */
	case S_INT:
	case S_HEX:
		if (sym_string_valid(sym, p)) {
			sym->def[def].val = xstrdup(p);
			sym->flags |= def_flags;
		} else {
			if (def != S_DEF_AUTO)
				conf_warning("symbol value '%s' invalid for %s",
					     p, sym->name);
			return 1;
		}
		break;
	default:
		;
	}
	return 0;
}

#define LINE_GROWTH 16
/**
 * Adds a byte to a dynamically allocated buffer, resizing it if necessary.
 *
 * This function appends a single byte `c` to the buffer pointed to by `lineptr`.
 * If the buffer does not have enough space to accommodate the new byte, it is
 * resized. The new size is calculated by doubling the current size and adding
 * `LINE_GROWTH - 1` to ensure sufficient growth. The actual size of the buffer
 * is tracked by `n`, and the current length of the buffer is given by `slen`.
 *
 * @param c The byte to be added to the buffer.
 * @param lineptr A pointer to the buffer. This may be updated if the buffer is resized.
 * @param slen The current length of the buffer (number of bytes already stored).
 * @param n A pointer to the current allocated size of the buffer. This is updated if the buffer is resized.
 *
 * @return 0 on success, or -1 if memory allocation fails.
 */
static int add_byte(int c, char **lineptr, size_t slen, size_t *n)
{
	char *nline;
	size_t new_size = slen + 1;
	if (new_size > *n) {
		new_size += LINE_GROWTH - 1;
		new_size *= 2;
		nline = xrealloc(*lineptr, new_size);
		if (!nline)
			return -1;

		*lineptr = nline;
		*n = new_size;
	}

	(*lineptr)[slen] = c;

	return 0;
}

/**
 * Reads a line from the specified file stream and stores it in a dynamically allocated buffer.
 *
 * This function reads characters from the given file stream until a newline character ('\n') or
 * the end-of-file (EOF) is encountered. The line, including the newline character (if present),
 * is stored in a buffer pointed to by `*lineptr`. The buffer is dynamically resized as needed.
 *
 * @param lineptr Pointer to a buffer where the line will be stored. If `*lineptr` is NULL, a new
 *                buffer will be allocated. If `*lineptr` is not NULL, it must point to a buffer
 *                allocated by malloc, which may be resized if necessary.
 * @param n       Pointer to the size of the buffer. If a new buffer is allocated or the existing
 *                buffer is resized, `*n` will be updated to reflect the new size.
 * @param stream  The file stream from which to read the line.
 *
 * @return The number of characters read, including the newline character but excluding the
 *         null terminator. If no characters were read (e.g., EOF is encountered immediately),
 *         the function returns -1. On error (e.g., memory allocation failure), the function
 *         also returns -1, and the buffer is null-terminated at the last successfully read
 *         character.
 */
static ssize_t compat_getline(char **lineptr, size_t *n, FILE *stream)
{
	char *line = *lineptr;
	size_t slen = 0;

	for (;;) {
		int c = getc(stream);

		switch (c) {
		case '\n':
			if (add_byte(c, &line, slen, n) < 0)
				goto e_out;
			slen++;
			/* fall through */
		case EOF:
			if (add_byte('\0', &line, slen, n) < 0)
				goto e_out;
			*lineptr = line;
			if (slen == 0)
				return -1;
			return slen;
		default:
			if (add_byte(c, &line, slen, n) < 0)
				goto e_out;
			slen++;
		}
	}

e_out:
	line[slen-1] = '\0';
	*lineptr = line;
	return -1;
}

/**
 * Reads and processes a configuration file, updating symbol definitions based on the file's content.
 *
 * This function reads a configuration file specified by `name` and updates the internal symbol table
 * based on the configuration options found in the file. If `name` is NULL, it attempts to find a default
 * configuration file. The `def` parameter specifies the type of definition to apply (e.g., user-defined or
 * auto-generated).
 *
 * The function performs the following steps:
 * 1. Opens the configuration file. If `name` is NULL, it searches for a default configuration file.
 * 2. Initializes symbol flags and resets their values based on the `def` parameter.
 * 3. Reads the file line by line, parsing each line to update the corresponding symbol's value.
 * 4. Handles special cases such as comments, "is not set" directives, and choice symbols.
 * 5. Issues warnings for unexpected data or inconsistencies in the configuration.
 * 6. Closes the file and returns 0 on success, or 1 if the file could not be opened or processed.
 *
 * @param name The name of the configuration file to read. If NULL, a default configuration file is used.
 * @param def The type of definition to apply (e.g., S_DEF_USER or S_DEF_AUTO).
 * @return 0 on success, 1 on failure.
 */
int conf_read_simple(const char *name, int def)
{
	FILE *in = NULL;
	char   *line = NULL;
	size_t  line_asize = 0;
	char *p, *p2;
	struct symbol *sym;
	int i, def_flags;

	if (name) {
		in = zconf_fopen(name);
	} else {
		struct property *prop;

		name = conf_get_configname();
		in = zconf_fopen(name);
		if (in)
			goto load;
		sym_add_change_count(1);
		if (!sym_defconfig_list)
			return 1;

		for_all_defaults(sym_defconfig_list, prop) {
			if (expr_calc_value(prop->visible.expr) == no ||
			    prop->expr->type != E_SYMBOL)
				continue;
			sym_calc_value(prop->expr->left.sym);
			name = sym_get_string_value(prop->expr->left.sym);
			in = zconf_fopen(name);
			if (in) {
				conf_message("using defaults found in %s",
					 name);
				goto load;
			}
		}
	}
	if (!in)
		return 1;

load:
	conf_filename = name;
	conf_lineno = 0;
	conf_warnings = 0;

	def_flags = SYMBOL_DEF << def;
	for_all_symbols(i, sym) {
		sym->flags |= SYMBOL_CHANGED;
		sym->flags &= ~(def_flags|SYMBOL_VALID);
		if (sym_is_choice(sym))
			sym->flags |= def_flags;
		switch (sym->type) {
		case S_INT:
		case S_HEX:
		case S_STRING:
			if (sym->def[def].val)
				free(sym->def[def].val);
			/* fall through */
		default:
			sym->def[def].val = NULL;
			sym->def[def].tri = no;
		}
	}

	while (compat_getline(&line, &line_asize, in) != -1) {
		conf_lineno++;
		sym = NULL;
		if (line[0] == '#') {
			if (memcmp(line + 2, CONFIG_, strlen(CONFIG_)))
				continue;
			p = strchr(line + 2 + strlen(CONFIG_), ' ');
			if (!p)
				continue;
			*p++ = 0;
			if (strncmp(p, "is not set", 10))
				continue;
			if (def == S_DEF_USER) {
				sym = sym_find(line + 2 + strlen(CONFIG_));
				if (!sym) {
					sym_add_change_count(1);
					continue;
				}
			} else {
				sym = sym_lookup(line + 2 + strlen(CONFIG_), 0);
				if (sym->type == S_UNKNOWN)
					sym->type = S_BOOLEAN;
			}
			if (sym->flags & def_flags) {
				conf_warning("override: reassigning to symbol %s", sym->name);
			}
			switch (sym->type) {
			case S_BOOLEAN:
			case S_TRISTATE:
				sym->def[def].tri = no;
				sym->flags |= def_flags;
				break;
			default:
				;
			}
		} else if (memcmp(line, CONFIG_, strlen(CONFIG_)) == 0) {
			p = strchr(line + strlen(CONFIG_), '=');
			if (!p)
				continue;
			*p++ = 0;
			p2 = strchr(p, '\n');
			if (p2) {
				*p2-- = 0;
				if (*p2 == '\r')
					*p2 = 0;
			}

			sym = sym_find(line + strlen(CONFIG_));
			if (!sym) {
				if (def == S_DEF_AUTO)
					/*
					 * Reading from include/config/auto.conf
					 * If CONFIG_FOO previously existed in
					 * auto.conf but it is missing now,
					 * include/config/foo.h must be touched.
					 */
					conf_touch_dep(line + strlen(CONFIG_));
				else
					sym_add_change_count(1);
				continue;
			}

			if (sym->flags & def_flags) {
				conf_warning("override: reassigning to symbol %s", sym->name);
			}
			if (conf_set_sym_val(sym, def, def_flags, p))
				continue;
		} else {
			if (line[0] != '\r' && line[0] != '\n')
				conf_warning("unexpected data: %.*s",
					     (int)strcspn(line, "\r\n"), line);

			continue;
		}

		if (sym && sym_is_choice_value(sym)) {
			struct symbol *cs = prop_get_symbol(sym_get_choice_prop(sym));
			switch (sym->def[def].tri) {
			case no:
				break;
			case mod:
				if (cs->def[def].tri == yes) {
					conf_warning("%s creates inconsistent choice state", sym->name);
					cs->flags &= ~def_flags;
				}
				break;
			case yes:
				if (cs->def[def].tri != no)
					conf_warning("override: %s changes choice state", sym->name);
				cs->def[def].val = sym;
				break;
			}
			cs->def[def].tri = EXPR_OR(cs->def[def].tri, sym->def[def].tri);
		}
	}
	free(line);
	fclose(in);
	return 0;
}

/**
 * Reads and processes a configuration file specified by `name`.
 *
 * This function reads the configuration file and calculates the values of all symbols
 * defined in it. It then checks for unsaved changes by comparing the calculated values
 * with the saved values in the configuration. If discrepancies are found, it increments
 * the `conf_unsaved` counter. Additionally, it resets the values of generated symbols
 * that are not visible in the current configuration and handles out-of-range string values.
 *
 * The function also updates the change count based on the number of warnings or unsaved
 * changes detected during the process.
 *
 * @param name The name of the configuration file to read.
 * @return Returns 1 if the configuration file could not be read, otherwise returns 0.
 */
int conf_read(const char *name)
{
	struct symbol *sym;
	int conf_unsaved = 0;
	int i;

	sym_set_change_count(0);

	if (conf_read_simple(name, S_DEF_USER)) {
		sym_calc_value(modules_sym);
		return 1;
	}

	sym_calc_value(modules_sym);

	for_all_symbols(i, sym) {
		sym_calc_value(sym);
		if (sym_is_choice(sym) || (sym->flags & SYMBOL_NO_WRITE))
			continue;
		if (sym_has_value(sym) && (sym->flags & SYMBOL_WRITE)) {
			/* check that calculated value agrees with saved value */
			switch (sym->type) {
			case S_BOOLEAN:
			case S_TRISTATE:
				if (sym->def[S_DEF_USER].tri == sym_get_tristate_value(sym))
					continue;
				break;
			default:
				if (!strcmp(sym->curr.val, sym->def[S_DEF_USER].val))
					continue;
				break;
			}
		} else if (!sym_has_value(sym) && !(sym->flags & SYMBOL_WRITE))
			/* no previous value and not saved */
			continue;
		conf_unsaved++;
		/* maybe print value in verbose mode... */
	}

	for_all_symbols(i, sym) {
		if (sym_has_value(sym) && !sym_is_choice_value(sym)) {
			/* Reset values of generates values, so they'll appear
			 * as new, if they should become visible, but that
			 * doesn't quite work if the Kconfig and the saved
			 * configuration disagree.
			 */
			if (sym->visible == no && !conf_unsaved)
				sym->flags &= ~SYMBOL_DEF_USER;
			switch (sym->type) {
			case S_STRING:
			case S_INT:
			case S_HEX:
				/* Reset a string value if it's out of range */
				if (sym_string_within_range(sym, sym->def[S_DEF_USER].val))
					break;
				sym->flags &= ~(SYMBOL_VALID|SYMBOL_DEF_USER);
				conf_unsaved++;
				break;
			default:
				break;
			}
		}
	}

	sym_add_change_count(conf_warnings || conf_unsaved);

	return 0;
}

/*
 * Kconfig configuration printer
 *
 * This printer is used when generating the resulting configuration after
 * kconfig invocation and `defconfig' files. Unset symbol might be omitted by
 * passing a non-NULL argument to the printer.
 *
 */
static void
kconfig_print_symbol(FILE *fp, struct symbol *sym, const char *value, void *arg)
{

	switch (sym->type) {
	case S_BOOLEAN:
	case S_TRISTATE:
		if (*value == 'n') {
			bool skip_unset = (arg != NULL);

			if (!skip_unset)
				fprintf(fp, "# %s%s is not set\n",
				    CONFIG_, sym->name);
			return;
		}
		break;
	default:
		break;
	}

	fprintf(fp, "%s%s=%s\n", CONFIG_, sym->name, value);
}

/**
 * Prints a comment block to the specified file pointer.
 *
 * This function takes a multi-line comment string and formats it as a series of
 * comment lines, each prefixed with a '#'. The comment string is split at newline
 * characters, and each segment is written to the file as a separate comment line.
 * If a segment is non-empty, it is prefixed with a space after the '#'.
 *
 * @param fp    The file pointer to which the comment is written.
 * @param value The multi-line comment string to be printed.
 * @param arg   Unused parameter, included for compatibility with function pointer
 *              signatures. This parameter is not used in the function.
 */
static void
kconfig_print_comment(FILE *fp, const char *value, void *arg)
{
	const char *p = value;
	size_t l;

	for (;;) {
		l = strcspn(p, "\n");
		fprintf(fp, "#");
		if (l) {
			fprintf(fp, " ");
			xfwrite(p, l, 1, fp);
			p += l;
		}
		fprintf(fp, "\n");
		if (*p++ == '\0')
			break;
	}
}

static struct conf_printer kconfig_printer_cb =
{
	.print_symbol = kconfig_print_symbol,
	.print_comment = kconfig_print_comment,
};

/*
 * Header printer
 *
 * This printer is used when generating the `include/generated/autoconf.h' file.
 */
static void
header_print_symbol(FILE *fp, struct symbol *sym, const char *value, void *arg)
{

	switch (sym->type) {
	case S_BOOLEAN:
	case S_TRISTATE: {
		const char *suffix = "";

		switch (*value) {
		case 'n':
			break;
		case 'm':
			suffix = "_MODULE";
			/* fall through */
		default:
			fprintf(fp, "#define %s%s%s 1\n",
			    CONFIG_, sym->name, suffix);
		}
		break;
	}
	case S_HEX: {
		const char *prefix = "";

		if (value[0] != '0' || (value[1] != 'x' && value[1] != 'X'))
			prefix = "0x";
		fprintf(fp, "#define %s%s %s%s\n",
		    CONFIG_, sym->name, prefix, value);
		break;
	}
	case S_STRING:
	case S_INT:
		fprintf(fp, "#define %s%s %s\n",
		    CONFIG_, sym->name, value);
		break;
	default:
		break;
	}

}

/**
 * header_print_comment - Prints a multi-line comment block to a file.
 *
 * This function takes a string `value` and formats it as a multi-line comment block,
 * writing the result to the file pointer `fp`. The comment block is enclosed within
 * `/*` and `*/` delimiters. Each line of the input string is prefixed with ` * ` to
 * align it within the comment block. If the input string contains newline characters,
 * they are preserved, and each line is processed individually.
 *
 * @param fp    A pointer to the file where the comment block will be written.
 * @param value The string containing the comment text to be formatted and printed.
 * @param arg   Unused parameter, provided for compatibility with function pointer types.
 */
static void
header_print_comment(FILE *fp, const char *value, void *arg)
{
	const char *p = value;
	size_t l;

	fprintf(fp, "/*\n");
	for (;;) {
		l = strcspn(p, "\n");
		fprintf(fp, " *");
		if (l) {
			fprintf(fp, " ");
			xfwrite(p, l, 1, fp);
			p += l;
		}
		fprintf(fp, "\n");
		if (*p++ == '\0')
			break;
	}
	fprintf(fp, " */\n");
}

static struct conf_printer header_printer_cb =
{
	.print_symbol = header_print_symbol,
	.print_comment = header_print_comment,
};

/**
 * Writes the value of a symbol to a file using a specified printer function.
 *
 * This function handles different types of symbols and writes their values to the provided file pointer.
 * The actual writing is delegated to the `print_symbol` function of the `conf_printer` structure.
 *
 * @param fp          The file pointer to which the symbol value will be written.
 * @param sym         The symbol whose value is to be written.
 * @param printer     A pointer to a `conf_printer` structure containing the `print_symbol` function.
 * @param printer_arg An optional argument passed to the `print_symbol` function.
 *
 * The function behaves differently based on the type of the symbol:
 * - For `S_UNKNOWN` type, no action is taken.
 * - For `S_STRING` type, the string value is retrieved, escaped, and then written using the printer function.
 *   The escaped string is freed after use.
 * - For all other types, the string value is retrieved and written directly using the printer function.
 */
static void conf_write_symbol(FILE *fp, struct symbol *sym,
			      struct conf_printer *printer, void *printer_arg)
{
	const char *str;

	switch (sym->type) {
	case S_UNKNOWN:
		break;
	case S_STRING:
		str = sym_get_string_value(sym);
		str = sym_escape_string_value(str);
		printer->print_symbol(fp, sym, str, printer_arg);
		free((void *)str);
		break;
	default:
		str = sym_get_string_value(sym);
		printer->print_symbol(fp, sym, str, printer_arg);
	}
}

/**
 * Writes a heading comment to the specified file using the provided printer function.
 * The heading includes a warning that the file is automatically generated and should not be edited,
 * followed by the text from the root menu's prompt.
 *
 * @param fp The file pointer to which the heading will be written.
 * @param printer A pointer to a conf_printer structure containing the print_comment function.
 * @param printer_arg Additional arguments to be passed to the print_comment function.
 */
static void
conf_write_heading(FILE *fp, struct conf_printer *printer, void *printer_arg)
{
	char buf[256];

	snprintf(buf, sizeof(buf),
	    "\n"
	    "Automatically generated file; DO NOT EDIT.\n"
	    "%s\n",
	    rootmenu.prompt->text);

	printer->print_comment(fp, buf, printer_arg);
}

/*
 * Write out a minimal config.
 * All values that has default values are skipped as this is redundant.
 */
int conf_write_defconfig(const char *filename)
{
	struct symbol *sym;
	struct menu *menu;
	FILE *out;

	out = fopen(filename, "w");
	if (!out)
		return 1;

	sym_clear_all_valid();

	/* Traverse all menus to find all relevant symbols */
	menu = rootmenu.list;

	while (menu != NULL)
	{
		sym = menu->sym;
		if (sym == NULL) {
			if (!menu_is_visible(menu))
				goto next_menu;
		} else if (!sym_is_choice(sym)) {
			sym_calc_value(sym);
			if (!(sym->flags & SYMBOL_WRITE))
				goto next_menu;
			sym->flags &= ~SYMBOL_WRITE;
			/* If we cannot change the symbol - skip */
			if (!sym_is_changeable(sym))
				goto next_menu;
			/* If symbol equals to default value - skip */
			if (strcmp(sym_get_string_value(sym), sym_get_string_default(sym)) == 0)
				goto next_menu;

			/*
			 * If symbol is a choice value and equals to the
			 * default for a choice - skip.
			 * But only if value is bool and equal to "y" and
			 * choice is not "optional".
			 * (If choice is "optional" then all values can be "n")
			 */
			if (sym_is_choice_value(sym)) {
				struct symbol *cs;
				struct symbol *ds;

				cs = prop_get_symbol(sym_get_choice_prop(sym));
				ds = sym_choice_default(cs);
				if (!sym_is_optional(cs) && sym == ds) {
					if ((sym->type == S_BOOLEAN) &&
					    sym_get_tristate_value(sym) == yes)
						goto next_menu;
				}
			}
			conf_write_symbol(out, sym, &kconfig_printer_cb, NULL);
		}
next_menu:
		if (menu->list != NULL) {
			menu = menu->list;
		}
		else if (menu->next != NULL) {
			menu = menu->next;
		} else {
			while ((menu = menu->parent)) {
				if (menu->next != NULL) {
					menu = menu->next;
					break;
				}
			}
		}
	}
	fclose(out);
	return 0;
}

/**
 * Writes the current configuration to a file.
 *
 * This function writes the current configuration settings to the specified file. If the file
 * already exists, it can be overwritten or saved as a new file depending on the environment
 * variable `KCONFIG_OVERWRITECONFIG`. The function handles the creation of parent directories
 * if necessary and ensures that the configuration is written in a structured format.
 *
 * @param name The name of the file to which the configuration should be written. If `NULL`,
 *             the default configuration name is used.
 *
 * @return Returns 0 on success. Returns -1 if the configuration name is empty or if the specified
 *         path is a directory. Returns 1 if there is an error opening the file or renaming the
 *         temporary file.
 */
int conf_write(const char *name)
{
	FILE *out;
	struct symbol *sym;
	struct menu *menu;
	const char *str;
	char tmpname[PATH_MAX + 1], oldname[PATH_MAX + 1];
	char *env;
	int i;
	bool need_newline = false;

	if (!name)
		name = conf_get_configname();

	if (!*name) {
		fprintf(stderr, "config name is empty\n");
		return -1;
	}

	if (is_dir(name)) {
		fprintf(stderr, "%s: Is a directory\n", name);
		return -1;
	}

	if (make_parent_dir(name))
		return -1;

	env = getenv("KCONFIG_OVERWRITECONFIG");
	if (env && *env) {
		*tmpname = 0;
		out = fopen(name, "w");
	} else {
		snprintf(tmpname, sizeof(tmpname), "%s.%d.tmp",
			 name, (int)getpid());
		out = fopen(tmpname, "w");
	}
	if (!out)
		return 1;

	conf_write_heading(out, &kconfig_printer_cb, NULL);

	if (!conf_get_changed())
		sym_clear_all_valid();

	menu = rootmenu.list;
	while (menu) {
		sym = menu->sym;
		if (!sym) {
			if (!menu_is_visible(menu))
				goto next;
			str = menu_get_prompt(menu);
			fprintf(out, "\n"
				     "#\n"
				     "# %s\n"
				     "#\n", str);
			need_newline = false;
		} else if (!(sym->flags & SYMBOL_CHOICE) &&
			   !(sym->flags & SYMBOL_WRITTEN)) {
			sym_calc_value(sym);
			if (!(sym->flags & SYMBOL_WRITE))
				goto next;
			if (need_newline) {
				fprintf(out, "\n");
				need_newline = false;
			}
			sym->flags |= SYMBOL_WRITTEN;
			conf_write_symbol(out, sym, &kconfig_printer_cb, NULL);
		}

next:
		if (menu->list) {
			menu = menu->list;
			continue;
		}
		if (menu->next)
			menu = menu->next;
		else while ((menu = menu->parent)) {
			if (!menu->sym && menu_is_visible(menu) &&
			    menu != &rootmenu) {
				str = menu_get_prompt(menu);
				fprintf(out, "# end of %s\n", str);
				need_newline = true;
			}
			if (menu->next) {
				menu = menu->next;
				break;
			}
		}
	}
	fclose(out);

	for_all_symbols(i, sym)
		sym->flags &= ~SYMBOL_WRITTEN;

	if (*tmpname) {
		if (is_same(name, tmpname)) {
			conf_message("No change to %s", name);
			unlink(tmpname);
			sym_set_change_count(0);
			return 0;
		}

		snprintf(oldname, sizeof(oldname), "%s.old", name);
		rename(name, oldname);
		if (rename(tmpname, name))
			return 1;
	}

	conf_message("configuration written to %s", name);

	sym_set_change_count(0);

	return 0;
}

/* write a dependency file as used by kbuild to track dependencies */
static int conf_write_dep(const char *name)
{
	struct file *file;
	FILE *out;

	out = fopen("..config.tmp", "w");
	if (!out)
		return 1;
	fprintf(out, "deps_config := \\\n");
	for (file = file_list; file; file = file->next) {
		if (file->next)
			fprintf(out, "\t%s \\\n", file->name);
		else
			fprintf(out, "\t%s\n", file->name);
	}
	fprintf(out, "\n%s: \\\n"
		     "\t$(deps_config)\n\n", conf_get_autoconfig_name());

	env_write_dep(out, conf_get_autoconfig_name());

	fprintf(out, "\n$(deps_config): ;\n");
	fclose(out);

	if (make_parent_dir(name))
		return 1;
	rename("..config.tmp", name);
	return 0;
}

/**
 * conf_touch_deps - Update dependency files for symbols that have changed.
 *
 * This function iterates over all symbols in the configuration and updates
 * the dependency files for symbols that have been modified. It first sets up
 * the dependency file path and reads the auto-configuration file. Then, it
 * calculates the value of each symbol and checks if it has been written or
 * modified. For symbols that have changed, it updates the corresponding
 * dependency file by calling `conf_touch_dep`.
 *
 * The function handles different types of symbols (e.g., boolean, tristate,
 * string, hex, int) and compares their current values with their previous
 * values to determine if they have changed. If a symbol has no previous value,
 * it is considered changed if its current value is not 'no' (unset).
 *
 * @return Returns 0 on success, or a non-zero error code if an error occurs
 *         while updating the dependency files.
 */
static int conf_touch_deps(void)
{
	const char *name;
	struct symbol *sym;
	int res, i;

	strcpy(depfile_path, "include/config/");
	depfile_prefix_len = strlen(depfile_path);

	name = conf_get_autoconfig_name();
	conf_read_simple(name, S_DEF_AUTO);
	sym_calc_value(modules_sym);

	for_all_symbols(i, sym) {
		sym_calc_value(sym);
		if ((sym->flags & SYMBOL_NO_WRITE) || !sym->name)
			continue;
		if (sym->flags & SYMBOL_WRITE) {
			if (sym->flags & SYMBOL_DEF_AUTO) {
				/*
				 * symbol has old and new value,
				 * so compare them...
				 */
				switch (sym->type) {
				case S_BOOLEAN:
				case S_TRISTATE:
					if (sym_get_tristate_value(sym) ==
					    sym->def[S_DEF_AUTO].tri)
						continue;
					break;
				case S_STRING:
				case S_HEX:
				case S_INT:
					if (!strcmp(sym_get_string_value(sym),
						    sym->def[S_DEF_AUTO].val))
						continue;
					break;
				default:
					break;
				}
			} else {
				/*
				 * If there is no old value, only 'no' (unset)
				 * is allowed as new value.
				 */
				switch (sym->type) {
				case S_BOOLEAN:
				case S_TRISTATE:
					if (sym_get_tristate_value(sym) == no)
						continue;
					break;
				default:
					break;
				}
			}
		} else if (!(sym->flags & SYMBOL_DEF_AUTO))
			/* There is neither an old nor a new value. */
			continue;
		/* else
		 *	There is an old value, but no new value ('no' (unset)
		 *	isn't saved in auto.conf, so the old value is always
		 *	different from 'no').
		 */

		res = conf_touch_dep(sym->name);
		if (res)
			return res;
	}

	return 0;
}

/**
 * Writes the auto-configuration files (`auto.conf` and `autoconf.h`) based on the current
 * symbol values in the configuration. This function handles the creation and updating
 * of these files, ensuring they reflect the current state of the configuration.
 *
 * The function performs the following steps:
 * 1. Checks if the `auto.conf` file already exists and whether it should be overwritten
 *    based on the `overwrite` parameter. If overwriting is not allowed and the file exists,
 *    the function returns without making any changes.
 * 2. Writes dependency information to `include/config/auto.conf.cmd`.
 * 3. Creates temporary files (`.tmpconfig` and `.tmpconfig.h`) to store the new configuration
 *    data.
 * 4. Iterates over all symbols in the configuration, calculates their values, and writes
 *    them to the temporary files.
 * 5. Renames the temporary files to their final names (`auto.conf` and `autoconf.h`),
 *    ensuring the parent directories exist.
 * 6. Returns 0 on success or 1 if any step fails.
 *
 * @param overwrite If non-zero, allows overwriting the existing `auto.conf` file.
 *                 If zero, the function returns early if `auto.conf` already exists.
 * @return 0 on success, 1 on failure.
 */
int conf_write_autoconf(int overwrite)
{
	struct symbol *sym;
	const char *name;
	const char *autoconf_name = conf_get_autoconfig_name();
	FILE *out, *out_h;
	int i;

	if (!overwrite && is_present(autoconf_name))
		return 0;

	conf_write_dep("include/config/auto.conf.cmd");

	if (conf_touch_deps())
		return 1;

	out = fopen(".tmpconfig", "w");
	if (!out)
		return 1;

	out_h = fopen(".tmpconfig.h", "w");
	if (!out_h) {
		fclose(out);
		return 1;
	}

	conf_write_heading(out, &kconfig_printer_cb, NULL);
	conf_write_heading(out_h, &header_printer_cb, NULL);

	for_all_symbols(i, sym) {
		sym_calc_value(sym);
		if (!(sym->flags & SYMBOL_WRITE) || !sym->name)
			continue;

		/* write symbols to auto.conf and autoconf.h */
		conf_write_symbol(out, sym, &kconfig_printer_cb, (void *)1);
		conf_write_symbol(out_h, sym, &header_printer_cb, NULL);
	}
	fclose(out);
	fclose(out_h);

	name = getenv("KCONFIG_AUTOHEADER");
	if (!name)
		name = "include/generated/autoconf.h";
	if (make_parent_dir(name))
		return 1;
	if (rename(".tmpconfig.h", name))
		return 1;

	if (make_parent_dir(autoconf_name))
		return 1;
	/*
	 * This must be the last step, kbuild has a dependency on auto.conf
	 * and this marks the successful completion of the previous steps.
	 */
	if (rename(".tmpconfig", autoconf_name))
		return 1;

	return 0;
}

static int sym_change_count;
static void (*conf_changed_callback)(void);

/**
 * @brief Updates the symbol change count and triggers a configuration change callback if necessary.
 *
 * This method sets the global symbol change count to the specified value. Before updating,
 * it stores the current value of the symbol change count. If a configuration change callback
 * is registered and the boolean representation of the previous count differs from the boolean
 * representation of the new count, the callback is invoked to notify of the change.
 *
 * @param count The new value to set for the symbol change count.
 */
void sym_set_change_count(int count)
{
	int _sym_change_count = sym_change_count;
	sym_change_count = count;
	if (conf_changed_callback &&
	    (bool)_sym_change_count != (bool)count)
		conf_changed_callback();
}

/**
 * Adds the specified count to the current change count in the system.
 * This method retrieves the current change count using `sym_change_count`,
 * adds the provided `count` to it, and then updates the change count
 * by calling `sym_set_change_count` with the new value.
 *
 * @param count The amount to add to the current change count.
 */
void sym_add_change_count(int count)
{
    sym_set_change_count(count + sym_change_count);
}

/**
 * @brief Checks if the configuration has been changed.
 *
 * This function returns a boolean value indicating whether the configuration
 * has been modified since it was last loaded or saved. It does this by checking
 * the value of `sym_change_count`, which is incremented whenever a configuration
 * setting is altered.
 *
 * @return bool Returns `true` if the configuration has been changed, otherwise `false`.
 */
bool conf_get_changed(void)
{
	return sym_change_count;
}

/**
 * Sets the callback function to be invoked when a configuration change is detected.
 *
 * This function assigns the provided function pointer `fn` to the internal
 * `conf_changed_callback` variable. The callback function should have no
 * parameters and return void. It will be called whenever a configuration
 * change is detected, allowing the application to respond to such changes.
 *
 * @param fn A pointer to the callback function that will be invoked on
 *           configuration changes. If `NULL`, the callback is disabled.
 */
void conf_set_changed_callback(void (*fn)(void))
{
	conf_changed_callback = fn;
}

/**
 * Randomizes the selection of values within a choice symbol.
 *
 * This function randomly selects one of the available symbols within a choice
 * block and sets it to 'yes', while setting all other symbols in the choice
 * block to 'no'. This ensures that only one symbol is selected within the choice.
 * The function operates only if the choice symbol's current state is 'yes'.
 * If the choice symbol is in a 'mod' or 'no' state, the function does nothing
 * and returns `false`.
 *
 * @param csym Pointer to the choice symbol whose values are to be randomized.
 * @return `true` if the randomization was successful, `false` if the choice
 *         symbol is not in a 'yes' state or if no randomization was performed.
 */
static bool randomize_choice_values(struct symbol *csym)
{
	struct property *prop;
	struct symbol *sym;
	struct expr *e;
	int cnt, def;

	/*
	 * If choice is mod then we may have more items selected
	 * and if no then no-one.
	 * In both cases stop.
	 */
	if (csym->curr.tri != yes)
		return false;

	prop = sym_get_choice_prop(csym);

	/* count entries in choice block */
	cnt = 0;
	expr_list_for_each_sym(prop->expr, e, sym)
		cnt++;

	/*
	 * find a random value and set it to yes,
	 * set the rest to no so we have only one set
	 */
	def = (rand() % cnt);

	cnt = 0;
	expr_list_for_each_sym(prop->expr, e, sym) {
		if (def == cnt++) {
			sym->def[S_DEF_USER].tri = yes;
			csym->def[S_DEF_USER].val = sym;
		}
		else {
			sym->def[S_DEF_USER].tri = no;
		}
		sym->flags |= SYMBOL_DEF_USER;
		/* clear VALID to get value calculated */
		sym->flags &= ~SYMBOL_VALID;
	}
	csym->flags |= SYMBOL_DEF_USER;
	/* clear VALID to get value calculated */
	csym->flags &= ~(SYMBOL_VALID);

	return true;
}

/**
 * Sets all non-assigned choice values to "no" for a given choice symbol.
 *
 * This function iterates over all symbols associated with the choice property
 * of the provided symbol `csym`. For each symbol that does not have a value
 * assigned, it sets the symbol's default user value to "no". Additionally, it
 * marks the choice symbol as having a user-defined value by setting the
 * `SYMBOL_DEF_USER` flag. Finally, it clears the `SYMBOL_VALID` and
 * `SYMBOL_NEED_SET_CHOICE_VALUES` flags to ensure that the symbol's value is
 * recalculated.
 *
 * @param csym The choice symbol whose associated symbols' values are to be set.
 */
void set_all_choice_values(struct symbol *csym)
{
	struct property *prop;
	struct symbol *sym;
	struct expr *e;

	prop = sym_get_choice_prop(csym);

	/*
	 * Set all non-assinged choice values to no
	 */
	expr_list_for_each_sym(prop->expr, e, sym) {
		if (!sym_has_value(sym))
			sym->def[S_DEF_USER].tri = no;
	}
	csym->flags |= SYMBOL_DEF_USER;
	/* clear VALID to get value calculated */
	csym->flags &= ~(SYMBOL_VALID | SYMBOL_NEED_SET_CHOICE_VALUES);
}

/**
 * conf_set_all_new_symbols - Configures all new symbols based on the specified mode.
 *
 * This function sets the values of all new symbols in the configuration based on the provided mode.
 * The mode determines how the symbols are set:
 * - `def_yes`: Sets all symbols to 'yes'.
 * - `def_mod`: Sets all symbols to 'mod'.
 * - `def_no`: Sets all symbols to 'no'.
 * - `def_random`: Sets symbols randomly based on probabilities defined by the environment variable
 *   `KCONFIG_PROBABILITY`. If not set, default probabilities are used (50% for boolean symbols,
 *   33% for tristate symbols to be 'yes', and 33% to be 'mod').
 *
 * The function also handles choice symbols, ensuring that only one symbol is selected in a choice
 * block when the mode is not `def_random`. In `def_random` mode, choice values are randomized.
 *
 * @param mode The configuration mode to apply (def_yes, def_mod, def_no, def_random).
 * @return Returns `true` if any symbol's value was changed, otherwise `false`.
 */
bool conf_set_all_new_symbols(enum conf_def_mode mode)
{
	struct symbol *sym, *csym;
	int i, cnt, pby, pty, ptm;	/* pby: probability of bool     = y
					 * pty: probability of tristate = y
					 * ptm: probability of tristate = m
					 */

	pby = 50; pty = ptm = 33; /* can't go as the default in switch-case
				   * below, otherwise gcc whines about
				   * -Wmaybe-uninitialized */
	if (mode == def_random) {
		int n, p[3];
		char *env = getenv("KCONFIG_PROBABILITY");
		n = 0;
		while( env && *env ) {
			char *endp;
			int tmp = strtol( env, &endp, 10 );
			if( tmp >= 0 && tmp <= 100 ) {
				p[n++] = tmp;
			} else {
				errno = ERANGE;
				perror( "KCONFIG_PROBABILITY" );
				exit( 1 );
			}
			env = (*endp == ':') ? endp+1 : endp;
			if( n >=3 ) {
				break;
			}
		}
		switch( n ) {
		case 1:
			pby = p[0]; ptm = pby/2; pty = pby-ptm;
			break;
		case 2:
			pty = p[0]; ptm = p[1]; pby = pty + ptm;
			break;
		case 3:
			pby = p[0]; pty = p[1]; ptm = p[2];
			break;
		}

		if( pty+ptm > 100 ) {
			errno = ERANGE;
			perror( "KCONFIG_PROBABILITY" );
			exit( 1 );
		}
	}
	bool has_changed = false;

	for_all_symbols(i, sym) {
		if (sym_has_value(sym) || (sym->flags & SYMBOL_VALID))
			continue;
		switch (sym_get_type(sym)) {
		case S_BOOLEAN:
		case S_TRISTATE:
			has_changed = true;
			switch (mode) {
			case def_yes:
				sym->def[S_DEF_USER].tri = yes;
				break;
			case def_mod:
				sym->def[S_DEF_USER].tri = mod;
				break;
			case def_no:
				if (sym->flags & SYMBOL_ALLNOCONFIG_Y)
					sym->def[S_DEF_USER].tri = yes;
				else
					sym->def[S_DEF_USER].tri = no;
				break;
			case def_random:
				sym->def[S_DEF_USER].tri = no;
				cnt = rand() % 100;
				if (sym->type == S_TRISTATE) {
					if (cnt < pty)
						sym->def[S_DEF_USER].tri = yes;
					else if (cnt < (pty+ptm))
						sym->def[S_DEF_USER].tri = mod;
				} else if (cnt < pby)
					sym->def[S_DEF_USER].tri = yes;
				break;
			default:
				continue;
			}
			if (!(sym_is_choice(sym) && mode == def_random))
				sym->flags |= SYMBOL_DEF_USER;
			break;
		default:
			break;
		}

	}

	sym_clear_all_valid();

	/*
	 * We have different type of choice blocks.
	 * If curr.tri equals to mod then we can select several
	 * choice symbols in one block.
	 * In this case we do nothing.
	 * If curr.tri equals yes then only one symbol can be
	 * selected in a choice block and we set it to yes,
	 * and the rest to no.
	 */
	if (mode != def_random) {
		for_all_symbols(i, csym) {
			if ((sym_is_choice(csym) && !sym_has_value(csym)) ||
			    sym_is_choice_value(csym))
				csym->flags |= SYMBOL_NEED_SET_CHOICE_VALUES;
		}
	}

	for_all_symbols(i, csym) {
		if (sym_has_value(csym) || !sym_is_choice(csym))
			continue;

		sym_calc_value(csym);
		if (mode == def_random)
			has_changed |= randomize_choice_values(csym);
		else {
			set_all_choice_values(csym);
			has_changed = true;
		}
	}

	return has_changed;
}

/**
 * Rewrites the configuration mode for all symbols based on the specified mode.
 * 
 * This function iterates over all symbols in the configuration and updates their
 * user-defined tri-state value (`S_DEF_USER`) based on the provided `mode`. 
 * 
 * - If `mode` is `def_y2m`, the function changes symbols with a tri-state value of `yes` to `mod`.
 * - If `mode` is not `def_y2m`, the function changes symbols with a tri-state value of `mod` to `yes`.
 * 
 * After updating the symbols, the function clears the validity of all symbols to ensure
 * that the changes are properly reflected in subsequent configuration checks.
 * 
 * @param mode The configuration mode to apply, which determines how the tri-state values are rewritten.
 */
void conf_rewrite_mod_or_yes(enum conf_def_mode mode)
{
	struct symbol *sym;
	int i;
	tristate old_val = (mode == def_y2m) ? yes : mod;
	tristate new_val = (mode == def_y2m) ? mod : yes;

	for_all_symbols(i, sym) {
		if (sym_get_type(sym) == S_TRISTATE &&
		    sym->def[S_DEF_USER].tri == old_val)
			sym->def[S_DEF_USER].tri = new_val;
	}
	sym_clear_all_valid();
}
