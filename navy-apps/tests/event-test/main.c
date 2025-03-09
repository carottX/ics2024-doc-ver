#include <stdio.h>
#include <NDL.h>

/**
 * @brief Main entry point for the application.
 *
 * This method initializes the NDL (Native Development Library) and enters an infinite loop
 * to continuously poll for events. If an event is received, it is printed to the console.
 * The loop continues indefinitely until the program is terminated externally.
 *
 * @return int Always returns 0 to indicate successful execution.
 */
int main() {
  NDL_Init(0);
  while (1) {
    char buf[64];
    if (NDL_PollEvent(buf, sizeof(buf))) {
      printf("receive event: %s\n", buf);
    }
  }
  return 0;
}
