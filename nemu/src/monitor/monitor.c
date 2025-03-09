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
#include <memory/paddr.h>

void init_rand();
void init_log(const char *log_file);
void init_mem();
void init_difftest(char *ref_so_file, long img_size, int port);
void init_device();
void init_sdb();
void init_disasm();

/**
 * Displays a welcome message and logs important information about the current build configuration.
 * 
 * This method logs the status of the trace feature (enabled or disabled) with color-coded output,
 * and provides additional information if the trace feature is enabled. It also logs the build time
 * and date, and prints a welcome message to the user. The welcome message includes the guest ISA
 * in a stylized format. Finally, it prompts the user to type "help" for assistance and logs an
 * exercise reminder to remove a specific line in the source code. The method asserts false at the
 * end, which will cause the program to terminate if assertions are enabled.
 */
static void welcome() {
  Log("Trace: %s", MUXDEF(CONFIG_TRACE, ANSI_FMT("ON", ANSI_FG_GREEN), ANSI_FMT("OFF", ANSI_FG_RED)));
  IFDEF(CONFIG_TRACE, Log("If trace is enabled, a log file will be generated "
        "to record the trace. This may lead to a large log file. "
        "If it is not necessary, you can disable it in menuconfig"));
  Log("Build time: %s, %s", __TIME__, __DATE__);
  printf("Welcome to %s-NEMU!\n", ANSI_FMT(str(__GUEST_ISA__), ANSI_FG_YELLOW ANSI_BG_RED));
  printf("For help, type \"help\"\n");
  Log("Exercise: Please remove me in the source code and compile NEMU again.");
  assert(0);
}

#ifndef CONFIG_TARGET_AM
#include <getopt.h>

void sdb_set_batch_mode();

static char *log_file = NULL;
static char *diff_so_file = NULL;
static char *img_file = NULL;
static int difftest_port = 1234;

/**
 * Loads an image file into memory and returns its size.
 * 
 * This method attempts to open and read the image file specified by the global
 * variable `img_file`. If `img_file` is NULL, the method logs a message indicating
 * that no image is provided and returns a default built-in image size of 4096 bytes.
 * 
 * If `img_file` is not NULL, the method opens the file in binary read mode, determines
 * its size, and reads the entire content into memory starting at the address returned
 * by `guest_to_host(RESET_VECTOR)`. The method logs the file name and size, and ensures
 * that the file is properly closed after reading.
 * 
 * @return The size of the loaded image in bytes. If no image is provided, returns 4096.
 * 
 * @note The method asserts that the file is successfully opened and that the entire
 *       content is read without errors.
 */
static long load_img() {
  if (img_file == NULL) {
    Log("No image is given. Use the default build-in image.");
    return 4096; // built-in image size
  }

  FILE *fp = fopen(img_file, "rb");
  Assert(fp, "Can not open '%s'", img_file);

  fseek(fp, 0, SEEK_END);
  long size = ftell(fp);

  Log("The image is %s, size = %ld", img_file, size);

  fseek(fp, 0, SEEK_SET);
  int ret = fread(guest_to_host(RESET_VECTOR), size, 1, fp);
  assert(ret == 1);

  fclose(fp);
  return size;
}

/**
 * Parses command-line arguments and configures the program accordingly.
 *
 * This method uses `getopt_long` to process command-line options and arguments.
 * It supports the following options:
 *   -b, --batch: Enables batch mode.
 *   -l, --log=FILE: Specifies the log file to which output will be written.
 *   -d, --diff=REF_SO: Specifies the reference shared object file for DiffTest.
 *   -p, --port=PORT: Specifies the port number for DiffTest.
 *   -h, --help: Displays a help message and exits.
 *
 * The method also expects a positional argument `IMAGE`, which is treated as the image file.
 * If no valid options are provided or if the help option is used, the method prints a usage
 * message and exits.
 *
 * @param argc The number of command-line arguments.
 * @param argv The array of command-line argument strings.
 * @return Returns 0 on successful parsing of arguments.
 */
static int parse_args(int argc, char *argv[]) {
  const struct option table[] = {
    {"batch"    , no_argument      , NULL, 'b'},
    {"log"      , required_argument, NULL, 'l'},
    {"diff"     , required_argument, NULL, 'd'},
    {"port"     , required_argument, NULL, 'p'},
    {"help"     , no_argument      , NULL, 'h'},
    {0          , 0                , NULL,  0 },
  };
  int o;
  while ( (o = getopt_long(argc, argv, "-bhl:d:p:", table, NULL)) != -1) {
    switch (o) {
      case 'b': sdb_set_batch_mode(); break;
      case 'p': sscanf(optarg, "%d", &difftest_port); break;
      case 'l': log_file = optarg; break;
      case 'd': diff_so_file = optarg; break;
      case 1: img_file = optarg; return 0;
      default:
        printf("Usage: %s [OPTION...] IMAGE [args]\n\n", argv[0]);
        printf("\t-b,--batch              run with batch mode\n");
        printf("\t-l,--log=FILE           output log to FILE\n");
        printf("\t-d,--diff=REF_SO        run DiffTest with reference REF_SO\n");
        printf("\t-p,--port=PORT          run DiffTest with port PORT\n");
        printf("\n");
        exit(0);
    }
  }
  return 0;
}

/**
 * Initializes the monitor system by performing a series of global setup tasks.
 * This includes parsing command-line arguments, setting the random seed, opening
 * the log file, initializing memory and devices, performing ISA-specific
 * initialization, loading an image into memory, setting up differential testing,
 * initializing the simple debugger, and displaying a welcome message.
 *
 * @param argc The number of command-line arguments.
 * @param argv An array of command-line argument strings.
 */
void init_monitor(int argc, char *argv[]) {
  /* Perform some global initialization. */

  /* Parse arguments. */
  parse_args(argc, argv);

  /* Set random seed. */
  init_rand();

  /* Open the log file. */
  init_log(log_file);

  /* Initialize memory. */
  init_mem();

  /* Initialize devices. */
  IFDEF(CONFIG_DEVICE, init_device());

  /* Perform ISA dependent initialization. */
  init_isa();

  /* Load the image to memory. This will overwrite the built-in image. */
  long img_size = load_img();

  /* Initialize differential testing. */
  init_difftest(diff_so_file, img_size, difftest_port);

  /* Initialize the simple debugger. */
  init_sdb();

  IFDEF(CONFIG_ITRACE, init_disasm());

  /* Display welcome message. */
  welcome();
}
#else // CONFIG_TARGET_AM
static long load_img() {
  extern char bin_start, bin_end;
  size_t size = &bin_end - &bin_start;
  Log("img size = %ld", size);
  memcpy(guest_to_host(RESET_VECTOR), &bin_start, size);
  return size;
}

/**
 * Initializes the monitor system by setting up various components required for
 * the system to function properly. This includes initializing the random number
 * generator, memory management, instruction set architecture (ISA), and loading
 * the system image. If the device configuration is enabled, it also initializes
 * the device subsystem. Finally, it displays a welcome message to indicate that
 * the system is ready for use.
 *
 * The method performs the following steps in sequence:
 * 1. Initializes the random number generator using `init_rand()`.
 * 2. Initializes the memory management system using `init_mem()`.
 * 3. Initializes the instruction set architecture (ISA) using `init_isa()`.
 * 4. Loads the system image using `load_img()`.
 * 5. If the `CONFIG_DEVICE` macro is defined, initializes the device subsystem
 *    using `init_device()`.
 * 6. Displays a welcome message using `welcome()`.
 */
void am_init_monitor() {
  init_rand();
  init_mem();
  init_isa();
  load_img();
  IFDEF(CONFIG_DEVICE, init_device());
  welcome();
}
#endif
