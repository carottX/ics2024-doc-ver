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

#include <cpu/cpu.h>

void sdb_mainloop();

/**
 * Starts the engine of the system, which initializes and begins the main execution loop.
 * The behavior of this method depends on the configuration of the target environment:
 * - If the target is AM (Abstract Machine), it starts the CPU execution loop indefinitely by
 *   calling `cpu_exec(-1)`, which continuously executes instructions until interrupted.
 * - If the target is not AM, it enters the main loop of the Simple Debugger (SDB) by calling
 *   `sdb_mainloop()`. This loop waits for and processes user commands, allowing interactive
 *   debugging and control of the system.
 */
void engine_start() {
#ifdef CONFIG_TARGET_AM
  cpu_exec(-1);
#else
  /* Receive commands from user. */
  sdb_mainloop();
#endif
}
