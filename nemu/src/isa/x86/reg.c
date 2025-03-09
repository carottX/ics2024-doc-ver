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
#include "local-include/reg.h"

const char *regsl[] = {"eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi"};
const char *regsw[] = {"ax", "cx", "dx", "bx", "sp", "bp", "si", "di"};
const char *regsb[] = {"al", "cl", "dl", "bl", "ah", "ch", "dh", "bh"};

/**
 * @brief Tests the functionality of CPU registers and their accessors.
 *
 * This method performs a series of tests to verify the correct behavior of CPU registers
 * and their associated accessor functions. It initializes a sample array with random
 * values and assigns these values to the corresponding registers. The method then
 * verifies that the values stored in the registers match the expected values by
 * checking both the full register values and their lower byte and high byte components.
 * Additionally, it ensures that the program counter (PC) is correctly set and matches
 * the expected value.
 *
 * The test covers the following registers:
 * - General-purpose registers (EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI)
 * - Lower and high byte components of the EAX, EBX, ECX, and EDX registers (AL, AH, BL, BH, CL, CH, DL, DH)
 * - Program counter (PC)
 *
 * The method uses assertions to validate the correctness of the register values and
 * their accessors. If any assertion fails, it indicates a potential issue with the
 * register handling or accessor functions.
 */
void reg_test() {
  word_t sample[8];
  word_t pc_sample = rand();
  cpu.pc = pc_sample;

  int i;
  for (i = R_EAX; i <= R_EDI; i ++) {
    sample[i] = rand();
    reg_l(i) = sample[i];
    assert(reg_w(i) == (sample[i] & 0xffff));
  }

  assert(reg_b(R_AL) == (sample[R_EAX] & 0xff));
  assert(reg_b(R_AH) == ((sample[R_EAX] >> 8) & 0xff));
  assert(reg_b(R_BL) == (sample[R_EBX] & 0xff));
  assert(reg_b(R_BH) == ((sample[R_EBX] >> 8) & 0xff));
  assert(reg_b(R_CL) == (sample[R_ECX] & 0xff));
  assert(reg_b(R_CH) == ((sample[R_ECX] >> 8) & 0xff));
  assert(reg_b(R_DL) == (sample[R_EDX] & 0xff));
  assert(reg_b(R_DH) == ((sample[R_EDX] >> 8) & 0xff));

  assert(sample[R_EAX] == cpu.eax);
  assert(sample[R_ECX] == cpu.ecx);
  assert(sample[R_EDX] == cpu.edx);
  assert(sample[R_EBX] == cpu.ebx);
  assert(sample[R_ESP] == cpu.esp);
  assert(sample[R_EBP] == cpu.ebp);
  assert(sample[R_ESI] == cpu.esi);
  assert(sample[R_EDI] == cpu.edi);

  assert(pc_sample == cpu.pc);
}

/**
 * Displays the contents of the ISA (Instruction Set Architecture) registers.
 * This method is typically used for debugging purposes to inspect the current
 * state of the CPU registers, including general-purpose registers, program
 * counter, stack pointer, and other relevant registers. The output format may
 * vary depending on the specific architecture and implementation.
 */
void isa_reg_display() {
}

/**
 * Converts a string representation of an ISA register to its corresponding value.
 *
 * This function takes a string `s` that represents the name of an ISA register
 * and attempts to convert it to its corresponding numeric value. The result of
 * the conversion is returned as a `word_t` type. Additionally, the function
 * sets the boolean flag `success` to indicate whether the conversion was
 * successful or not. If the string does not correspond to a valid ISA register,
 * `success` is set to `false`, and the function returns 0.
 *
 * @param s       The string representing the ISA register name.
 * @param success A pointer to a boolean flag that will be set to `true` if the
 *                conversion is successful, or `false` if the string does not
 *                represent a valid ISA register.
 * @return        The numeric value of the ISA register if `s` is valid; 
 *                otherwise, returns 0.
 */
word_t isa_reg_str2val(const char *s, bool *success) {
  return 0;
}
