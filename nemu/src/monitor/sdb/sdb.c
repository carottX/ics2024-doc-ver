/***************************************************************************************
* Copyright (c) 2014-2024 Zihao Yu, Nanjing University
*
* NEMU is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*          http://license.coscl.org.cn/MulanPSL2
*
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
*
* See the Mulan PSL v2 for more details.
***************************************************************************************/

#include <isa.h>
#include <cpu/cpu.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "sdb.h"

static int is_batch_mode = false;

void init_regex();
void init_wp_pool();

/* We use the `readline' library to provide more flexibility to read from stdin. */
static char* rl_gets() {
  static char *line_read = NULL;

  if (line_read) {
    free(line_read);
    line_read = NULL;
  }

  line_read = readline("(nemu) ");

  if (line_read && *line_read) {
    add_history(line_read);
  }

  return line_read;
}

/**
 * Executes the CPU continuously until an event occurs or the program terminates.
 * This function is typically used to start or continue the execution of the CPU
 * in a debugger or emulator context. It runs the CPU indefinitely by passing
 * a value of -1 to the `cpu_exec` function, which indicates that the CPU should
 * execute without a predefined instruction limit.
 *
 * @param args A pointer to a string containing optional arguments. This parameter
 *             is currently unused in the implementation.
 * @return Always returns 0, indicating successful execution.
 */
static int cmd_c(char *args) {
  cpu_exec(-1);
  return 0;
}


/**
 * Processes the 'q' command.
 *
 * This method is intended to handle the 'q' command, which typically represents
 * a request to quit or exit a program or a specific mode. The method currently
 * returns -1, indicating that the command is not implemented or that the 
 * operation failed.
 *
 * @param args A string containing any additional arguments passed with the 'q' 
 *             command. This parameter is currently unused.
 * @return Returns -1 to indicate that the command is not implemented or that 
 *         the operation failed.
 */
static int cmd_q(char *args) {
  return -1;
}

static int cmd_help(char *args);

static struct {
  const char *name;
  const char *description;
  int (*handler) (char *);
} cmd_table [] = {
  { "help", "Display information about all supported commands", cmd_help },
  { "c", "Continue the execution of the program", cmd_c },
  { "q", "Exit NEMU", cmd_q },

  /* TODO: Add more commands */

};

#define NR_CMD ARRLEN(cmd_table)

/**
 * @brief Displays help information for available commands or details about a specific command.
 *
 * This function processes the `help` command. If no specific command is provided as an argument,
 * it lists all available commands along with their descriptions. If a specific command is provided,
 * it searches for that command in the command table and displays its description. If the command
 * is not found, it prints an error message indicating that the command is unknown.
 *
 * @param args A string containing the arguments passed to the `help` command. 
 *             If `NULL` or empty, the function lists all commands.
 * @return Always returns 0, indicating successful execution.
 */
static int cmd_help(char *args) {
  /* extract the first argument */
  char *arg = strtok(NULL, " ");
  int i;

  if (arg == NULL) {
    /* no argument given */
    for (i = 0; i < NR_CMD; i ++) {
      printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
    }
  }
  else {
    for (i = 0; i < NR_CMD; i ++) {
      if (strcmp(arg, cmd_table[i].name) == 0) {
        printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
        return 0;
      }
    }
    printf("Unknown command '%s'\n", arg);
  }
  return 0;
}

/**
 * Enables batch mode in the system by setting the `is_batch_mode` flag to true.
 * This method is typically used to switch the system into a mode where operations
 * are processed in bulk or deferred, rather than being executed immediately.
 * Batch mode can improve performance by reducing the overhead of individual
 * operations and optimizing resource usage.
 */
void sdb_set_batch_mode() {
  is_batch_mode = true;
}

/**
 * @brief The main loop of the Simple Debugger (SDB).
 *
 * This method handles the primary execution loop of the debugger. It operates in two modes:
 * 1. Batch Mode: If the debugger is in batch mode, it executes a predefined command (`cmd_c`)
 *    with no arguments and exits immediately.
 * 2. Interactive Mode: In interactive mode, it continuously reads input from the user via
 *    `rl_gets()` (a readline-like function). Each input string is parsed into a command and
 *    its arguments. The command is then looked up in the `cmd_table`, and if found, its
 *    corresponding handler is invoked with the arguments. If the command is not recognized,
 *    an "Unknown command" message is printed. The loop continues until the user provides
 *    no input (e.g., by pressing Ctrl+D) or a command handler returns a negative value,
 *    indicating termination.
 *
 * In interactive mode, the input string is split into a command and arguments using `strtok()`.
 * The first token is treated as the command, and the remaining part of the string is treated
 * as arguments. If the `CONFIG_DEVICE` macro is defined, the `sdl_clear_event_queue()` function
 * is called to clear the event queue before processing the command.
 *
 * @note This function does not take any parameters or return any value. It relies on global
 *       state, such as `is_batch_mode`, `cmd_table`, and `NR_CMD`, to determine its behavior.
 */
void sdb_mainloop() {
  if (is_batch_mode) {
    cmd_c(NULL);
    return;
  }

  for (char *str; (str = rl_gets()) != NULL; ) {
    char *str_end = str + strlen(str);

    /* extract the first token as the command */
    char *cmd = strtok(str, " ");
    if (cmd == NULL) { continue; }

    /* treat the remaining string as the arguments,
     * which may need further parsing
     */
    char *args = cmd + strlen(cmd) + 1;
    if (args >= str_end) {
      args = NULL;
    }

#ifdef CONFIG_DEVICE
    extern void sdl_clear_event_queue();
    sdl_clear_event_queue();
#endif

    int i;
    for (i = 0; i < NR_CMD; i ++) {
      if (strcmp(cmd, cmd_table[i].name) == 0) {
        if (cmd_table[i].handler(args) < 0) { return; }
        break;
      }
    }

    if (i == NR_CMD) { printf("Unknown command '%s'\n", cmd); }
  }
}

/**
 * Initializes the Simple Debugger (SDB) by performing the following steps:
 * 1. Compiles the regular expressions required for command parsing and matching.
 * 2. Initializes the watchpoint pool, which is used to manage and track watchpoints
 *    during debugging sessions.
 * This function must be called before using any other SDB functionality to ensure
 * that the necessary components are properly set up.
 */
void init_sdb() {
  /* Compile the regular expressions. */
  init_regex();

  /* Initialize the watchpoint pool. */
  init_wp_pool();
}
