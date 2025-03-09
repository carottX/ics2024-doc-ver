// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2002 Roman Zippel <zippel@linux-m68k.org>
 */

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <errno.h>

#include "lkc.h"

static void conf(struct menu *menu);
static void check_conf(struct menu *menu);

enum input_mode {
	oldaskconfig,
	syncconfig,
	oldconfig,
	allnoconfig,
	allyesconfig,
	allmodconfig,
	alldefconfig,
	randconfig,
	defconfig,
	savedefconfig,
	listnewconfig,
	helpnewconfig,
	olddefconfig,
	yes2modconfig,
	mod2yesconfig,
};
static enum input_mode input_mode = oldaskconfig;

static int indent = 1;
static int tty_stdio;
static int sync_kconfig;
static int conf_cnt;
static char line[PATH_MAX];
static struct menu *rootEntry;

/**
 * print_help - Prints the extended help information for a given menu.
 *
 * This function generates and displays the extended help text associated with
 * the specified menu. It first creates a new string buffer to store the help
 * text. The `menu_get_ext_help` function is then called to populate this buffer
 * with the relevant help information. Finally, the help text is printed to the
 * standard output, and the string buffer is freed to release allocated memory.
 *
 * @menu: Pointer to the menu structure for which the help information is to be
 *        printed. The menu structure should contain the necessary data to
 *        generate the help text.
 */
static void print_help(struct menu *menu)
{
	struct gstr help = str_new();

	menu_get_ext_help(menu, &help);

	printf("\n%s\n", str_get(&help));
	str_free(&help);
}

/**
 * Removes leading and trailing whitespace characters from the given string.
 * 
 * This function modifies the input string in place by stripping all whitespace
 * characters (as determined by `isspace`) from the beginning and end of the string.
 * The string is left unchanged if it contains no leading or trailing whitespace.
 * 
 * @param str A pointer to the null-terminated string to be stripped. The string
 *            is modified directly, and the result is stored in the same memory
 *            location.
 * 
 * @note If the string consists entirely of whitespace, it will be reduced to an
 *       empty string (i.e., the first character will be set to '\0').
 */
static void strip(char *str)
{
	char *p = str;
	int l;

	while ((isspace(*p)))
		p++;
	l = strlen(p);
	if (p != str)
		memmove(str, p, l + 1);
	if (!l)
		return;
	p = str + l - 1;
	while ((isspace(*p)))
		*p-- = 0;
}

/* Helper function to facilitate fgets() by Jean Sacren. */
static void xfgets(char *str, int size, FILE *in)
{
	if (!fgets(str, size, in))
		fprintf(stderr, "\nError in reading or end of file.\n");

	if (!tty_stdio)
		printf("%s", str);
}

/**
 * @brief Prompts the user to input a value for a configuration symbol.
 *
 * This function handles the logic for asking the user to input a value for a given
 * configuration symbol (`sym`). It checks the symbol's type, whether it has a value,
 * and whether it is changeable. Depending on the input mode (e.g., `oldconfig`,
 * `syncconfig`, or `oldaskconfig`), it either prints the default value (`def`) or
 * prompts the user for input. The function also handles special cases for symbols
 * of type `S_INT`, `S_HEX`, or `S_STRING`.
 *
 * @param sym The configuration symbol for which the value is being asked.
 * @param def The default value to be used if the symbol does not have a value or
 *            is not changeable.
 * @return Returns 1 if the user is prompted for input or if the symbol is of type
 *         `S_INT`, `S_HEX`, or `S_STRING`. Returns 0 if the symbol is not changeable
 *         or if the input mode is `oldconfig` or `syncconfig` and the symbol already
 *         has a value.
 */
static int conf_askvalue(struct symbol *sym, const char *def)
{
	enum symbol_type type = sym_get_type(sym);

	if (!sym_has_value(sym))
		printf("(NEW) ");

	line[0] = '\n';
	line[1] = 0;

	if (!sym_is_changeable(sym)) {
		printf("%s\n", def);
		line[0] = '\n';
		line[1] = 0;
		return 0;
	}

	switch (input_mode) {
	case oldconfig:
	case syncconfig:
		if (sym_has_value(sym)) {
			printf("%s\n", def);
			return 0;
		}
		/* fall through */
	case oldaskconfig:
		fflush(stdout);
		xfgets(line, sizeof(line), stdin);
		return 1;
	default:
		break;
	}

	switch (type) {
	case S_INT:
	case S_HEX:
	case S_STRING:
		printf("%s\n", def);
		return 1;
	default:
		;
	}
	printf("%s", line);
	return 1;
}

/**
 * @brief Prompts the user to input a string value for a configuration symbol.
 *
 * This method continuously prompts the user to enter a string value for a given configuration symbol
 * associated with a menu item. The current value of the symbol is displayed as a default option. The user
 * can either accept the default value by pressing Enter, input a new value, or request help by entering '?'.
 * If the user inputs a new value, it is validated and set as the new value for the symbol. The method returns
 * 0 if the user successfully sets a value or exits the prompt.
 *
 * @param menu Pointer to the menu structure containing the configuration symbol and prompt.
 * @return int Returns 0 if the user successfully sets a value or exits the prompt.
 */
static int conf_string(struct menu *menu)
{
	struct symbol *sym = menu->sym;
	const char *def;

	while (1) {
		printf("%*s%s ", indent - 1, "", menu->prompt->text);
		printf("(%s) ", sym->name);
		def = sym_get_string_value(sym);
		if (sym_get_string_value(sym))
			printf("[%s] ", def);
		if (!conf_askvalue(sym, def))
			return 0;
		switch (line[0]) {
		case '\n':
			break;
		case '?':
			/* print help */
			if (line[1] == '\n') {
				print_help(menu);
				def = NULL;
				break;
			}
			/* fall through */
		default:
			line[strlen(line)-1] = 0;
			def = line;
		}
		if (def && sym_set_string_value(sym, def))
			return 0;
	}
}

/**
 * conf_sym - Configures the tristate value of a symbol associated with a menu.
 *
 * This function interacts with the user to set the tristate value (yes, mod, no) of a symbol
 * associated with a menu. It displays the current value of the symbol and prompts the user to
 * input a new value. The function validates the input and updates the symbol's value accordingly.
 * If the user inputs '?', the function displays help information for the menu.
 *
 * @menu: A pointer to the menu structure containing the symbol to be configured.
 *
 * Return: Returns 0 if the symbol's value is successfully updated or if the user cancels the
 *         operation. The function continues to prompt the user until a valid input is provided.
 */
static int conf_sym(struct menu *menu)
{
	struct symbol *sym = menu->sym;
	tristate oldval, newval;

	while (1) {
		printf("%*s%s ", indent - 1, "", menu->prompt->text);
		if (sym->name)
			printf("(%s) ", sym->name);
		putchar('[');
		oldval = sym_get_tristate_value(sym);
		switch (oldval) {
		case no:
			putchar('N');
			break;
		case mod:
			putchar('M');
			break;
		case yes:
			putchar('Y');
			break;
		}
		if (oldval != no && sym_tristate_within_range(sym, no))
			printf("/n");
		if (oldval != mod && sym_tristate_within_range(sym, mod))
			printf("/m");
		if (oldval != yes && sym_tristate_within_range(sym, yes))
			printf("/y");
		printf("/?] ");
		if (!conf_askvalue(sym, sym_get_string_value(sym)))
			return 0;
		strip(line);

		switch (line[0]) {
		case 'n':
		case 'N':
			newval = no;
			if (!line[1] || !strcmp(&line[1], "o"))
				break;
			continue;
		case 'm':
		case 'M':
			newval = mod;
			if (!line[1])
				break;
			continue;
		case 'y':
		case 'Y':
			newval = yes;
			if (!line[1] || !strcmp(&line[1], "es"))
				break;
			continue;
		case 0:
			newval = oldval;
			break;
		case '?':
			goto help;
		default:
			continue;
		}
		if (sym_set_tristate_value(sym, newval))
			return 0;
help:
		print_help(menu);
	}
}

/**
 * conf_choice - Handles the configuration choice for a menu entry.
 *
 * This function processes a menu entry that represents a choice in the configuration.
 * It displays the available options, prompts the user to select one, and recursively
 * configures the selected submenu. The function handles both tristate (yes, no, mod)
 * and single-choice configurations.
 *
 * @menu: Pointer to the menu structure representing the choice.
 *
 * The function performs the following steps:
 * 1. Checks if the symbol associated with the menu is changeable.
 * 2. If changeable, it invokes `conf_sym` to configure the symbol and calculates its value.
 * 3. Depending on the tristate value (no, mod, yes), it returns an appropriate status.
 * 4. If not changeable, it processes the symbol based on its tristate value.
 * 5. Displays the available choices and prompts the user to select one.
 * 6. Validates the user input and sets the chosen value.
 * 7. Recursively configures the submenus of the selected choice.
 *
 * Return: 1 if the configuration is successful, 0 otherwise.
 */
static int conf_choice(struct menu *menu)
{
	struct symbol *sym, *def_sym;
	struct menu *child;
	bool is_new;

	sym = menu->sym;
	is_new = !sym_has_value(sym);
	if (sym_is_changeable(sym)) {
		conf_sym(menu);
		sym_calc_value(sym);
		switch (sym_get_tristate_value(sym)) {
		case no:
			return 1;
		case mod:
			return 0;
		case yes:
			break;
		}
	} else {
		switch (sym_get_tristate_value(sym)) {
		case no:
			return 1;
		case mod:
			printf("%*s%s\n", indent - 1, "", menu_get_prompt(menu));
			return 0;
		case yes:
			break;
		}
	}

	while (1) {
		int cnt, def;

		printf("%*s%s\n", indent - 1, "", menu_get_prompt(menu));
		def_sym = sym_get_choice_value(sym);
		cnt = def = 0;
		line[0] = 0;
		for (child = menu->list; child; child = child->next) {
			if (!menu_is_visible(child))
				continue;
			if (!child->sym) {
				printf("%*c %s\n", indent, '*', menu_get_prompt(child));
				continue;
			}
			cnt++;
			if (child->sym == def_sym) {
				def = cnt;
				printf("%*c", indent, '>');
			} else
				printf("%*c", indent, ' ');
			printf(" %d. %s", cnt, menu_get_prompt(child));
			if (child->sym->name)
				printf(" (%s)", child->sym->name);
			if (!sym_has_value(child->sym))
				printf(" (NEW)");
			printf("\n");
		}
		printf("%*schoice", indent - 1, "");
		if (cnt == 1) {
			printf("[1]: 1\n");
			goto conf_childs;
		}
		printf("[1-%d?]: ", cnt);
		switch (input_mode) {
		case oldconfig:
		case syncconfig:
			if (!is_new) {
				cnt = def;
				printf("%d\n", cnt);
				break;
			}
			/* fall through */
		case oldaskconfig:
			fflush(stdout);
			xfgets(line, sizeof(line), stdin);
			strip(line);
			if (line[0] == '?') {
				print_help(menu);
				continue;
			}
			if (!line[0])
				cnt = def;
			else if (isdigit(line[0]))
				cnt = atoi(line);
			else
				continue;
			break;
		default:
			break;
		}

	conf_childs:
		for (child = menu->list; child; child = child->next) {
			if (!child->sym || !menu_is_visible(child))
				continue;
			if (!--cnt)
				break;
		}
		if (!child)
			continue;
		if (line[0] && line[strlen(line) - 1] == '?') {
			print_help(child);
			continue;
		}
		sym_set_choice_value(sym, child->sym);
		for (child = child->list; child; child = child->next) {
			indent += 2;
			conf(child);
			indent -= 2;
		}
		return 1;
	}
}

/**
 * Processes a menu and its children to generate configuration output.
 *
 * This function handles the configuration of a given menu and its child menus.
 * It first checks if the menu is visible; if not, it returns immediately.
 * If the menu has a prompt, it processes the prompt based on its type (e.g., P_MENU or P_COMMENT)
 * and prints the appropriate output. For menus with symbols, it handles different types of symbols
 * (e.g., choices, integers, hex values, strings) by delegating to specific configuration functions
 * (e.g., conf_choice, conf_string, conf_sym). Finally, it recursively processes all child menus,
 * adjusting the indentation level as needed to reflect the menu hierarchy.
 *
 * @param menu Pointer to the menu structure to be processed.
 */
static void conf(struct menu *menu)
{
	struct symbol *sym;
	struct property *prop;
	struct menu *child;

	if (!menu_is_visible(menu))
		return;

	sym = menu->sym;
	prop = menu->prompt;
	if (prop) {
		const char *prompt;

		switch (prop->type) {
		case P_MENU:
			/*
			 * Except in oldaskconfig mode, we show only menus that
			 * contain new symbols.
			 */
			if (input_mode != oldaskconfig && rootEntry != menu) {
				check_conf(menu);
				return;
			}
			/* fall through */
		case P_COMMENT:
			prompt = menu_get_prompt(menu);
			if (prompt)
				printf("%*c\n%*c %s\n%*c\n",
					indent, '*',
					indent, '*', prompt,
					indent, '*');
		default:
			;
		}
	}

	if (!sym)
		goto conf_childs;

	if (sym_is_choice(sym)) {
		conf_choice(menu);
		if (sym->curr.tri != mod)
			return;
		goto conf_childs;
	}

	switch (sym->type) {
	case S_INT:
	case S_HEX:
	case S_STRING:
		conf_string(menu);
		break;
	default:
		conf_sym(menu);
		break;
	}

conf_childs:
	if (sym)
		indent += 2;
	for (child = menu->list; child; child = child->next)
		conf(child);
	if (sym)
		indent -= 2;
}

/**
 * Recursively checks and processes the configuration menu and its child menus.
 * 
 * This function traverses the menu structure starting from the given `menu` node.
 * It checks if the menu is visible and processes the associated symbol if it exists.
 * The processing depends on the current `input_mode`:
 * - In `listnewconfig` mode, it prints the symbol's name and value in a format suitable
 *   for generating a new configuration file.
 * - In `helpnewconfig` mode, it prints help information for the menu.
 * - In other modes, it restarts the configuration process if necessary and recursively
 *   processes child menus.
 * 
 * @param menu The root menu node to start the configuration check from.
 */
static void check_conf(struct menu *menu)
{
	struct symbol *sym;
	struct menu *child;

	if (!menu_is_visible(menu))
		return;

	sym = menu->sym;
	if (sym && !sym_has_value(sym)) {
		if (sym_is_changeable(sym) ||
		    (sym_is_choice(sym) && sym_get_tristate_value(sym) == yes)) {
			if (input_mode == listnewconfig) {
				if (sym->name) {
					const char *str;

					if (sym->type == S_STRING) {
						str = sym_get_string_value(sym);
						str = sym_escape_string_value(str);
						printf("%s%s=%s\n", CONFIG_, sym->name, str);
						free((void *)str);
					} else {
						str = sym_get_string_value(sym);
						printf("%s%s=%s\n", CONFIG_, sym->name, str);
					}
				}
			} else if (input_mode == helpnewconfig) {
				printf("-----\n");
				print_help(menu);
				printf("-----\n");

			} else {
				if (!conf_cnt++)
					printf("*\n* Restart config...\n*\n");
				rootEntry = menu_get_parent_menu(menu);
				conf(rootEntry);
			}
		}
	}

	for (child = menu->list; child; child = child->next)
		check_conf(child);
}

static struct option long_opts[] = {
	{"oldaskconfig",    no_argument,       NULL, oldaskconfig},
	{"oldconfig",       no_argument,       NULL, oldconfig},
	{"syncconfig",      no_argument,       NULL, syncconfig},
	{"defconfig",       required_argument, NULL, defconfig},
	{"savedefconfig",   required_argument, NULL, savedefconfig},
	{"allnoconfig",     no_argument,       NULL, allnoconfig},
	{"allyesconfig",    no_argument,       NULL, allyesconfig},
	{"allmodconfig",    no_argument,       NULL, allmodconfig},
	{"alldefconfig",    no_argument,       NULL, alldefconfig},
	{"randconfig",      no_argument,       NULL, randconfig},
	{"listnewconfig",   no_argument,       NULL, listnewconfig},
	{"helpnewconfig",   no_argument,       NULL, helpnewconfig},
	{"olddefconfig",    no_argument,       NULL, olddefconfig},
	{"yes2modconfig",   no_argument,       NULL, yes2modconfig},
	{"mod2yesconfig",   no_argument,       NULL, mod2yesconfig},
	{NULL, 0, NULL, 0}
};

/**
 * @brief Prints the usage information for the configuration utility.
 *
 * This function displays a detailed help message that explains how to use the
 * configuration utility. It lists all available command-line options and their
 * respective functionalities. The `progname` parameter is used to dynamically
 * include the program's name in the usage message.
 *
 * @param progname The name of the program, typically `argv[0]`, used to
 *                 personalize the usage message.
 *
 * The following options are supported:
 * - `--listnewconfig`: Lists new configuration options.
 * - `--helpnewconfig`: Lists new options along with their help text.
 * - `--oldaskconfig`: Starts a new configuration using a line-oriented program.
 * - `--oldconfig`: Updates a configuration using a provided `.config` file as a base.
 * - `--syncconfig`: Similar to `oldconfig` but generates configuration in `include/{generated/,config/}`.
 * - `--olddefconfig`: Same as `oldconfig` but sets new symbols to their default value.
 * - `--defconfig <file>`: Creates a new configuration with defaults defined in `<file>`.
 * - `--savedefconfig <file>`: Saves the minimal current configuration to `<file>`.
 * - `--allnoconfig`: Creates a new configuration where all options are answered with `no`.
 * - `--allyesconfig`: Creates a new configuration where all options are answered with `yes`.
 * - `--allmodconfig`: Creates a new configuration where all options are answered with `mod`.
 * - `--alldefconfig`: Creates a new configuration with all symbols set to their default values.
 * - `--randconfig`: Creates a new configuration with random answers to all options.
 * - `--yes2modconfig`: Changes answers from `yes` to `mod` if possible.
 * - `--mod2yesconfig`: Changes answers from `mod` to `yes` if possible.
 */
static void conf_usage(const char *progname)
{
	printf("Usage: %s [-s] [option] <kconfig-file>\n", progname);
	printf("[option] is _one_ of the following:\n");
	printf("  --listnewconfig         List new options\n");
	printf("  --helpnewconfig         List new options and help text\n");
	printf("  --oldaskconfig          Start a new configuration using a line-oriented program\n");
	printf("  --oldconfig             Update a configuration using a provided .config as base\n");
	printf("  --syncconfig            Similar to oldconfig but generates configuration in\n"
	       "                          include/{generated/,config/}\n");
	printf("  --olddefconfig          Same as oldconfig but sets new symbols to their default value\n");
	printf("  --defconfig <file>      New config with default defined in <file>\n");
	printf("  --savedefconfig <file>  Save the minimal current configuration to <file>\n");
	printf("  --allnoconfig           New config where all options are answered with no\n");
	printf("  --allyesconfig          New config where all options are answered with yes\n");
	printf("  --allmodconfig          New config where all options are answered with mod\n");
	printf("  --alldefconfig          New config with all symbols set to default\n");
	printf("  --randconfig            New config with random answer to all options\n");
	printf("  --yes2modconfig         Change answers from yes to mod if possible\n");
	printf("  --mod2yesconfig         Change answers from mod to yes if possible\n");
}

/**
 * @brief Main entry point for the Kconfig configuration tool.
 *
 * This function processes command-line arguments to determine the configuration mode
 * and performs the corresponding actions. It supports various configuration modes such as
 * `defconfig`, `savedefconfig`, `randconfig`, `oldconfig`, `allnoconfig`, `allyesconfig`,
 * `allmodconfig`, `alldefconfig`, and more. The function reads and writes configuration files,
 * sets default values for new symbols, and handles configuration updates.
 *
 * @param ac The number of command-line arguments.
 * @param av An array of command-line argument strings.
 *
 * @return Returns 0 on success, or 1 if an error occurs during configuration processing.
 *
 * @details The function performs the following steps:
 * 1. Initializes the program name and checks if standard input/output is a terminal.
 * 2. Parses command-line options using `getopt_long` to determine the configuration mode.
 * 3. Reads the specified Kconfig file and processes it based on the selected mode.
 * 4. Sets default values for new symbols depending on the mode (e.g., `def_yes`, `def_no`).
 * 5. Handles special cases like `randconfig` by seeding the random number generator.
 * 6. Writes the configuration to the appropriate files (e.g., `.config`, `auto.conf`).
 * 7. Ensures compatibility with GNU Make by creating or updating `auto.conf` if necessary.
 */
int main(int ac, char **av)
{
	const char *progname = av[0];
	int opt;
	const char *name, *defconfig_file = NULL /* gcc uninit */;
	int no_conf_write = 0;

	tty_stdio = isatty(0) && isatty(1);

	while ((opt = getopt_long(ac, av, "s", long_opts, NULL)) != -1) {
		if (opt == 's') {
			conf_set_message_callback(NULL);
			continue;
		}
		input_mode = (enum input_mode)opt;
		switch (opt) {
		case syncconfig:
			/*
			 * syncconfig is invoked during the build stage.
			 * Suppress distracting "configuration written to ..."
			 */
			conf_set_message_callback(NULL);
			sync_kconfig = 1;
			break;
		case defconfig:
		case savedefconfig:
			defconfig_file = optarg;
			break;
		case randconfig:
		{
			struct timeval now;
			unsigned int seed;
			char *seed_env;

			/*
			 * Use microseconds derived seed,
			 * compensate for systems where it may be zero
			 */
			gettimeofday(&now, NULL);
			seed = (unsigned int)((now.tv_sec + 1) * (now.tv_usec + 1));

			seed_env = getenv("KCONFIG_SEED");
			if( seed_env && *seed_env ) {
				char *endp;
				int tmp = (int)strtol(seed_env, &endp, 0);
				if (*endp == '\0') {
					seed = tmp;
				}
			}
			fprintf( stderr, "KCONFIG_SEED=0x%X\n", seed );
			srand(seed);
			break;
		}
		case oldaskconfig:
		case oldconfig:
		case allnoconfig:
		case allyesconfig:
		case allmodconfig:
		case alldefconfig:
		case listnewconfig:
		case helpnewconfig:
		case olddefconfig:
		case yes2modconfig:
		case mod2yesconfig:
			break;
		case '?':
			conf_usage(progname);
			exit(1);
			break;
		}
	}
	if (ac == optind) {
		fprintf(stderr, "%s: Kconfig file missing\n", av[0]);
		conf_usage(progname);
		exit(1);
	}
	name = av[optind];
	conf_parse(name);
	//zconfdump(stdout);

	switch (input_mode) {
	case defconfig:
		if (conf_read(defconfig_file)) {
			fprintf(stderr,
				"***\n"
				  "*** Can't find default configuration \"%s\"!\n"
				  "***\n",
				defconfig_file);
			exit(1);
		}
		break;
	case savedefconfig:
	case syncconfig:
	case oldaskconfig:
	case oldconfig:
	case listnewconfig:
	case helpnewconfig:
	case olddefconfig:
	case yes2modconfig:
	case mod2yesconfig:
		conf_read(NULL);
		break;
	case allnoconfig:
	case allyesconfig:
	case allmodconfig:
	case alldefconfig:
	case randconfig:
		name = getenv("KCONFIG_ALLCONFIG");
		if (!name)
			break;
		if ((strcmp(name, "") != 0) && (strcmp(name, "1") != 0)) {
			if (conf_read_simple(name, S_DEF_USER)) {
				fprintf(stderr,
					"*** Can't read seed configuration \"%s\"!\n",
					name);
				exit(1);
			}
			break;
		}
		switch (input_mode) {
		case allnoconfig:	name = "allno.config"; break;
		case allyesconfig:	name = "allyes.config"; break;
		case allmodconfig:	name = "allmod.config"; break;
		case alldefconfig:	name = "alldef.config"; break;
		case randconfig:	name = "allrandom.config"; break;
		default: break;
		}
		if (conf_read_simple(name, S_DEF_USER) &&
		    conf_read_simple("all.config", S_DEF_USER)) {
			fprintf(stderr,
				"*** KCONFIG_ALLCONFIG set, but no \"%s\" or \"all.config\" file found\n",
				name);
			exit(1);
		}
		break;
	default:
		break;
	}

	if (sync_kconfig) {
		name = getenv("KCONFIG_NOSILENTUPDATE");
		if (name && *name) {
			if (conf_get_changed()) {
				fprintf(stderr,
					"\n*** The configuration requires explicit update.\n\n");
				return 1;
			}
			no_conf_write = 1;
		}
	}

	switch (input_mode) {
	case allnoconfig:
		conf_set_all_new_symbols(def_no);
		break;
	case allyesconfig:
		conf_set_all_new_symbols(def_yes);
		break;
	case allmodconfig:
		conf_set_all_new_symbols(def_mod);
		break;
	case alldefconfig:
		conf_set_all_new_symbols(def_default);
		break;
	case randconfig:
		/* Really nothing to do in this loop */
		while (conf_set_all_new_symbols(def_random)) ;
		break;
	case defconfig:
		conf_set_all_new_symbols(def_default);
		break;
	case savedefconfig:
		break;
	case yes2modconfig:
		conf_rewrite_mod_or_yes(def_y2m);
		break;
	case mod2yesconfig:
		conf_rewrite_mod_or_yes(def_m2y);
		break;
	case oldaskconfig:
		rootEntry = &rootmenu;
		conf(&rootmenu);
		input_mode = oldconfig;
		/* fall through */
	case oldconfig:
	case listnewconfig:
	case helpnewconfig:
	case syncconfig:
		/* Update until a loop caused no more changes */
		do {
			conf_cnt = 0;
			check_conf(&rootmenu);
		} while (conf_cnt);
		break;
	case olddefconfig:
	default:
		break;
	}

	if (input_mode == savedefconfig) {
		if (conf_write_defconfig(defconfig_file)) {
			fprintf(stderr, "n*** Error while saving defconfig to: %s\n\n",
				defconfig_file);
			return 1;
		}
	} else if (input_mode != listnewconfig && input_mode != helpnewconfig) {
		if (!no_conf_write && conf_write(NULL)) {
			fprintf(stderr, "\n*** Error during writing of the configuration.\n\n");
			exit(1);
		}

		/*
		 * Create auto.conf if it does not exist.
		 * This prevents GNU Make 4.1 or older from emitting
		 * "include/config/auto.conf: No such file or directory"
		 * in the top-level Makefile
		 *
		 * syncconfig always creates or updates auto.conf because it is
		 * used during the build.
		 */
		if (conf_write_autoconf(sync_kconfig) && sync_kconfig) {
			fprintf(stderr,
				"\n*** Error during sync of the configuration.\n\n");
			return 1;
		}
	}
	return 0;
}
