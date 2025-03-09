#ifndef __NTERM_H__
#define __NTERM_H__

#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

#define W 48
#define H 16

class Terminal {
private:
  struct Pattern {
    const char *pattern;
    void (Terminal::*handle)(int *args);
  };
  static Pattern esc_seqs[];

  char *buf, input[256], cooked[256];
  uint8_t *color;
  bool *dirty;
  int inp_len;

  void move_one();
  void backspace();
  size_t write_escape(const char *str, size_t count);
  void scroll_up();


  void esc_move(int *args);
  void esc_movefirst(int *args);
  void esc_moveup(int *args);
  void esc_movedown(int *args);
  void esc_moveleft(int *args);
  void esc_moveright(int *args);
  void esc_save(int *args);
  void esc_restore(int *args);
  void esc_clear(int *args);
  void esc_erase(int *args);
  void esc_setattr1(int *args);
  void esc_setattr2(int *args);
  void esc_setattr3(int *args);
  void esc_rawmode(int *args);
  void esc_cookmode(int *args);

public:
  enum /**
  enum  * @brief Writes a string of characters to the terminal at the current cursor position.
  enum  *
  enum  * This method writes the specified number of characters from the given string to the terminal
  enum  * starting at the current cursor position. The cursor position is updated after each character
  enum  * is written. If the string contains newline characters or other control characters, they are
  enum  * handled appropriately (e.g., moving the cursor to the next line for a newline character).
  enum  *
  enum  * @param str A pointer to the string of characters to be written.
  enum  * @param count The number of characters to write from the string.
  enum  */
  enum void write(const char *str, size_t count);;

extern Terminal *term;

void refresh_terminal();

#endif
