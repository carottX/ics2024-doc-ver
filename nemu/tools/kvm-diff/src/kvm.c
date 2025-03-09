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

// from NEMU
#include <memory/paddr.h>
#include <isa-def.h>
#include <difftest-def.h>

#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/kvm.h>

/* CR0 bits */
#define CR0_PE 1u
#define CR0_PG (1u << 31)

#define RFLAGS_ID  (1u << 21)
#define RFLAGS_AC  (1u << 18)
#define RFLAGS_RF  (1u << 16)
#define RFLAGS_TF  (1u << 8)
#define RFLAGS_AF  (1u << 4)
#define RFLAGS_FIX_MASK (RFLAGS_ID | RFLAGS_AC | RFLAGS_RF | RFLAGS_TF | RFLAGS_AF)

struct vm {
  int sys_fd;
  int fd;
  uint8_t *mem;
  uint8_t *mmio;
};

struct vcpu {
  int fd;
  struct kvm_run *kvm_run;
  int int_wp_state;
  int has_error_code;
  uint32_t entry;
};

enum {
  STATE_IDLE,      // if encounter an int instruction, then set watchpoint
  STATE_INT_INST, // if hit the watchpoint, then delete the watchpoint
  STATE_IRET_INST,// if hit the watchpoint, then delete the watchpoint
};

static struct vm vm;
static struct vcpu vcpu;
static FILE *log_fp = NULL; // only to pass linking

// This should be called everytime after KVM_SET_REGS.
// It seems that KVM_SET_REGS will clean the state of single step.
static void kvm_set_step_mode(bool watch, uint32_t watch_addr) {
  struct kvm_guest_debug debug = {};
  debug.control = KVM_GUESTDBG_ENABLE | KVM_GUESTDBG_SINGLESTEP | KVM_GUESTDBG_USE_HW_BP;
  debug.arch.debugreg[0] = watch_addr;
  debug.arch.debugreg[7] = (watch ? 0x1 : 0x0); // watch instruction fetch at `watch_addr`
  if (ioctl(vcpu.fd, KVM_SET_GUEST_DEBUG, &debug) < 0) {
    perror("KVM_SET_GUEST_DEBUG");
    assert(0);
  }
}

/**
 * Sets the general-purpose registers for the virtual CPU (VCPU) in the KVM (Kernel-based Virtual Machine) context.
 * This function uses the `ioctl` system call with the `KVM_SET_REGS` command to update the VCPU's registers with the
 * values provided in the `kvm_regs` structure. If the `ioctl` call fails, an error message is printed using `perror`,
 * and the program is terminated with an assertion failure. After successfully setting the registers, the function
 * disables single-step mode for the VCPU by calling `kvm_set_step_mode(false, 0)`.
 *
 * @param r Pointer to a `kvm_regs` structure containing the new register values to be set for the VCPU.
 */
static void kvm_setregs(const struct kvm_regs *r) {
  if (ioctl(vcpu.fd, KVM_SET_REGS, r) < 0) {
    perror("KVM_SET_REGS");
    assert(0);
  }
  kvm_set_step_mode(false, 0);
}

/**
 * Retrieves the special registers of the virtual CPU (VCPU) and stores them in the provided
 * `kvm_sregs` structure. Special registers include segment registers, control registers, and
 * other architecture-specific registers that are not part of the general-purpose register set.
 *
 * This function uses the `ioctl` system call with the `KVM_GET_SREGS` command to fetch the
 * special registers from the VCPU. If the operation fails, an error message is printed using
 * `perror`, and the program is terminated with an assertion failure.
 *
 * @param r Pointer to a `kvm_sregs` structure where the special registers will be stored.
 *          The structure must be allocated and initialized by the caller.
 *
 * @note This function assumes that `vcpu.fd` is a valid file descriptor for the VCPU.
 *       If the `ioctl` call fails, the program will terminate abruptly with an assertion failure.
 */
static void kvm_getsregs(struct kvm_sregs *r) {
  if (ioctl(vcpu.fd, KVM_GET_SREGS, r) < 0) {
    perror("KVM_GET_SREGS");
    assert(0);
  }
}

/**
 * Sets the special registers of the virtual CPU (VCPU) using the provided `kvm_sregs` structure.
 * This function uses the `ioctl` system call with the `KVM_SET_SREGS` command to configure the
 * special registers of the VCPU. If the `ioctl` call fails, an error message is printed using
 * `perror`, and the program terminates with an assertion failure.
 *
 * @param r Pointer to a `kvm_sregs` structure containing the special register values to be set.
 *          The structure must be properly initialized before calling this function.
 *
 * @note This function assumes that the `vcpu` global variable is properly initialized and contains
 *       a valid file descriptor (`fd`) for the VCPU. If the `ioctl` call fails, the program will
 *       terminate abruptly due to the assertion failure.
 */
static void kvm_setsregs(const struct kvm_sregs *r) {
  if (ioctl(vcpu.fd, KVM_SET_SREGS, r) < 0) {
    perror("KVM_SET_SREGS");
    assert(0);
  }
}

/**
 * Allocates a memory region and maps it into the KVM (Kernel-based Virtual Machine) guest's address space.
 *
 * This function performs the following steps:
 * 1. Allocates a memory region of the specified size using `mmap` with the following flags:
 *    - `PROT_READ | PROT_WRITE`: The memory is readable and writable.
 *    - `MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE`: The memory is private, not backed by a file, and does not reserve swap space.
 * 2. If the `mmap` call fails, an error message is printed using `perror`, and the program aborts with `assert(0)`.
 * 3. Advises the kernel that the memory region is mergeable using `madvise` with the `MADV_MERGEABLE` flag.
 * 4. Sets up a `kvm_userspace_memory_region` structure with the following fields:
 *    - `slot`: The memory slot identifier.
 *    - `flags`: No special flags are set (0).
 *    - `guest_phys_addr`: The base physical address in the guest's address space.
 *    - `memory_size`: The size of the memory region.
 *    - `userspace_addr`: The host virtual address of the allocated memory.
 * 5. Maps the memory region into the KVM guest's address space using the `KVM_SET_USER_MEMORY_REGION` ioctl.
 *    If the ioctl fails, an error message is printed using `perror`, and the program aborts with `assert(0)`.
 * 6. Returns the pointer to the allocated memory region.
 *
 * @param slot The memory slot identifier to be used in the KVM guest.
 * @param base The base physical address in the guest's address space where the memory will be mapped.
 * @param mem_size The size of the memory region to allocate and map.
 * @return A pointer to the allocated memory region on success. The program aborts on failure.
 */
static void* create_mem(int slot, uintptr_t base, size_t mem_size) {
  void *mem = mmap(NULL, mem_size, PROT_READ | PROT_WRITE,
      MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
  if (mem == MAP_FAILED) {
    perror("mmap mem");
    assert(0);
  }

  madvise(mem, mem_size, MADV_MERGEABLE);

  struct kvm_userspace_memory_region memreg;
  memreg.slot = slot;
  memreg.flags = 0;
  memreg.guest_phys_addr = base;
  memreg.memory_size = mem_size;
  memreg.userspace_addr = (unsigned long)mem;
  if (ioctl(vm.fd, KVM_SET_USER_MEMORY_REGION, &memreg) < 0) {
    perror("KVM_SET_USER_MEMORY_REGION");
    assert(0);
  }
  return mem;
}

/**
 * Initializes a virtual machine (VM) using the Kernel-based Virtual Machine (KVM) API.
 * 
 * This function performs the following steps:
 * 1. Opens the KVM device file `/dev/kvm` to obtain a file descriptor for system-level KVM operations.
 * 2. Retrieves the KVM API version and verifies it matches the expected version (`KVM_API_VERSION`).
 * 3. Creates a new VM instance using the KVM API and stores the resulting file descriptor.
 * 4. Sets the Task State Segment (TSS) address for the VM to a predefined value (`0xfffbd000`).
 * 5. Allocates memory for the VM's main memory and memory-mapped I/O (MMIO) regions using `create_mem`.
 * 
 * @param mem_size The size of the VM's main memory to allocate.
 * 
 * @note This function will terminate the program with an assertion failure if any step fails.
 */
static void vm_init(size_t mem_size) {
  int api_ver;

  vm.sys_fd = open("/dev/kvm", O_RDWR);
  if (vm.sys_fd < 0) {
    perror("open /dev/kvm");
    assert(0);
  }

  api_ver = ioctl(vm.sys_fd, KVM_GET_API_VERSION, 0);
  if (api_ver < 0) {
    perror("KVM_GET_API_VERSION");
    assert(0);
  }

  if (api_ver != KVM_API_VERSION) {
    fprintf(stderr, "Got KVM api version %d, expected %d\n",
        api_ver, KVM_API_VERSION);
    assert(0);
  }

  vm.fd = ioctl(vm.sys_fd, KVM_CREATE_VM, 0);
  if (vm.fd < 0) {
    perror("KVM_CREATE_VM");
    assert(0);
  }

  if (ioctl(vm.fd, KVM_SET_TSS_ADDR, 0xfffbd000) < 0) {
    perror("KVM_SET_TSS_ADDR");
    assert(0);
  }

  vm.mem = create_mem(0, 0, mem_size);
  vm.mmio = create_mem(1, 0xa1000000, 0x1000);
}

/**
 * Initializes a Virtual CPU (vCPU) for a KVM (Kernel-based Virtual Machine) instance.
 * This function performs the following steps:
 * 1. Creates a vCPU by invoking the KVM_CREATE_VCPU ioctl on the VM file descriptor.
 *    If the creation fails, an error is logged and the program terminates.
 * 2. Retrieves the required size for the vCPU's memory-mapped region using the
 *    KVM_GET_VCPU_MMAP_SIZE ioctl on the system file descriptor. If the size is invalid,
 *    an error is logged and the program terminates.
 * 3. Maps the vCPU's memory-mapped region into the process's address space using mmap.
 *    If the mapping fails, an error is logged and the program terminates.
 * 4. Configures the vCPU's valid register set to include x86 general-purpose and segment registers.
 * 5. Sets the vCPU's interrupt wait state to IDLE.
 */
static void vcpu_init() {
  int vcpu_mmap_size;

  vcpu.fd = ioctl(vm.fd, KVM_CREATE_VCPU, 0);
  if (vcpu.fd < 0) {
    perror("KVM_CREATE_VCPU");
    assert(0);
  }

  vcpu_mmap_size = ioctl(vm.sys_fd, KVM_GET_VCPU_MMAP_SIZE, 0);
  if (vcpu_mmap_size <= 0) {
    perror("KVM_GET_VCPU_MMAP_SIZE");
    assert(0);
  }

  vcpu.kvm_run = mmap(NULL, vcpu_mmap_size, PROT_READ | PROT_WRITE,
      MAP_SHARED, vcpu.fd, 0);
  if (vcpu.kvm_run == MAP_FAILED) {
    perror("mmap kvm_run");
    assert(0);
  }

  vcpu.kvm_run->kvm_valid_regs = KVM_SYNC_X86_REGS | KVM_SYNC_X86_SREGS;
  vcpu.int_wp_state = STATE_IDLE;
}

static const uint8_t mbr[] = {
  // start32:
  0x0f, 0x01, 0x15, 0x28, 0x7c, 0x00, 0x00,  // lgdtl 0x7c28
  0xea, 0x0e, 0x7c, 0x00, 0x00, 0x08, 0x00,  // ljmp $0x8, 0x7c0e

  // here:
  0xeb, 0xfe,  // jmp here

  // GDT
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0xff, 0xff, 0x00, 0x00, 0x00, 0x9a, 0xcf, 0x00,
  0xff, 0xff, 0x00, 0x00, 0x00, 0x92, 0xcf, 0x00,

  // GDT descriptor
  0x17, 0x00, 0x10, 0x7c, 0x00, 0x00
};

/**
 * @brief Configures the CPU registers to enter protected mode.
 *
 * This function sets up the CPU segment registers and control registers to transition
 * the CPU into protected mode. It initializes the code segment (CS) and data segment
 * (DS, ES, FS, GS, SS) descriptors with appropriate attributes for protected mode operation.
 *
 * The code segment is configured as executable, readable, and accessed, with a descriptor
 * privilege level (DPL) of 0 (highest privilege). The data segments are configured as
 * readable, writable, and accessed, also with a DPL of 0. The segment descriptors are set
 * to use 4KB granularity and a 32-bit default operation size.
 *
 * Additionally, the function sets the Protection Enable (PE) bit in the CR0 register to
 * enable protected mode.
 *
 * @param sregs Pointer to the struct kvm_sregs containing the CPU segment and control
 *              registers to be configured.
 */
static void setup_protected_mode(struct kvm_sregs *sregs) {
  struct kvm_segment seg = {
    .base = 0,
    .limit = 0xffffffff,
    .selector = 1 << 3,
    .present = 1,
    .type = 11, /* Code: execute, read, accessed */
    .dpl = 0,
    .db = 1,
    .s = 1, /* Code/data */
    .l = 0,
    .g = 1, /* 4KB granularity */
  };

  sregs->cr0 |= CR0_PE; /* enter protected mode */

  sregs->cs = seg;

  seg.type = 3; /* Data: read/write, accessed */
  seg.selector = 2 << 3;
  sregs->ds = sregs->es = sregs->fs = sregs->gs = sregs->ss = seg;
}

/**
 * Translates a virtual address (VA) to a physical address (PA) using KVM's translation mechanism.
 * 
 * This function checks if paging is enabled by examining the CR0_PG bit in the CR0 register of the VCPU.
 * If paging is enabled, it uses the KVM_TRANSLATE ioctl to request the translation of the virtual address
 * to a physical address. The result of the translation is returned if it is valid; otherwise, the function
 * returns -1ull (a 64-bit unsigned integer with all bits set to 1) to indicate an invalid translation.
 * If paging is not enabled, the virtual address is returned directly as it is treated as the physical address.
 *
 * @param va The virtual address to be translated.
 * @return The corresponding physical address if the translation is valid, or -1ull if the translation is invalid.
 *         If paging is disabled, the virtual address is returned as the physical address.
 */
static uint64_t va2pa(uint64_t va) {
  if (vcpu.kvm_run->s.regs.sregs.cr0 & CR0_PG) {
    struct kvm_translation t = { .linear_address = va };
    int ret = ioctl(vcpu.fd, KVM_TRANSLATE, &t);
    assert(ret == 0);
    return t.valid ? t.physical_address : -1ull;
  }
  return va;
}

/**
 * Performs patching for special instructions in a virtual machine (VM) context.
 * This method handles specific x86 instructions (PUSHF, POPF, and IRET) by
 * modifying the VM's state and registers accordingly. It ensures proper handling
 * of flags, stack pointers, and instruction pointers during execution.
 *
 * The method performs the following operations:
 * 1. Converts the virtual address of the instruction pointer (RIP) to a physical
 *    address (PA) to locate the current instruction.
 * 2. If the instruction is PUSHF (0x9c):
 *    - Adjusts the stack pointer (RSP) to push the current flags (RFLAGS) onto the stack.
 *    - Updates the flags register to set the Trap Flag (TF).
 *    - Increments the instruction pointer (RIP) to proceed to the next instruction.
 * 3. If the instruction is POPF (0x9d):
 *    - Pops the flags from the stack into the RFLAGS register, setting the TF and
 *      reserved bit 2.
 *    - Adjusts the stack pointer (RSP) and increments the instruction pointer (RIP).
 * 4. If the instruction is IRET (0xcf):
 *    - Retrieves the return address from the stack and sets it as the entry point.
 *    - Enables single-step mode for the VM and marks the state as handling an IRET
 *      instruction.
 *
 * @return Returns 1 if the instruction was successfully handled (PUSHF or POPF),
 *         or 0 if the instruction was not handled or if certain conditions
 *         (e.g., interrupt watchpoint state) prevent execution.
 */
static int patching() {
  // patching for special instructions
  uint32_t pc = va2pa(vcpu.kvm_run->s.regs.regs.rip);
  if (pc == 0xffffffff) return 0;
  if (vm.mem[pc] == 0x9c) {  // pushf
    if (vcpu.int_wp_state == STATE_INT_INST) return 0;
    vcpu.kvm_run->s.regs.regs.rsp -= 4;
    uint32_t esp = va2pa(vcpu.kvm_run->s.regs.regs.rsp);
    *(uint32_t *)(vm.mem + esp) = vcpu.kvm_run->s.regs.regs.rflags & ~RFLAGS_FIX_MASK;
    vcpu.kvm_run->s.regs.regs.rflags |= RFLAGS_TF;
    vcpu.kvm_run->s.regs.regs.rip ++;
    vcpu.kvm_run->kvm_dirty_regs = KVM_SYNC_X86_REGS;
    return 1;
  }
  else if (vm.mem[pc] == 0x9d) {  // popf
    if (vcpu.int_wp_state == STATE_INT_INST) return 0;
    uint32_t esp = va2pa(vcpu.kvm_run->s.regs.regs.rsp);
    vcpu.kvm_run->s.regs.regs.rflags = *(uint32_t *)(vm.mem + esp) | RFLAGS_TF | 2;
    vcpu.kvm_run->s.regs.regs.rsp += 4;
    vcpu.kvm_run->s.regs.regs.rip ++;
    vcpu.kvm_run->kvm_dirty_regs = KVM_SYNC_X86_REGS;
    return 1;
  }
  else if (vm.mem[pc] == 0xcf) { // iret
    uint32_t ret_addr = va2pa(vcpu.kvm_run->s.regs.regs.rsp);
    uint32_t eip = *(uint32_t *)(vm.mem + ret_addr);
    vcpu.entry = eip;
    kvm_set_step_mode(true, eip);
    vcpu.int_wp_state = STATE_IRET_INST;
    return 0;
  }
  return 0;
}

/**
 * Fixes the push operation for the segment register by ensuring that only the lower 16 bits
 * are written to the stack. This method retrieves the current stack pointer (RSP) from the
 * virtual CPU's register state, converts it to a physical address, and then modifies the value
 * at that address in the virtual machine's memory to clear the upper 16 bits, leaving only
 * the lower 16 bits intact.
 *
 * This is typically used to ensure compatibility with x86 segment register behavior, where
 * only the lower 16 bits of a segment register are pushed onto the stack during certain
 * operations.
 */
static void fix_push_sreg() {
  uint32_t esp = va2pa(vcpu.kvm_run->s.regs.regs.rsp);
  *(uint32_t *)(vm.mem + esp) &= 0x0000ffff;
}

/**
 * Handles patching operations after executing an instruction at a given program counter (PC).
 * This function is responsible for fixing the state of the virtual machine (VM) after executing
 * specific instructions that modify segment registers. It checks the opcode at the provided PC
 * and performs necessary adjustments if the instruction is a segment register push operation.
 *
 * The function first converts the virtual address (VA) of the last PC to a physical address (PA).
 * If the physical address is invalid (0xffffffff), the function returns immediately. Otherwise,
 * it retrieves the opcode at the physical address and checks if it corresponds to a segment
 * register push operation (e.g., push %ds, push %es, or push %fs). If such an operation is detected,
 * the function calls `fix_push_sreg()` to correct the VM state and asserts that the instruction
 * pointer (RIP) has been updated correctly.
 *
 * @param last_pc The virtual address of the last executed instruction's program counter (PC).
 */
static void patching_after(uint64_t last_pc) {
  uint32_t pc = va2pa(last_pc);
  if (pc == 0xffffffff) return;
  uint8_t opcode = vm.mem[pc];
  if (opcode == 0x1e || opcode == 0x06) {  // push %ds/%es
    fix_push_sreg();
    assert(vcpu.kvm_run->s.regs.regs.rip == last_pc + 1);
  }
  else if (opcode == 0x0f) {
    uint8_t opcode2 = vm.mem[pc + 1];
    if (opcode2 == 0xa0) { // push %fs
      fix_push_sreg();
      assert(vcpu.kvm_run->s.regs.regs.rip == last_pc + 2);
    }
  }
}

/**
 * Executes a KVM (Kernel-based Virtual Machine) guest for a specified number of iterations.
 * 
 * This method runs the KVM guest in a loop for `n` iterations, handling various exit reasons
 * and patching operations. It monitors the guest's execution state, particularly focusing on
 * debug exits (`KVM_EXIT_DEBUG`), and performs necessary adjustments to the guest's state.
 * 
 * The method handles the following scenarios:
 * - Patching: If patching is active, it skips the current iteration.
 * - KVM_RUN ioctl: Executes the KVM_RUN ioctl to run the guest. If interrupted (`EINTR`), it
 *   retries the operation by incrementing the iteration count.
 * - Exit reasons: 
 *   - If the exit reason is `KVM_EXIT_HLT`, the method returns immediately.
 *   - If the exit reason is not `KVM_EXIT_DEBUG`, it logs an error and asserts.
 *   - For `KVM_EXIT_DEBUG`, it performs post-patching operations and handles specific states
 *     (`STATE_INT_INST` and `STATE_IRET_INST`) by adjusting the guest's flags and state.
 * 
 * @param n The number of iterations to execute the KVM guest. If `n` is 0, the method does nothing.
 */
static void kvm_exec(uint64_t n) {
  for (; n > 0; n --) {
    if (patching()) continue;

    uint64_t pc = vcpu.kvm_run->s.regs.regs.rip;
    if (ioctl(vcpu.fd, KVM_RUN, 0) < 0) {
      if (errno == EINTR) {
        n ++;
        continue;
      }
      perror("KVM_RUN");
      assert(0);
    }

    if (vcpu.kvm_run->exit_reason != KVM_EXIT_DEBUG) {
      if (vcpu.kvm_run->exit_reason == KVM_EXIT_HLT) return;
      fprintf(stderr,	"Got exit_reason %d at pc = 0x%llx, expected KVM_EXIT_DEBUG (%d)\n",
          vcpu.kvm_run->exit_reason, vcpu.kvm_run->s.regs.regs.rip, KVM_EXIT_DEBUG);
      assert(0);
    } else {
      patching_after(pc);
      if (vcpu.int_wp_state == STATE_INT_INST) {
        uint32_t eflag_offset = 8 + (vcpu.has_error_code ? 4 : 0);
        uint32_t eflag_addr = va2pa(vcpu.kvm_run->s.regs.regs.rsp + eflag_offset);
        *(uint32_t *)(vm.mem + eflag_addr) &= ~RFLAGS_FIX_MASK;

        Assert(vcpu.entry == vcpu.kvm_run->debug.arch.pc,
            "entry not match, right = 0x%llx, wrong = 0x%x", vcpu.kvm_run->debug.arch.pc, vcpu.entry);
        kvm_set_step_mode(false, 0);
        vcpu.int_wp_state = STATE_IDLE;
      //Log("exception = %d, pc = %llx, dr6 = %llx, dr7 = %llx", vcpu.kvm_run->debug.arch.exception,
      //    vcpu.kvm_run->debug.arch.pc, vcpu.kvm_run->debug.arch.dr6, vcpu.kvm_run->debug.arch.dr7);
      } else if (vcpu.int_wp_state == STATE_IRET_INST) {
        Assert(vcpu.entry == vcpu.kvm_run->debug.arch.pc,
            "entry not match, right = 0x%llx, wrong = 0x%x", vcpu.kvm_run->debug.arch.pc, vcpu.entry);
        kvm_set_step_mode(false, 0);
        vcpu.int_wp_state = STATE_IDLE;
      }
    }
  }
}

/**
 * Executes the CPU in protected mode by setting up the necessary segment registers,
 * general-purpose registers, and loading the Master Boot Record (MBR) into memory.
 * 
 * This function performs the following steps:
 * 1. Retrieves the current segment registers using `kvm_getsregs`.
 * 2. Configures the CPU for protected mode by calling `setup_protected_mode` with the retrieved segment registers.
 * 3. Updates the segment registers with the new configuration using `kvm_setsregs`.
 * 4. Initializes the general-purpose registers, setting the RFLAGS register to 2 (indicating interrupts are disabled)
 *    and the RIP (Instruction Pointer) to 0x7C00, which is the typical entry point for the MBR.
 * 5. Updates the general-purpose registers using `kvm_setregs`.
 * 6. Copies the MBR code into the virtual machine's memory at the address 0x7C00.
 * 7. Executes 10 instructions in the virtual machine using `kvm_exec` to ensure the Global Descriptor Table (GDT) is loaded.
 */
static void run_protected_mode() {
  struct kvm_sregs sregs;
  kvm_getsregs(&sregs);
  setup_protected_mode(&sregs);
  kvm_setsregs(&sregs);

  struct kvm_regs regs;
  memset(&regs, 0, sizeof(regs));
  regs.rflags = 2;
  regs.rip = 0x7c00;
  // this will also set KVM_GUESTDBG_ENABLE
  kvm_setregs(&regs);

  memcpy(vm.mem + 0x7c00, mbr, sizeof(mbr));
  // run enough instructions to load GDT
  kvm_exec(10);
}

/**
 * Copies data between the emulator's memory and a buffer based on the specified direction.
 *
 * This function facilitates data transfer between the emulator's memory and an external buffer.
 * The direction of the transfer is determined by the `direction` parameter:
 * - If `direction` is `DIFFTEST_TO_REF`, the function copies `n` bytes from the external buffer `buf`
 *   to the emulator's memory starting at the specified address `addr`.
 * - If `direction` is not `DIFFTEST_TO_REF`, the function copies `n` bytes from the emulator's memory
 *   starting at the specified address `addr` to the external buffer `buf`.
 *
 * @param addr The starting address in the emulator's memory where the data transfer begins.
 * @param buf A pointer to the external buffer involved in the data transfer.
 * @param n The number of bytes to copy.
 * @param direction A boolean flag indicating the direction of the data transfer.
 *                  Use `DIFFTEST_TO_REF` to copy from the buffer to the emulator's memory,
 *                  or any other value to copy from the emulator's memory to the buffer.
 */
__EXPORT void difftest_memcpy(paddr_t addr, void *buf, size_t n, bool direction) {
  if (direction == DIFFTEST_TO_REF) memcpy(vm.mem + addr, buf, n);
  else memcpy(buf, vm.mem + addr, n);
}

/**
 * Copies CPU register values between the reference KVM registers and the x86 CPU state.
 *
 * This function is used to synchronize the CPU register values between two different
 * representations: the reference KVM registers (`vcpu.kvm_run->s.regs.regs`) and the
 * x86 CPU state (`x86_CPU_state`). The direction of the copy is determined by the
 * `direction` parameter.
 *
 * @param r          A pointer to the x86 CPU state structure (`x86_CPU_state`).
 * @param direction  A boolean flag indicating the direction of the copy:
 *                   - `DIFFTEST_TO_REF`: Copies the register values from the x86 CPU state
 *                     to the reference KVM registers.
 *                   - `false`: Copies the register values from the reference KVM registers
 *                     to the x86 CPU state.
 *
 * When copying to the reference KVM registers (`DIFFTEST_TO_REF`), the function also sets
 * the `KVM_SYNC_X86_REGS` flag in `vcpu.kvm_run->kvm_dirty_regs` to indicate that the
 * registers have been modified and need to be synchronized with the KVM.
 */
__EXPORT void difftest_regcpy(void *r, bool direction) {
  struct kvm_regs *ref = &(vcpu.kvm_run->s.regs.regs);
  x86_CPU_state *x86 = r;
  if (direction == DIFFTEST_TO_REF) {
    ref->rax = x86->eax;
    ref->rbx = x86->ebx;
    ref->rcx = x86->ecx;
    ref->rdx = x86->edx;
    ref->rsp = x86->esp;
    ref->rbp = x86->ebp;
    ref->rsi = x86->esi;
    ref->rdi = x86->edi;
    ref->rip = x86->pc;
    ref->rflags |= RFLAGS_TF;
    vcpu.kvm_run->kvm_dirty_regs = KVM_SYNC_X86_REGS;
  } else {
    x86->eax = ref->rax;
    x86->ebx = ref->rbx;
    x86->ecx = ref->rcx;
    x86->edx = ref->rdx;
    x86->esp = ref->rsp;
    x86->ebp = ref->rbp;
    x86->esi = ref->rsi;
    x86->edi = ref->rdi;
    x86->pc  = ref->rip;
  }
}

/**
 * @brief Executes a specified number of instructions in the KVM (Kernel-based Virtual Machine) environment.
 *
 * This function triggers the execution of `n` instructions in the KVM virtual machine. It is typically used
 * for differential testing or debugging purposes, where the behavior of the virtual machine needs to be
 * compared against a reference implementation or model.
 *
 * @param n The number of instructions to execute in the KVM environment.
 * @return void
 */
__EXPORT void difftest_exec(uint64_t n) {
  kvm_exec(n);
}

/**
 * @brief Raises an interrupt in the virtual CPU (VCPU) by simulating the interrupt handling process.
 *
 * This method is responsible for raising an interrupt in the VCPU by performing the following steps:
 * 1. Calculates the virtual address of the interrupt gate descriptor in the Interrupt Descriptor Table (IDT)
 *    based on the interrupt number `NO`.
 * 2. Converts the virtual address of the interrupt gate descriptor to a physical address using `va2pa`.
 * 3. Reads the interrupt gate descriptor from the virtual machine's memory to determine the entry point
 *    of the interrupt handler.
 * 4. Sets the VCPU to step mode and configures the interrupt state, including the entry point of the handler
 *    and whether an error code is associated with the interrupt.
 * 5. If the interrupt number is 48 (timer interrupt), it injects the interrupt into the VCPU using the KVM API.
 *
 * @param NO The interrupt number to raise. This number corresponds to the index in the IDT.
 *
 * @note This method assumes that the code segment base is 0 when calculating the entry point.
 * @note The method asserts that the `ioctl` call for injecting the interrupt succeeds.
 */
__EXPORT void difftest_raise_intr(word_t NO) {
  uint32_t pgate_vaddr = vcpu.kvm_run->s.regs.sregs.idt.base + NO * 8;
  uint32_t pgate = va2pa(pgate_vaddr);
  // assume code.base = 0
  uint32_t entry = vm.mem[pgate] | (vm.mem[pgate + 1] << 8) |
    (vm.mem[pgate + 6] << 16) | (vm.mem[pgate + 7] << 24);
  kvm_set_step_mode(true, entry);
  vcpu.int_wp_state = STATE_INT_INST;
  vcpu.has_error_code = (NO == 14);
  vcpu.entry = entry;

  if (NO == 48) {
    // inject timer interrupt
    struct kvm_interrupt intr = { .irq = NO };
    int ret = ioctl(vcpu.fd, KVM_INTERRUPT, &intr);
    assert(ret == 0);
  }
}

/**
 * Initializes the difftest environment by setting up the virtual machine, virtual CPU,
 * and transitioning to protected mode. This method is typically called to prepare the
 * system for differential testing, where the behavior of the system is compared against
 * a reference implementation.
 *
 * @param port The port number to be used for communication during the difftest. This
 *             parameter is currently unused in the implementation but may be utilized
 *             in future extensions.
 *
 * The method performs the following steps:
 * 1. Initializes the virtual machine with the configured memory size (CONFIG_MSIZE).
 * 2. Initializes the virtual CPU to prepare it for execution.
 * 3. Transitions the system to protected mode, enabling the execution of privileged
 *    instructions and setting up the necessary environment for testing.
 */
__EXPORT void difftest_init(int port) {
  vm_init(CONFIG_MSIZE);
  vcpu_init();
  run_protected_mode();
}
