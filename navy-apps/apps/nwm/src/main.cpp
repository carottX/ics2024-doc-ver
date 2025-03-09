#include <nwm.h>
#include <NDL.h>

#define RENDER_FPS 30

/**
 * @brief The main entry point of the application.
 * 
 * This method initializes the necessary components for the application, including the 
 * NDL (Native Display Library) and the canvas. It creates a WindowManager instance to 
 * handle window-related operations. The method then enters an infinite loop where it 
 * continuously polls for events and handles them using the WindowManager. Additionally, 
 * it ensures that the window is rendered at a consistent frame rate by periodically 
 * triggering a render event.
 * 
 * The method performs the following steps:
 * 1. Initializes the NDL library.
 * 2. Opens a canvas with the specified width and height.
 * 3. Creates a WindowManager instance to manage the window and its events.
 * 4. Enters a loop where it:
 *    - Polls for events and passes them to the WindowManager for handling.
 *    - Triggers a render event at a fixed interval to maintain the desired frame rate.
 * 
 * @return int Returns 0 upon successful execution.
 */
int main() {
  int w = 0, h = 0;
  NDL_Init(0);
  NDL_OpenCanvas(&w, &h);
  WindowManager *wm = new WindowManager(w, h);

  uint32_t last = 0, now = 0;
  char buf[64];
  while (1) {
    if (NDL_PollEvent(buf, sizeof(buf))) {
      wm->handle_event(buf);
    }

    if ((now = NDL_GetTicks()) - last > 1000 / RENDER_FPS) {
      wm->handle_event("t");
      last = now;
    }
  }
  return 0;
}
