#include <am.h>
#include <nemu.h>

#define KEYDOWN_MASK 0x8000

/**
 * Resets the state of the keyboard input structure.
 *
 * This function initializes the `AM_INPUT_KEYBRD_T` structure by setting the
 * `keydown` flag to 0, indicating no key is currently pressed, and setting the
 * `keycode` to `AM_KEY_NONE`, indicating no key code is active. This is typically
 * used to clear or reset the keyboard input state before processing new input events.
 *
 * @param kbd Pointer to the `AM_INPUT_KEYBRD_T` structure to be reset.
 */
void __am_input_keybrd(AM_INPUT_KEYBRD_T *kbd) {
  kbd->keydown = 0;
  kbd->keycode = AM_KEY_NONE;
}
