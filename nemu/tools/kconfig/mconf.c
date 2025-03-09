// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2002 Roman Zippel <zippel@linux-m68k.org>
 *
 * Introduced single menu mode (show all sub-menus in one large tree).
 * 2002-11-06 Petr Baudis <pasky@ucw.cz>
 *
 * i18n, 2005, Arnaldo Carvalho de Melo <acme@conectiva.com.br>
 */

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <signal.h>
#include <unistd.h>

#include "lkc.h"
#include "lxdialog/dialog.h"

static const char mconf_readme[] =
"Overview\n"
"--------\n"
"This interface lets you select features and parameters for the build.\n"
"Features can either be built-in, modularized, or ignored. Parameters\n"
"must be entered in as decimal or hexadecimal numbers or text.\n"
"\n"
"Menu items beginning with following braces represent features that\n"
"  [ ] can be built in or removed\n"
"  < > can be built in, modularized or removed\n"
"  { } can be built in or modularized (selected by other feature)\n"
"  - - are selected by other feature,\n"
"while *, M or whitespace inside braces means to build in, build as\n"
"a module or to exclude the feature respectively.\n"
"\n"
"To change any of these features, highlight it with the cursor\n"
"keys and press <Y> to build it in, <M> to make it a module or\n"
"<N> to remove it.  You may also press the <Space Bar> to cycle\n"
"through the available options (i.e. Y->N->M->Y).\n"
"\n"
"Some additional keyboard hints:\n"
"\n"
"Menus\n"
"----------\n"
"o  Use the Up/Down arrow keys (cursor keys) to highlight the item you\n"
"   wish to change or the submenu you wish to select and press <Enter>.\n"
"   Submenus are designated by \"--->\", empty ones by \"----\".\n"
"\n"
"   Shortcut: Press the option's highlighted letter (hotkey).\n"
"             Pressing a hotkey more than once will sequence\n"
"             through all visible items which use that hotkey.\n"
"\n"
"   You may also use the <PAGE UP> and <PAGE DOWN> keys to scroll\n"
"   unseen options into view.\n"
"\n"
"o  To exit a menu use the cursor keys to highlight the <Exit> button\n"
"   and press <ENTER>.\n"
"\n"
"   Shortcut: Press <ESC><ESC> or <E> or <X> if there is no hotkey\n"
"             using those letters.  You may press a single <ESC>, but\n"
"             there is a delayed response which you may find annoying.\n"
"\n"
"   Also, the <TAB> and cursor keys will cycle between <Select>,\n"
"   <Exit>, <Help>, <Save>, and <Load>.\n"
"\n"
"o  To get help with an item, use the cursor keys to highlight <Help>\n"
"   and press <ENTER>.\n"
"\n"
"   Shortcut: Press <H> or <?>.\n"
"\n"
"o  To toggle the display of hidden options, press <Z>.\n"
"\n"
"\n"
"Radiolists  (Choice lists)\n"
"-----------\n"
"o  Use the cursor keys to select the option you wish to set and press\n"
"   <S> or the <SPACE BAR>.\n"
"\n"
"   Shortcut: Press the first letter of the option you wish to set then\n"
"             press <S> or <SPACE BAR>.\n"
"\n"
"o  To see available help for the item, use the cursor keys to highlight\n"
"   <Help> and Press <ENTER>.\n"
"\n"
"   Shortcut: Press <H> or <?>.\n"
"\n"
"   Also, the <TAB> and cursor keys will cycle between <Select> and\n"
"   <Help>\n"
"\n"
"\n"
"Data Entry\n"
"-----------\n"
"o  Enter the requested information and press <ENTER>\n"
"   If you are entering hexadecimal values, it is not necessary to\n"
"   add the '0x' prefix to the entry.\n"
"\n"
"o  For help, use the <TAB> or cursor keys to highlight the help option\n"
"   and press <ENTER>.  You can try <TAB><H> as well.\n"
"\n"
"\n"
"Text Box    (Help Window)\n"
"--------\n"
"o  Use the cursor keys to scroll up/down/left/right.  The VI editor\n"
"   keys h,j,k,l function here as do <u>, <d>, <SPACE BAR> and <B> for\n"
"   those who are familiar with less and lynx.\n"
"\n"
"o  Press <E>, <X>, <q>, <Enter> or <Esc><Esc> to exit.\n"
"\n"
"\n"
"Alternate Configuration Files\n"
"-----------------------------\n"
"Menuconfig supports the use of alternate configuration files for\n"
"those who, for various reasons, find it necessary to switch\n"
"between different configurations.\n"
"\n"
"The <Save> button will let you save the current configuration to\n"
"a file of your choosing.  Use the <Load> button to load a previously\n"
"saved alternate configuration.\n"
"\n"
"Even if you don't use alternate configuration files, but you find\n"
"during a Menuconfig session that you have completely messed up your\n"
"settings, you may use the <Load> button to restore your previously\n"
"saved settings from \".config\" without restarting Menuconfig.\n"
"\n"
"Other information\n"
"-----------------\n"
"If you use Menuconfig in an XTERM window, make sure you have your\n"
"$TERM variable set to point to an xterm definition which supports\n"
"color.  Otherwise, Menuconfig will look rather bad.  Menuconfig will\n"
"not display correctly in an RXVT window because rxvt displays only one\n"
"intensity of color, bright.\n"
"\n"
"Menuconfig will display larger menus on screens or xterms which are\n"
"set to display more than the standard 25 row by 80 column geometry.\n"
"In order for this to work, the \"stty size\" command must be able to\n"
"display the screen's current row and column geometry.  I STRONGLY\n"
"RECOMMEND that you make sure you do NOT have the shell variables\n"
"LINES and COLUMNS exported into your environment.  Some distributions\n"
"export those variables via /etc/profile.  Some ncurses programs can\n"
"become confused when those variables (LINES & COLUMNS) don't reflect\n"
"the true screen size.\n"
"\n"
"Optional personality available\n"
"------------------------------\n"
"If you prefer to have all of the options listed in a single menu,\n"
"rather than the default multimenu hierarchy, run the menuconfig with\n"
"MENUCONFIG_MODE environment variable set to single_menu. Example:\n"
"\n"
"make MENUCONFIG_MODE=single_menu menuconfig\n"
"\n"
"<Enter> will then unroll the appropriate category, or enfold it if it\n"
"is already unrolled.\n"
"\n"
"Note that this mode can eventually be a little more CPU expensive\n"
"(especially with a larger number of unrolled categories) than the\n"
"default mode.\n"
"\n"
"Different color themes available\n"
"--------------------------------\n"
"It is possible to select different color themes using the variable\n"
"MENUCONFIG_COLOR. To select a theme use:\n"
"\n"
"make MENUCONFIG_COLOR=<theme> menuconfig\n"
"\n"
"Available themes are\n"
" mono       => selects colors suitable for monochrome displays\n"
" blackbg    => selects a color scheme with black background\n"
" classic    => theme with blue background. The classic look\n"
" bluetitle  => an LCD friendly version of classic. (default)\n"
"\n",
menu_instructions[] =
	"Arrow keys navigate the menu.  "
	"<Enter> selects submenus ---> (or empty submenus ----).  "
	"Highlighted letters are hotkeys.  "
	"Pressing <Y> includes, <N> excludes, <M> modularizes features.  "
	"Press <Esc><Esc> to exit, <?> for Help, </> for Search.  "
	"Legend: [*] built-in  [ ] excluded  <M> module  < > module capable",
radiolist_instructions[] =
	"Use the arrow keys to navigate this window or "
	"press the hotkey of the item you wish to select "
	"followed by the <SPACE BAR>. "
	"Press <?> for additional information about this option.",
inputbox_instructions_int[] =
	"Please enter a decimal value. "
	"Fractions will not be accepted.  "
	"Use the <TAB> key to move from the input field to the buttons below it.",
inputbox_instructions_hex[] =
	"Please enter a hexadecimal value. "
	"Use the <TAB> key to move from the input field to the buttons below it.",
inputbox_instructions_string[] =
	"Please enter a string value. "
	"Use the <TAB> key to move from the input field to the buttons below it.",
setmod_text[] =
	"This feature depends on another which has been configured as a module.\n"
	"As a result, this feature will be built as a module.",
load_config_text[] =
	"Enter the name of the configuration file you wish to load.  "
	"Accept the name shown to restore the configuration you "
	"last retrieved.  Leave blank to abort.",
load_config_help[] =
	"\n"
	"For various reasons, one may wish to keep several different\n"
	"configurations available on a single machine.\n"
	"\n"
	"If you have saved a previous configuration in a file other than the\n"
	"default one, entering its name here will allow you to modify that\n"
	"configuration.\n"
	"\n"
	"If you are uncertain, then you have probably never used alternate\n"
	"configuration files. You should therefore leave this blank to abort.\n",
save_config_text[] =
	"Enter a filename to which this configuration should be saved "
	"as an alternate.  Leave blank to abort.",
save_config_help[] =
	"\n"
	"For various reasons, one may wish to keep different configurations\n"
	"available on a single machine.\n"
	"\n"
	"Entering a file name here will allow you to later retrieve, modify\n"
	"and use the current configuration as an alternate to whatever\n"
	"configuration options you have selected at that time.\n"
	"\n"
	"If you are uncertain what all this means then you should probably\n"
	"leave this blank.\n",
search_help[] =
	"\n"
	"Search for symbols and display their relations.\n"
	"Regular expressions are allowed.\n"
	"Example: search for \"^FOO\"\n"
	"Result:\n"
	"-----------------------------------------------------------------\n"
	"Symbol: FOO [=m]\n"
	"Type  : tristate\n"
	"Prompt: Foo bus is used to drive the bar HW\n"
	"  Location:\n"
	"    -> Bus options (PCI, PCMCIA, EISA, ISA)\n"
	"      -> PCI support (PCI [=y])\n"
	"(1)     -> PCI access mode (<choice> [=y])\n"
	"  Defined at drivers/pci/Kconfig:47\n"
	"  Depends on: X86_LOCAL_APIC && X86_IO_APIC || IA64\n"
	"  Selects: LIBCRC32\n"
	"  Selected by: BAR [=n]\n"
	"-----------------------------------------------------------------\n"
	"o The line 'Type:' shows the type of the configuration option for\n"
	"  this symbol (bool, tristate, string, ...)\n"
	"o The line 'Prompt:' shows the text used in the menu structure for\n"
	"  this symbol\n"
	"o The 'Defined at' line tells at what file / line number the symbol\n"
	"  is defined\n"
	"o The 'Depends on:' line tells what symbols need to be defined for\n"
	"  this symbol to be visible in the menu (selectable)\n"
	"o The 'Location:' lines tells where in the menu structure this symbol\n"
	"  is located\n"
	"    A location followed by a [=y] indicates that this is a\n"
	"    selectable menu item - and the current value is displayed inside\n"
	"    brackets.\n"
	"    Press the key in the (#) prefix to jump directly to that\n"
	"    location. You will be returned to the current search results\n"
	"    after exiting this new menu.\n"
	"o The 'Selects:' line tells what symbols will be automatically\n"
	"  selected if this symbol is selected (y or m)\n"
	"o The 'Selected by' line tells what symbol has selected this symbol\n"
	"\n"
	"Only relevant lines are shown.\n"
	"\n\n"
	"Search examples:\n"
	"Examples: USB	=> find all symbols containing USB\n"
	"          ^USB => find all symbols starting with USB\n"
	"          USB$ => find all symbols ending with USB\n"
	"\n";

static int indent;
static struct menu *current_menu;
static int child_count;
static int single_menu_mode;
static int show_all_options;
static int save_and_exit;
static int silent;

static void conf(struct menu *menu, struct menu *active_menu);
static void conf_choice(struct menu *menu);
static void conf_string(struct menu *menu);
static void conf_load(void);
static void conf_save(void);
static int show_textbox_ext(const char *title, char *text, int r, int c,
			    int *keys, int *vscroll, int *hscroll,
			    update_text_fn update_text, void *data);
static void show_textbox(const char *title, const char *text, int r, int c);
static void show_helptext(const char *title, const char *text);
static void show_help(struct menu *menu);

static char filename[PATH_MAX+1];
/**
 * Sets the configuration filename and updates the menu backtitle accordingly.
 * 
 * This function takes a configuration filename as input and performs the following operations:
 * 1. Constructs a new menu backtitle by concatenating the provided `config_filename` with the 
 *    text from `rootmenu.prompt->text`. The result is stored in a static buffer `menu_backtitle`.
 *    If the concatenated string exceeds the buffer size, it is truncated to fit.
 * 2. Updates the dialog backtitle using the constructed `menu_backtitle` string.
 * 3. Stores the provided `config_filename` in a global or static `filename` buffer. If the filename 
 *    exceeds the buffer size, it is truncated to fit.
 * 
 * @param config_filename The configuration filename to be set and used in the menu backtitle.
 */
static void set_config_filename(const char *config_filename)
{
	static char menu_backtitle[PATH_MAX+128];
	int size;

	size = snprintf(menu_backtitle, sizeof(menu_backtitle),
			"%s - %s", config_filename, rootmenu.prompt->text);
	if (size >= sizeof(menu_backtitle))
		menu_backtitle[sizeof(menu_backtitle)-1] = '\0';
	set_dialog_backtitle(menu_backtitle);

	size = snprintf(filename, sizeof(filename), "%s", config_filename);
	if (size >= sizeof(filename))
		filename[sizeof(filename)-1] = '\0';
}

struct subtitle_part {
	struct list_head entries;
	const char *text;
};
static LIST_HEAD(trail);

static struct subtitle_list *subtitles;
/**
 * set_subtitle - Updates the subtitle list based on the trail entries.
 *
 * This function performs the following operations:
 * 1. Clears the existing subtitle list by iterating through the `subtitles` linked list,
 *    freeing each node, and setting `subtitles` to NULL.
 * 2. Iterates through the `trail` list using `list_for_each_entry`, and for each entry
 *    with a non-NULL `text` field, allocates a new node in the `subtitles` linked list
 *    and copies the `text` from the trail entry to the new node.
 * 3. Calls `set_dialog_subtitles` with the updated `subtitles` list to apply the changes.
 *
 * The function ensures that the subtitle list is dynamically updated and memory is
 * properly managed by freeing the old list and allocating new nodes as needed.
 */
static void set_subtitle(void)
{
	struct subtitle_part *sp;
	struct subtitle_list *pos, *tmp;

	for (pos = subtitles; pos != NULL; pos = tmp) {
		tmp = pos->next;
		free(pos);
	}

	subtitles = NULL;
	list_for_each_entry(sp, &trail, entries) {
		if (sp->text) {
			if (pos) {
				pos->next = xcalloc(1, sizeof(*pos));
				pos = pos->next;
			} else {
				subtitles = pos = xcalloc(1, sizeof(*pos));
			}
			pos->text = sp->text;
		}
	}

	set_dialog_subtitles(subtitles);
}

/**
 * Resets the subtitle list by freeing all allocated memory for the subtitle entries
 * and setting the global subtitle list pointer to NULL. Additionally, it updates
 * the dialog subtitles to reflect the cleared state.
 *
 * This function iterates through the linked list of subtitles, starting from the
 * global `subtitles` pointer, and frees each node in the list. After all nodes
 * are freed, the `subtitles` pointer is set to NULL to indicate an empty list.
 * Finally, it calls `set_dialog_subtitles` with NULL to ensure that any UI or
 * dialog elements displaying subtitles are updated to reflect the reset state.
 *
 * @note This function assumes that `subtitles` is the head of a valid linked list
 *       or NULL. It does not handle cases where the list is corrupted.
 */
static void reset_subtitle(void)
{
	struct subtitle_list *pos, *tmp;

	for (pos = subtitles; pos != NULL; pos = tmp) {
		tmp = pos->next;
		free(pos);
	}
	subtitles = NULL;
	set_dialog_subtitles(subtitles);
}

struct search_data {
	struct list_head *head;
	struct menu **targets;
	int *keys;
};

/**
 * Updates a text buffer by inserting jump key headers at specified positions.
 *
 * This function iterates through a list of `jump_key` structures and inserts
 * headers into the provided buffer (`buf`) at positions defined by the `offset`
 * field of each `jump_key`. The headers are inserted only if the `offset` falls
 * within the range [`start`, `end`). The headers are formatted as `(x)`, where
 * `x` is a numeric key derived from the `index` field of the `jump_key`. The
 * function also stores the associated key and target in the `search_data`
 * structure for later use.
 *
 * @param buf The text buffer to be updated.
 * @param start The starting position in the buffer (inclusive) where headers
 *              can be inserted.
 * @param end The ending position in the buffer (exclusive) where headers can
 *            be inserted.
 * @param _data A pointer to a `search_data` structure containing the list of
 *              `jump_key` entries and storage for keys and targets.
 */
static void update_text(char *buf, size_t start, size_t end, void *_data)
{
	struct search_data *data = _data;
	struct jump_key *pos;
	int k = 0;

	list_for_each_entry(pos, data->head, entries) {
		if (pos->offset >= start && pos->offset < end) {
			char header[4];

			if (k < JUMP_NB) {
				int key = '0' + (pos->index % JUMP_NB) + 1;

				sprintf(header, "(%c)", key);
				data->keys[k] = key;
				data->targets[k] = pos->target;
				k++;
			} else {
				sprintf(header, "   ");
			}

			memcpy(buf + pos->offset, header, sizeof(header) - 1);
		}
	}
	data->keys[k] = 0;
}

/**
 * Searches for configuration parameters based on user input.
 * 
 * This function prompts the user to enter a search string or regular expression
 * to search for configuration parameters. The search can include or exclude the
 * configuration prefix (CONFIG_). The function displays the search results in a
 * textbox and allows the user to navigate through the results. If the user selects
 * a result, the corresponding configuration menu is displayed. The function handles
 * user input, performs the search, and manages the display of results and navigation
 * through the configuration menu hierarchy.
 * 
 * The function uses dialog boxes for user interaction and dynamically allocates
 * and frees memory for strings and search results. It also handles scrolling and
 * updates the display based on user actions.
 * 
 * @note The function assumes that the necessary dialog and string handling functions
 * are available and properly initialized. It also assumes that the global configuration
 * structure and menu hierarchy are correctly set up.
 */
static void search_conf(void)
{
	struct symbol **sym_arr;
	struct gstr res;
	struct gstr title;
	char *dialog_input;
	int dres, vscroll = 0, hscroll = 0;
	bool again;
	struct gstr sttext;
	struct subtitle_part stpart;

	title = str_new();
	str_printf( &title, "Enter (sub)string or regexp to search for "
			      "(with or without \"%s\")", CONFIG_);

again:
	dialog_clear();
	dres = dialog_inputbox("Search Configuration Parameter",
			      str_get(&title),
			      10, 75, "");
	switch (dres) {
	case 0:
		break;
	case 1:
		show_helptext("Search Configuration", search_help);
		goto again;
	default:
		str_free(&title);
		return;
	}

	/* strip the prefix if necessary */
	dialog_input = dialog_input_result;
	if (strncasecmp(dialog_input_result, CONFIG_, strlen(CONFIG_)) == 0)
		dialog_input += strlen(CONFIG_);

	sttext = str_new();
	str_printf(&sttext, "Search (%s)", dialog_input_result);
	stpart.text = str_get(&sttext);
	list_add_tail(&stpart.entries, &trail);

	sym_arr = sym_re_search(dialog_input);
	do {
		LIST_HEAD(head);
		struct menu *targets[JUMP_NB];
		int keys[JUMP_NB + 1], i;
		struct search_data data = {
			.head = &head,
			.targets = targets,
			.keys = keys,
		};
		struct jump_key *pos, *tmp;

		res = get_relations_str(sym_arr, &head);
		set_subtitle();
		dres = show_textbox_ext("Search Results", (char *)
					str_get(&res), 0, 0, keys, &vscroll,
					&hscroll, &update_text, (void *)
					&data);
		again = false;
		for (i = 0; i < JUMP_NB && keys[i]; i++)
			if (dres == keys[i]) {
				conf(targets[i]->parent, targets[i]);
				again = true;
			}
		str_free(&res);
		list_for_each_entry_safe(pos, tmp, &head, entries)
			free(pos);
	} while (again);
	free(sym_arr);
	str_free(&title);
	list_del(trail.prev);
	str_free(&sttext);
}

/**
 * build_conf - Recursively builds the configuration menu structure.
 *
 * This function processes a given menu and its children to build a hierarchical
 * configuration menu. It handles different types of menu items, including
 * prompts, choices, and comments, and updates the display accordingly. The
 * function also manages indentation to reflect the hierarchical structure of
 * the menu.
 *
 * @param menu Pointer to the menu structure to be processed.
 *
 * The function performs the following operations:
 * 1. Checks if the menu is visible and whether it should be displayed based on
 *    the `show_all_options` flag.
 * 2. Processes the menu's symbol and prompt, handling different types of
 *    prompts (e.g., menus, comments).
 * 3. For choice symbols, it determines the default value and updates the
 *    display accordingly.
 * 4. For other symbols, it updates the display based on the symbol's type
 *    (e.g., boolean, tristate) and value.
 * 5. Recursively processes child menus, adjusting indentation to reflect the
 *    menu hierarchy.
 *
 * The function uses various helper functions like `menu_is_visible`,
 * `menu_has_prompt`, `sym_get_type`, `sym_get_tristate_value`, and
 * `menu_get_prompt` to determine the state and properties of the menu and its
 * symbols.
 */
static void build_conf(struct menu *menu)
{
	struct symbol *sym;
	struct property *prop;
	struct menu *child;
	int type, tmp, doint = 2;
	tristate val;
	char ch;
	bool visible;

	/*
	 * note: menu_is_visible() has side effect that it will
	 * recalc the value of the symbol.
	 */
	visible = menu_is_visible(menu);
	if (show_all_options && !menu_has_prompt(menu))
		return;
	else if (!show_all_options && !visible)
		return;

	sym = menu->sym;
	prop = menu->prompt;
	if (!sym) {
		if (prop && menu != current_menu) {
			const char *prompt = menu_get_prompt(menu);
			switch (prop->type) {
			case P_MENU:
				child_count++;
				if (single_menu_mode) {
					item_make("%s%*c%s",
						  menu->data ? "-->" : "++>",
						  indent + 1, ' ', prompt);
				} else
					item_make("   %*c%s  %s",
						  indent + 1, ' ', prompt,
						  menu_is_empty(menu) ? "----" : "--->");
				item_set_tag('m');
				item_set_data(menu);
				if (single_menu_mode && menu->data)
					goto conf_childs;
				return;
			case P_COMMENT:
				if (prompt) {
					child_count++;
					item_make("   %*c*** %s ***", indent + 1, ' ', prompt);
					item_set_tag(':');
					item_set_data(menu);
				}
				break;
			default:
				if (prompt) {
					child_count++;
					item_make("---%*c%s", indent + 1, ' ', prompt);
					item_set_tag(':');
					item_set_data(menu);
				}
			}
		} else
			doint = 0;
		goto conf_childs;
	}

	type = sym_get_type(sym);
	if (sym_is_choice(sym)) {
		struct symbol *def_sym = sym_get_choice_value(sym);
		struct menu *def_menu = NULL;

		child_count++;
		for (child = menu->list; child; child = child->next) {
			if (menu_is_visible(child) && child->sym == def_sym)
				def_menu = child;
		}

		val = sym_get_tristate_value(sym);
		if (sym_is_changeable(sym)) {
			switch (type) {
			case S_BOOLEAN:
				item_make("[%c]", val == no ? ' ' : '*');
				break;
			case S_TRISTATE:
				switch (val) {
				case yes: ch = '*'; break;
				case mod: ch = 'M'; break;
				default:  ch = ' '; break;
				}
				item_make("<%c>", ch);
				break;
			}
			item_set_tag('t');
			item_set_data(menu);
		} else {
			item_make("   ");
			item_set_tag(def_menu ? 't' : ':');
			item_set_data(menu);
		}

		item_add_str("%*c%s", indent + 1, ' ', menu_get_prompt(menu));
		if (val == yes) {
			if (def_menu) {
				item_add_str(" (%s)", menu_get_prompt(def_menu));
				item_add_str("  --->");
				if (def_menu->list) {
					indent += 2;
					build_conf(def_menu);
					indent -= 2;
				}
			}
			return;
		}
	} else {
		if (menu == current_menu) {
			item_make("---%*c%s", indent + 1, ' ', menu_get_prompt(menu));
			item_set_tag(':');
			item_set_data(menu);
			goto conf_childs;
		}
		child_count++;
		val = sym_get_tristate_value(sym);
		if (sym_is_choice_value(sym) && val == yes) {
			item_make("   ");
			item_set_tag(':');
			item_set_data(menu);
		} else {
			switch (type) {
			case S_BOOLEAN:
				if (sym_is_changeable(sym))
					item_make("[%c]", val == no ? ' ' : '*');
				else
					item_make("-%c-", val == no ? ' ' : '*');
				item_set_tag('t');
				item_set_data(menu);
				break;
			case S_TRISTATE:
				switch (val) {
				case yes: ch = '*'; break;
				case mod: ch = 'M'; break;
				default:  ch = ' '; break;
				}
				if (sym_is_changeable(sym)) {
					if (sym->rev_dep.tri == mod)
						item_make("{%c}", ch);
					else
						item_make("<%c>", ch);
				} else
					item_make("-%c-", ch);
				item_set_tag('t');
				item_set_data(menu);
				break;
			default:
				tmp = 2 + strlen(sym_get_string_value(sym)); /* () = 2 */
				item_make("(%s)", sym_get_string_value(sym));
				tmp = indent - tmp + 4;
				if (tmp < 0)
					tmp = 0;
				item_add_str("%*c%s%s", tmp, ' ', menu_get_prompt(menu),
					     (sym_has_value(sym) || !sym_is_changeable(sym)) ?
					     "" : " (NEW)");
				item_set_tag('s');
				item_set_data(menu);
				goto conf_childs;
			}
		}
		item_add_str("%*c%s%s", indent + 1, ' ', menu_get_prompt(menu),
			  (sym_has_value(sym) || !sym_is_changeable(sym)) ?
			  "" : " (NEW)");
		if (menu->prompt->type == P_MENU) {
			item_add_str("  %s", menu_is_empty(menu) ? "----" : "--->");
			return;
		}
	}

conf_childs:
	indent += doint;
	for (child = menu->list; child; child = child->next)
		build_conf(child);
	indent -= doint;
}

/**
 * @brief Handles the configuration menu navigation and interaction.
 *
 * This method is responsible for managing the configuration menu system. It displays the menu,
 * processes user input, and navigates through submenus based on the user's selections. The method
 * supports various operations such as toggling menu items, saving and loading configurations,
 * displaying help text, and handling different types of menu items (e.g., tristate, string, etc.).
 *
 * @param menu A pointer to the current menu structure to be displayed.
 * @param active_menu A pointer to the currently active menu structure, which may be used
 *                    to highlight or focus on a specific menu item.
 *
 * The method performs the following steps:
 * 1. Initializes the menu subtitle and adds it to the trail of visited menus.
 * 2. Enters a loop where it continuously displays the menu and waits for user input.
 * 3. Processes the user's input to determine the next action, such as navigating to a submenu,
 *    toggling a setting, or displaying help.
 * 4. Handles specific cases for different types of menu items (e.g., tristate, string, etc.).
 * 5. Supports operations like saving and loading configurations, displaying help text, and
 *    toggling visibility of all options.
 * 6. Exits the loop and cleans up the menu trail when the user chooses to exit or when no
 *    further child menus are available.
 */
static void conf(struct menu *menu, struct menu *active_menu)
{
	struct menu *submenu;
	const char *prompt = menu_get_prompt(menu);
	struct subtitle_part stpart;
	struct symbol *sym;
	int res;
	int s_scroll = 0;

	if (menu != &rootmenu)
		stpart.text = menu_get_prompt(menu);
	else
		stpart.text = NULL;
	list_add_tail(&stpart.entries, &trail);

	while (1) {
		item_reset();
		current_menu = menu;
		build_conf(menu);
		if (!child_count)
			break;
		set_subtitle();
		dialog_clear();
		res = dialog_menu(prompt ? prompt : "Main Menu",
				  menu_instructions,
				  active_menu, &s_scroll);
		if (res == 1 || res == KEY_ESC || res == -ERRDISPLAYTOOSMALL)
			break;
		if (item_count() != 0) {
			if (!item_activate_selected())
				continue;
			if (!item_tag())
				continue;
		}
		submenu = item_data();
		active_menu = item_data();
		if (submenu)
			sym = submenu->sym;
		else
			sym = NULL;

		switch (res) {
		case 0:
			switch (item_tag()) {
			case 'm':
				if (single_menu_mode)
					submenu->data = (void *) (long) !submenu->data;
				else
					conf(submenu, NULL);
				break;
			case 't':
				if (sym_is_choice(sym) && sym_get_tristate_value(sym) == yes)
					conf_choice(submenu);
				else if (submenu->prompt->type == P_MENU)
					conf(submenu, NULL);
				break;
			case 's':
				conf_string(submenu);
				break;
			}
			break;
		case 2:
			if (sym)
				show_help(submenu);
			else {
				reset_subtitle();
				show_helptext("README", mconf_readme);
			}
			break;
		case 3:
			reset_subtitle();
			conf_save();
			break;
		case 4:
			reset_subtitle();
			conf_load();
			break;
		case 5:
			if (item_is_tag('t')) {
				if (sym_set_tristate_value(sym, yes))
					break;
				if (sym_set_tristate_value(sym, mod))
					show_textbox(NULL, setmod_text, 6, 74);
			}
			break;
		case 6:
			if (item_is_tag('t'))
				sym_set_tristate_value(sym, no);
			break;
		case 7:
			if (item_is_tag('t'))
				sym_set_tristate_value(sym, mod);
			break;
		case 8:
			if (item_is_tag('t'))
				sym_toggle_tristate_value(sym);
			else if (item_is_tag('m'))
				conf(submenu, NULL);
			break;
		case 9:
			search_conf();
			break;
		case 10:
			show_all_options = !show_all_options;
			break;
		}
	}

	list_del(trail.prev);
}

/**
 * Displays a textbox dialog with the specified title and text content.
 * The textbox allows the user to scroll through the text vertically and horizontally.
 * The function also supports custom key bindings and an optional callback to update
 * the text dynamically.
 *
 * @param title       The title of the textbox dialog. If NULL, no title is displayed.
 * @param text        The text content to display in the textbox. Must be a null-terminated string.
 * @param r           The number of rows in the textbox. If 0, the height is calculated automatically.
 * @param c           The number of columns in the textbox. If 0, the width is calculated automatically.
 * @param keys        An array of key bindings for custom actions. If NULL, default bindings are used.
 * @param vscroll     Pointer to store the vertical scroll position. Can be NULL.
 * @param hscroll     Pointer to store the horizontal scroll position. Can be NULL.
 * @param update_text Callback function to dynamically update the text. Can be NULL.
 * @param data        User-defined data passed to the update_text callback. Can be NULL.
 *
 * @return            The key code of the user's action (e.g., ESC, ENTER, or a custom key).
 *                    Returns -1 if an error occurs.
 */
static int show_textbox_ext(const char *title, char *text, int r, int c, int *keys, int *vscroll, int *hscroll, update_text_fn update_text, void *data)
{
	dialog_clear();
	return dialog_textbox(title, text, r, c, keys, vscroll, hscroll, update_text, data);
}

/**
 * Displays a text box with the specified title and text at the given position.
 * 
 * This function is a convenience wrapper for `show_textbox_ext` that simplifies the
 * display of a basic text box. It takes a title and text to display, along with the
 * row (`r`) and column (`c`) coordinates where the text box should be positioned on
 * the screen. The function internally calls `show_textbox_ext` with default values
 * for additional parameters, such as an empty array for colors and NULL for callbacks.
 *
 * @param title The title of the text box. This will be displayed at the top of the box.
 * @param text The content to display inside the text box.
 * @param r The row position on the screen where the text box should be displayed.
 * @param c The column position on the screen where the text box should be displayed.
 */
static void show_textbox(const char *title, const char *text, int r, int c)
{
	show_textbox_ext(title, (char *) text, r, c, (int []) {0}, NULL, NULL,
			 NULL, NULL);
}

/**
 * Displays a text box with the specified title and content.
 *
 * This function creates and shows a text box using the `show_textbox` function,
 * passing the provided title and text as the content. The text box is positioned
 * at the default coordinates (0, 0) on the screen.
 *
 * @param title The title of the text box. This is displayed at the top of the box.
 * @param text The content to be displayed inside the text box.
 */
static void show_helptext(const char *title, const char *text)
{
	show_textbox(title, text, 0, 0);
}

/**
 * Handles configuration messages based on the current state of the application.
 * 
 * If the `save_and_exit` flag is set, the message `s` is printed to the standard output
 * unless the `silent` flag is also set. If `save_and_exit` is not set, the message is
 * displayed in a textbox with a default size of 6 rows and 60 columns.
 * 
 * @param s The configuration message to be processed and displayed.
 */
static void conf_message_callback(const char *s)
{
	if (save_and_exit) {
		if (!silent)
			printf("%s", s);
	} else {
		show_textbox(NULL, s, 6, 60);
	}
}

/**
 * Displays help text for a given menu in a formatted manner.
 * 
 * This function generates and displays the extended help text associated with the provided menu.
 * The help text is formatted to fit within the terminal window, with a maximum width calculated
 * based on the current terminal width minus a margin of 10 characters. The function first creates
 * a dynamic string (`gstr`) to store the help text, retrieves the extended help text for the menu,
 * and then displays it using the `show_helptext` function. Finally, the dynamically allocated string
 * is freed to avoid memory leaks.
 * 
 * @param menu A pointer to the `menu` structure for which the help text is to be displayed.
 */
static void show_help(struct menu *menu)
{
	struct gstr help = str_new();

	help.max_width = getmaxx(stdscr) - 10;
	menu_get_ext_help(menu, &help);

	show_helptext(menu_get_prompt(menu), str_get(&help));
	str_free(&help);
}

/**
 * Handles the configuration choice selection for a given menu in a text-based UI.
 * 
 * This function presents a checklist dialog to the user, allowing them to select a choice
 * from the provided menu. The menu's prompt is displayed at the top of the dialog, and
 * each visible child menu item is listed as an option. The currently active choice is
 * pre-selected. The user can navigate through the options, select a new choice, or view
 * help information for a specific option.
 *
 * The function operates in a loop, continuously displaying the dialog until the user
 * either makes a selection, cancels the operation, or the display is too small to render
 * the dialog properly. If the user selects a new choice, the corresponding symbol is set
 * to 'yes'. If the user requests help, the help information for the selected item or the
 * parent menu is displayed.
 *
 * @param menu Pointer to the menu structure representing the current menu. The menu's
 *             prompt and child items are used to populate the dialog.
 *
 * @note The function uses a dialog-based UI, and the dialog's dimensions are determined
 *       by the constants MENUBOX_HEIGTH_MIN, MENUBOX_WIDTH_MIN, and CHECKLIST_HEIGTH_MIN.
 *       The function handles errors such as a display that is too small to render the
 *       dialog by returning early.
 */
static void conf_choice(struct menu *menu)
{
	const char *prompt = menu_get_prompt(menu);
	struct menu *child;
	struct symbol *active;

	active = sym_get_choice_value(menu->sym);
	while (1) {
		int res;
		int selected;
		item_reset();

		current_menu = menu;
		for (child = menu->list; child; child = child->next) {
			if (!menu_is_visible(child))
				continue;
			if (child->sym)
				item_make("%s", menu_get_prompt(child));
			else {
				item_make("*** %s ***", menu_get_prompt(child));
				item_set_tag(':');
			}
			item_set_data(child);
			if (child->sym == active)
				item_set_selected(1);
			if (child->sym == sym_get_choice_value(menu->sym))
				item_set_tag('X');
		}
		dialog_clear();
		res = dialog_checklist(prompt ? prompt : "Main Menu",
					radiolist_instructions,
					MENUBOX_HEIGTH_MIN,
					MENUBOX_WIDTH_MIN,
					CHECKLIST_HEIGTH_MIN);
		selected = item_activate_selected();
		switch (res) {
		case 0:
			if (selected) {
				child = item_data();
				if (!child->sym)
					break;

				sym_set_tristate_value(child->sym, yes);
			}
			return;
		case 1:
			if (selected) {
				child = item_data();
				show_help(child);
				active = child->sym;
			} else
				show_help(menu);
			break;
		case KEY_ESC:
			return;
		case -ERRDISPLAYTOOSMALL:
			return;
		}
	}
}

/**
 * conf_string - Handles the configuration of a string, integer, or hexadecimal value
 *               for a given menu item in a dialog-based interface.
 *
 * This function continuously prompts the user to input a value for the specified
 * menu item. The type of input (integer, hexadecimal, or string) is determined by
 * the symbol type associated with the menu. The function displays appropriate
 * instructions based on the input type and validates the user's input. If the input
 * is valid, the function updates the symbol's value and exits. If the input is invalid,
 * an error message is displayed, and the user is prompted again. The user can also
 * access help information or exit the dialog by pressing the ESC key.
 *
 * @param menu Pointer to the menu structure containing the symbol to be configured.
 *             The menu structure includes a prompt and a symbol whose value is to be
 *             set based on user input.
 *
 * The function uses a loop to repeatedly display a dialog box for user input. The
 * dialog box includes a prompt and instructions specific to the input type. The
 * user's input is validated, and if valid, the symbol's value is updated. If the
 * input is invalid, an error message is shown, and the user is prompted again. The
 * loop continues until the user provides valid input or exits the dialog.
 */
static void conf_string(struct menu *menu)
{
	const char *prompt = menu_get_prompt(menu);

	while (1) {
		int res;
		const char *heading;

		switch (sym_get_type(menu->sym)) {
		case S_INT:
			heading = inputbox_instructions_int;
			break;
		case S_HEX:
			heading = inputbox_instructions_hex;
			break;
		case S_STRING:
			heading = inputbox_instructions_string;
			break;
		default:
			heading = "Internal mconf error!";
		}
		dialog_clear();
		res = dialog_inputbox(prompt ? prompt : "Main Menu",
				      heading, 10, 75,
				      sym_get_string_value(menu->sym));
		switch (res) {
		case 0:
			if (sym_set_string_value(menu->sym, dialog_input_result))
				return;
			show_textbox(NULL, "You have made an invalid entry.", 5, 43);
			break;
		case 1:
			show_help(menu);
			break;
		case KEY_ESC:
			return;
		}
	}
}

/**
 * Loads a configuration file interactively through a dialog-based interface.
 *
 * This method continuously displays an input dialog prompting the user to enter
 * the filename of a configuration file to load. The user can:
 * - Enter a filename to attempt loading the configuration.
 * - Access help text for additional information.
 * - Cancel the operation by pressing the ESC key.
 *
 * If the user provides a valid filename and the configuration is successfully
 * loaded, the method updates the global configuration filename and marks the
 * configuration as changed. If the file does not exist, an error message is
 * displayed. The loop continues until the user either successfully loads a
 * configuration or cancels the operation.
 *
 * The method uses the following helper functions:
 * - `dialog_clear()`: Clears the dialog screen.
 * - `dialog_inputbox()`: Displays an input dialog to get the filename.
 * - `conf_read()`: Attempts to read and load the configuration from the file.
 * - `set_config_filename()`: Updates the global configuration filename.
 * - `sym_set_change_count()`: Marks the configuration as changed.
 * - `show_textbox()`: Displays an error message if the file does not exist.
 * - `show_helptext()`: Displays help text for the load configuration operation.
 */
static void conf_load(void)
{
	while (1) {
		int res;
		dialog_clear();
		res = dialog_inputbox(NULL, load_config_text,
				      11, 55, filename);
		switch(res) {
		case 0:
			if (!dialog_input_result[0])
				return;
			if (!conf_read(dialog_input_result)) {
				set_config_filename(dialog_input_result);
				sym_set_change_count(1);
				return;
			}
			show_textbox(NULL, "File does not exist!", 5, 38);
			break;
		case 1:
			show_helptext("Load Alternate Configuration", load_config_help);
			break;
		case KEY_ESC:
			return;
		}
	}
}

/**
 * Saves the current configuration to a file specified by the user.
 *
 * This method enters an infinite loop where it repeatedly prompts the user to input a filename
 * to save the configuration. The user is presented with a dialog box to enter the filename.
 * Depending on the user's response, the method performs the following actions:
 * - If the user confirms the input (res == 0):
 *   - If the input is empty, the method returns without saving.
 *   - If the input is valid, it attempts to write the configuration to the specified file.
 *     - If the write operation is successful, the filename is set as the new configuration filename
 *       and the method returns.
 *     - If the write operation fails, an error message is displayed to the user.
 * - If the user requests help (res == 1), a help text is displayed.
 * - If the user presses the ESC key (res == KEY_ESC), the method returns without saving.
 *
 * The loop continues until the user either successfully saves the configuration or cancels the operation.
 */
static void conf_save(void)
{
	while (1) {
		int res;
		dialog_clear();
		res = dialog_inputbox(NULL, save_config_text,
				      11, 55, filename);
		switch(res) {
		case 0:
			if (!dialog_input_result[0])
				return;
			if (!conf_write(dialog_input_result)) {
				set_config_filename(dialog_input_result);
				return;
			}
			show_textbox(NULL, "Can't create file!", 5, 60);
			break;
		case 1:
			show_helptext("Save Alternate Configuration", save_config_help);
			break;
		case KEY_ESC:
			return;
		}
	}
}

/**
 * @brief Handles the exit process for the configuration menu.
 *
 * This method manages the exit sequence when the user decides to quit the configuration menu.
 * It first sets the `save_and_exit` flag to indicate that the program should exit after saving.
 * It then resets the subtitle, clears the dialog, and checks if the configuration has been changed.
 * If the configuration has been modified, it prompts the user with a dialog asking whether they
 * wish to save the new configuration. If the configuration has not been changed, it skips the prompt.
 *
 * Based on the user's response, it either saves the configuration to a file or discards the changes.
 * If the configuration is successfully saved, it also writes the auto-generated configuration file.
 * Finally, it displays appropriate messages to the user indicating the end of the configuration
 * process and provides instructions for the next steps (e.g., running 'make' to start the build).
 *
 * @return int Returns 0 if the exit process is successful or if the user chooses not to save changes.
 *             Returns 1 if there is an error while saving the configuration. If the user presses
 *             the ESC key, it returns the corresponding key code.
 */
static int handle_exit(void)
{
	int res;

	save_and_exit = 1;
	reset_subtitle();
	dialog_clear();
	if (conf_get_changed())
		res = dialog_yesno(NULL,
				   "Do you wish to save your new configuration?\n"
				     "(Press <ESC><ESC> to continue kernel configuration.)",
				   6, 60);
	else
		res = -1;

	end_dialog(saved_x, saved_y);

	switch (res) {
	case 0:
		if (conf_write(filename)) {
			fprintf(stderr, "\n\n"
					  "Error while writing of the configuration.\n"
					  "Your configuration changes were NOT saved."
					  "\n\n");
			return 1;
		}
		conf_write_autoconf(0);
		/* fall through */
	case -1:
		if (!silent)
			printf("\n\n"
				 "*** End of the configuration.\n"
				 "*** Execute 'make' to start the build or try 'make help'."
				 "\n\n");
		res = 0;
		break;
	default:
		if (!silent)
			fprintf(stderr, "\n\n"
					  "Your configuration changes were NOT saved."
					  "\n\n");
		if (res != KEY_ESC)
			res = 0;
	}

	return res;
}

/**
 * @brief Signal handler function that gracefully exits the program.
 *
 * This function is designed to handle specific signals (e.g., SIGINT, SIGTERM)
 * and initiate a controlled exit of the program. When invoked, it calls the
 * `handle_exit()` function to perform any necessary cleanup or finalization
 * tasks, and then terminates the program using the `exit()` function.
 *
 * @param signo The signal number that triggered this handler. This parameter
 *              is used to identify the specific signal being handled.
 *
 * @note The `handle_exit()` function should be implemented to return an integer
 *       exit status code, which will be passed to the `exit()` function.
 */
static void sig_handler(int signo)
{
	exit(handle_exit());
}

/**
 * @brief Main entry point for the Menuconfig application.
 *
 * This function initializes and runs the Menuconfig application, which provides
 * a text-based user interface for configuring kernel options. It handles command-line
 * arguments, sets up signal handling, parses configuration files, and initializes
 * the dialog interface. The application runs in a loop, allowing the user to navigate
 * and modify configuration options until they exit or save their changes.
 *
 * @param ac The number of command-line arguments.
 * @param av An array of command-line argument strings.
 *
 * @return Returns an integer representing the exit status of the application:
 *         - 0 on successful completion.
 *         - 1 if the display is too small to run the application.
 *         - Other values may indicate specific errors or user actions (e.g., KEY_ESC).
 */
int main(int ac, char **av)
{
	char *mode;
	int res;

	signal(SIGINT, sig_handler);

	if (ac > 1 && strcmp(av[1], "-s") == 0) {
		silent = 1;
		/* Silence conf_read() until the real callback is set up */
		conf_set_message_callback(NULL);
		av++;
	}
	conf_parse(av[1]);
	conf_read(NULL);

	mode = getenv("MENUCONFIG_MODE");
	if (mode) {
		if (!strcasecmp(mode, "single_menu"))
			single_menu_mode = 1;
	}

	if (init_dialog(NULL)) {
		fprintf(stderr, "Your display is too small to run Menuconfig!\n");
		fprintf(stderr, "It must be at least 19 lines by 80 columns.\n");
		return 1;
	}

	set_config_filename(conf_get_configname());
	conf_set_message_callback(conf_message_callback);
	do {
		conf(&rootmenu, NULL);
		res = handle_exit();
	} while (res == KEY_ESC);

	return res;
}
