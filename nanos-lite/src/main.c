#include <common.h>

void init_mm(void);
void init_device(void);
void init_ramdisk(void);
void init_irq(void);
void init_fs(void);
void init_proc(void);

/**
 * @brief The main entry point of the Nanos-lite operating system.
 *
 * This method initializes the Nanos-lite operating system by performing the following steps:
 * 1. Prints a predefined logo to the console.
 * 2. Logs a "Hello World!" message along with the build time and date.
 * 3. Initializes the memory management system.
 * 4. Initializes the device drivers.
 * 5. Initializes the RAM disk.
 * 6. If the system supports context tracking (HAS_CTE), initializes the interrupt handling mechanism.
 * 7. Initializes the file system.
 * 8. Initializes the process management system.
 * 9. Logs a message indicating the completion of the initialization process.
 * 10. If the system supports context tracking (HAS_CTE), yields control to the scheduler.
 * 11. Panics the system if it reaches an unexpected state, indicating a critical failure.
 *
 * @return int This method does not return a meaningful value; it either panics or yields control.
 */
int main() {
  extern const char logo[];
  printf("%s", logo);
  Log("'Hello World!' from Nanos-lite");
  Log("Build time: %s, %s", __TIME__, __DATE__);

  init_mm();

  init_device();

  init_ramdisk();

#ifdef HAS_CTE
  init_irq();
#endif

  init_fs();

  init_proc();

  Log("Finish initialization");

#ifdef HAS_CTE
  yield();
#endif

  panic("Should not reach here");
}
