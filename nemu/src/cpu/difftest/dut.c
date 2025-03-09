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

#include <dlfcn.h>

#include <isa.h>
#include <cpu/cpu.h>
#include <memory/paddr.h>
#include <utils.h>
#include <difftest-def.h>

void (*ref_difftest_memcpy)(paddr_t addr, void *buf, size_t n, bool direction) = NULL;
void (*ref_difftest_regcpy)(void *dut, bool direction) = NULL;
void (*ref_difftest_exec)(uint64_t n) = NULL;
void (*ref_difftest_raise_intr)(uint64_t NO) = NULL;

#ifdef CONFIG_DIFFTEST

static bool is_skip_ref = false;
static int skip_dut_nr_inst = 0;

// this is used to let ref skip instructions which
// can not produce consistent behavior with NEMU
void difftest_skip_ref() {
  is_skip_ref = true;
  // If such an instruction is one of the instruction packing in QEMU
  // (see below), we end the process of catching up with QEMU's pc to
  // keep the consistent behavior in our best.
  // Note that this is still not perfect: if the packed instructions
  // already write some memory, and the incoming instruction in NEMU
  // will load that memory, we will encounter false negative. But such
  // situation is infrequent.
  skip_dut_nr_inst = 0;
}

// this is used to deal with instruction packing in QEMU.
// Sometimes letting QEMU step once will execute multiple instructions.
// We should skip checking until NEMU's pc catches up with QEMU's pc.
// The semantic is
//   Let REF run `nr_ref` instructions first.
//   We expect that DUT will catch up with REF within `nr_dut` instructions.
void difftest_skip_dut(int nr_ref, int nr_dut) {
  skip_dut_nr_inst += nr_dut;

  while (nr_ref -- > 0) {
    ref_difftest_exec(1);
  }
}

/**
 * Initializes differential testing by loading a shared library containing reference
 * implementations of key functions and setting up the testing environment.
 *
 * This function performs the following steps:
 * 1. Asserts that the provided shared library file path is not NULL.
 * 2. Dynamically loads the shared library using `dlopen`.
 * 3. Resolves and assigns the following functions from the shared library:
 *    - `difftest_memcpy`: Copies memory between the reference and the guest.
 *    - `difftest_regcpy`: Copies register state between the reference and the guest.
 *    - `difftest_exec`: Executes a single instruction on the reference.
 *    - `difftest_raise_intr`: Raises an interrupt on the reference.
 *    - `difftest_init`: Initializes the reference with the specified port.
 * 4. Logs the activation of differential testing and its implications.
 * 5. Initializes the reference using the provided port.
 * 6. Copies the initial memory and register state from the guest to the reference.
 *
 * @param ref_so_file Path to the shared library containing the reference implementations.
 * @param img_size Size of the memory image to be copied.
 * @param port Port number to be used for initializing the reference.
 */
void init_difftest(char *ref_so_file, long img_size, int port) {
  assert(ref_so_file != NULL);

  void *handle;
  handle = dlopen(ref_so_file, RTLD_LAZY);
  assert(handle);

  ref_difftest_memcpy = dlsym(handle, "difftest_memcpy");
  assert(ref_difftest_memcpy);

  ref_difftest_regcpy = dlsym(handle, "difftest_regcpy");
  assert(ref_difftest_regcpy);

  ref_difftest_exec = dlsym(handle, "difftest_exec");
  assert(ref_difftest_exec);

  ref_difftest_raise_intr = dlsym(handle, "difftest_raise_intr");
  assert(ref_difftest_raise_intr);

  void (*ref_difftest_init)(int) = dlsym(handle, "difftest_init");
  assert(ref_difftest_init);

  Log("Differential testing: %s", ANSI_FMT("ON", ANSI_FG_GREEN));
  Log("The result of every instruction will be compared with %s. "
      "This will help you a lot for debugging, but also significantly reduce the performance. "
      "If it is not necessary, you can turn it off in menuconfig.", ref_so_file);

  ref_difftest_init(port);
  ref_difftest_memcpy(RESET_VECTOR, guest_to_host(RESET_VECTOR), img_size, DIFFTEST_TO_REF);
  ref_difftest_regcpy(&cpu, DIFFTEST_TO_REF);
}

/**
 * Checks the CPU registers against a reference state and handles discrepancies.
 *
 * This method compares the current CPU registers with a provided reference state
 * using the `isa_difftest_checkregs` function. If the registers do not match the
 * reference state, the NEMU (NJU Emulator) state is set to `NEMU_ABORT`, indicating
 * an emulation error. Additionally, the program counter (`pc`) at the point of
 * discrepancy is recorded in `nemu_state.halt_pc`, and the current register values
 * are displayed using `isa_reg_display`.
 *
 * @param ref Pointer to the reference CPU state to compare against.
 * @param pc The program counter value at the point of the check.
 */
static void checkregs(CPU_state *ref, vaddr_t pc) {
  if (!isa_difftest_checkregs(ref, pc)) {
    nemu_state.state = NEMU_ABORT;
    nemu_state.halt_pc = pc;
    isa_reg_display();
  }
}

/**
 * Performs a differential testing step between the Device Under Test (DUT) and the reference model.
 * This function is used to compare the state of the DUT with the reference model after executing
 * a single instruction or a sequence of instructions. It handles cases where certain instructions
 * need to be skipped or where the DUT needs to catch up with the reference model.
 *
 * @param pc The program counter value of the current instruction in the DUT.
 * @param npc The program counter value of the next instruction in the DUT.
 *
 * The function performs the following steps:
 * 1. If `skip_dut_nr_inst` is greater than 0, it copies the register state from the reference model
 *    to the DUT and checks if the DUT's next program counter (`npc`) matches the reference model's
 *    program counter. If they match, it resets `skip_dut_nr_inst` and performs a register check.
 *    If they do not match, it decrements `skip_dut_nr_inst` and panics if it reaches 0.
 * 2. If `is_skip_ref` is true, it skips the checking of the current instruction by copying the
 *    register state from the DUT to the reference model and resets `is_skip_ref`.
 * 3. Otherwise, it executes a single instruction in the reference model, copies the register state
 *    from the reference model to the DUT, and performs a register check.
 */
void difftest_step(vaddr_t pc, vaddr_t npc) {
  CPU_state ref_r;

  if (skip_dut_nr_inst > 0) {
    ref_difftest_regcpy(&ref_r, DIFFTEST_TO_DUT);
    if (ref_r.pc == npc) {
      skip_dut_nr_inst = 0;
      checkregs(&ref_r, npc);
      return;
    }
    skip_dut_nr_inst --;
    if (skip_dut_nr_inst == 0)
      panic("can not catch up with ref.pc = " FMT_WORD " at pc = " FMT_WORD, ref_r.pc, pc);
    return;
  }

  if (is_skip_ref) {
    // to skip the checking of an instruction, just copy the reg state to reference design
    ref_difftest_regcpy(&cpu, DIFFTEST_TO_REF);
    is_skip_ref = false;
    return;
  }

  ref_difftest_exec(1);
  ref_difftest_regcpy(&ref_r, DIFFTEST_TO_DUT);

  checkregs(&ref_r, pc);
}
#else
/**
 * Initializes the difftest environment for testing purposes.
 * 
 * This method sets up the difftest framework by loading the reference shared object file,
 * specifying the size of the memory image to be tested, and setting the communication port
 * for synchronization between the test harness and the reference model.
 *
 * @param ref_so_file Path to the shared object file containing the reference implementation.
 * @param img_size Size of the memory image to be tested, in bytes.
 * @param port Communication port used for synchronization between the test harness and the reference model.
 */
void init_difftest(char *ref_so_file, long img_size, int port) { }
#endif
