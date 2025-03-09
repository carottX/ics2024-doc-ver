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
 * Checks if the registers in the current CPU state match the reference registers.
 * This function is typically used in differential testing to compare the state of
 * the CPU against a reference implementation or model.
 *
 * @param ref_r Pointer to the reference CPU state containing the expected register values.
 * @param pc The program counter value at which the check is being performed.
 * @return Returns `true` if the registers match the reference state, otherwise returns `false`.
 *         Currently, this implementation always returns `false` as a placeholder.
 */
bool isa_difftest_checkregs(CPU_state *ref_r, vaddr_t pc) {
  return false;
}

/**
 * Attaches the differential testing framework to the current ISA (Instruction Set Architecture)
 * simulation environment. This method initializes the necessary components and hooks required
 * for differential testing, allowing the comparison of the simulation's execution against a
 * reference model or another implementation. It ensures that the simulation environment is
 * properly configured to support differential testing, enabling the detection of discrepancies
 * in the execution of instructions.
 */
void isa_difftest_attach() {
}
