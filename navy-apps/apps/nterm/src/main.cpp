#include <nterm.h>
#include <SDL.h>
#include <SDL_bdf.h>

static const char *font_fname = "/share/fonts/Courier-7.bdf";
static BDF_Font *font = NULL;
static SDL_Surface *screen = NULL;
Terminal *term = NULL;

void builtin_sh_run();
void extern_app_run(const char *app_path);

/**
 * @brief Main entry point for the application.
 *
 * This method initializes the SDL library, sets up the display using a BDF font,
 * and creates a terminal window. Depending on the command-line arguments, it
 * either runs a built-in shell or an external application.
 *
 * @param argc The number of command-line arguments.
 * @param argv An array of command-line arguments. If the second argument is
 *             provided, it specifies the external application to run. If not,
 *             the built-in shell is executed.
 *
 * @return int This method should not return under normal circumstances. If it
 *             does, it indicates an error, and the program will terminate with
 *             an assertion failure.
 */
int main(int argc, char *argv[]) {
  SDL_Init(0);
  font = new BDF_Font(font_fname);

  // setup display
  int win_w = font->w * W;
  int win_h = font->h * H;
  screen = SDL_SetVideoMode(win_w, win_h, 32, SDL_HWSURFACE);

  term = new Terminal(W, H);

  if (argc < 2) { builtin_sh_run(); }
  else { extern_app_run(argv[1]); }

  // should not reach here
  assert(0);
}

/**
 * @brief Draws a character on the screen at the specified position with the given foreground and background colors.
 *
 * This method creates an SDL_Surface representing the specified character using the provided font, 
 * foreground color (fg), and background color (bg). The character is then blitted onto the global 
 * screen surface at the coordinates (x, y). The created surface is freed after the blit operation 
 * to avoid memory leaks.
 *
 * @param x The x-coordinate on the screen where the character will be drawn.
 * @param y The y-coordinate on the screen where the character will be drawn.
 * @param ch The character to be drawn.
 * @param fg The foreground color of the character, represented as a 32-bit unsigned integer.
 * @param bg The background color of the character, represented as a 32-bit unsigned integer.
 */
static void draw_ch(int x, int y, char ch, uint32_t fg, uint32_t bg) {
  SDL_Surface *s = BDF_CreateSurface(font, ch, fg, bg);
  SDL_Rect dstrect = { .x = x, .y = y };
  SDL_BlitSurface(s, NULL, screen, &dstrect);
  SDL_FreeSurface(s);
}

/**
 * @brief Refreshes the terminal display by redrawing dirty cells and updating the cursor.
 * 
 * This method iterates over the terminal grid (W x H) and checks if each cell is marked as dirty.
 * If a cell is dirty, it redraws the character at that position using the terminal's foreground
 * and background colors. After redrawing all dirty cells, the terminal's dirty state is cleared.
 * 
 * Additionally, this method handles cursor blinking. If the terminal has not been refreshed in
 * the last 500 milliseconds or if there were dirty cells to redraw, the cursor is updated.
 * The cursor alternates between the foreground and background colors every 500 milliseconds
 * to create a blinking effect. Finally, the screen is updated to reflect the changes.
 */
void refresh_terminal() {
  int needsync = 0;
  for (int i = 0; i < W; i ++)
    for (int j = 0; j < H; j ++)
      if (term->is_dirty(i, j)) {
        draw_ch(i * font->w, j * font->h, term->getch(i, j), term->foreground(i, j), term->background(i, j));
        needsync = 1;
      }
  term->clear();

  static uint32_t last = 0;
  static int flip = 0;
  uint32_t now = SDL_GetTicks();
  if (now - last > 500 || needsync) {
    int x = term->cursor.x, y = term->cursor.y;
    uint32_t color = (flip ? term->foreground(x, y) : term->background(x, y));
    draw_ch(x * font->w, y * font->h, ' ', 0, color);
    SDL_UpdateRect(screen, 0, 0, 0, 0);
    if (now - last > 500) {
      flip = !flip;
      last = now;
    }
  }
}

#define ENTRY(KEYNAME, NOSHIFT, SHIFT) { SDLK_##KEYNAME, #KEYNAME, NOSHIFT, SHIFT }
static const struct {
  int keycode;
  const char *name;
  char noshift, shift;
} SHIFT[] = {
  ENTRY(ESCAPE,       '\033', '\033'),
  ENTRY(SPACE,        ' ' , ' '),
  ENTRY(RETURN,       '\n', '\n'),
  ENTRY(BACKSPACE,    '\b', '\b'),
  ENTRY(1,            '1',  '!'),
  ENTRY(2,            '2',  '@'),
  ENTRY(3,            '3',  '#'),
  ENTRY(4,            '4',  '$'),
  ENTRY(5,            '5',  '%'),
  ENTRY(6,            '6',  '^'),
  ENTRY(7,            '7',  '&'),
  ENTRY(8,            '8',  '*'),
  ENTRY(9,            '9',  '('),
  ENTRY(0,            '0',  ')'),
  ENTRY(GRAVE,        '`',  '~'),
  ENTRY(MINUS,        '-',  '_'),
  ENTRY(EQUALS,       '=',  '+'),
  ENTRY(SEMICOLON,    ';',  ':'),
  ENTRY(APOSTROPHE,   '\'', '"'),
  ENTRY(LEFTBRACKET,  '[',  '{'),
  ENTRY(RIGHTBRACKET, ']',  '}'),
  ENTRY(BACKSLASH,    '\\', '|'),
  ENTRY(COMMA,        ',',  '<'),
  ENTRY(PERIOD,       '.',  '>'),
  ENTRY(SLASH,        '/',  '?'),
  ENTRY(A,            'a',  'A'),
  ENTRY(B,            'b',  'B'),
  ENTRY(C,            'c',  'C'),
  ENTRY(D,            'd',  'D'),
  ENTRY(E,            'e',  'E'),
  ENTRY(F,            'f',  'F'),
  ENTRY(G,            'g',  'G'),
  ENTRY(H,            'h',  'H'),
  ENTRY(I,            'i',  'I'),
  ENTRY(J,            'j',  'J'),
  ENTRY(K,            'k',  'K'),
  ENTRY(L,            'l',  'L'),
  ENTRY(M,            'm',  'M'),
  ENTRY(N,            'n',  'N'),
  ENTRY(O,            'o',  'O'),
  ENTRY(P,            'p',  'P'),
  ENTRY(Q,            'q',  'Q'),
  ENTRY(R,            'r',  'R'),
  ENTRY(S,            's',  'S'),
  ENTRY(T,            't',  'T'),
  ENTRY(U,            'u',  'U'),
  ENTRY(V,            'v',  'V'),
  ENTRY(W,            'w',  'W'),
  ENTRY(X,            'x',  'X'),
  ENTRY(Y,            'y',  'Y'),
  ENTRY(Z,            'z',  'Z'),
};

/**
 * @brief Handles a key event based on the input buffer.
 *
 * This method processes a key event described by the input buffer `buf`. It extracts the key
 * information from the buffer and determines the appropriate character to return based on the
 * key and the current shift state. The shift state is toggled when the key is either "LSHIFT"
 * or "RSHIFT". For other keys, the method checks if the key is a single uppercase letter and
 * returns the corresponding lowercase letter if the shift state is inactive. For special keys
 * defined in the `SHIFT` array, the method returns the appropriate character based on the shift
 * state. If no valid key is found, the method returns the null character ('\0').
 *
 * @param buf A pointer to the input buffer containing the key event information.
 * @return The character corresponding to the key event, or '\0' if no valid key is found.
 */
char handle_key(const char *buf) {
  char key[32];
  static int shift = 0;
  sscanf(buf + 2, "%s", key);

  if (strcmp(key, "LSHIFT") == 0 || strcmp(key, "RSHIFT") == 0)  { shift ^= 1; return '\0'; }

  if (buf[0] == 'd') {
    if (key[0] >= 'A' && key[0] <= 'Z' && key[1] == '\0') {
      if (shift) return key[0];
      else return key[0] - 'A' + 'a';
    }
    for (auto item: SHIFT) {
      if (strcmp(item.name, key) == 0) {
        if (shift) return item.shift;
        else return item.noshift;
      }
    }
  }
  return '\0';
}

/**
 * @brief Handles a keyboard event and returns the corresponding character.
 *
 * This method processes a given SDL_Event to determine the character associated
 * with the key press. It tracks the state of the shift key (both left and right)
 * using a static variable. If the event is a shift key press, it toggles the shift
 * state and returns a null character ('\0'). For other key events, it checks if the
 * key is in the SHIFT mapping. If the key is found, it returns the appropriate
 * character based on the current shift state (shifted or unshifted). If the key is
 * not found or the event is not a key down event, it returns a null character ('\0').
 *
 * @param ev Pointer to the SDL_Event to be processed.
 * @return The character corresponding to the key event, or '\0' if no character
 *         is associated with the event or if the event is a shift key press.
 */
char handle_key(SDL_Event *ev) {
  static int shift = 0;
  int key = ev->key.keysym.sym;
  if (key == SDLK_LSHIFT || key == SDLK_RSHIFT) { shift ^= 1; return '\0'; }

  if (ev->type == SDL_KEYDOWN) {
    for (auto item: SHIFT) {
      if (item.keycode == key) {
        if (shift) return item.shift;
        else return item.noshift;
      }
    }
  }
  return '\0';
}
