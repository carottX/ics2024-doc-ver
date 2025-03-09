#include <nterm.h>
#include <fcntl.h>
#include <unistd.h>

static int read_fd, write_fd, nterm_to_app[2], app_to_nterm[2]; // file desc

char handle_key(const char *buf);

/**
 * @brief Polls the terminal for input and writes the received data to the terminal buffer.
 * 
 * This method reads data from the file descriptor `read_fd` into a static buffer `buf` of size 4096 bytes.
 * If data is successfully read (i.e., `nread` is greater than 0), the method writes the received data
 * to the terminal using the `term->write()` method. This function is typically used in a loop to
 * continuously monitor and process input from the terminal.
 * 
 * @note The buffer `buf` is static, meaning it retains its contents between calls to this function.
 * The `read_fd` and `term` variables are assumed to be defined and initialized elsewhere in the code.
 */
static void poll_terminal() {
  static char buf[4096];
  int nread = read(read_fd, buf, sizeof(buf));
  if (nread > 0) {
    term->write(buf, nread);
  }
}

/**
 * @brief Forks a child process and executes a specified terminal application.
 *
 * This method forks a child process using `vfork()` and executes a terminal application
 * specified by `nterm_proc`. It sets up the environment variables `LINES` and `COLUMNS`
 * to the values of `H` and `W` respectively, and sets the `TERM` environment variable to
 * "ansi". It also creates two pipes for communication between the parent and child processes:
 * one for sending data to the terminal application (`nterm_to_app`) and one for receiving
 * data from the terminal application (`app_to_nterm`). The child process's standard input,
 * output, and error are redirected to these pipes. After the child process is forked and
 * the terminal application is executed, the parent process restores its standard input,
 * output, and error file descriptors and closes the duplicated file descriptors.
 *
 * @param nterm_proc The path to the terminal application to be executed by the child process.
 *
 * @note This method uses `vfork()` to create the child process, which is more efficient
 * than `fork()` but requires careful handling of shared resources. The child process
 * immediately calls `execve()` to replace its address space with the terminal application.
 * If `execve()` fails, the child process will terminate with an assertion failure.
 */
static void fork_child(const char *nterm_proc) {
  const char *argv[] = {
    nterm_proc,
    NULL,
  };
  char env_lines[32]; sprintf(env_lines, "LINES=%d", H);
  char env_columns[32]; sprintf(env_columns, "COLUMNS=%d", W);
  const char *envp[] = {
    env_lines,
    env_columns,
    "TERM=ansi",
    NULL
  };

  assert(0 == pipe(nterm_to_app));
  assert(0 == pipe(app_to_nterm));
  read_fd = app_to_nterm[0];
  write_fd = nterm_to_app[1];

  int flags = fcntl(read_fd, F_GETFL, 0);
  fcntl(read_fd, F_SETFL, flags | O_NONBLOCK);

  int stdin_fd = dup(0), stdout_fd = dup(1), stderr_fd = dup(2);

  dup2(nterm_to_app[0], 0);
  dup2(app_to_nterm[1], 1);
  dup2(app_to_nterm[1], 2);

  pid_t p = vfork();
  if (p == 0) {
    execve(argv[0], (char**)argv, (char**)envp);
    assert(0);
  } else {
    dup2(stdin_fd, 0); dup2(stdout_fd, 1); dup2(stderr_fd, 2);
    close(stdin_fd); close(stdout_fd); close(stderr_fd);
  }
}

/**
 * @brief Runs an external application and manages its interaction with the terminal.
 *
 * This method forks a child process to execute the external application specified by `app_path`.
 * It then enters an infinite loop where it continuously polls the terminal for input, reads characters
 * from stdin, and processes them. If the input starts with the character 'k', it interprets the
 * subsequent characters as a keypress and sends the corresponding response to the external application.
 * The terminal is refreshed after each iteration of the loop to reflect any changes.
 *
 * The method does not return and will terminate the program with an assertion failure if it exits
 * the loop unexpectedly.
 *
 * @param app_path The path to the external application to be executed.
 */
void extern_app_run(const char *app_path) {
  int elapse = -1, ntick = 0, last_k = 0;

  fork_child(app_path); // fork the child process and setup fds

  while (1) {
    poll_terminal();
    char buf[256], *p = buf, ch;
    while ((ch = getc(stdin)) != -1) {
      *p ++ = ch;
      if (ch == '\n') break;
    }
    *p = '\0';

    if (buf[0] == 'k') {
      const char *res = term->keypress(handle_key(buf + 1));
      if (res) {
        write(write_fd, res, strlen(res));
      }
    }

    refresh_terminal();
  }

  assert(0);
}
