#ifndef LOONGARCH32R_H__
#define LOONGARCH32R_H__

#include <stdint.h>

/**
 * Reads a single byte from the specified memory-mapped I/O address.
 * 
 * This function is used to perform an input operation from a hardware port or
 * memory-mapped I/O location. It reads an 8-bit value from the given address.
 * The address is treated as a volatile pointer to ensure that the compiler
 * does not optimize away the read operation, as it may have side effects.
 * 
 * @param addr The memory-mapped I/O address from which to read the byte.
 * @return The 8-bit value read from the specified address.
 */
static inline uint8_t inb(uintptr_t addr) { return *(volatile uint8_t *)addr; }
/**
 * Reads a 16-bit unsigned integer from a specified memory-mapped I/O address.
 *
 * This function is typically used to access hardware registers or memory-mapped
 * I/O locations. It performs a volatile read operation to ensure that the compiler
 * does not optimize away the access, as the value at the address may change
 * outside the program's control (e.g., by hardware).
 *
 * @param addr The memory address from which to read the 16-bit value. This address
 *             should be a valid memory-mapped I/O location.
 * @return The 16-bit unsigned integer value read from the specified address.
 */
static inline uint16_t inw(uintptr_t addr) { return *(volatile uint16_t *)addr; }
/**
 * Reads a 32-bit unsigned integer from a specified memory address.
 *
 * This function performs a volatile read operation on the memory location
 * pointed to by `addr`. The volatile qualifier ensures that the compiler
 * does not optimize away the read, which is important when accessing
 * memory-mapped hardware registers or shared memory that may change
 * asynchronously.
 *
 * @param addr The memory address from which to read the 32-bit value.
 *             This should be a valid address in the process's address space.
 * @return The 32-bit unsigned integer value read from the specified address.
 */
static inline uint32_t inl(uintptr_t addr) { return *(volatile uint32_t *)addr; }

/**
 * Writes a single byte of data to a specified I/O port address.
 *
 * This function is used to perform an I/O port write operation, typically in low-level
 * hardware programming. It writes the provided 8-bit data value to the memory-mapped
 * I/O port address specified by `addr`. The address is treated as a volatile pointer
 * to ensure that the compiler does not optimize away the write operation.
 *
 * @param addr The memory-mapped I/O port address to write to. This is typically a
 *             hardware-specific address representing an I/O port.
 * @param data The 8-bit data value to write to the specified I/O port.
 */
static inline void outb(uintptr_t addr, uint8_t data) { *(volatile uint8_t *)addr = data; }
/**
 * Writes a 16-bit value to a specified memory-mapped I/O address.
 *
 * This function is used to output a 16-bit data value to a hardware device
 * that is memory-mapped to the specified address. The address is treated
 * as a pointer to a volatile 16-bit unsigned integer, ensuring that the
 * compiler does not optimize away the write operation.
 *
 * @param addr The memory-mapped I/O address where the data will be written.
 *             This address is cast to a volatile uint16_t pointer.
 * @param data The 16-bit value to be written to the specified address.
 */
static inline void outw(uintptr_t addr, uint16_t data) { *(volatile uint16_t *)addr = data; }
/**
 * Writes a 32-bit value to a specified memory-mapped I/O address.
 *
 * This function is used to output a 32-bit data value to a memory-mapped I/O
 * location. The address is treated as a volatile pointer to ensure that the
 * compiler does not optimize away the write operation. This is commonly used
 * in low-level hardware programming to interact with device registers.
 *
 * @param addr The memory-mapped I/O address where the data will be written.
 *             This is typically a physical address mapped to a device register.
 * @param data The 32-bit value to be written to the specified address.
 */
static inline void outl(uintptr_t addr, uint32_t data) { *(volatile uint32_t *)addr = data; }

#define PTE_V 0x1
#define PTE_D 0x2

// Page directory and page table constants
#define PTXSHFT   12      // Offset of PTX in a linear address
#define PDXSHFT   22      // Offset of PDX in a linear address

#define PDX(va)     (((uint32_t)(va) >> PDXSHFT) & 0x3ff)
#define PTX(va)     (((uint32_t)(va) >> PTXSHFT) & 0x3ff)

#endif
