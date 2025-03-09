#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/**
 * The main function of the program. It accepts command-line arguments and performs the following steps:
 * 1. Parses the first command-line argument (if provided) as an integer. If no argument is provided, it defaults to 1.
 * 2. Prints the program's name (argv[0]) and the parsed integer value.
 * 3. Increments the parsed integer by 1 and converts it to a string.
 * 4. Re-executes the program with the incremented integer as the new command-line argument using execl.
 * 5. If execl fails, the program returns 0.
 *
 * @param argc The number of command-line arguments.
 * @param argv An array of strings representing the command-line arguments.
 * @return Always returns 0, as the program either re-executes itself or terminates.
 */
int main(int argc, char *argv[]) {
  int n = (argc >= 2 ? atoi(argv[1]) : 1);
  printf("%s: argv[1] = %d\n", argv[0], n);

  char buf[16];
  sprintf(buf, "%d", n + 1);
  execl(argv[0], argv[0], buf, NULL);
  return 0;
}
