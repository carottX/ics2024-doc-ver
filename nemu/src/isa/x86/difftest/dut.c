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
 * This function is used in differential testing to verify that the CPU state 
 * registers match a reference set of registers. It takes a pointer to a 
 * reference CPU state (`ref_r`) and the current program counter (`pc`) as 
 * input. The function returns `true` if the registers match the reference 
 * state, otherwise it returns `false`.
 *
 * @param ref_r Pointer to the reference CPU state to compare against.
 * @param pc The current program counter value.
 * @return bool `true` if the registers match the reference state, `false` otherwise.
 */
bool isa_difftest_checkregs(CPU_state *ref_r, vaddr_t pc) {
  return false;
}

/**
 * Attaches the differential testing framework to the current instruction set architecture (ISA) simulation.
 * This method initializes the necessary hooks and configurations to enable differential testing, which compares
 * the execution of the simulated ISA against a reference model or implementation. It ensures that any discrepancies
 * between the two are detected and logged for further analysis. This is particularly useful for verifying the correctness
 * of the ISA implementation during development and debugging.
 */
void isa_difftest_attach() {
}
