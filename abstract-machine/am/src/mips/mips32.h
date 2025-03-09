#ifndef MIPS32_H__
#define MIPS32_H__

#include <stdint.h>

/**
 * Reads a single byte from the specified memory-mapped I/O address.
 * 
 * This function is typically used to interact with hardware devices that expose
 * their registers through memory-mapped I/O. It performs a volatile read to ensure
 * that the compiler does not optimize away the access, as hardware registers can
 * change value outside the program's control.
 * 
 * @param addr The memory address from which to read the byte. This is typically
 *             a memory-mapped I/O address.
 * @return The byte read from the specified address.
 */
static inline uint8_t inb(uintptr_t addr) { return *(volatile uint8_t *)addr; }
/**
 * Reads a 16-bit unsigned integer from a specified memory address.
 *
 * This function performs a read operation on the given memory address, treating
 * the location as a volatile 16-bit unsigned integer. The `volatile` keyword
 * ensures that the compiler does not optimize away the read, making it suitable
 * for accessing hardware registers or memory-mapped I/O devices where the value
 * can change unexpectedly.
 *
 * @param addr The memory address from which to read the 16-bit value.
 * @return The 16-bit unsigned integer value read from the specified address.
 */
static inline uint16_t inw(uintptr_t addr) { return *(volatile uint16_t *)addr; }
/**
 * Reads a 32-bit unsigned integer from a specified memory address.
 * 
 * This function performs a volatile read operation on the memory location 
 * pointed to by `addr`, ensuring that the compiler does not optimize away 
 * the read. It is typically used to access hardware registers or memory-mapped 
 * I/O devices where the value at the address can change outside the program's 
 * control.
 * 
 * @param addr The memory address from which to read the 32-bit value.
 * @return The 32-bit unsigned integer value read from the specified address.
 */
static inline uint32_t inl(uintptr_t addr) { return *(volatile uint32_t *)addr; }

/**
 * Writes a single byte of data to a specified I/O port address.
 * 
 * This function is typically used in low-level system programming to interact
 * with hardware devices by writing a byte of data to a specific I/O port.
 * The address is cast to a volatile pointer to ensure that the compiler does
 * not optimize away the write operation, as it may have side effects.
 * 
 * @param addr The I/O port address to which the data will be written. This is
 *             typically a memory-mapped I/O address.
 * @param data The 8-bit data value to be written to the specified I/O port.
 */
static inline void outb(uintptr_t addr, uint8_t  data) { *(volatile uint8_t  *)addr = data; }
/**
 * Writes a 16-bit value to a specified memory-mapped I/O address.
 *
 * This function is used to output a 16-bit data value to a hardware device
 * that is memory-mapped at the given address. The address is treated as a
 * volatile pointer to ensure that the compiler does not optimize out the
 * write operation, which is critical for hardware interactions.
 *
 * @param addr The memory-mapped I/O address where the data will be written.
 *             This address is cast to a volatile pointer to a 16-bit unsigned integer.
 * @param data The 16-bit value to be written to the specified address.
 */
static inline void outw(uintptr_t addr, uint16_t data) { *(volatile uint16_t *)addr = data; }
/**
 * Writes a 32-bit value to a specified memory-mapped I/O address.
 *
 * This function is used to perform a memory-mapped I/O write operation. It takes
 * a memory address (`addr`) and a 32-bit data value (`data`), and writes the data
 * to the specified address. The address is treated as a volatile pointer to a
 * 32-bit unsigned integer, ensuring that the compiler does not optimize away
 * the write operation.
 *
 * @param addr The memory-mapped I/O address where the data will be written.
 *             This is typically a hardware register address.
 * @param data The 32-bit value to be written to the specified address.
 */
static inline void outl(uintptr_t addr, uint32_t data) { *(volatile uint32_t *)addr = data; }

#define PTE_V 0x2
#define PTE_D 0x4

// Page directory and page table constants
#define PTXSHFT   12      // Offset of PTX in a linear address
#define PDXSHFT   22      // Offset of PDX in a linear address

#define PDX(va)     (((uint32_t)(va) >> PDXSHFT) & 0x3ff)
#define PTX(va)     (((uint32_t)(va) >> PTXSHFT) & 0x3ff)

#endif
