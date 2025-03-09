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

#ifndef __CPU_DIFFTEST_H__
#define __CPU_DIFFTEST_H__

#include <common.h>
#include <difftest-def.h>

#ifdef CONFIG_DIFFTEST
void difftest_skip_ref();
void difftest_skip_dut(int nr_ref, int nr_dut);
void difftest_set_patch(void (*fn)(void *arg), void *arg);
void difftest_step(vaddr_t pc, vaddr_t npc);
void difftest_detach();
void difftest_attach();
#else
/**
 * Skips the reference model's execution in the difftest framework.
 * This method is typically used to bypass the reference model's execution
 * during testing or debugging, allowing the test to proceed without
 * comparing against the reference model's output. It is useful in scenarios
 * where the reference model's behavior is either unnecessary or irrelevant
 * for the current test case.
 */
static inline void difftest_skip_ref() {}
/**
 * Skips the Device Under Test (DUT) in the difftest comparison process.
 * This method is used to bypass the DUT for a specified number of reference
 * and DUT instructions, effectively ignoring the DUT's execution for those
 * instructions during the difftest. This can be useful in scenarios where
 * the DUT's behavior is known to diverge from the reference model for certain
 * instructions or when specific instructions need to be excluded from the
 * comparison.
 *
 * @param nr_ref The number of reference instructions to skip.
 * @param nr_dut The number of DUT instructions to skip.
 */
static inline void difftest_skip_dut(int nr_ref, int nr_dut) {}
/**
 * @brief Sets a patch function for difftesting purposes.
 *
 * This method allows the caller to specify a custom function (`fn`) that will be invoked
 * during difftesting. The function is passed a user-defined argument (`arg`) that can be
 * used to provide context or data to the function. This is typically used to inject
 * specific behavior or modifications during testing scenarios.
 *
 * @param fn A pointer to the function to be called during difftesting. The function must
 *           accept a single `void*` argument and return `void`.
 * @param arg A pointer to the argument that will be passed to the function `fn`. This can
 *            be used to provide context or data to the function.
 */
static inline void difftest_set_patch(void (*fn)(void *arg), void *arg) {}
/**
 * @brief Simulates a single step in the differential testing process.
 *
 * This method is used to compare the execution state of the current system against a reference model
 * (e.g., a golden model or simulator) at a specific program counter (PC) and next program counter (NPC).
 * It is typically called after each instruction execution to verify that the system's state matches
 * the expected state provided by the reference model. The method is marked as `static inline` for
 * performance optimization, as it is often called frequently during testing.
 *
 * @param pc The current program counter (PC) value, representing the address of the instruction
 *           that was just executed.
 * @param npc The next program counter (NPC) value, representing the address of the next instruction
 *            to be executed.
 */
static inline void difftest_step(vaddr_t pc, vaddr_t npc) {}
/**
 * Detaches the difftest component from the current execution environment.
 * This method is typically used to stop the difftest mechanism, which is responsible
 * for comparing the execution of two systems (e.g., a reference model and a target model).
 * After calling this method, the difftest component will no longer perform any comparisons
 * or logging of discrepancies between the two systems. This is useful when the difftest
 * functionality is no longer needed or when transitioning to a different testing phase.
 */
static inline void difftest_detach() {}
/**
 * Attaches the difftest framework to the current simulation environment.
 * This method is responsible for initializing and connecting the difftest
 * mechanism, which is used to compare the execution of the simulated system
 * against a reference model or golden model. It ensures that the simulation
 * can track and verify the correctness of the system's behavior by comparing
 * its state with the expected state at key points during execution.
 * This method is typically called during the setup phase of the simulation.
 * It is marked as `static inline` for performance optimization, as it is
 * expected to be called frequently and with minimal overhead.
 */
static inline void difftest_attach() {}
#endif

extern void (*ref_difftest_memcpy)(paddr_t addr, void *buf, size_t n, bool direction);
extern void (*ref_difftest_regcpy)(void *dut, bool direction);
extern void (*ref_difftest_exec)(uint64_t n);
extern void (*ref_difftest_raise_intr)(uint64_t NO);

/**
 * @brief Compares the reference value (`ref`) with the device under test value (`dut`) 
 *        and logs a message if they differ.
 *
 * This function is used to verify that the device under test (DUT) produces the expected 
 * result (`ref`) after executing an instruction at a specific program counter (`pc`). 
 * If the values differ, a detailed log message is generated, including the name of the 
 * register or value being compared, the program counter, the expected value (`ref`), 
 * the actual value (`dut`), and the bitwise difference between the two values.
 *
 * @param name The name of the register or value being compared (for logging purposes).
 * @param pc The program counter (address) of the instruction being checked.
 * @param ref The expected reference value.
 * @param dut The actual value produced by the device under test.
 * @return `true` if `ref` and `dut` are equal, `false` otherwise.
 */
static inline bool difftest_check_reg(const char *name, vaddr_t pc, word_t ref, word_t dut) {
  if (ref != dut) {
    Log("%s is different after executing instruction at pc = " FMT_WORD
        ", right = " FMT_WORD ", wrong = " FMT_WORD ", diff = " FMT_WORD,
        name, pc, ref, dut, ref ^ dut);
    return false;
  }
  return true;
}

#endif
