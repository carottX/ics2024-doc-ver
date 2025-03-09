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

#include <utils.h>

NEMUState nemu_state = { .state = NEMU_STOP };

/**
 * Determines whether the NEMU (NJU Emulator) exit status is bad.
 * 
 * The function checks the current state of the NEMU emulator. If the emulator
 * has ended (`NEMU_END`) with a halt return value of 0, or if the emulator has
 * quit (`NEMU_QUIT`), the exit status is considered good. Otherwise, the exit
 * status is considered bad.
 * 
 * @return int Returns 1 if the exit status is bad (i.e., the state is neither
 *             `NEMU_END` with a halt return value of 0 nor `NEMU_QUIT`), 
 *             otherwise returns 0.
 */
int is_exit_status_bad() {
  int good = (nemu_state.state == NEMU_END && nemu_state.halt_ret == 0) ||
    (nemu_state.state == NEMU_QUIT);
  return !good;
}
