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
#include <memory/paddr.h>

// this is not consistent with uint8_t
// but it is ok since we do not access the array directly
static const uint32_t img [] = {
  0x3c048000,  // lui a0, 0x8000
  0xac800000,  // sw  zero, 0(a0)
  0x8c820000,  // lw  v0,0(a0)
  0x7000003f,  // sdbbp (used as nemu_trap)
};

/**
 * Restarts the CPU by resetting its state to the initial conditions.
 * This involves setting the program counter (PC) to the reset vector,
 * which is the address where the CPU begins execution after a reset.
 * Additionally, the zero register (GPR[0]) is explicitly set to 0,
 * ensuring it maintains its invariant of always holding the value 0.
 */
static void restart() {
  /* Set the initial program counter. */
  cpu.pc = RESET_VECTOR;

  /* The zero register is always 0. */
  cpu.gpr[0] = 0;
}

/**
 * Initializes the Instruction Set Architecture (ISA) of the virtual computer system.
 * This method performs two main tasks:
 * 1. Loads a built-in image into the guest memory at the location specified by the RESET_VECTOR.
 *    The image is copied from the source 'img' to the guest memory using memcpy.
 * 2. Initializes the virtual computer system by calling the 'restart' function, which
 *    resets the system to its initial state.
 * This method is typically called during the bootstrapping process of the virtual machine.
 */
void init_isa() {
  /* Load built-in image. */
  memcpy(guest_to_host(RESET_VECTOR), img, sizeof(img));

  /* Initialize this virtual computer system. */
  restart();
}
