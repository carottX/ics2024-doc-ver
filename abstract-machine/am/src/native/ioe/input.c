#include <am.h>
#include <SDL.h>

#define KEYDOWN_MASK 0x8000

#define KEY_QUEUE_LEN 1024
static int key_queue[KEY_QUEUE_LEN] = {};
static int key_f = 0, key_r = 0;
static SDL_mutex *key_queue_lock = NULL;

#define XX(k) [SDL_SCANCODE_##k] = AM_KEY_##k,
static int keymap[256] = {
  AM_KEYS(XX)
};

/**
 * The `event_thread` function is a static method that serves as an event handling loop for SDL (Simple DirectMedia Layer) events.
 * It continuously waits for events and processes them based on their type. The function runs in an infinite loop, making it
 * suitable for running in a separate thread.
 *
 * The function handles two main types of events:
 * 1. `SDL_QUIT`: When a quit event is detected, the function calls the `halt(0)` method to terminate the program.
 * 2. `SDL_KEYDOWN` and `SDL_KEYUP`: When a key event (either key press or key release) is detected, the function:
 *    - Extracts the key's scancode and determines whether it is a key press or key release.
 *    - Maps the scancode to a custom key code using the `keymap` array.
 *    - Adds the mapped key code to a circular buffer (`key_queue`) for further processing.
 *    - Sends a keyboard interrupt using the `__am_send_kbd_intr()` function.
 *
 * The function uses a mutex (`key_queue_lock`) to ensure thread-safe access to the `key_queue` buffer.
 *
 * @param args A pointer to optional arguments (unused in this implementation).
 * @return This function does not return a value as it runs in an infinite loop.
 */
static int event_thread(void *args) {
  SDL_Event event;
  while (1) {
    SDL_WaitEvent(&event);
    switch (event.type) {
      case SDL_QUIT: halt(0);
      case SDL_KEYDOWN:
      case SDL_KEYUP: {
        SDL_Keysym k = event.key.keysym;
        int keydown = event.key.type == SDL_KEYDOWN;
        int scancode = k.scancode;
        if (keymap[scancode] != 0) {
          int am_code = keymap[scancode] | (keydown ? KEYDOWN_MASK : 0);
          SDL_LockMutex(key_queue_lock);
          key_queue[key_r] = am_code;
          key_r = (key_r + 1) % KEY_QUEUE_LEN;
          SDL_UnlockMutex(key_queue_lock);
          void __am_send_kbd_intr();
          __am_send_kbd_intr();
        }
        break;
      }
    }
  }
}

/**
 * Initializes the input handling system for the application.
 * This function performs the following tasks:
 * 1. Creates a mutex (`key_queue_lock`) to ensure thread-safe access to the key event queue.
 * 2. Spawns a new thread (`event_thread`) dedicated to handling input events asynchronously.
 * The `event_thread` is responsible for processing input events and managing the key event queue.
 * This setup allows the main application to continue running while input events are processed
 * in the background.
 */
void __am_input_init() {
  key_queue_lock = SDL_CreateMutex();
  SDL_CreateThread(event_thread, "event thread", NULL);
}

/**
 * Configures the input device status in the provided configuration structure.
 * This method sets the 'present' field of the AM_INPUT_CONFIG_T structure to 'true',
 * indicating that the input device is present and available for use.
 *
 * @param cfg Pointer to an AM_INPUT_CONFIG_T structure where the input device
 *            configuration will be stored.
 */
void __am_input_config(AM_INPUT_CONFIG_T *cfg) {
  cfg->present = true;
}

/**
 * Processes the next keyboard input event and updates the provided keyboard structure.
 * 
 * This method retrieves the next key event from a circular key event queue, if available,
 * and updates the given `AM_INPUT_KEYBRD_T` structure with the key event details. The key
 * event includes information about whether the key was pressed down or released, as well
 * as the specific key code. The method ensures thread safety by locking and unlocking
 * a mutex when accessing the shared key event queue.
 * 
 * @param kbd A pointer to an `AM_INPUT_KEYBRD_T` structure that will be updated with the
 *            key event details. The structure contains two fields:
 *            - `keydown`: A boolean indicating whether the key was pressed down (true) or released (false).
 *            - `keycode`: The code of the key that was pressed or released.
 */
void __am_input_keybrd(AM_INPUT_KEYBRD_T *kbd) {
  int k = AM_KEY_NONE;

  SDL_LockMutex(key_queue_lock);
  if (key_f != key_r) {
    k = key_queue[key_f];
    key_f = (key_f + 1) % KEY_QUEUE_LEN;
  }
  SDL_UnlockMutex(key_queue_lock);

  kbd->keydown = (k & KEYDOWN_MASK ? true : false);
  kbd->keycode = k & ~KEYDOWN_MASK;
}
