#include "x86-qemu.h"

Area heap = {};
int __am_ncpu = 0;

int main(const char *args);

/**
 * Calls the `main` function with the provided arguments and halts the program 
 * with the return value of `main`.
 *
 * This function serves as a wrapper around the `main` function, allowing it to 
 * be invoked with a string of arguments. After `main` executes, the program 
 * halts using the return value of `main` as the halt code.
 *
 * @param args A string containing the arguments to be passed to the `main` function.
 *             This string is typically a space-separated list of arguments.
 */
static void call_main(const char *args) {
  halt(main(args));
}

/**
 * @brief Entry point for the CPU initialization process.
 *
 * This function is responsible for determining the role of the current CPU
 * during the boot process and taking appropriate actions based on that role.
 * If the current CPU is identified as an Application Processor (AP) through
 * the boot record, it will jump to the `__am_othercpu_entry` function to
 * initialize the AP. If the current CPU is the Boot Processor (BP), it will
 * first initialize the boot CPU using `__am_bootcpu_init`, then switch to a
 * new stack and call the main function with the provided arguments.
 *
 * @param args A pointer to the arguments that will be passed to the main
 *             function when the boot CPU is initialized.
 */
void _start_c(char *args) {
  if (boot_record()->is_ap) {
    __am_othercpu_entry();
  } else {
    __am_bootcpu_init();
    stack_switch_call(stack_top(&CPU->stack), call_main, (uintptr_t)args);
  }
}

/**
 * Initializes the boot CPU and associated hardware components.
 * This function performs the following steps:
 * 1. Initializes the heap memory by calling `__am_heap_init()`.
 * 2. Initializes the Local APIC (Advanced Programmable Interrupt Controller) by calling `__am_lapic_init()`.
 * 3. Initializes the I/O APIC by calling `__am_ioapic_init()`.
 * 4. Initializes per-CPU data structures by calling `__am_percpu_init()`.
 * This function is typically called during the early boot process to set up the environment for the boot CPU.
 */
void __am_bootcpu_init() {
  heap = __am_heap_init();
  __am_lapic_init();
  __am_ioapic_init();
  __am_percpu_init();
}

/**
 * @brief Initializes per-CPU data structures and hardware components.
 *
 * This function is responsible for setting up the necessary per-CPU data
 * structures and initializing hardware components that are specific to each
 * CPU core. It performs the following tasks:
 * 1. Initializes the Global Descriptor Table (GDT) for the current CPU.
 * 2. Initializes the Local Advanced Programmable Interrupt Controller (LAPIC)
 *    for the current CPU.
 * 3. Initializes interrupt handling mechanisms for the current CPU.
 *
 * This function should be called during the boot process for each CPU core
 * to ensure proper initialization of per-CPU resources.
 */
void __am_percpu_init() {
  __am_percpu_initgdt();
  __am_percpu_initlapic();
  __am_percpu_initirq();
}

/**
 * Writes a single character to the COM1 serial port.
 * 
 * This function sends the specified character `ch` to the COM1 serial port, 
 * which is typically used for serial communication. The COM1 port is 
 * addressed at the I/O port 0x3f8. The character is transmitted using the 
 * `outb` assembly instruction, which writes a byte to the specified I/O port.
 * 
 * @param ch The character to be written to the COM1 serial port.
 */
void putch(char ch) {
  #define COM1 0x3f8
  outb(COM1, ch);
}

/**
 * Halts the CPU and outputs a formatted message indicating the halt status.
 * 
 * This method performs the following steps:
 * 1. Disables interrupts using `cli()` to ensure no further interruptions.
 * 2. Stops the world using `__am_stop_the_world()`, effectively halting all other CPUs.
 * 3. Iterates over a predefined format string (`fmt`) to construct and output a message:
 *    - Replaces '$' with the current CPU identifier in hexadecimal.
 *    - Replaces '0' and '4' with the corresponding nibbles of the `code` parameter in hexadecimal.
 *    - Outputs all other characters as-is.
 * 4. Sends a specific value (`0x2000`) to port `0x604`, which is a QEMU-specific command to exit the emulator.
 * 5. Enters an infinite loop using `hlt()` to keep the CPU in a halted state indefinitely.
 *
 * @param code An integer whose nibbles are used in the output message to indicate the halt reason or status.
 */
void halt(int code) {
  const char *hex = "0123456789abcdef";
  const char *fmt = "CPU #$ Halt (40).\n";
  cli();
  __am_stop_the_world();
  for (const char *p = fmt; *p; p++) {
    char ch = *p;
    switch (ch) {
      case '$':
        putch(hex[cpu_current()]);
        break;
      case '0': case '4':
        putch(hex[(code >> (ch - '0')) & 0xf]);
        break;
      default:
        putch(ch);
    }
  }
  outw(0x604, 0x2000); // offer of qemu :)
  while (1) hlt();
}

/**
 * Initializes the heap memory area by determining its start and end addresses.
 * 
 * This function performs the following steps:
 * 1. Retrieves the end of the BSS section using the `end` symbol.
 * 2. Reads the lower and upper bytes of the CMOS memory location to determine the size of the available memory.
 * 3. Combines the lower and upper bytes to calculate the total available memory.
 * 4. Rounds up the `end` address to the nearest 1 MiB boundary to determine the start of the heap.
 * 5. Returns a memory range (Area) that represents the heap, starting from the rounded-up `end` address and extending to the calculated memory limit.
 * 
 * @return Area - A memory range representing the heap area.
 */
Area __am_heap_init() {
  extern char end;
  outb(0x70, 0x34);
  uint32_t lo = inb(0x71);
  outb(0x70, 0x35);
  uint32_t hi = inb(0x71) + 1;
  return RANGE(ROUNDUP(&end, 1 << 20), (uintptr_t)((lo | hi << 8) << 16));
}

/**
 * Initializes the Local APIC (Advanced Programmable Interrupt Controller) by searching for the
 * MP (Multiprocessor) configuration table in the BIOS memory region. The method scans the memory
 * range from 0xf0000 to 0xffffff to locate the MP floating pointer structure, which contains
 * the address of the MP configuration table. Once the MP configuration table is found, the
 * Local APIC base address is extracted from it. Additionally, the method iterates through the
 * processor entries in the MP configuration table to count the number of CPUs and ensures that
 * the number of CPUs does not exceed the maximum supported limit (MAX_CPU). If the MP floating
 * pointer or configuration table is not found, the method triggers a bug condition.
 *
 * The MP floating pointer is identified by the signature 0x5f504d5f ("_MP_"). The MP configuration
 * table contains information about the system's processors, including the Local APIC base address.
 * The method updates the global variable `__am_lapic` with the Local APIC base address and
 * increments the global variable `__am_ncpu` for each processor found.
 *
 * @note This method assumes that the MP configuration table is present in the specified memory
 *       region and that the system follows the MP specification. If the MP configuration table
 *       is not found, the method will trigger a bug condition.
 *
 * @warning The method does not handle cases where the MP configuration table is corrupted or
 *          incomplete. It also assumes that the memory region being scanned is accessible and
 *          valid.
 */
void __am_lapic_init() {
  for (char *st = (char *)0xf0000; st != (char *)0xffffff; st ++) {
    if (*(volatile uint32_t *)st == 0x5f504d5f) {
      uint32_t mpconf_ptr = ((volatile MPDesc *)st)->conf;
      MPConf *conf = (void *)((uintptr_t)(mpconf_ptr));
      __am_lapic = (void *)((uintptr_t)(conf->lapicaddr));
      for (volatile char *ptr = (char *)(conf + 1);
           ptr < (char *)conf + conf->length; ptr += 8) {
        if (*ptr == '\0') {
          ptr += 12;
          panic_on(++__am_ncpu > MAX_CPU, "cannot support > MAX_CPU processors");
        }
      }
      return;
    }
  }
  bug();
}

/**
 * Initializes the Global Descriptor Table (GDT) for the current CPU.
 * 
 * This function sets up the GDT entries for kernel and user code/data segments,
 * as well as the Task State Segment (TSS). The GDT is a critical data structure
 * in x86 architecture that defines memory segments and their attributes.
 * 
 * On x86_64, the function initializes 64-bit segment descriptors for kernel and
 * user code/data segments, and a 16-bit TSS descriptor. It ensures the TSS
 * address fits within 32 bits and then loads the GDT and TSS into the CPU.
 * 
 * On x86 (32-bit), the function initializes 32-bit segment descriptors for
 * kernel and user code/data segments, and a 16-bit TSS descriptor. It then
 * loads the GDT and TSS into the CPU.
 * 
 * The function uses the CPU's GDT and TSS pointers to configure the segments
 * and sets the appropriate privilege levels (DPL_KERN for kernel, DPL_USER for
 * user). Finally, it updates the CPU's GDT and TSS registers.
 */
void __am_percpu_initgdt() {
#if __x86_64__
  SegDesc *gdt = CPU->gdt;
  TSS64 *tss = &CPU->tss;
  gdt[SEG_KCODE] = SEG64(STA_X | STA_R,                      DPL_KERN);
  gdt[SEG_KDATA] = SEG64(STA_W,                              DPL_KERN);
  gdt[SEG_UCODE] = SEG64(STA_X | STA_R,                      DPL_USER);
  gdt[SEG_UDATA] = SEG64(STA_W,                              DPL_USER);
  gdt[SEG_TSS]   = SEG16(STS_T32A,      tss, sizeof(*tss)-1, DPL_KERN);
  bug_on((uintptr_t)tss >> 32);
  set_gdt(gdt, sizeof(gdt[0]) * (NR_SEG + 1));
  set_tr(KSEL(SEG_TSS));
#else
  SegDesc *gdt = CPU->gdt;
  TSS32 *tss = &CPU->tss;
  gdt[SEG_KCODE] = SEG32(STA_X | STA_R,   0,     0xffffffff, DPL_KERN);
  gdt[SEG_KDATA] = SEG32(STA_W,           0,     0xffffffff, DPL_KERN);
  gdt[SEG_UCODE] = SEG32(STA_X | STA_R,   0,     0xffffffff, DPL_USER);
  gdt[SEG_UDATA] = SEG32(STA_W,           0,     0xffffffff, DPL_USER);
  gdt[SEG_TSS]   = SEG16(STS_T32A,      tss, sizeof(*tss)-1, DPL_KERN);
  set_gdt(gdt, sizeof(gdt[0]) * NR_SEG);
  set_tr(KSEL(SEG_TSS));
#endif
}
