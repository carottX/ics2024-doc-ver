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
#include <cpu/ifetch.h>
#include <isa.h>
#include <cpu/difftest.h>

/**
 * Sets the state of the NEMU (NJU Emulator) and updates related state variables.
 * This function is typically used to control the emulator's execution state and
 * record the program counter (PC) and halt return value when the emulator halts.
 *
 * @param state The new state to set for the NEMU. This could represent different
 *              execution states such as running, halted, or paused.
 * @param pc The program counter (PC) value to be recorded when the emulator halts.
 *            This represents the address of the instruction where the emulator
 *            stopped execution.
 * @param halt_ret The return value to be associated with the halt state. This
 *                 could indicate the reason for halting or provide additional
 *                 context about the halt event.
 *
 * @note This function internally calls `difftest_skip_ref()` to skip reference
 *       model checking, which is useful when updating the emulator state without
 *       triggering unnecessary difftest operations.
 */
void set_nemu_state(int state, vaddr_t pc, int halt_ret) {
  difftest_skip_ref();
  nemu_state.state = state;
  nemu_state.halt_pc = pc;
  nemu_state.halt_ret = halt_ret;
}

/**
 * @brief Handles an invalid instruction exception by printing detailed diagnostic information.
 *
 * This method is invoked when an invalid or unimplemented instruction is encountered during execution.
 * It fetches the instruction bytes at the current program counter (PC) and prints them in both
 * hexadecimal and byte formats. Additionally, it provides guidance on diagnosing the cause of the
 * exception, which could be either an unimplemented instruction or an implementation error.
 *
 * The method performs the following steps:
 * 1. Fetches the next two 4-byte instructions from the current PC.
 * 2. Prints the fetched instruction bytes in hexadecimal and byte formats.
 * 3. Outputs diagnostic messages to help identify the root cause of the exception:
 *    - Whether the instruction is unimplemented.
 *    - Whether there is an implementation error.
 * 4. Prints additional guidance for debugging, including a reference to the ISA documentation
 *    and a reminder about the importance of testing.
 * 5. Sets the emulator state to `NEMU_ABORT` to halt execution.
 *
 * @param thispc The program counter (PC) where the invalid instruction was encountered.
 */
__attribute__((noinline))
void invalid_inst(vaddr_t thispc) {
  uint32_t temp[2];
  vaddr_t pc = thispc;
  temp[0] = inst_fetch(&pc, 4);
  temp[1] = inst_fetch(&pc, 4);

  uint8_t *p = (uint8_t *)temp;
  printf("invalid opcode(PC = " FMT_WORD "):\n"
      "\t%02x %02x %02x %02x %02x %02x %02x %02x ...\n"
      "\t%08x %08x...\n",
      thispc, p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], temp[0], temp[1]);

  printf("There are two cases which will trigger this unexpected exception:\n"
      "1. The instruction at PC = " FMT_WORD " is not implemented.\n"
      "2. Something is implemented incorrectly.\n", thispc);
  printf("Find this PC(" FMT_WORD ") in the disassembling result to distinguish which case it is.\n\n", thispc);
  printf(ANSI_FMT("If it is the first case, see\n%s\nfor more details.\n\n"
        "If it is the second case, remember:\n"
        "* The machine is always right!\n"
        "* Every line of untested code is always wrong!\n\n", ANSI_FG_RED), isa_logo);

  set_nemu_state(NEMU_ABORT, thispc, -1);
}
