// SPDX-License-Identifier: GPL-2.0+
/*
 *  util.c
 *
 *  ORIGINAL AUTHOR: Savio Lam (lam836@cs.cuhk.hk)
 *  MODIFIED FOR LINUX KERNEL CONFIG BY: William Roadcap (roadcap@cfw.com)
 */

#include <stdarg.h>

#include "dialog.h"

/* Needed in signal handler in mconf.c */
int saved_x, saved_y;

struct dialog_info dlg;

/**
 * Sets the monochrome theme for the dialog interface by configuring the display
 * attributes for various UI elements. This method assigns attributes such as
 * A_NORMAL, A_BOLD, A_REVERSE, and A_DIM to elements like the screen, shadow,
 * dialog, title, buttons, input boxes, menus, and other components to ensure
 * a consistent and readable monochrome appearance. The attributes control the
 * visual styling of text and borders, such as normal text, bold text, reversed
 * text (for active elements), and dimmed text (for inactive elements).
 */
static void set_mono_theme(void)
{
	dlg.screen.atr = A_NORMAL;
	dlg.shadow.atr = A_NORMAL;
	dlg.dialog.atr = A_NORMAL;
	dlg.title.atr = A_BOLD;
	dlg.border.atr = A_NORMAL;
	dlg.button_active.atr = A_REVERSE;
	dlg.button_inactive.atr = A_DIM;
	dlg.button_key_active.atr = A_REVERSE;
	dlg.button_key_inactive.atr = A_BOLD;
	dlg.button_label_active.atr = A_REVERSE;
	dlg.button_label_inactive.atr = A_NORMAL;
	dlg.inputbox.atr = A_NORMAL;
	dlg.inputbox_border.atr = A_NORMAL;
	dlg.searchbox.atr = A_NORMAL;
	dlg.searchbox_title.atr = A_BOLD;
	dlg.searchbox_border.atr = A_NORMAL;
	dlg.position_indicator.atr = A_BOLD;
	dlg.menubox.atr = A_NORMAL;
	dlg.menubox_border.atr = A_NORMAL;
	dlg.item.atr = A_NORMAL;
	dlg.item_selected.atr = A_REVERSE;
	dlg.tag.atr = A_BOLD;
	dlg.tag_selected.atr = A_REVERSE;
	dlg.tag_key.atr = A_BOLD;
	dlg.tag_key_selected.atr = A_REVERSE;
	dlg.check.atr = A_BOLD;
	dlg.check_selected.atr = A_REVERSE;
	dlg.uarrow.atr = A_BOLD;
	dlg.darrow.atr = A_BOLD;
}

#define DLG_COLOR(dialog, f, b, h) \
do {                               \
	dlg.dialog.fg = (f);       \
	dlg.dialog.bg = (b);       \
	dlg.dialog.hl = (h);       \
} while (0)

/**
 * Sets the classic theme for the dialog interface by configuring the colors for various UI elements.
 * This function defines the foreground and background colors, as well as the emphasis (bold) state,
 * for components such as the screen, dialog boxes, buttons, input fields, menus, and more.
 * The classic theme uses a combination of cyan, blue, white, black, yellow, and green colors to
 * create a traditional and easily readable interface. Each UI element is configured using the
 * `DLG_COLOR` macro, which takes the element name, foreground color, background color, and a boolean
 * indicating whether the text should be bold.
 */
static void set_classic_theme(void)
{
	DLG_COLOR(screen,                COLOR_CYAN,   COLOR_BLUE,   true);
	DLG_COLOR(shadow,                COLOR_BLACK,  COLOR_BLACK,  true);
	DLG_COLOR(dialog,                COLOR_BLACK,  COLOR_WHITE,  false);
	DLG_COLOR(title,                 COLOR_YELLOW, COLOR_WHITE,  true);
	DLG_COLOR(border,                COLOR_WHITE,  COLOR_WHITE,  true);
	DLG_COLOR(button_active,         COLOR_WHITE,  COLOR_BLUE,   true);
	DLG_COLOR(button_inactive,       COLOR_BLACK,  COLOR_WHITE,  false);
	DLG_COLOR(button_key_active,     COLOR_WHITE,  COLOR_BLUE,   true);
	DLG_COLOR(button_key_inactive,   COLOR_RED,    COLOR_WHITE,  false);
	DLG_COLOR(button_label_active,   COLOR_YELLOW, COLOR_BLUE,   true);
	DLG_COLOR(button_label_inactive, COLOR_BLACK,  COLOR_WHITE,  true);
	DLG_COLOR(inputbox,              COLOR_BLACK,  COLOR_WHITE,  false);
	DLG_COLOR(inputbox_border,       COLOR_BLACK,  COLOR_WHITE,  false);
	DLG_COLOR(searchbox,             COLOR_BLACK,  COLOR_WHITE,  false);
	DLG_COLOR(searchbox_title,       COLOR_YELLOW, COLOR_WHITE,  true);
	DLG_COLOR(searchbox_border,      COLOR_WHITE,  COLOR_WHITE,  true);
	DLG_COLOR(position_indicator,    COLOR_YELLOW, COLOR_WHITE,  true);
	DLG_COLOR(menubox,               COLOR_BLACK,  COLOR_WHITE,  false);
	DLG_COLOR(menubox_border,        COLOR_WHITE,  COLOR_WHITE,  true);
	DLG_COLOR(item,                  COLOR_BLACK,  COLOR_WHITE,  false);
	DLG_COLOR(item_selected,         COLOR_WHITE,  COLOR_BLUE,   true);
	DLG_COLOR(tag,                   COLOR_YELLOW, COLOR_WHITE,  true);
	DLG_COLOR(tag_selected,          COLOR_YELLOW, COLOR_BLUE,   true);
	DLG_COLOR(tag_key,               COLOR_YELLOW, COLOR_WHITE,  true);
	DLG_COLOR(tag_key_selected,      COLOR_YELLOW, COLOR_BLUE,   true);
	DLG_COLOR(check,                 COLOR_BLACK,  COLOR_WHITE,  false);
	DLG_COLOR(check_selected,        COLOR_WHITE,  COLOR_BLUE,   true);
	DLG_COLOR(uarrow,                COLOR_GREEN,  COLOR_WHITE,  true);
	DLG_COLOR(darrow,                COLOR_GREEN,  COLOR_WHITE,  true);
}

/**
 * Sets the color theme for a dialog interface with a black background.
 * This method configures the color scheme for various UI elements in the dialog,
 * ensuring a consistent and visually appealing appearance. The theme uses a black
 * background as the base and assigns specific foreground and background colors
 * to elements such as the screen, shadow, dialog, title, border, buttons, input boxes,
 * search boxes, menus, items, tags, checkboxes, and arrows. Each UI element is
 * assigned a foreground color and a background color, with some elements also
 * having an optional bold attribute for enhanced visibility. The color scheme
 * emphasizes red, yellow, white, and black for contrast and readability.
 */
static void set_blackbg_theme(void)
{
	DLG_COLOR(screen, COLOR_RED,   COLOR_BLACK, true);
	DLG_COLOR(shadow, COLOR_BLACK, COLOR_BLACK, false);
	DLG_COLOR(dialog, COLOR_WHITE, COLOR_BLACK, false);
	DLG_COLOR(title,  COLOR_RED,   COLOR_BLACK, false);
	DLG_COLOR(border, COLOR_BLACK, COLOR_BLACK, true);

	DLG_COLOR(button_active,         COLOR_YELLOW, COLOR_RED,   false);
	DLG_COLOR(button_inactive,       COLOR_YELLOW, COLOR_BLACK, false);
	DLG_COLOR(button_key_active,     COLOR_YELLOW, COLOR_RED,   true);
	DLG_COLOR(button_key_inactive,   COLOR_RED,    COLOR_BLACK, false);
	DLG_COLOR(button_label_active,   COLOR_WHITE,  COLOR_RED,   false);
	DLG_COLOR(button_label_inactive, COLOR_BLACK,  COLOR_BLACK, true);

	DLG_COLOR(inputbox,         COLOR_YELLOW, COLOR_BLACK, false);
	DLG_COLOR(inputbox_border,  COLOR_YELLOW, COLOR_BLACK, false);

	DLG_COLOR(searchbox,        COLOR_YELLOW, COLOR_BLACK, false);
	DLG_COLOR(searchbox_title,  COLOR_YELLOW, COLOR_BLACK, true);
	DLG_COLOR(searchbox_border, COLOR_BLACK,  COLOR_BLACK, true);

	DLG_COLOR(position_indicator, COLOR_RED, COLOR_BLACK,  false);

	DLG_COLOR(menubox,          COLOR_YELLOW, COLOR_BLACK, false);
	DLG_COLOR(menubox_border,   COLOR_BLACK,  COLOR_BLACK, true);

	DLG_COLOR(item,             COLOR_WHITE, COLOR_BLACK, false);
	DLG_COLOR(item_selected,    COLOR_WHITE, COLOR_RED,   false);

	DLG_COLOR(tag,              COLOR_RED,    COLOR_BLACK, false);
	DLG_COLOR(tag_selected,     COLOR_YELLOW, COLOR_RED,   true);
	DLG_COLOR(tag_key,          COLOR_RED,    COLOR_BLACK, false);
	DLG_COLOR(tag_key_selected, COLOR_YELLOW, COLOR_RED,   true);

	DLG_COLOR(check,            COLOR_YELLOW, COLOR_BLACK, false);
	DLG_COLOR(check_selected,   COLOR_YELLOW, COLOR_RED,   true);

	DLG_COLOR(uarrow, COLOR_RED, COLOR_BLACK, false);
	DLG_COLOR(darrow, COLOR_RED, COLOR_BLACK, false);
}

/**
 * Sets a custom blue-themed color scheme for the dialog interface.
 * This method first applies the classic theme by calling `set_classic_theme()`
 * and then overrides specific color attributes to create a blue-themed appearance.
 * The following elements are styled:
 * - Title: Blue background with white text.
 * - Active button key: Yellow text on a blue background.
 * - Active button label: White text on a blue background.
 * - Search box title: Blue background with white text.
 * - Position indicator: Blue background with white text.
 * - Tag: Blue background with white text.
 * - Tag key: Blue background with white text.
 * All overrides use bold text for better visibility.
 */
static void set_bluetitle_theme(void)
{
	set_classic_theme();
	DLG_COLOR(title,               COLOR_BLUE,   COLOR_WHITE, true);
	DLG_COLOR(button_key_active,   COLOR_YELLOW, COLOR_BLUE,  true);
	DLG_COLOR(button_label_active, COLOR_WHITE,  COLOR_BLUE,  true);
	DLG_COLOR(searchbox_title,     COLOR_BLUE,   COLOR_WHITE, true);
	DLG_COLOR(position_indicator,  COLOR_BLUE,   COLOR_WHITE, true);
	DLG_COLOR(tag,                 COLOR_BLUE,   COLOR_WHITE, true);
	DLG_COLOR(tag_key,             COLOR_BLUE,   COLOR_WHITE, true);
}

/*
 * Select color theme
 */
static int set_theme(const char *theme)
{
	int use_color = 1;
	if (!theme)
		set_bluetitle_theme();
	else if (strcmp(theme, "classic") == 0)
		set_classic_theme();
	else if (strcmp(theme, "bluetitle") == 0)
		set_bluetitle_theme();
	else if (strcmp(theme, "blackbg") == 0)
		set_blackbg_theme();
	else if (strcmp(theme, "mono") == 0)
		use_color = 0;

	return use_color;
}

/**
 * Initializes a single color pair for a dialog element based on the provided 
 * color attributes. This function increments a static pair counter to ensure 
 * unique color pairs are created. It initializes a color pair using the 
 * foreground (`fg`) and background (`bg`) colors from the `dialog_color` 
 * structure. If the `hl` (highlight) flag is set, the color pair is combined 
 * with the `A_BOLD` attribute to emphasize the text. The resulting attribute 
 * (`atr`) is stored in the `dialog_color` structure for later use.
 *
 * @param color Pointer to a `dialog_color` structure containing the foreground 
 *              color (`fg`), background color (`bg`), highlight flag (`hl`), 
 *              and the resulting attribute (`atr`).
 */
static void init_one_color(struct dialog_color *color)
{
	static int pair = 0;

	pair++;
	init_pair(pair, color->fg, color->bg);
	if (color->hl)
		color->atr = A_BOLD | COLOR_PAIR(pair);
	else
		color->atr = COLOR_PAIR(pair);
}

/**
 * Initializes the color settings for various components of a dialog box.
 * This method sets up the colors for the screen, shadow, dialog, title, border,
 * buttons (active, inactive, key active, key inactive, label active, label inactive),
 * input boxes (including their borders), search boxes (including their titles and borders),
 * position indicators, menu boxes (including their borders), items (selected and unselected),
 * tags (selected and unselected, including their keys), checkboxes (selected and unselected),
 * and up/down arrows. Each color is initialized by calling the `init_one_color` function
 * with the corresponding component's color reference.
 */
static void init_dialog_colors(void)
{
	init_one_color(&dlg.screen);
	init_one_color(&dlg.shadow);
	init_one_color(&dlg.dialog);
	init_one_color(&dlg.title);
	init_one_color(&dlg.border);
	init_one_color(&dlg.button_active);
	init_one_color(&dlg.button_inactive);
	init_one_color(&dlg.button_key_active);
	init_one_color(&dlg.button_key_inactive);
	init_one_color(&dlg.button_label_active);
	init_one_color(&dlg.button_label_inactive);
	init_one_color(&dlg.inputbox);
	init_one_color(&dlg.inputbox_border);
	init_one_color(&dlg.searchbox);
	init_one_color(&dlg.searchbox_title);
	init_one_color(&dlg.searchbox_border);
	init_one_color(&dlg.position_indicator);
	init_one_color(&dlg.menubox);
	init_one_color(&dlg.menubox_border);
	init_one_color(&dlg.item);
	init_one_color(&dlg.item_selected);
	init_one_color(&dlg.tag);
	init_one_color(&dlg.tag_selected);
	init_one_color(&dlg.tag_key);
	init_one_color(&dlg.tag_key_selected);
	init_one_color(&dlg.check);
	init_one_color(&dlg.check_selected);
	init_one_color(&dlg.uarrow);
	init_one_color(&dlg.darrow);
}

/*
 * Setup for color display
 */
static void color_setup(const char *theme)
{
	int use_color;

	use_color = set_theme(theme);
	if (use_color && has_colors()) {
		start_color();
		init_dialog_colors();
	} else
		set_mono_theme();
}

/*
 * Set window to attribute 'attr'
 */
void attr_clear(WINDOW * win, int height, int width, chtype attr)
{
	int i, j;

	wattrset(win, attr);
	for (i = 0; i < height; i++) {
		wmove(win, i, 0);
		for (j = 0; j < width; j++)
			waddch(win, ' ');
	}
	touchwin(win);
}

/**
 * Clears the dialog screen and redraws its background and title.
 * 
 * This function clears the entire screen by using the `attr_clear` function
 * with the dimensions of the current screen (`lines` and `columns`) and the
 * dialog's screen attribute (`dlg.screen.atr`). If a backtitle is set in the
 * dialog (`dlg.backtitle`), it is displayed at the top of the screen. Additionally,
 * if subtitles are present in the dialog (`dlg.subtitles`), they are displayed
 * below the backtitle, with an arrow separator between them. If the combined
 * length of the subtitles exceeds the available screen width, an ellipsis ("[...]")
 * is displayed, and the subtitles are truncated accordingly. The function also
 * draws a horizontal line (`ACS_HLINE`) to fill the remaining space in the subtitle
 * row. Finally, the screen is refreshed using `wnoutrefresh` to update the display.
 */
void dialog_clear(void)
{
	int lines, columns;

	lines = getmaxy(stdscr);
	columns = getmaxx(stdscr);

	attr_clear(stdscr, lines, columns, dlg.screen.atr);
	/* Display background title if it exists ... - SLH */
	if (dlg.backtitle != NULL) {
		int i, len = 0, skip = 0;
		struct subtitle_list *pos;

		wattrset(stdscr, dlg.screen.atr);
		mvwaddstr(stdscr, 0, 1, (char *)dlg.backtitle);

		for (pos = dlg.subtitles; pos != NULL; pos = pos->next) {
			/* 3 is for the arrow and spaces */
			len += strlen(pos->text) + 3;
		}

		wmove(stdscr, 1, 1);
		if (len > columns - 2) {
			const char *ellipsis = "[...] ";
			waddstr(stdscr, ellipsis);
			skip = len - (columns - 2 - strlen(ellipsis));
		}

		for (pos = dlg.subtitles; pos != NULL; pos = pos->next) {
			if (skip == 0)
				waddch(stdscr, ACS_RARROW);
			else
				skip--;

			if (skip == 0)
				waddch(stdscr, ' ');
			else
				skip--;

			if (skip < strlen(pos->text)) {
				waddstr(stdscr, pos->text + skip);
				skip = 0;
			} else
				skip -= strlen(pos->text);

			if (skip == 0)
				waddch(stdscr, ' ');
			else
				skip--;
		}

		for (i = len + 1; i < columns - 1; i++)
			waddch(stdscr, ACS_HLINE);
	}
	wnoutrefresh(stdscr);
}

/*
 * Do some initialization for dialog
 */
int init_dialog(const char *backtitle)
{
	int height, width;

	initscr();		/* Init curses */

	/* Get current cursor position for signal handler in mconf.c */
	getyx(stdscr, saved_y, saved_x);

	getmaxyx(stdscr, height, width);
	if (height < WINDOW_HEIGTH_MIN || width < WINDOW_WIDTH_MIN) {
		endwin();
		return -ERRDISPLAYTOOSMALL;
	}

	dlg.backtitle = backtitle;
	color_setup(getenv("MENUCONFIG_COLOR"));

	keypad(stdscr, TRUE);
	cbreak();
	noecho();
	dialog_clear();

	return 0;
}

/**
 * Sets the backtitle for the dialog.
 *
 * This function assigns the provided `backtitle` string to the `backtitle` field
 * of the `dlg` structure. The backtitle is typically displayed at the top of the
 * dialog window, providing context or a title for the dialog.
 *
 * @param backtitle A pointer to a null-terminated string that represents the
 *                  backtitle to be set. If NULL, the backtitle will be cleared.
 */
void set_dialog_backtitle(const char *backtitle)
{
	dlg.backtitle = backtitle;
}

/**
 * Sets the subtitles for the dialog by assigning the provided subtitle list
 * to the dialog's subtitle field. This method is used to associate a list of
 * subtitles with the dialog, which can then be displayed or manipulated as needed.
 *
 * @param subtitles A pointer to a `subtitle_list` structure containing the
 *                  subtitles to be assigned to the dialog. If NULL, the dialog
 *                  will have no subtitles.
 */
void set_dialog_subtitles(struct subtitle_list *subtitles)
{
	dlg.subtitles = subtitles;
}

/*
 * End using dialog functions.
 */
void end_dialog(int x, int y)
{
	/* move cursor back to original position */
	move(y, x);
	refresh();
	endwin();
}

/* Print the title of the dialog. Center the title and truncate
 * tile if wider than dialog (- 2 chars).
 **/
void print_title(WINDOW *dialog, const char *title, int width)
{
	if (title) {
		int tlen = MIN(width - 2, strlen(title));
		wattrset(dialog, dlg.title.atr);
		mvwaddch(dialog, 0, (width - tlen) / 2 - 1, ' ');
		mvwaddnstr(dialog, 0, (width - tlen)/2, title, tlen);
		waddch(dialog, ' ');
	}
}

/*
 * Print a string of text in a window, automatically wrap around to the
 * next line if the string is too long to fit on one line. Newline
 * characters '\n' are propperly processed.  We start on a new line
 * if there is no room for at least 4 nonblanks following a double-space.
 */
void print_autowrap(WINDOW * win, const char *prompt, int width, int y, int x)
{
	int newl, cur_x, cur_y;
	int prompt_len, room, wlen;
	char tempstr[MAX_LEN + 1], *word, *sp, *sp2, *newline_separator = 0;

	strcpy(tempstr, prompt);

	prompt_len = strlen(tempstr);

	if (prompt_len <= width - x * 2) {	/* If prompt is short */
		wmove(win, y, (width - prompt_len) / 2);
		waddstr(win, tempstr);
	} else {
		cur_x = x;
		cur_y = y;
		newl = 1;
		word = tempstr;
		while (word && *word) {
			sp = strpbrk(word, "\n ");
			if (sp && *sp == '\n')
				newline_separator = sp;

			if (sp)
				*sp++ = 0;

			/* Wrap to next line if either the word does not fit,
			   or it is the first word of a new sentence, and it is
			   short, and the next word does not fit. */
			room = width - cur_x;
			wlen = strlen(word);
			if (wlen > room ||
			    (newl && wlen < 4 && sp
			     && wlen + 1 + strlen(sp) > room
			     && (!(sp2 = strpbrk(sp, "\n "))
				 || wlen + 1 + (sp2 - sp) > room))) {
				cur_y++;
				cur_x = x;
			}
			wmove(win, cur_y, cur_x);
			waddstr(win, word);
			getyx(win, cur_y, cur_x);

			/* Move to the next line if the word separator was a newline */
			if (newline_separator) {
				cur_y++;
				cur_x = x;
				newline_separator = 0;
			} else
				cur_x++;

			if (sp && *sp == ' ') {
				cur_x++;	/* double space */
				while (*++sp == ' ') ;
				newl = 1;
			} else
				newl = 0;
			word = sp;
		}
	}
}

/*
 * Print a button
 */
void print_button(WINDOW * win, const char *label, int y, int x, int selected)
{
	int i, temp;

	wmove(win, y, x);
	wattrset(win, selected ? dlg.button_active.atr
		 : dlg.button_inactive.atr);
	waddstr(win, "<");
	temp = strspn(label, " ");
	label += temp;
	wattrset(win, selected ? dlg.button_label_active.atr
		 : dlg.button_label_inactive.atr);
	for (i = 0; i < temp; i++)
		waddch(win, ' ');
	wattrset(win, selected ? dlg.button_key_active.atr
		 : dlg.button_key_inactive.atr);
	waddch(win, label[0]);
	wattrset(win, selected ? dlg.button_label_active.atr
		 : dlg.button_label_inactive.atr);
	waddstr(win, (char *)label + 1);
	wattrset(win, selected ? dlg.button_active.atr
		 : dlg.button_inactive.atr);
	waddstr(win, ">");
	wmove(win, y, x + temp + 1);
}

/*
 * Draw a rectangular box with line drawing characters
 */
void
draw_box(WINDOW * win, int y, int x, int height, int width,
	 chtype box, chtype border)
{
	int i, j;

	wattrset(win, 0);
	for (i = 0; i < height; i++) {
		wmove(win, y + i, x);
		for (j = 0; j < width; j++)
			if (!i && !j)
				waddch(win, border | ACS_ULCORNER);
			else if (i == height - 1 && !j)
				waddch(win, border | ACS_LLCORNER);
			else if (!i && j == width - 1)
				waddch(win, box | ACS_URCORNER);
			else if (i == height - 1 && j == width - 1)
				waddch(win, box | ACS_LRCORNER);
			else if (!i)
				waddch(win, border | ACS_HLINE);
			else if (i == height - 1)
				waddch(win, box | ACS_HLINE);
			else if (!j)
				waddch(win, border | ACS_VLINE);
			else if (j == width - 1)
				waddch(win, box | ACS_VLINE);
			else
				waddch(win, box | ' ');
	}
}

/*
 * Draw shadows along the right and bottom edge to give a more 3D look
 * to the boxes
 */
void draw_shadow(WINDOW * win, int y, int x, int height, int width)
{
	int i;

	if (has_colors()) {	/* Whether terminal supports color? */
		wattrset(win, dlg.shadow.atr);
		wmove(win, y + height, x + 2);
		for (i = 0; i < width; i++)
			waddch(win, winch(win) & A_CHARTEXT);
		for (i = y + 1; i < y + height + 1; i++) {
			wmove(win, i, x + width);
			waddch(win, winch(win) & A_CHARTEXT);
			waddch(win, winch(win) & A_CHARTEXT);
		}
		wnoutrefresh(win);
	}
}

/*
 *  Return the position of the first alphabetic character in a string.
 */
int first_alpha(const char *string, const char *exempt)
{
	int i, in_paren = 0, c;

	for (i = 0; i < strlen(string); i++) {
		c = tolower(string[i]);

		if (strchr("<[(", c))
			++in_paren;
		if (strchr(">])", c) && in_paren > 0)
			--in_paren;

		if ((!in_paren) && isalpha(c) && strchr(exempt, c) == 0)
			return i;
	}

	return 0;
}

/*
 * ncurses uses ESC to detect escaped char sequences. This resutl in
 * a small timeout before ESC is actually delivered to the application.
 * lxdialog suggest <ESC> <ESC> which is correctly translated to two
 * times esc. But then we need to ignore the second esc to avoid stepping
 * out one menu too much. Filter away all escaped key sequences since
 * keypad(FALSE) turn off ncurses support for escape sequences - and thats
 * needed to make notimeout() do as expected.
 */
int on_key_esc(WINDOW *win)
{
	int key;
	int key2;
	int key3;

	nodelay(win, TRUE);
	keypad(win, FALSE);
	key = wgetch(win);
	key2 = wgetch(win);
	do {
		key3 = wgetch(win);
	} while (key3 != ERR);
	nodelay(win, FALSE);
	keypad(win, TRUE);
	if (key == KEY_ESC && key2 == ERR)
		return KEY_ESC;
	else if (key != ERR && key != KEY_ESC && key2 == ERR)
		ungetch(key);

	return -1;
}

/* redraw screen in new size */
int on_key_resize(void)
{
	dialog_clear();
	return KEY_RESIZE;
}

struct dialog_list *item_cur;
struct dialog_list item_nil;
struct dialog_list *item_head;

/**
 * Resets the item list by freeing all allocated memory for the items and resetting
 * the list head and current item pointer.
 *
 * This function iterates through the linked list of items, starting from the head
 * of the list (`item_head`). For each item in the list, it frees the memory allocated
 * for the item and moves to the next item in the list. After all items have been freed,
 * the function sets the list head (`item_head`) to NULL and the current item pointer
 * (`item_cur`) to a predefined null item (`item_nil`), effectively resetting the list
 * to an empty state.
 *
 * @note This function assumes that the items in the list are dynamically allocated
 * and that the list is properly initialized before calling this function.
 */
void item_reset(void)
{
	struct dialog_list *p, *next;

	for (p = item_head; p; p = next) {
		next = p->next;
		free(p);
	}
	item_head = NULL;
	item_cur = &item_nil;
}

/**
 * @brief Creates a new dialog list item and appends it to the linked list.
 *
 * This function dynamically allocates memory for a new dialog list item and
 * initializes it with the provided formatted string. The item is then appended
 * to the end of the linked list. If the list is empty, the new item becomes the
 * head of the list. The function uses a variable argument list to support
 * formatted string input.
 *
 * @param fmt The format string for the dialog item's text, similar to printf.
 * @param ... Variable arguments to be formatted into the string.
 *
 * @note The caller is responsible for managing the memory of the linked list
 *       to avoid memory leaks.
 */
void item_make(const char *fmt, ...)
{
	va_list ap;
	struct dialog_list *p = malloc(sizeof(*p));

	if (item_head)
		item_cur->next = p;
	else
		item_head = p;
	item_cur = p;
	memset(p, 0, sizeof(*p));

	va_start(ap, fmt);
	vsnprintf(item_cur->node.str, sizeof(item_cur->node.str), fmt, ap);
	va_end(ap);
}

/**
 * Appends a formatted string to the current item's string buffer.
 *
 * This function takes a format string and a variable number of arguments, similar to `printf`.
 * It appends the formatted string to the existing string stored in `item_cur->node.str`.
 * The function ensures that the resulting string does not exceed the buffer's capacity by
 * calculating the available space and truncating the result if necessary. The buffer is
 * always null-terminated.
 *
 * @param fmt The format string specifying how the subsequent arguments are converted for output.
 * @param ... A variable number of arguments to be formatted and appended to the string.
 */
void item_add_str(const char *fmt, ...)
{
	va_list ap;
	size_t avail;

	avail = sizeof(item_cur->node.str) - strlen(item_cur->node.str);

	va_start(ap, fmt);
	vsnprintf(item_cur->node.str + strlen(item_cur->node.str),
		  avail, fmt, ap);
	item_cur->node.str[sizeof(item_cur->node.str) - 1] = '\0';
	va_end(ap);
}

/**
 * Sets the tag of the current item in the linked list.
 * 
 * This method assigns the specified tag value to the 'tag' field of the node
 * associated with the current item. The current item is typically managed
 * by a global or context-specific pointer, such as 'item_cur'. The tag is
 * used to categorize or identify the item within the list.
 *
 * @param tag The character value to be assigned as the tag for the current item.
 */
void item_set_tag(char tag)
{
	item_cur->node.tag = tag;
}
/**
 * Sets the data pointer of the current item's node.
 *
 * This function assigns the provided pointer `ptr` to the `data` field of the
 * node associated with the current item (`item_cur`). It is typically used to
 * associate arbitrary data with the current item in a data structure.
 *
 * @param ptr A pointer to the data to be associated with the current item's node.
 *            This can be any type of data, as it is passed as a `void*`.
 */
void item_set_data(void *ptr)
{
	item_cur->node.data = ptr;
}

/**
 * Sets the selected state of the current item.
 * 
 * This method updates the `selected` field of the `node` structure within the current item
 * (`item_cur`) to the specified value `val`. The `selected` field typically indicates whether
 * the item is currently selected (e.g., in a list or menu).
 *
 * @param val The value to set for the selected state. Use `1` to mark the item as selected
 *            and `0` to mark it as unselected.
 */
void item_set_selected(int val)
{
	item_cur->node.selected = val;
}

/**
 * @brief Activates the selected item by iterating through all items and checking if any are selected.
 *
 * This method iterates through all items using the `item_foreach()` function. For each item,
 * it checks if the item is selected using the `item_is_selected()` function. If any item is found
 * to be selected, the method immediately returns 1, indicating that a selected item was activated.
 * If no items are selected after iterating through all items, the method returns 0.
 *
 * @return int Returns 1 if a selected item is found and activated, otherwise returns 0.
 */
int item_activate_selected(void)
{
	item_foreach()
		if (item_is_selected())
			return 1;
	return 0;
}

/**
 * Retrieves the data associated with the current item in the list.
 * This method accesses the 'data' field of the node pointed to by 'item_cur',
 * which is assumed to be a pointer to the current item in a linked list or similar structure.
 * 
 * @return A pointer to the data stored in the current item's node. The type of the data
 *         is determined by the context in which this method is used.
 */
void *item_data(void)
{
	return item_cur->node.data;
}

/**
 * Retrieves the tag of the current item in the context.
 *
 * This method accesses the `tag` field of the `node` structure within the current item
 * (`item_cur`). The current item is typically managed by an external context or state.
 *
 * @return The tag associated with the current item's node. The tag is of type `char`.
 */
char item_tag(void)
{
	return item_cur->node.tag;
}

/**
 * @brief Counts the number of items in the linked list starting from `item_head`.
 *
 * This function iterates through the linked list of `dialog_list` structures,
 * starting from the head of the list (`item_head`), and increments a counter
 * for each node encountered. The final count of items in the list is returned.
 *
 * @return int The total number of items in the linked list.
 */
int item_count(void)
{
	int n = 0;
	struct dialog_list *p;

	for (p = item_head; p; p = p->next)
		n++;
	return n;
}

/**
 * Sets the current item to the nth item in the collection by iterating through the items.
 * This function uses the `item_foreach` macro to traverse the items in the collection.
 * It increments a counter `i` for each item and stops when `i` matches the specified index `n`.
 * If `n` is out of bounds (i.e., greater than or equal to the number of items), the function
 * will not perform any action and will return without setting an item.
 *
 * @param n The zero-based index of the item to set as the current item.
 */
void item_set(int n)
{
	int i = 0;
	item_foreach()
		if (i++ == n)
			return;
}

/**
 * @brief Counts the number of items in the linked list until the current item is reached.
 *
 * This method iterates through the linked list starting from the head (`item_head`) and 
 * increments a counter (`n`) for each item until it encounters the current item (`item_cur`). 
 * If the current item is found, the method returns the count of items traversed so far. 
 * If the current item is not found in the list, the method returns 0.
 *
 * @return int The number of items traversed before reaching the current item, or 0 if the 
 * current item is not found in the list.
 */
int item_n(void)
{
	int n = 0;
	struct dialog_list *p;

	for (p = item_head; p; p = p->next) {
		if (p == item_cur)
			return n;
		n++;
	}
	return 0;
}

/**
 * @brief Retrieves the string value associated with the current item.
 *
 * This method returns a pointer to the string stored in the `str` field of the 
 * `node` structure within the current item (`item_cur`). The string is assumed 
 * to be null-terminated. The caller should ensure that `item_cur` is valid and 
 * points to a properly initialized item before calling this method.
 *
 * @return A pointer to the string value of the current item. If `item_cur` is 
 *         NULL or the `str` field is not initialized, the behavior is undefined.
 */
const char *item_str(void)
{
	return item_cur->node.str;
}

/**
 * @brief Checks if the current item is selected.
 *
 * This function checks the `selected` field of the `node` structure within the
 * current item (`item_cur`). If the `selected` field is non-zero, the item is
 * considered selected.
 *
 * @return int Returns 1 if the current item is selected, otherwise 0.
 */
int item_is_selected(void)
{
	return (item_cur->node.selected != 0);
}

/**
 * @brief Checks if the current item's tag matches the specified tag.
 *
 * This function compares the tag of the current item's node with the provided
 * tag. It returns a non-zero value (true) if the tags match, otherwise it
 * returns zero (false).
 *
 * @param tag The tag to compare against the current item's node tag.
 * @return int Returns 1 if the tags match, otherwise returns 0.
 */
int item_is_tag(char tag)
{
	return (item_cur->node.tag == tag);
}
