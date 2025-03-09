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
#include <difftest-def.h>
#include <memory/paddr.h>

/**
 * @brief Copies data between a memory buffer and a specified address.
 *
 * This function is intended to facilitate memory copy operations for difftesting purposes.
 * It copies `n` bytes of data between a memory buffer (`buf`) and a specified address (`addr`).
 * The `direction` parameter determines the direction of the copy operation:
 * - If `direction` is `true`, data is copied from the buffer to the specified address.
 * - If `direction` is `false`, data is copied from the specified address to the buffer.
 *
 * @param addr The target address in memory where the data will be copied to/from.
 * @param buf A pointer to the buffer containing the data to be copied or where the data will be stored.
 * @param n The number of bytes to copy.
 * @param direction A boolean flag indicating the direction of the copy operation.
 *                 `true` for buffer to address, `false` for address to buffer.
 *
 * @note This function currently asserts `0` and does not perform any actual operation.
 *       It is a placeholder and should be implemented as needed.
 */
__EXPORT void difftest_memcpy(paddr_t addr, void *buf, size_t n, bool direction) {
  assert(0);
}

/**
 * @brief Copies register values between the DUT (Device Under Test) and the reference model.
 *
 * This function is responsible for transferring register values either from the DUT to the
 * reference model or vice versa, depending on the specified direction. It is typically used
 * in a co-simulation environment to ensure that the DUT and the reference model remain
 * synchronized during the execution of a test.
 *
 * @param dut A pointer to the DUT, which contains the register values to be copied.
 * @param direction A boolean flag indicating the direction of the copy operation:
 *                  - `true` to copy from the DUT to the reference model.
 *                  - `false` to copy from the reference model to the DUT.
 *
 * @note This function is currently unimplemented and will trigger an assertion failure if called.
 */
__EXPORT void difftest_regcpy(void *dut, bool direction) {
  assert(0);
}

/**
 * @brief Executes a difftest for a specified number of steps.
 *
 * This method is intended to simulate or compare the execution of a system under test
 * for a given number of steps (`n`). The primary purpose of this function is to support
 * differential testing, where the behavior of the system is compared against a reference
 * implementation or model. Currently, this method is unimplemented and will trigger
 * an assertion failure if called.
 *
 * @param n The number of steps to execute in the difftest.
 * @return void
 */
__EXPORT void difftest_exec(uint64_t n) {
  assert(0);
}

/**
 * @brief Raises an interrupt for differential testing purposes.
 *
 * This method is intended to simulate the raising of an interrupt during differential testing.
 * It takes a single parameter, `NO`, which represents the interrupt number or identifier.
 * The method currently contains an `assert(0)` statement, which will always trigger an assertion failure,
 * indicating that the functionality is not yet implemented or that this method should not be called
 * under normal circumstances.
 *
 * @param NO The interrupt number or identifier to be raised.
 * @return void
 */
__EXPORT void difftest_raise_intr(word_t NO) {
  assert(0);
}

/**
 * @brief Initializes the difftest environment.
 *
 * This function sets up the necessary components for the difftest framework.
 * It first initializes the memory subsystem by calling `init_mem()`, which prepares
 * the memory for subsequent operations. After memory initialization, it performs
 * ISA (Instruction Set Architecture) dependent initialization by calling `init_isa()`.
 * This ensures that the environment is properly configured for the specific ISA
 * being tested.
 *
 * @param port The port number to be used for the difftest initialization.
 *             This parameter is currently unused in the implementation but may
 *             be utilized in future extensions.
 */
__EXPORT void difftest_init(int port) {
  void init_mem();
  init_mem();
  /* Perform ISA dependent initialization. */
  init_isa();
}
