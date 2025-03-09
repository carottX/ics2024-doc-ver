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
#include "local-include/reg.h"

static const uint8_t img []  = {
  0xb8, 0x34, 0x12, 0x00, 0x00,        // 100000:  movl  $0x1234,%eax
  0xb9, 0x27, 0x00, 0x10, 0x00,        // 100005:  movl  $0x100027,%ecx
  0x89, 0x01,                          // 10000a:  movl  %eax,(%ecx)
  0x66, 0xc7, 0x41, 0x04, 0x01, 0x00,  // 10000c:  movw  $0x1,0x4(%ecx)
  0xbb, 0x02, 0x00, 0x00, 0x00,        // 100012:  movl  $0x2,%ebx
  0x66, 0xc7, 0x84, 0x99, 0x00, 0xe0,  // 100017:  movw  $0x1,-0x2000(%ecx,%ebx,4)
  0xff, 0xff, 0x01, 0x00,
  0xb8, 0x00, 0x00, 0x00, 0x00,        // 100021:  movl  $0x0,%eax
  0xcc,                                // 100026:  int3 (used as nemu_trap)
};

/**
 * Restarts the CPU by resetting the program counter (PC) to the initial 
 * instruction pointer address. This method sets the PC to the value 
 * stored in the RESET_VECTOR, which is typically the memory address 
 * where the CPU begins execution after a reset or restart.
 */
static void restart() {
  /* Set the initial instruction pointer. */
  cpu.pc = RESET_VECTOR;
}

/**
 * Initializes the Instruction Set Architecture (ISA) for the virtual computer system.
 * This function performs the following operations:
 * 1. Tests the implementation of the `CPU_state` structure by calling `reg_test()` if
 *    the system is not configured to be deterministic (i.e., `CONFIG_DETERMINISTIC` is not defined).
 * 2. Loads a built-in image into the system's memory by copying it to the reset vector location.
 * 3. Initializes the virtual computer system by calling `restart()`.
 * 
 * Note: The function includes commented-out code for initializing various hardware components
 * (e.g., i8259a, mc146818rtc, i8253, ioport80, i8237a), which can be enabled by uncommenting
 * the relevant sections if needed.
 */
void init_isa() {
  /* Test the implementation of the `CPU_state' structure. */
  void reg_test();
#ifndef CONFIG_DETERMINISTIC
  reg_test();
#endif

  /* Load built-in image. */
  memcpy(guest_to_host(RESET_VECTOR), img, sizeof(img));

  /* Initialize this virtual computer system. */
  restart();

#if 0
  void init_i8259a();
  void init_mc146818rtc();
  void init_i8253();
  void init_ioport80();
  void init_i8237a();
  init_i8259a();
  init_mc146818rtc();
  init_i8253();
  init_ioport80();
  init_i8237a();
#endif
}
