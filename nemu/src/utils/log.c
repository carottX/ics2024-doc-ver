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

#include <common.h>

extern uint64_t g_nr_guest_inst;

#ifndef CONFIG_TARGET_AM
FILE *log_fp = NULL;

/**
 * Initializes the logging system by setting up the log output destination.
 * 
 * This function configures the logging output to either a specified file or 
 * standard output (stdout). If a valid log file path is provided, the function 
 * attempts to open the file in write mode and sets it as the logging destination. 
 * If the file cannot be opened, an assertion error is triggered. If no log file 
 * is provided (i.e., `log_file` is NULL), the logging output defaults to stdout.
 * 
 * After setting the log destination, a log message is generated indicating 
 * where the log output is being written (either the specified file or stdout).
 * 
 * @param log_file A pointer to a null-terminated string specifying the path 
 *                 to the log file. If NULL, logging output is directed to stdout.
 */
void init_log(const char *log_file) {
  log_fp = stdout;
  if (log_file != NULL) {
    FILE *fp = fopen(log_file, "w");
    Assert(fp, "Can not open '%s'", log_file);
    log_fp = fp;
  }
  Log("Log is written to %s", log_file ? log_file : "stdout");
}

/**
 * Determines whether logging is enabled based on the current configuration and the number of guest instructions executed.
 *
 * This method checks if logging is enabled by evaluating the following conditions:
 * - If `CONFIG_TRACE` is defined, logging is enabled only if the number of guest instructions (`g_nr_guest_inst`) falls within the range
 *   specified by `CONFIG_TRACE_START` and `CONFIG_TRACE_END`. Specifically, `g_nr_guest_inst` must be greater than or equal to
 *   `CONFIG_TRACE_START` and less than or equal to `CONFIG_TRACE_END`.
 * - If `CONFIG_TRACE` is not defined, logging is disabled, and the method returns `false`.
 *
 * @return `true` if logging is enabled based on the conditions above, otherwise `false`.
 */
bool log_enable() {
  return MUXDEF(CONFIG_TRACE, (g_nr_guest_inst >= CONFIG_TRACE_START) &&
         (g_nr_guest_inst <= CONFIG_TRACE_END), false);
}
#endif
