#include <nterm.h>

#define EMPTY ' '

enum Color {
  BLACK = 0, 
  RED,
  GREEN,
  YELLOW,
  BLUE,
  MAGENTA,
  CYAN,
  WHITE,
};

static int colors[] = {
  [Color::BLACK  ] = 0x323232,
  [Color::RED    ] = 0xe41b17,
  [Color::GREEN  ] = 0x4aa02c,
  [Color::YELLOW ] = 0xe8a317,
  [Color::BLUE   ] = 0x0041c2,
  [Color::MAGENTA] = 0xe3319d,
  [Color::CYAN   ] = 0x77bfc7,
  [Color::WHITE  ] = 0xf0f0f0,
};

Terminal::Pattern Terminal::esc_seqs[] = {
  {"\033[1t", &Terminal::esc_cookmode}, // added by us
  {"\033[2t", &Terminal::esc_rawmode}, // added by us

  {"\033[s", &Terminal::esc_save},
  {"\033[u", &Terminal::esc_restore},
  {"\033[J", &Terminal::esc_clear},
  {"\033[2J", &Terminal::esc_clear},
  {"\033[K", &Terminal::esc_erase},
  {"\033[f", &Terminal::esc_movefirst},
  {"\033[H", &Terminal::esc_movefirst},
  {"\033[#;#f", &Terminal::esc_move},
  {"\033[#;#H", &Terminal::esc_move},
  {"\033[#A", &Terminal::esc_moveup},
  {"\033[#B", &Terminal::esc_movedown},
  {"\033[#C", &Terminal::esc_moveright},
  {"\033[#D", &Terminal::esc_moveleft},
  {"\033[#m", &Terminal::esc_setattr1},
  {"\033[#;#m", &Terminal::esc_setattr2},
  {"\033[#;#;#m", &Terminal::esc_setattr3},
};

/**
 * @brief Returns the minimum of two integers.
 * 
 * This function compares two integers, `x` and `y`, and returns the smaller of the two.
 * It uses a ternary operator to perform the comparison and return the result.
 * 
 * @param x The first integer to compare.
 * @param y The second integer to compare.
 * @return The smaller of the two integers, `x` or `y`.
 */
static inline int min(int x, int y) { return x < y ? x : y; }
/**
 * @brief Returns the maximum of two integer values.
 * 
 * This function compares two integer values, `x` and `y`, and returns the larger of the two.
 * If `x` and `y` are equal, the function returns `x`.
 * 
 * @param x The first integer to compare.
 * @param y The second integer to compare.
 * @return The larger of the two integers, or `x` if they are equal.
 */
static inline int max(int x, int y) { return x > y ? x : y; }

/**
 * @brief Moves the terminal cursor to the specified position based on the provided arguments.
 *
 * This method updates the cursor's position in the terminal. The `args` array is expected to contain
 * two integers: the first element (`args[0]`) represents the row (y-coordinate), and the second element
 * (`args[1]`) represents the column (x-coordinate). The cursor's position is adjusted by subtracting 1
 * from both coordinates to convert from 1-based indexing (typical for terminal commands) to 0-based
 * indexing (used internally).
 *
 * @param args An array of integers where `args[0]` is the row (y-coordinate) and `args[1]` is the column (x-coordinate).
 */
void Terminal::esc_move(int *args) {
  cursor = { .x = args[1] - 1, .y = args[0] - 1 };
}

/**
 * @brief Moves the cursor to the first position (top-left corner) of the terminal.
 * 
 * This method sets the cursor's x and y coordinates to 0, effectively moving it
 * to the beginning of the terminal screen. The `args` parameter is unused in this
 * implementation but may be provided for compatibility with other functions or
 * future extensions.
 * 
 * @param args Unused parameter, provided for potential future use or compatibility.
 */
void Terminal::esc_movefirst(int *args) {
  cursor = { .x = 0, .y = 0 };
}

/**
 * Moves the terminal cursor up by a specified number of rows.
 * 
 * This method adjusts the vertical position of the cursor by subtracting the 
 * value provided in the first element of the `args` array from the current 
 * y-coordinate of the cursor. The cursor's position is clamped to the valid 
 * range of the terminal screen after the adjustment.
 *
 * @param args An array of integers where the first element specifies the number 
 *             of rows to move the cursor up. If the value is negative or zero, 
 *             the cursor position remains unchanged.
 */
void Terminal::esc_moveup(int *args) {
  cursor.y -= args[0];
}

/**
 * @brief Moves the terminal cursor down by a specified number of rows.
 *
 * This method adjusts the vertical position (y-coordinate) of the cursor by 
 * incrementing it by the value provided in the first element of the `args` array.
 * The cursor's new position is constrained within the terminal's boundaries.
 *
 * @param args An array of integers where the first element specifies the number 
 *             of rows to move the cursor down. If `args` is null or empty, the 
 *             behavior is undefined.
 */
void Terminal::esc_movedown(int *args) {
  cursor.y += args[0];
}

/**
 * @brief Moves the terminal cursor to the left by a specified number of columns.
 *
 * This method adjusts the horizontal position of the cursor by subtracting the value
 * provided in the first element of the `args` array from the current cursor's x-coordinate.
 * The cursor's x-coordinate is updated to reflect the new position.
 *
 * @param args An array of integers where the first element specifies the number of columns
 *             to move the cursor to the left. The value must be non-negative.
 */
void Terminal::esc_moveleft(int *args) {
  cursor.x -= args[0];
}

/**
 * Moves the terminal cursor to the right by the specified number of columns.
 * This method updates the cursor's x-coordinate by adding the value provided in the first element of the `args` array.
 * If the cursor moves beyond the terminal's width, it will be clamped to the maximum allowed column.
 *
 * @param args An array of integers where the first element specifies the number of columns to move the cursor to the right.
 *             The value must be a non-negative integer.
 */
void Terminal::esc_moveright(int *args) {
  cursor.x += args[0];
}

/**
 * @brief Saves the current cursor position to the `saved` member variable.
 *
 * This method is typically used in response to an escape sequence that requests
 * the current cursor position to be saved. The cursor position is stored in the
 * `saved` member variable, which can later be used to restore the cursor to this
 * position.
 *
 * @param args An array of integer arguments that may be passed to the method.
 *             This parameter is currently unused in the implementation.
 */
void Terminal::esc_save(int *args) {
  saved = cursor;
}

/**
 * @brief Restores the cursor position to the previously saved position.
 *
 * This method updates the current cursor position to the position that was 
 * previously saved. The saved cursor position is typically stored in the 
 * `saved` member variable of the Terminal class. This is useful for restoring 
 * the cursor to a known state after performing operations that may have moved it.
 *
 * @param args An array of integer arguments (unused in this method).
 */
void Terminal::esc_restore(int *args) {
  cursor = saved;
}

/**
 * @brief Clears the terminal screen by setting all character positions to EMPTY.
 *
 * This method iterates over the entire terminal screen, which has a width of `w`
 * and a height of `h`, and sets each character position to the value of `EMPTY`.
 * After clearing the screen, the cursor is reset to the top-left corner of the
 * terminal (position {0, 0}).
 *
 * @param args An array of integer arguments. This parameter is not used in the
 *             current implementation but may be utilized in future extensions.
 */
void Terminal::esc_clear(int *args) {
  for (int i = 0; i < w; i ++)
    for (int j = 0; j < h; j ++) {
      putch(i, j, EMPTY);
    }
  cursor = {.x = 0, .y = 0};
}

/**
 * @brief Sets the terminal text and background colors based on the provided attribute.
 *
 * This method interprets the first integer in the `args` array as an attribute code
 * and updates the terminal's foreground (`col_f`) and background (`col_b`) colors accordingly.
 * The attribute codes follow the ANSI escape sequence standard for text colorization:
 * - 0: Resets the colors to default (black text on white background).
 * - 30-37: Sets the foreground color (30=black, 31=red, 32=green, 33=yellow, 34=blue, 35=magenta, 36=cyan, 37=white).
 * - 40-47: Sets the background color (40=black, 41=red, 42=green, 43=yellow, 44=blue, 45=magenta, 46=cyan, 47=white).
 *
 * @param args An array of integers where the first element specifies the attribute code.
 */
void Terminal::esc_setattr1(int *args) {
  int attr = args[0];
  switch (attr) {
    case 0:  col_f = Color::BLACK; col_b = Color::WHITE; break; // reset
    case 30: col_f = Color::BLACK; break;
    case 31: col_f = Color::RED; break;
    case 32: col_f = Color::GREEN; break;
    case 33: col_f = Color::YELLOW; break;
    case 34: col_f = Color::BLUE; break;
    case 35: col_f = Color::MAGENTA; break;
    case 36: col_f = Color::CYAN; break;
    case 37: col_f = Color::WHITE; break;
    case 40: col_b = Color::BLACK; break;
    case 41: col_b = Color::RED; break;
    case 42: col_b = Color::GREEN; break;
    case 43: col_b = Color::YELLOW; break;
    case 44: col_b = Color::BLUE; break;
    case 45: col_b = Color::MAGENTA; break;
    case 46: col_b = Color::CYAN; break;
    case 47: col_b = Color::WHITE; break;
  }
}

/**
 * @brief Applies terminal attribute settings from the provided arguments.
 *
 * This method processes the first two elements of the `args` array by passing them
 * to the `esc_setattr1` method. It is typically used to set terminal attributes
 * such as text color, background color, or other visual properties. The method
 * assumes that `args` contains at least two valid integer values.
 *
 * @param args An array of integers representing terminal attribute settings.
 *             The first two elements are processed by this method.
 */
void Terminal::esc_setattr2(int *args) {
  esc_setattr1(&args[0]);
  esc_setattr1(&args[1]);
}

/**
 * @brief Applies attribute settings to the terminal for three consecutive arguments.
 *
 * This method processes three integer arguments by applying attribute settings to the terminal
 * for each argument. It delegates the actual attribute setting to the `esc_setattr1` method,
 * passing each argument individually. The method is typically used to handle sequences of
 * attribute changes in terminal control sequences.
 *
 * @param args A pointer to an array of three integers, where each integer represents an
 *             attribute setting to be applied to the terminal.
 */
void Terminal::esc_setattr3(int *args) {
  esc_setattr1(&args[0]);
  esc_setattr1(&args[1]);
  esc_setattr1(&args[2]);
}

/**
 * @brief Erases characters from the current cursor position to the end of the line.
 * 
 * This method iterates from the current cursor position (`cursor.x`) to the end of the terminal width (`w`).
 * For each position in this range, it places an empty character (`EMPTY`) at the corresponding 
 * coordinates on the terminal screen. This effectively clears the line from the cursor position 
 * to the end of the line.
 * 
 * @param args An array of integer arguments (not used in this method).
 */
void Terminal::esc_erase(int *args) {
  for (int i = cursor.x; i < w; i ++) {
    putch(i, cursor.y, EMPTY);
  }
}

/**
 * @brief Enables raw mode for the terminal.
 *
 * This method sets the terminal's mode to `Mode::raw`, which typically
 * disables line buffering, echoing, and special character processing.
 * This is useful for applications that need to process input character
 * by character, such as text editors or command-line interfaces.
 *
 * @param args A pointer to an integer array (unused in this implementation).
 */
void Terminal::esc_rawmode(int *args) {
  mode = Mode::raw;
}

/**
 * @brief Switches the terminal mode to "cook" mode.
 * 
 * This method sets the internal mode of the terminal to `Mode::cook`, which is typically
 * used to enable a specific processing behavior for input or output. The exact behavior
 * of "cook" mode depends on the implementation of the `Mode` class and how it is utilized
 * within the terminal system.
 * 
 * @param args A pointer to an integer array that may contain additional parameters or
 *             configuration options for the mode switch. This parameter is currently
 *             unused in the implementation but may be utilized in future updates.
 */
void Terminal::esc_cookmode(int *args) {
  mode = Mode::cook;
}

/**
 * @brief Constructs a Terminal object with the specified width and height.
 * 
 * Initializes the terminal with the given dimensions, setting up the internal
 * state and buffers required for terminal operations. The terminal starts in
 * "cook" mode, with the cursor positioned at the top-left corner (0, 0). The
 * internal buffer for storing characters, color information, and dirty flags
 * are allocated based on the provided dimensions. The terminal's default
 * foreground color is set to black, and the background color is set to white.
 * The input buffer is initialized to an empty string. All cells in the terminal
 * are initialized to the EMPTY character.
 * 
 * @param width The width of the terminal in characters.
 * @param height The height of the terminal in characters.
 */
Terminal::Terminal(int width, int height) {
  w = width; h = height;
  mode = Mode::cook;
  cursor = {.x = 0, .y = 0};
  saved = cursor;
  buf = new char[w * h];
  color = new uint8_t[w * h];
  dirty = new bool[w * h];
  inp_len = 0;
  col_f = Color::BLACK;
  col_b = Color::WHITE;
  input[0] = '\0';

  for (int x = 0; x < w; x ++) {
    for (int y = 0; y < h; y ++) {
      putch(x, y, EMPTY);
    }
  }
}

/**
 * @brief Destructor for the Terminal class.
 * 
 * This method is responsible for cleaning up dynamically allocated memory
 * used by the Terminal class. It deletes the arrays `buf`, `color`, and `dirty`
 * to free up the memory and prevent memory leaks. This ensures that all resources
 * associated with the Terminal object are properly released when the object
 * is destroyed.
 */
Terminal::~Terminal() {
  delete [] buf;
  delete [] color;
  delete [] dirty;
}

/**
 * @brief Moves the cursor back one position and clears the character at the new cursor position.
 *
 * This method handles the backspace operation in the terminal. It decrements the cursor's x-coordinate.
 * If the cursor moves past the left edge of the terminal (x < 0), it wraps the cursor to the end of the
 * previous line (x = w - 1, y--). If the cursor moves past the top of the terminal (y < 0), it resets
 * the cursor to the origin (0, 0). After adjusting the cursor, it sets the character at the new cursor
 * position to EMPTY and marks the position as dirty to indicate that it needs to be redrawn.
 */
void Terminal::backspace() {
  cursor.x --;
  if (cursor.x < 0) {
    cursor.x = w - 1;
    cursor.y --;
    if (cursor.y < 0) cursor.x = cursor.y = 0;
  }
  buf[cursor.y * w + cursor.x] = EMPTY;
  dirty[cursor.y * w + cursor.x] = true;
}

/**
 * @brief Moves the cursor one position forward in the terminal.
 * 
 * This method increments the cursor's x-coordinate by one. If the cursor reaches the end of the current line
 * (i.e., the x-coordinate equals or exceeds the terminal width `w`), it wraps around to the start of the next line
 * by resetting the x-coordinate to 0 and incrementing the y-coordinate. If the cursor reaches the bottom of the terminal
 * (i.e., the y-coordinate equals or exceeds the terminal height `h`), the terminal scrolls up by one line, and the
 * cursor is moved to the start of the new bottom line. The method also calculates and adjusts the return value `ret`
 * to reflect the new cursor position after the move.
 */
void Terminal::move_one() {
  auto &c = cursor;
  int ret = c.y * w + c.x;
  c.x ++;
  if (c.x >= w) {
    c.x = 0;
    c.y ++;
  }
  if (c.y >= h) {
    scroll_up();
    ret -= w;
    c.y --;
  }
}

/**
 * Scrolls the contents of the terminal up by one line.
 * 
 * This method shifts the contents of the terminal buffer and color buffer up by one line. 
 * The first line of the buffer is overwritten by the second line, the second line by the third, 
 * and so on, until the last line is reached. The last line is then cleared by filling it with 
 * the `EMPTY` character. Additionally, the `dirty` array is marked as `true` for all positions 
 * in the buffer to indicate that the entire terminal needs to be redrawn.
 * 
 * The operation is performed using `memmove` for efficient memory shifting. The `buf` array 
 * represents the terminal's character buffer, the `color` array represents the color attributes 
 * for each character, and the `dirty` array tracks which parts of the terminal need to be 
 * updated on the screen.
 */
void Terminal::scroll_up() {
  memmove(buf, buf + w, w * (h - 1));
  memmove(color, color + w, w * (h - 1));
  for (int i = 0; i < w; i ++) {
    putch(i, h - 1, EMPTY);
  }
  for (int i = 0; i < w * h; i ++) {
    dirty[i] = true;
  }
}

/**
 * @brief Writes an escape sequence to the terminal and processes it.
 *
 * This method iterates over a predefined list of escape sequences (`esc_seqs`) and attempts to match
 * the input string `str` against each sequence's pattern. If a match is found, the corresponding
 * handler function is invoked with the extracted arguments. The cursor position is then clamped
 * to ensure it remains within the terminal's bounds.
 *
 * The escape sequence pattern can contain the following elements:
 * - Literal characters that must match the input string exactly.
 * - The '#' character, which acts as a placeholder for a numeric argument. Numeric arguments are
 *   extracted from the input string and passed to the handler function.
 *
 * @param str Pointer to the input string containing the escape sequence.
 * @param count The maximum number of characters to process from the input string.
 * @return The number of characters processed from the input string. If no match is found, returns 1.
 */
size_t Terminal::write_escape(const char *str, size_t count) {
  for (auto &p: esc_seqs) {
    bool match = false;
    int len = 0, args[4], narg = 0;

    for (const char *cur = p.pattern, *s = str; ; cur ++) {
      if (*cur == '\0') { // found a match.
        match = true; break;
      }
      if (*cur != '#') {
        if (*s != *cur) break;
        s ++;
        len ++;
      } else {
        int data = 0;
        for (; *s >= '0' && *s <= '9' && s - str < count; s ++, len ++) {
          data = data * 10 + *s - '0';
        }
        args[narg ++] = data;
      }
    }

    if (match) {
      (this->*p.handle)(args);
      cursor.x = max(min(cursor.x, w - 1), 0);
      cursor.y = max(min(cursor.y, h - 1), 0);
      return len;
    }
  }
  return 1;
}

/**
 * @brief Writes a string of characters to the terminal.
 *
 * This method processes the input string `str` of length `count` and writes it to the terminal.
 * It handles special characters such as escape sequences, control characters, and regular characters.
 * 
 * - Escape sequences (starting with '\033') are processed by the `write_escape` method.
 * - Control characters like '\n' (newline), '\r' (carriage return), '\t' (tab), and '\x07' (bell) are handled appropriately:
 *   - '\n' moves the cursor to the beginning of the next line, scrolling the terminal if necessary.
 *   - '\r' moves the cursor to the beginning of the current line.
 *   - '\t' is a placeholder for future tab implementation.
 *   - '\x07' is ignored (bell character).
 * - Regular characters are written to the terminal at the current cursor position, and the cursor is moved forward.
 *
 * @param str Pointer to the input string to be written.
 * @param count Length of the input string.
 */
void Terminal::write(const char *str, size_t count) {
  for (size_t i = 0; i != count && str[i]; ) {
    char ch = str[i];
    if (ch == '\033') {
        i += write_escape(&str[i], count - i);
    } else {
      switch (ch) {
        case '\x07':
          break;
        case '\n':
          cursor.x = 0;
          cursor.y ++;
          if (cursor.y >= h) {
            scroll_up();
            cursor.y --;
          }
          break;
        case '\t':
          // TODO: implement it.
          break;
        case '\r':
          cursor.x = 0;
          break;
        default:
          putch(cursor.x, cursor.y, ch);
          move_one();
      }
      i ++;
    }
  }
}

/**
 * @brief Processes a keypress event based on the current terminal mode.
 *
 * This method handles a keypress event by interpreting the character `ch` according to the terminal's current mode (raw or cook).
 * - In `Mode::raw`, the character is directly stored in the `input` buffer and returned as a null-terminated string.
 * - In `Mode::cook`, the character is processed differently based on its value:
 *   - Escape sequences (e.g., `\033`) are ignored.
 *   - Newline (`\n`) triggers the end of input, appends a newline to the `cooked` buffer, writes a newline to the terminal, and resets the input length.
 *   - Backspace (`\b`) removes the last character from the input buffer and updates the terminal display accordingly.
 *   - Other characters are appended to the `input` buffer if there is space, and the character is written to the terminal.
 *
 * @param ch The character representing the keypress event.
 * @return A pointer to the processed input string in `Mode::raw` or the `cooked` buffer in `Mode::cook` for certain events (e.g., newline). Returns `nullptr` if the character is not processed or if `ch` is `\0`.
 */
const char *Terminal::keypress(char ch) {
  if (ch == '\0') return nullptr;
  if (mode == Mode::raw) {
    input[0] = ch;
    input[1] = '\0';
    return input;
  } else if (mode == Mode::cook) {
    const char *ret = nullptr;
    switch (ch) {
      case '\033':
        break;
      case '\n':
        strcpy(cooked, input);
        strcat(cooked, "\n");
        ret = cooked;
        write("\n", 1);
        inp_len = 0;
        break;
      case '\b':
        if (inp_len > 0) {
          inp_len --;
          backspace();
        }
        break;
      default:
        if (inp_len + 1 < sizeof(input)) {
          input[inp_len ++] = ch;
          write(&ch, 1);
        }
    }
    input[inp_len] = '\0';
    return ret;
  }
  return nullptr;
}

/**
 * @brief Retrieves the character at the specified position in the terminal buffer.
 * 
 * This method calculates the index in the terminal buffer based on the provided
 * coordinates (x, y) and returns the character stored at that position. The buffer
 * is treated as a 2D array with a width of `w`, where the index is computed as
 * `x + y * w`.
 * 
 * @param x The horizontal position (column) of the character to retrieve.
 * @param y The vertical position (row) of the character to retrieve.
 * @return The character at the specified position in the terminal buffer.
 */
char Terminal::getch(int x, int y) {
  return buf[x + y * w];
}

/**
 * @brief Places a character at a specified position in the terminal buffer.
 *
 * This method updates the terminal buffer, color buffer, and dirty flag for the
 * specified position (x, y). The character `ch` is placed in the buffer at the
 * calculated index. The color buffer is updated with the current foreground and
 * background colors, combined into a single byte. The dirty flag is set to true
 * to indicate that the position has been modified and needs to be redrawn.
 *
 * @param x The x-coordinate (column) where the character should be placed.
 * @param y The y-coordinate (row) where the character should be placed.
 * @param ch The character to place in the buffer.
 */
void Terminal::putch(int x, int y, char ch) {
  buf[x + y * w] = ch;
  color[x + y * w] = (col_f << 4) | col_b;
  dirty[x + y * w] = true;
}

/**
 * @brief Retrieves the foreground color of the character at the specified coordinates (x, y) in the terminal.
 * 
 * The method calculates the index of the character in the color array based on the provided coordinates (x, y).
 * It then extracts the foreground color by shifting the color value right by 4 bits and uses the result as an index
 * to fetch the corresponding color from the `colors` array.
 * 
 * @param x The x-coordinate (column) of the character in the terminal.
 * @param y The y-coordinate (row) of the character in the terminal.
 * @return uint32_t The foreground color of the character at the specified coordinates, represented as a 32-bit unsigned integer.
 */
uint32_t Terminal::foreground(int x, int y) {
  return colors[color[x + y * w] >> 4];
}

/**
 * @brief Retrieves the background color of the terminal cell at the specified coordinates.
 *
 * This method calculates the index of the cell at the given (x, y) position in the terminal grid.
 * It then extracts the background color from the cell's color attribute and uses it to index into
 * the `colors` array to retrieve the corresponding color value.
 *
 * @param x The x-coordinate of the cell in the terminal grid.
 * @param y The y-coordinate of the cell in the terminal grid.
 * @return The background color of the cell as a 32-bit unsigned integer.
 */
uint32_t Terminal::background(int x, int y) {
  return colors[color[x + y * w] & 0xf];
}

/**
 * @brief Checks if the terminal cell at the specified coordinates is marked as dirty.
 * 
 * This method determines whether the terminal cell located at the given (x, y) coordinates
 * has been marked as dirty. A dirty cell indicates that its content has changed and needs
 * to be redrawn or updated.
 * 
 * @param x The x-coordinate of the cell to check (column index).
 * @param y The y-coordinate of the cell to check (row index).
 * @return true if the cell is marked as dirty, false otherwise.
 */
bool Terminal::is_dirty(int x, int y) {
  return dirty[x + y * w];
}

/**
 * Clears the terminal's dirty flag array, marking all positions as not dirty.
 * The dirty flag array is used to track which parts of the terminal have been
 * modified and need to be redrawn. After clearing the array, the method marks
 * the current cursor position as dirty to ensure it is redrawn in the next
 * rendering cycle.
 *
 * The method iterates through the entire dirty flag array (of size width * height)
 * and sets each element to false. Then, it calculates the index corresponding to
 * the current cursor position (cursor.x + cursor.y * width) and sets that specific
 * flag to true.
 */
void Terminal::clear() {
  for (int i = 0; i < w * h; i ++) dirty[i] = false;
  dirty[cursor.x + cursor.y * w] = true;
}

