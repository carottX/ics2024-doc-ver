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
 * @brief Compares the CPU state registers with a reference set of registers.
 *
 * This function is used to verify the correctness of the CPU state by comparing
 * the current CPU registers with a reference set of registers. It is typically
 * used in differential testing to ensure that the CPU state matches the expected
 * state at a specific program counter (pc) address.
 *
 * @param ref_r Pointer to the reference CPU state containing the expected register values.
 * @param pc The program counter address at which the comparison is performed.
 * @return bool Returns `true` if the CPU registers match the reference registers,
 *              otherwise returns `false`. Currently, the implementation always
 *              returns `false` and should be extended to perform the actual comparison.
 */
bool isa_difftest_checkregs(CPU_state *ref_r, vaddr_t pc) {
  return false;
}

/**
 * Attaches the differential testing framework to the current ISA (Instruction Set Architecture) simulation.
 * This method initializes and sets up the necessary components for differential testing, which involves
 * comparing the execution of instructions between two implementations (e.g., a reference model and the DUT
 * - Device Under Test) to ensure consistency and correctness. The method typically configures the test
 * environment, establishes communication channels, and prepares the system for subsequent differential
 * testing operations. It is a crucial step in validating the accuracy of the ISA implementation.
 */
void isa_difftest_attach() {
}
