// CPU rings
#define DPL_KERN       0x0     // Kernel (ring 0)
#define DPL_USER       0x3     // User (ring 3)

// Application Segment type bits
#define STA_X          0x8     // Executable segment
#define STA_W          0x2     // Writeable (non-executable segments)
#define STA_R          0x2     // Readable (executable segments)

// System Segment type bits
#define STS_T32A       0x9     // Available 32-bit TSS
#define STS_IG         0xe     // 32/64-bit Interrupt Gate
#define STS_TG         0xf     // 32/64-bit Trap Gate

// EFLAGS register
#define FL_IF          0x00000200  // Interrupt Enable

// Control Register flags
#define CR0_PE         0x00000001  // Protection Enable
#define CR0_PG         0x80000000  // Paging
#define CR4_PAE        0x00000020  // Physical Address Extension

// Page table/directory entry flags
#define PTE_P          0x001   // Present
#define PTE_W          0x002   // Writeable
#define PTE_U          0x004   // User
#define PTE_PS         0x080   // Large Page (1 GiB or 2 MiB)

// GDT selectors
#define KSEL(seg)      (((seg) << 3) | DPL_KERN)
#define USEL(seg)      (((seg) << 3) | DPL_USER)

// Interrupts and exceptions
#define T_IRQ0         32
#define IRQ_TIMER      0
#define IRQ_KBD        1
#define IRQ_COM1       4
#define IRQ_ERROR      19
#define IRQ_SPURIOUS   31
#define EX_DE          0
#define EX_UD          6
#define EX_NM          7
#define EX_DF          8
#define EX_TS          10
#define EX_NP          11
#define EX_SS          12
#define EX_GP          13
#define EX_PF          14
#define EX_MF          15
#define EX_SYSCALL     0x80
#define EX_YIELD       0x81

// List of interrupts and exceptions (#irq, DPL, hardware errorcode)
#define IRQS(_) \
  _(  0, KERN, NOERR) \
  _(  1, KERN, NOERR) \
  _(  2, KERN, NOERR) \
  _(  3, KERN, NOERR) \
  _(  4, KERN, NOERR) \
  _(  5, KERN, NOERR) \
  _(  6, KERN, NOERR) \
  _(  7, KERN, NOERR) \
  _(  8, KERN,   ERR) \
  _(  9, KERN, NOERR) \
  _( 10, KERN,   ERR) \
  _( 11, KERN,   ERR) \
  _( 12, KERN,   ERR) \
  _( 13, KERN,   ERR) \
  _( 14, KERN,   ERR) \
  _( 15, KERN, NOERR) \
  _( 16, KERN, NOERR) \
  _( 19, KERN, NOERR) \
  _( 31, KERN, NOERR) \
  _( 32, KERN, NOERR) \
  _( 33, KERN, NOERR) \
  _( 34, KERN, NOERR) \
  _( 35, KERN, NOERR) \
  _( 36, KERN, NOERR) \
  _( 37, KERN, NOERR) \
  _( 38, KERN, NOERR) \
  _( 39, KERN, NOERR) \
  _( 40, KERN, NOERR) \
  _( 41, KERN, NOERR) \
  _( 42, KERN, NOERR) \
  _( 43, KERN, NOERR) \
  _( 44, KERN, NOERR) \
  _( 45, KERN, NOERR) \
  _( 46, KERN, NOERR) \
  _( 47, KERN, NOERR) \
  _(128, USER, NOERR) \
  _(129, USER, NOERR)

// AM-specific configurations
#define MAX_CPU       8
#define BOOTREC_ADDR  0x07000
#define MAINARG_ADDR  0x10000

// Below are only visible to c/c++ files
#ifndef __ASSEMBLER__

#include <stdint.h>

// Segment Descriptor
typedef struct {
  uint32_t lim_15_0   : 16; // Low bits of segment limit
  uint32_t base_15_0  : 16; // Low bits of segment base address
  uint32_t base_23_16 :  8; // Middle bits of segment base address
  uint32_t type       :  4; // Segment type (see STS_ constants)
  uint32_t s          :  1; // 0 = system, 1 = application
  uint32_t dpl        :  2; // Descriptor Privilege Level
  uint32_t p          :  1; // Present
  uint32_t lim_19_16  :  4; // High bits of segment limit
  uint32_t avl        :  1; // Unused (available for software use)
  uint32_t l          :  1; // 64-bit segment
  uint32_t db         :  1; // 32-bit segment
  uint32_t g          :  1; // Granularity: limit scaled by 4K when set
  uint32_t base_31_24 :  8; // High bits of segment base address
} SegDesc;

// Gate descriptors for interrupts and traps
typedef struct {
  uint32_t off_15_0  : 16; // Low 16 bits of offset in segment
  uint32_t cs        : 16; // Code segment selector
  uint32_t args      :  5; // # args, 0 for interrupt/trap gates
  uint32_t rsv1      :  3; // Reserved(should be zero I guess)
  uint32_t type      :  4; // Type(STS_{TG,IG32,TG32})
  uint32_t s         :  1; // Must be 0 (system)
  uint32_t dpl       :  2; // Descriptor(meaning new) privilege level
  uint32_t p         :  1; // Present
  uint32_t off_31_16 : 16; // High bits of offset in segment
} GateDesc32;

typedef struct {
  uint32_t off_15_0  : 16;
  uint32_t cs        : 16;
  uint32_t isv       :  3;
  uint32_t zero1     :  5;
  uint32_t type      :  4;
  uint32_t zero2     :  1;
  uint32_t dpl       :  2;
  uint32_t p         :  1;
  uint32_t off_31_16 : 16;
  uint32_t off_63_32 : 32;
  uint32_t rsv       : 32;
} GateDesc64;

// Task State Segment (TSS)
typedef struct {
  uint32_t link;     // Unused
  uint32_t esp0;     // Stack pointers and segment selectors
  uint32_t ss0;      //   after an increase in privilege level
  uint32_t padding[23];
} __attribute__((packed)) TSS32;

typedef struct {
  uint32_t rsv;
  uint64_t rsp0, rsp1, rsp2;
  uint32_t padding[19];
} __attribute__((packed)) TSS64;

// Multiprocesor configuration
typedef struct {          // configuration table header
  uint8_t  signature[4];  // "PCMP"
  uint16_t length;        // total table length
  uint8_t  version;       // [14]
  uint8_t  checksum;      // all bytes must add up to 0
  uint8_t  product[20];   // product id
  uint32_t oemtable;      // OEM table pointer
  uint16_t oemlength;     // OEM table length
  uint16_t entry;         // entry count
  uint32_t lapicaddr;     // address of local APIC
  uint16_t xlength;       // extended table length
  uint8_t  xchecksum;     // extended table checksum
  uint8_t  reserved;
} MPConf;

typedef struct {
  int      magic;
  uint32_t conf;     // MP config table addr
  uint8_t  length;   // 1
  uint8_t  specrev;  // [14]
  uint8_t  checksum; // all bytes add to 0
  uint8_t  type;     // config type
  uint8_t  imcrp;
  uint8_t  reserved[3];
} MPDesc;

typedef struct {
  uint32_t jmp_code;
  int32_t is_ap;
} BootRecord;

#define SEG16(type, base, lim, dpl) (SegDesc)        \
{ (lim) & 0xffff, (uintptr_t)(base) & 0xffff,        \
  ((uintptr_t)(base) >> 16) & 0xff, type, 0, dpl, 1, \
  (uintptr_t)(lim) >> 16, 0, 0, 1, 0, (uintptr_t)(base) >> 24 }

#define SEG32(type, base, lim, dpl) (SegDesc)         \
{ ((lim) >> 12) & 0xffff, (uintptr_t)(base) & 0xffff, \
  ((uintptr_t)(base) >> 16) & 0xff, type, 1, dpl, 1,  \
  (uintptr_t)(lim) >> 28, 0, 0, 1, 1, (uintptr_t)(base) >> 24 }

#define SEG64(type, dpl) (SegDesc) \
  { 0, 0, 0, type, 1, dpl, 1, 0, 0, 1, 0, 0 }

#define SEGTSS64(type, base, lim, dpl) (SegDesc)    \
{ (lim) & 0xffff, (uint32_t)(base) & 0xffff,        \
  ((uint32_t)(base) >> 16) & 0xff, type, 0, dpl, 1, \
  (uint32_t)(lim) >> 16, 0, 0, 0, 0, (uint32_t)(base) >> 24 }

#define GATE32(type, cs, entry, dpl) (GateDesc32)              \
  {  (uint32_t)(entry) & 0xffff, (cs), 0, 0, (type), 0, (dpl), \
  1, (uint32_t)(entry) >> 16 }

#define GATE64(type, cs, entry, dpl) (GateDesc64)             \
  { (uint64_t)(entry) & 0xffff, (cs), 0, 0, (type), 0, (dpl), \
    1, ((uint64_t)(entry) >> 16) & 0xffff, (uint64_t)(entry) >> 32, 0 }

// Instruction wrappers

static inline uint8_t inb(int port) {
  uint8_t data;
  asm volatile ("inb %1, %0" : "=a"(data) : "d"((uint16_t)port));
  return data;
}

/**
 * Reads a 16-bit value from the specified I/O port.
 *
 * This function uses the `inw` assembly instruction to read a 16-bit value
 * from the given I/O port. The port number is passed as an argument, and the
 * result is returned as a 16-bit unsigned integer.
 *
 * @param port The I/O port number from which to read the data.
 * @return The 16-bit value read from the specified I/O port.
 */
static inline uint16_t inw(int port) {
  uint16_t data;
  asm volatile ("inw %1, %0" : "=a"(data) : "d"((uint16_t)port));
  return data;
}

/**
 * Reads a 32-bit value from the specified I/O port.
 *
 * This function performs an input operation from the given I/O port using the
 * `inl` assembly instruction. The port number is cast to a 16-bit unsigned integer
 * and passed to the instruction, and the resulting 32-bit value is returned.
 *
 * @param port The I/O port number from which to read the data.
 * @return The 32-bit value read from the specified I/O port.
 */
static inline uint32_t inl(int port) {
  uint32_t data;
  asm volatile ("inl %1, %0" : "=a"(data) : "d"((uint16_t)port));
  return data;
}

/**
 * Sends a byte of data to a specified hardware port.
 * 
 * This function uses inline assembly to execute the `outb` instruction, which writes
 * a single byte of data to a hardware port. The port is specified by the `port` parameter,
 * and the data to be sent is specified by the `data` parameter.
 * 
 * @param port The hardware port address to which the data will be sent. This is cast to a
 *             uint16_t to ensure compatibility with the `outb` instruction.
 * @param data The byte of data to be sent to the specified port. This is passed in the AL
 *             register of the CPU.
 * 
 * @note This function is marked as `inline` to suggest that the compiler should attempt to
 *       embed the function's code directly into the calling code, potentially reducing
 *       function call overhead. The `volatile` keyword ensures that the compiler does not
 *       optimize out the assembly instruction.
 */
static inline void outb(int port, uint8_t data) {
  asm volatile ("outb %%al, %%dx" : : "a"(data), "d"((uint16_t)port));
}

/**
 * Writes a 16-bit value to the specified I/O port.
 *
 * This function uses inline assembly to perform an `outw` instruction, which writes
 * the 16-bit value `data` to the I/O port specified by `port`. The `outw` instruction
 * is used to send data to hardware devices that are mapped to I/O ports.
 *
 * @param port The I/O port address to which the data will be written. The address
 *             must be a 16-bit value and is cast to `uint16_t` to ensure compatibility.
 * @param data The 16-bit value to be written to the specified I/O port.
 *
 * @note This function is marked as `inline` to suggest inlining by the compiler,
 *       which can reduce function call overhead. The `volatile` keyword ensures
 *       that the compiler does not optimize out the assembly instruction.
 */
static inline void outw(int port, uint16_t data) {
  asm volatile ("outw %%ax, %%dx" : : "a"(data), "d"((uint16_t)port));
}

/**
 * Writes a 32-bit value to a specified I/O port.
 *
 * This function uses inline assembly to execute the `outl` instruction, which writes
 * the 32-bit value `data` to the I/O port specified by `port`. The `port` parameter
 * is cast to a 16-bit value to ensure compatibility with the `outl` instruction.
 *
 * @param port The I/O port address to write to. Must be a 16-bit value.
 * @param data The 32-bit value to write to the specified I/O port.
 *
 * @note This function is marked as `inline` to suggest inlining by the compiler for
 *       performance optimization. The `volatile` keyword ensures that the compiler
 *       does not optimize away the assembly instruction.
 */
static inline void outl(int port, uint32_t data) {
  asm volatile ("outl %%eax, %%dx" : : "a"(data), "d"((uint16_t)port));
}

/**
 * Disables interrupts on the current processor by executing the `cli` (Clear Interrupt Flag) instruction.
 * This method is typically used in low-level system programming to ensure that a critical section of code
 * is executed without being interrupted by hardware interrupts. 
 * 
 * Note: After calling this method, interrupts remain disabled until they are explicitly re-enabled using
 * the `sti` (Set Interrupt Flag) instruction or by other means. Prolonged use of this method can lead to
 * missed interrupts and system instability, so it should be used with caution and for the shortest
 * duration necessary.
 */
static inline void cli() {
  asm volatile ("cli");
}

/**
 * Enables interrupts by setting the interrupt flag in the CPU's status register.
 * This is achieved by executing the 'sti' (Set Interrupt Flag) assembly instruction.
 * When interrupts are enabled, the CPU can respond to hardware interrupts, allowing
 * for timely handling of external events such as I/O operations or timer ticks.
 * This function is typically used in contexts where critical sections of code have
 * completed and it is safe to allow interrupts to be processed again.
 */
static inline void sti() {
  asm volatile ("sti");
}

/**
 * @brief Executes the HLT (Halt) instruction, causing the CPU to enter a low-power state.
 *
 * This inline function uses inline assembly to execute the HLT instruction, which halts the CPU
 * until an interrupt occurs. The CPU remains in a low-power state until it is resumed by an
 * interrupt, such as a hardware interrupt or a non-maskable interrupt (NMI). This function is
 * typically used in low-level system programming, such as operating system kernels, to pause
 * the CPU when there is no work to be done.
 *
 * @note This function is declared as `static inline` to allow for inlining, which can reduce
 *       function call overhead in performance-critical code. The `volatile` keyword ensures
 *       that the compiler does not optimize away the HLT instruction.
 */
static inline void hlt() {
  asm volatile ("hlt");
}

/**
 * @brief Introduces a short delay in the execution of the current thread.
 *
 * This method uses the `pause` assembly instruction, which is typically used
 * in spin-wait loops to reduce CPU power consumption and improve performance
 * on hyper-threaded processors. The `pause` instruction signals to the CPU
 * that the code is in a spin-wait loop, allowing it to optimize resource usage
 * by reducing the frequency of memory access and avoiding pipeline stalls.
 *
 * This function is particularly useful in scenarios where a thread is waiting
 * for a condition to be met, such as in lock-free or low-level synchronization
 * primitives. It helps prevent the CPU from being overly busy while waiting.
 *
 * @note The exact duration of the delay is implementation-dependent and may
 * vary across different CPU architectures.
 */
static inline void pause() {
  asm volatile ("pause");
}

/**
 * Retrieves the current value of the EFLAGS register.
 *
 * The EFLAGS register is a 32-bit register that contains the status flags, control flags,
 * and system flags of the CPU. This function uses inline assembly to push the EFLAGS
 * register onto the stack and then pop it into a variable, effectively capturing its
 * current state.
 *
 * @return The 32-bit value of the EFLAGS register at the time of the function call.
 */
static inline uint32_t get_efl() {
  volatile uintptr_t efl;
  asm volatile ("pushf; pop %0": "=r"(efl));
  return efl;
}

/**
 * Reads the value of the CR0 (Control Register 0) register.
 *
 * The CR0 register is a control register in the x86 architecture that contains
 * various system control flags. These flags control operations such as paging,
 * protected mode, and hardware task switching. This function uses inline assembly
 * to move the value of the CR0 register into a variable and returns it.
 *
 * @return The current value of the CR0 register as an unsigned integer pointer.
 */
static inline uintptr_t get_cr0(void) {
  volatile uintptr_t val;
  asm volatile ("mov %%cr0, %0" : "=r"(val));
  return val;
}

/**
 * Sets the value of the CR0 (Control Register 0) on the CPU.
 * 
 * This function is an inline assembly function that directly modifies the CR0 register
 * with the provided value. The CR0 register is a critical control register in x86
 * architecture that controls various system-level operations, such as enabling
 * protected mode, paging, and other processor features.
 * 
 * @param cr0 The value to be written to the CR0 register. This value should be a valid
 *            bitmask that represents the desired configuration for the CR0 register.
 * 
 * @note This function uses inline assembly and is platform-specific to x86 architecture.
 *       It should be used with caution as improper modification of the CR0 register
 *       can lead to system instability or crashes.
 */
static inline void set_cr0(uintptr_t cr0) {
  asm volatile ("mov %0, %%cr0" : : "r"(cr0));
}

/**
 * Sets the Interrupt Descriptor Table (IDT) for the system.
 *
 * This function configures the IDT by loading the provided IDT descriptor
 * into the CPU's IDT register (IDTR). The IDT descriptor consists of a pointer
 * to the IDT and its size in bytes. The function uses an inline assembly
 * instruction `lidt` to perform the actual loading of the IDT descriptor.
 *
 * @param idt A pointer to the Interrupt Descriptor Table (IDT) to be loaded.
 * @param size The size of the IDT in bytes. This value should be one less than
 *             the actual size (e.g., if the IDT is 256 entries, the size should
 *             be 256 * 8 - 1).
 *
 * @note The function uses a packed structure to ensure that the IDT descriptor
 *       is correctly aligned and formatted for the `lidt` instruction.
 * @note The `volatile` keyword ensures that the compiler does not optimize out
 *       the structure or the inline assembly.
 */
static inline void set_idt(void *idt, int size) {
  static volatile struct {
    int16_t size;
    void *idt;
  } __attribute__((packed)) data;
  data.size = size;
  data.idt = idt;
  asm volatile ("lidt (%0)" : : "r"(&data));
}

/**
 * Sets the Global Descriptor Table (GDT) for the CPU.
 *
 * This function configures the GDT by loading the provided GDT descriptor into the CPU's GDTR register.
 * The GDT descriptor includes the base address of the GDT and its size. The function uses inline assembly
 * to execute the `lgdt` instruction, which loads the GDT descriptor from the specified memory location.
 *
 * @param gdt A pointer to the Global Descriptor Table (GDT) to be loaded.
 * @param size The size of the GDT in bytes. This value is stored in the GDT descriptor.
 *
 * @note The GDT descriptor structure is packed to ensure proper alignment and size.
 * @note The function uses a volatile structure to prevent compiler optimizations that might interfere
 *       with the inline assembly operation.
 */
static inline void set_gdt(void *gdt, int size) {
  static volatile struct {
    int16_t size;
    void *gdt;
  } __attribute__((packed)) data;
  data.size = size;
  data.gdt = gdt;
  asm volatile ("lgdt (%0)" : : "r"(&data));
}

/**
 * Sets the Task Register (TR) with the specified selector.
 *
 * The Task Register (TR) is used in x86 architecture to point to the current
 * Task State Segment (TSS), which contains the processor state information
 * for task switching. This function uses the `ltr` (Load Task Register)
 * assembly instruction to load the provided selector into the TR.
 *
 * @param selector The 16-bit segment selector to load into the Task Register.
 *                 This selector must point to a valid TSS descriptor in the
 *                 Global Descriptor Table (GDT).
 */
static inline void set_tr(int selector) {
  asm volatile ("ltr %0" : : "r"((uint16_t)selector));
}

/**
 * Retrieves the value of the CR2 register.
 * 
 * The CR2 register is a control register in x86 architecture that holds the
 * page-fault linear address (the address that caused a page fault) when a
 * page fault exception occurs. This method uses inline assembly to read the
 * value of the CR2 register and return it.
 * 
 * @return The value of the CR2 register as an unsigned integer pointer.
 */
static inline uintptr_t get_cr2() {
  volatile uintptr_t val;
  asm volatile ("mov %%cr2, %0" : "=r"(val));
  return val;
}

/**
 * Retrieves the value of the CR3 register.
 *
 * The CR3 register is a control register in x86 architecture that holds the
 * physical address of the page directory for the current process. This method
 * uses inline assembly to move the value of the CR3 register into a variable.
 *
 * @return The value of the CR3 register as an unsigned integer pointer (uintptr_t).
 */
static inline uintptr_t get_cr3() {
  volatile uintptr_t val;
  asm volatile ("mov %%cr3, %0" : "=r"(val));
  return val;
}

/**
 * Sets the CR3 register to the specified page directory base address.
 * 
 * The CR3 register is used by the x86 architecture to store the physical address
 * of the page directory base. This function updates the CR3 register with the
 * provided page directory base address, effectively switching the current page
 * directory and thus the virtual memory mapping.
 * 
 * @param pdir A pointer to the page directory base address. This address must
 *             be a valid physical address and aligned to a page boundary.
 */
static inline void set_cr3(void *pdir) {
  asm volatile ("mov %0, %%cr3" : : "r"(pdir));
}

/**
 * Atomically exchanges the value at the specified memory address with a new value.
 * This function performs an atomic swap operation using the `xchg` instruction
 * with a `lock` prefix to ensure atomicity across multiple processors or cores.
 *
 * @param addr Pointer to the memory location whose value is to be exchanged.
 * @param newval The new value to be stored at the specified memory location.
 * @return The original value at the memory location before the exchange.
 *
 * @note This function uses inline assembly and is specific to x86 architecture.
 *       The `lock` prefix ensures that the operation is atomic, even in a
 *       multi-threaded or multi-core environment. The `cc` and `memory` clobbers
 *       indicate that the condition codes and memory are modified by the operation.
 */
static inline int xchg(int *addr, int newval) {
  int result;
  asm volatile ("lock xchg %0, %1":
    "+m"(*addr), "=a"(result) : "1"(newval) : "cc", "memory");
  return result;
}

/**
 * Reads the current value of the processor's Time Stamp Counter (TSC).
 *
 * The TSC is a 64-bit register that counts the number of CPU cycles since the last reset.
 * This method uses the `rdtsc` assembly instruction to read the lower 32 bits into `lo`
 * and the upper 32 bits into `hi`. It then combines these two 32-bit values into a single
 * 64-bit value representing the current TSC value.
 *
 * @return A 64-bit unsigned integer representing the current value of the TSC.
 */
static inline uint64_t rdtsc() {
  uint32_t lo, hi;
  asm volatile ("rdtsc": "=a"(lo), "=d"(hi));
  return ((uint64_t)hi << 32) | lo;
}

#define interrupt(id) \
  asm volatile ("int $" #id);

/**
 * @brief Switches the stack and calls a specified function with a given argument.
 *
 * This function performs a low-level stack switch and then jumps to the specified
 * entry point (function) with the provided argument. It is designed to work on both
 * x86_64 and non-x86_64 architectures, with the assembly code tailored for each.
 *
 * @param sp    The new stack pointer to switch to. On non-x86_64 architectures, this
 *              is adjusted by subtracting 8 to account for the argument placement.
 * @param entry The address of the function to call after switching the stack.
 * @param arg   The argument to pass to the function being called.
 *
 * @note This function is marked as `static inline` to encourage inlining and reduce
 * function call overhead. It uses inline assembly to directly manipulate the stack
 * pointer and perform the jump.
 */
static inline void stack_switch_call(void *sp, void *entry, uintptr_t arg) {
  asm volatile (
#if __x86_64__
    "movq %0, %%rsp; movq %2, %%rdi; jmp *%1" : : "b"((uintptr_t)sp),     "d"(entry), "a"(arg)
#else
    "movl %0, %%esp; movl %2, 4(%0); jmp *%1" : : "b"((uintptr_t)sp - 8), "d"(entry), "a"(arg)
#endif
  );
}

/**
 * @brief Retrieves a pointer to the BootRecord structure located at a fixed memory address.
 *
 * This method returns a volatile pointer to the BootRecord structure, which is assumed to be
 * stored at the memory address defined by the BOOTREC_ADDR macro. The volatile qualifier ensures
 * that the compiler does not optimize away accesses to this memory location, which is critical
 * for hardware-related or memory-mapped operations.
 *
 * @return A volatile pointer to the BootRecord structure.
 */
static inline volatile BootRecord *boot_record() {
  return (BootRecord *)BOOTREC_ADDR;
}

#endif // __ASSEMBLER__
