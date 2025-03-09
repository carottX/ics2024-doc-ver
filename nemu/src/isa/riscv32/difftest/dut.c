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
#include <cpu/difftest.h>
#include "../local-include/reg.h"

/**
 * Checks if the registers in the reference CPU state match the current CPU state.
 * This function is typically used in differential testing to verify that the
 * emulator's CPU state matches the reference implementation after executing an
 * instruction or a sequence of instructions.
 *
 * @param ref_r Pointer to the reference CPU state to compare against.
 * @param pc The program counter value at the time of the check.
 * @return `true` if the registers match, `false` otherwise.
 */
bool isa_difftest_checkregs(CPU_state *ref_r, vaddr_t pc) {
  return false;
}

/**
 * Attaches the differential testing framework to the current instruction set architecture (ISA) simulation.
 * This method initializes and configures the necessary components to enable differential testing, which involves
 * comparing the execution results of the simulated ISA against a reference model or another implementation to
 * ensure correctness and identify discrepancies. It typically sets up communication channels, initializes state,
 * and prepares the environment for subsequent differential testing operations.
 */
void isa_difftest_attach() {
}
