#include <unistd.h>
#include <stdio.h>

/**
 * The main function of the program, which continuously prints "Hello World!" and "Hello World from Navy-apps" messages.
 * 
 * The function starts by writing "Hello World!\n" to the standard output. It then enters an infinite loop where it increments
 * a volatile integer `j` until it reaches 10000. When `j` equals 10000, the function prints "Hello World from Navy-apps for the Xth time!"
 * to the standard output, where X is an incrementing integer starting from 2. After printing, `j` is reset to 0, and the loop continues indefinitely.
 * 
 * The use of `volatile` for `j` ensures that the compiler does not optimize away the loop, as `j` may be modified externally.
 * 
 * @return int The function is designed to run indefinitely and does not return a meaningful value.
 */
int main() {
  write(1, "Hello World!\n", 13);
  int i = 2;
  volatile int j = 0;
  while (1) {
    j ++;
    if (j == 10000) {
      printf("Hello World from Navy-apps for the %dth time!\n", i ++);
      j = 0;
    }
  }
  return 0;
}
