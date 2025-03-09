#include <am.h>

/**
 * Resets the state of the keyboard input structure.
 *
 * This function initializes the fields of the provided `AM_INPUT_KEYBRD_T` structure
 * to their default values. Specifically, it sets the `keydown` field to 0, indicating
 * that no key is currently pressed, and sets the `keycode` field to `AM_KEY_NONE`,
 * indicating that no key code is associated with the keyboard input.
 *
 * @param kbd Pointer to the `AM_INPUT_KEYBRD_T` structure to be reset.
 */
void __am_input_keybrd(AM_INPUT_KEYBRD_T *kbd) {
  kbd->keydown = 0;
  kbd->keycode = AM_KEY_NONE;
}
