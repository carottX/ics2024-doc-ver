/*
 * this is the first process in the OS:
 *   display a splash screen
 *   display a boot menu and receive input
 */

#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <fcntl.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <SDL.h>
#include <SDL_bmp.h>
#include <SDL_bdf.h>

const char *font_fname = "/share/fonts/Courier-7.bdf";
static BDF_Font *font;
static SDL_Surface *screen = NULL;
static SDL_Surface *logo_sf = NULL;

static void display_menu(int n);

struct MenuItem {
  const char *name, *bin, *arg1;
} items[] = {
  {"NJU Terminal", "/bin/nterm", NULL},
  {"NSlider", "/bin/nslider", NULL},
  {"FCEUX (Super Mario Bros)", "/bin/fceux", "/share/games/nes/mario.nes"},
  {"FCEUX (100 in 1)", "/bin/fceux", "/share/games/nes/100in1.nes"},
  {"Flappy Bird", "/bin/bird", NULL},
  {"PAL - Xian Jian Qi Xia Zhuan", "/bin/pal", NULL},
  {"NPlayer", "/bin/nplayer", NULL},
  {"coremark", "/bin/coremark", NULL},
  {"dhrystone", "/bin/dhrystone", NULL},
  {"typing-game", "/bin/typing-game", NULL},
  {"ONScripter", "/bin/onscripter", NULL},
};

#define nitems (sizeof(items) / sizeof(items[0]))
#define MAX_PAGE ((nitems - 1) / 10)
#define MAX_IDX_LAST_PAGE ((nitems - 1) % 10)

static int page = 0;
static int i_max = 0;

/**
 * @brief Sets the value of `i_max` based on the current page and predefined constants.
 * 
 * This method determines the maximum index (`i_max`) that can be used for the current page. 
 * If the current page (`page`) is equal to `MAX_PAGE`, `i_max` is set to `MAX_IDX_LAST_PAGE`. 
 * Otherwise, `i_max` is set to 9. Additionally, it prints the values of `page`, `MAX_PAGE`, 
 * and `MAX_IDX_LAST_PAGE` for debugging purposes.
 */
static void set_i_max() {
  i_max = (page == MAX_PAGE ? MAX_IDX_LAST_PAGE : 9);
  printf("page = %d, MAX_PAGE = %d, MAX_IDX_LAST_PAGE = %d\n", page, MAX_PAGE, MAX_IDX_LAST_PAGE);
}
/**
 * @brief Increments the current page number if it is less than the maximum allowed page.
 * 
 * This method checks if the current page number is less than the maximum allowed page (MAX_PAGE).
 * If true, it increments the page number by 1 and updates the maximum index (i_max) by calling
 * the `set_i_max()` method. If the current page is already at the maximum, no action is taken.
 */
static void next() {
  if (page < MAX_PAGE) {
    page ++;
    set_i_max();
  }
}

/**
 * @brief Decrements the current page index if it is greater than 0.
 * 
 * This method checks if the current page index (`page`) is greater than 0. 
 * If true, it decrements the page index by 1 and updates the maximum index (`i_max`) 
 * by calling the `set_i_max()` method. This is typically used to navigate to the 
 * previous page in a paginated context.
 */
static void prev() {
  if (page > 0) {
    page --;
    set_i_max();
  }
}

/**
 * @brief Clears the display by filling the entire screen with a white color.
 *
 * This method uses the SDL_FillRect function to fill the entire screen surface
 * with the color white (0xffffff in RGB format). The screen surface is assumed
 * to be a global or previously defined SDL_Surface pointer.
 *
 * @note This function assumes that the `screen` variable is a valid SDL_Surface
 * pointer that has been initialized prior to calling this method.
 */
static void clear_display(void) {
  SDL_FillRect(screen, NULL, 0xffffff);
}

/**
 * @brief The main entry point for the application.
 *
 * This method initializes the SDL library, sets up the video mode, loads a font and a logo image,
 * and enters an infinite loop to display a menu and handle user input. The menu allows the user to
 * select an item by pressing a numeric key (0-9) or navigate through the menu using the left and right arrow keys.
 * When a valid item is selected, the corresponding executable is launched using `execve`. If the execution fails,
 * an error message is printed. The loop continues indefinitely, allowing the user to make multiple selections.
 *
 * @param argc The number of command-line arguments.
 * @param argv The array of command-line arguments.
 * @param envp The array of environment variables.
 * @return Always returns -1, indicating that the program should not terminate normally.
 */
int main(int argc, char *argv[], char *envp[]) {
  SDL_Init(0);
  screen = SDL_SetVideoMode(0, 0, 32, SDL_HWSURFACE);

  font = new BDF_Font(font_fname);
  logo_sf = SDL_LoadBMP("/share/pictures/projectn.bmp");
  assert(logo_sf);
  set_i_max();

  while (1) {
    display_menu(i_max);

    SDL_Event e;
    do {
      SDL_WaitEvent(&e);
    } while (e.type != SDL_KEYDOWN);

    int i = -1;
    switch (e.key.keysym.sym) {
      case SDLK_0: i = 0; break;
      case SDLK_1: i = 1; break;
      case SDLK_2: i = 2; break;
      case SDLK_3: i = 3; break;
      case SDLK_4: i = 4; break;
      case SDLK_5: i = 5; break;
      case SDLK_6: i = 6; break;
      case SDLK_7: i = 7; break;
      case SDLK_8: i = 8; break;
      case SDLK_9: i = 9; break;
      case SDLK_LEFT: prev(); break;
      case SDLK_RIGHT: next(); break;
    }

    if (i != -1 && i <= i_max) {
      i += page * 10;
      auto *item = &items[i];
      const char *exec_argv[3];
      exec_argv[0] = item->bin;
      exec_argv[1] = item->arg1;
      exec_argv[2] = NULL;
      clear_display();
      SDL_UpdateRect(screen, 0, 0, 0, 0);
      execve(exec_argv[0], (char**)exec_argv, (char**)envp);
      fprintf(stderr, "\033[31m[ERROR]\033[0m Exec %s failed.\n\n", exec_argv[0]);
    } else {
      fprintf(stderr, "Choose a number between %d and %d\n\n", 0, i_max);
    }
  }
  return -1;
}

/**
 * @brief Draws a single character on the screen using a specified BDF font.
 *
 * This method creates a surface for the given character using the provided BDF font,
 * and then blits (copies) the surface onto the screen at the specified coordinates.
 * The character is rendered with the specified foreground and background colors.
 * After the character is drawn, the surface is freed to avoid memory leaks.
 *
 * @param font Pointer to the BDF_Font structure that defines the font to be used.
 * @param x The x-coordinate on the screen where the character will be drawn.
 * @param y The y-coordinate on the screen where the character will be drawn.
 * @param ch The character to be drawn.
 * @param fg The foreground color of the character, represented as a 32-bit integer.
 * @param bg The background color of the character, represented as a 32-bit integer.
 */
static void draw_ch(BDF_Font *font, int x, int y, char ch, uint32_t fg, uint32_t bg) {
  SDL_Surface *s = BDF_CreateSurface(font, ch, fg, bg);
  SDL_Rect dstrect = { .x = x, .y = y };
  SDL_BlitSurface(s, NULL, screen, &dstrect);
  SDL_FreeSurface(s);
}

/**
 * @brief Draws a string on the screen using a specified font and colors.
 *
 * This method iterates through each character in the provided string and draws it
 * at the specified (x, y) position using the given font. The foreground color (fp)
 * and background color (bg) are applied to each character. After drawing a character,
 * the x position is incremented by the width of the font to place the next character
 * adjacent to the previous one.
 *
 * @param font Pointer to the BDF_Font structure that defines the font to be used.
 * @param x The x-coordinate of the starting position where the string will be drawn.
 * @param y The y-coordinate of the starting position where the string will be drawn.
 * @param str Pointer to the null-terminated string that will be drawn.
 * @param fp The foreground color (as a 32-bit unsigned integer) to be used for the characters.
 * @param bg The background color (as a 32-bit unsigned integer) to be used for the characters.
 */
static void draw_str(BDF_Font *font, int x, int y, char *str, uint32_t fp, uint32_t bg) {
  while (*str) {
    draw_ch(font, x, y, *str, fp, bg);
    x += font->w;
    str ++;
  }
}

/**
 * @brief Draws a text row at a specified row position.
 *
 * This method adjusts the row position by adding an offset of 3, prints the text
 * to the console using `puts`, and then renders the text on the screen using a
 * specified font. The text is drawn at the calculated vertical position (based on
 * the adjusted row and the font height) with a specific foreground and background color.
 *
 * @param s Pointer to the null-terminated string containing the text to be drawn.
 * @param r The initial row position where the text will be drawn. This value is
 *          adjusted by adding 3 before rendering.
 */
static void draw_text_row(char *s, int r) {
  r += 3;
  puts(s);
  draw_str(font, 0, r * font->h, s, 0x123456, 0xffffff);
}

/**
 * @brief Displays a menu of available applications and navigation options on the screen.
 *
 * This method clears the display and renders a logo at the top-right corner of the screen. 
 * It then lists the available applications, each with a corresponding number for selection. 
 * The menu also displays the current page number, the total number of applications, and 
 * provides navigation options such as moving to the previous or next page, and selecting 
 * an application by its number. The menu is updated and rendered on the screen using SDL.
 *
 * @param n The number of applications to display on the current page. This should be less 
 *          than or equal to the total number of applications available on the page.
 *
 * @note The method assumes that `items`, `page`, `nitems`, `logo_sf`, and `screen` are 
 *       globally defined and properly initialized. It also assumes that `clear_display()`, 
 *       `SDL_BlitSurface()`, `draw_text_row()`, and `SDL_UpdateRect()` are implemented 
 *       and functional.
 */
static void display_menu(int n) {
  clear_display();
  SDL_Rect rect = { .x = screen->w - logo_sf->w, .y = 0 };
  SDL_BlitSurface(logo_sf, NULL, screen, &rect);
  printf("Available applications:\n");
  char buf[80];
  int i;
  for (i = 0; i <= n; i ++) {
    auto *item = &items[page * 10 + i];
    sprintf(buf, "  [%d] %s", i, item->name);
    draw_text_row(buf, i);
  }

  i = 11;

  sprintf(buf, "  page = %2d, #total apps = %d", page, nitems);
  draw_text_row(buf, i);
  i ++;

  sprintf(buf, "  help:");
  draw_text_row(buf, i);
  i ++;

  sprintf(buf, "  <-  Prev Page");
  draw_text_row(buf, i);
  i ++;

  sprintf(buf, "  ->  Next Page");
  draw_text_row(buf, i);
  i ++;

  sprintf(buf, "  0-9 Choose");
  draw_text_row(buf, i);
  i ++;

  SDL_UpdateRect(screen, 0, 0, 0, 0);

  printf("========================================\n");
  printf("Please Choose.\n");
  fflush(stdout);
}
