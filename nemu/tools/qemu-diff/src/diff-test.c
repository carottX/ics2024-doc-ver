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

#include "common.h"
#include <difftest-def.h>
#include <sys/prctl.h>
#include <signal.h>

bool gdb_connect_qemu(int);
bool gdb_memcpy_to_qemu(uint32_t, void *, int);
bool gdb_getregs(union isa_gdb_regs *);
bool gdb_setregs(union isa_gdb_regs *);
bool gdb_si();
void gdb_exit();

void init_isa();

/**
 * @brief Copies data between the host and the QEMU emulator's memory.
 *
 * This function is used to copy a block of data from the host's memory to the QEMU emulator's memory.
 * The function asserts that the direction of the copy operation is `DIFFTEST_TO_REF`, indicating that
 * the data is being copied from the host to the QEMU emulator. If the direction is valid, the function
 * calls `gdb_memcpy_to_qemu` to perform the memory copy operation and asserts that the operation was
 * successful.
 *
 * @param addr The starting address in the QEMU emulator's memory where the data will be copied.
 * @param buf A pointer to the buffer in the host's memory containing the data to be copied.
 * @param n The number of bytes to copy.
 * @param direction The direction of the copy operation. Must be `DIFFTEST_TO_REF`.
 */
__EXPORT void difftest_memcpy(paddr_t addr, void *buf, size_t n, bool direction) {
  assert(direction == DIFFTEST_TO_REF);
  if (direction == DIFFTEST_TO_REF) {
    bool ok = gdb_memcpy_to_qemu(addr, buf, n);
    assert(ok == 1);
  }
}

/**
 * Copies register values between the Device Under Test (DUT) and the reference model (QEMU).
 * 
 * This function retrieves the current register state from QEMU using `gdb_getregs` and then
 * performs a copy operation based on the specified direction. If the direction is set to
 * `DIFFTEST_TO_REF`, the register values from the DUT are copied to QEMU, updating its state.
 * Otherwise, the register values from QEMU are copied to the DUT, updating its state.
 * 
 * @param dut Pointer to the DUT's register state.
 * @param direction Specifies the direction of the copy operation. If `DIFFTEST_TO_REF`, 
 *                  the DUT's registers are copied to QEMU. Otherwise, QEMU's registers 
 *                  are copied to the DUT.
 */
__EXPORT void difftest_regcpy(void *dut, bool direction) {
  union isa_gdb_regs qemu_r;
  gdb_getregs(&qemu_r);
  if (direction == DIFFTEST_TO_REF) {
    memcpy(&qemu_r, dut, DIFFTEST_REG_SIZE);
    gdb_setregs(&qemu_r);
  } else {
    memcpy(dut, &qemu_r, DIFFTEST_REG_SIZE);
  }
}

/**
 * @brief Executes a specified number of steps in the differential testing environment.
 *
 * This function is used to simulate the execution of a given number of steps in a 
 * differential testing context. It repeatedly calls the `gdb_si()` function, which 
 * typically represents a single step of execution in a debugger or simulation environment.
 * The function continues to execute steps until the specified count `n` reaches zero.
 *
 * @param n The number of steps to execute. This value is decremented with each step 
 *          until it reaches zero, at which point the function completes.
 */
__EXPORT void difftest_exec(uint64_t n) {
  while (n --) gdb_si();
}

/**
 * Initializes a difftest environment by forking a child process to run QEMU and
 * connecting to it via GDB. The child process runs QEMU with specified arguments,
 * including a TCP port for GDB communication. The parent process connects to QEMU
 * using the provided port, initializes the ISA (Instruction Set Architecture),
 * and sets up an exit handler for cleanup.
 *
 * @param port The TCP port number to use for GDB communication with QEMU.
 *             This port is embedded in the GDB connection string.
 *
 * @details The method performs the following steps:
 * 1. Constructs a GDB connection string using the provided port.
 * 2. Forks a child process to run QEMU:
 *    - The child process sets up a parent death signal (SIGTERM) to terminate
 *      itself if the parent process dies.
 *    - It then executes QEMU with the specified arguments, including the GDB
 *      connection string.
 * 3. The parent process connects to QEMU via GDB using the provided port.
 * 4. It prints a success message upon successful connection.
 * 5. Registers an exit handler (`gdb_exit`) to clean up resources on program exit.
 * 6. Initializes the ISA for the difftest environment.
 *
 * @note If any critical step fails (e.g., fork, prctl, exec, or GDB connection),
 * the method terminates the program with an assertion error.
 */
__EXPORT void difftest_init(int port) {
  char buf[32];
  sprintf(buf, "tcp::%d", port);

  int ppid_before_fork = getpid();
  int pid = fork();
  if (pid == -1) {
    perror("fork");
    assert(0);
  }
  else if (pid == 0) {
    // child

    // install a parent death signal in the chlid
    int r = prctl(PR_SET_PDEATHSIG, SIGTERM);
    if (r == -1) {
      perror("prctl error");
      assert(0);
    }

    if (getppid() != ppid_before_fork) {
      printf("parent has died!\n");
      assert(0);
    }

    close(STDIN_FILENO);
    execlp(ISA_QEMU_BIN, ISA_QEMU_BIN, ISA_QEMU_ARGS "-S", "-gdb", buf, "-nographic",
        "-serial", "none", "-monitor", "none", NULL);
    perror("exec");
    assert(0);
  }
  else {
    // father

    gdb_connect_qemu(port);
    printf("Connect to QEMU with %s successfully\n", buf);

    atexit(gdb_exit);

    init_isa();
  }
}

/**
 * @brief Raises an interrupt for difftesting purposes.
 *
 * This method is intended to simulate the raising of an interrupt during
 * difftesting, which is a process used to verify the correctness of hardware
 * or software by comparing the behavior of two systems. However, this
 * implementation does not actually raise an interrupt. Instead, it prints
 * a message indicating that raising interrupts is not supported and then
 * triggers an assertion failure to halt execution.
 *
 * @param NO The interrupt number to be raised. This parameter is currently
 *           unused as the function does not support raising interrupts.
 *
 * @note This function is marked with __EXPORT, indicating that it is intended
 *       to be used externally, possibly as part of a larger testing framework.
 */
__EXPORT void difftest_raise_intr(uint64_t NO) {
  printf("raise_intr is not supported\n");
  assert(0);
}
