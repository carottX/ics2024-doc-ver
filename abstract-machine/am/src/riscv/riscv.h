#ifndef RISCV_H__
#define RISCV_H__

#include <stdint.h>

/**
 * Reads a single byte from the specified memory-mapped I/O address.
 * 
 * This function is used to perform an input operation from a hardware device
 * that is memory-mapped to the given address. The function treats the address
 * as a pointer to a volatile 8-bit unsigned integer, ensuring that the compiler
 * does not optimize away the read operation and that the read is performed
 * directly from the hardware.
 * 
 * @param addr The memory-mapped I/O address from which to read the byte.
 * @return The byte read from the specified address.
 */
static inline uint8_t inb(uintptr_t addr) { return *(volatile uint8_t *)addr; }
/**
 * Reads a 16-bit unsigned integer from a specified memory address.
 * 
 * This function is used to perform a direct memory read operation from the given
 * address, treating the data as a volatile 16-bit unsigned integer. The 'volatile'
 * keyword ensures that the compiler does not optimize away the read operation,
 * which is crucial when dealing with memory-mapped I/O or hardware registers.
 * 
 * @param addr The memory address from which to read the 16-bit value.
 * @return The 16-bit unsigned integer value read from the specified address.
 */
static inline uint16_t inw(uintptr_t addr) { return *(volatile uint16_t *)addr; }
/**
 * Reads a 32-bit unsigned integer from a specified memory address.
 *
 * This function performs a volatile read operation on the memory location
 * pointed to by `addr`. The volatile qualifier ensures that the compiler
 * does not optimize out the read operation, which is essential for accessing
 * memory-mapped hardware registers or shared memory locations that may change
 * asynchronously.
 *
 * @param addr The memory address from which to read the 32-bit value.
 * @return The 32-bit unsigned integer value read from the specified address.
 */
static inline uint32_t inl(uintptr_t addr) { return *(volatile uint32_t *)addr; }

/**
 * Writes a single byte of data to a specified I/O port address.
 *
 * This function is used to perform an output operation to an I/O port. It takes
 * a memory-mapped I/O address and a byte of data, then writes the data to the
 * specified address. The address is treated as a volatile pointer to ensure that
 * the compiler does not optimize away the write operation.
 *
 * @param addr The memory-mapped I/O address to which the data will be written.
 *             This address is typically a hardware register or I/O port.
 * @param data The 8-bit data value to be written to the specified address.
 */
static inline void outb(uintptr_t addr, uint8_t data) { *(volatile uint8_t *)addr = data; }
/**
 * Writes a 16-bit value to a specified memory-mapped I/O address.
 *
 * This function is used to perform a 16-bit write operation to a memory-mapped
 * I/O location. The address is cast to a volatile pointer to ensure that the
 * compiler does not optimize away the write operation, which is crucial for
 * hardware interactions.
 *
 * @param addr The memory-mapped I/O address to write to. This should be a valid
 *             physical address where the hardware device is mapped.
 * @param data The 16-bit value to write to the specified address.
 */
static inline void outw(uintptr_t addr, uint16_t data) { *(volatile uint16_t *)addr = data; }
/**
 * Writes a 32-bit value to a specified memory-mapped I/O address.
 *
 * This function is used to perform a write operation to a memory-mapped I/O location.
 * It takes a memory address `addr` and a 32-bit data value `data`, and writes `data`
 * to the memory location specified by `addr`. The address is treated as a volatile
 * pointer to a 32-bit unsigned integer, ensuring that the compiler does not optimize
 * away the write operation.
 *
 * @param addr The memory-mapped I/O address where the data will be written.
 * @param data The 32-bit value to be written to the specified address.
 */
static inline void outl(uintptr_t addr, uint32_t data) { *(volatile uint32_t *)addr = data; }

#define PTE_V 0x01
#define PTE_R 0x02
#define PTE_W 0x04
#define PTE_X 0x08
#define PTE_U 0x10
#define PTE_A 0x40
#define PTE_D 0x80

enum { MODE_U, MODE_S, MODE_M = 3 };
#define MSTATUS_MXR  (1 << 19)
#define MSTATUS_SUM  (1 << 18)

#if __riscv_xlen == 64
#define MSTATUS_SXL  (2ull << 34)
#define MSTATUS_UXL  (2ull << 32)
#else
#define MSTATUS_SXL  0
#define MSTATUS_UXL  0
#endif

#endif
