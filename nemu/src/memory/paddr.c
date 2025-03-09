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

#include <memory/host.h>
#include <memory/paddr.h>
#include <device/mmio.h>
#include <isa.h>

#if   defined(CONFIG_PMEM_MALLOC)
static uint8_t *pmem = NULL;
#else // CONFIG_PMEM_GARRAY
static uint8_t pmem[CONFIG_MSIZE] PG_ALIGN = {};
#endif

/**
 * Converts a guest physical address (paddr) to a host virtual address.
 *
 * This function takes a guest physical address and translates it into a host
 * virtual address by adjusting it based on the memory base address (CONFIG_MBASE).
 * The host virtual address is calculated by adding the guest physical address
 * to the base memory pointer (pmem) and then subtracting the memory base offset
 * (CONFIG_MBASE). This is typically used in emulation or virtualization contexts
 * where guest memory is mapped to host memory.
 *
 * @param paddr The guest physical address to be converted.
 * @return A pointer to the corresponding host virtual address.
 */
uint8_t* guest_to_host(paddr_t paddr) { return pmem + paddr - CONFIG_MBASE; }
/**
 * Converts a host virtual address to a guest physical address.
 *
 * This function calculates the guest physical address by first determining the offset
 * of the given host virtual address `haddr` from the start of the physical memory `pmem`.
 * It then adds this offset to the base address of the guest memory `CONFIG_MBASE` to
 * obtain the corresponding guest physical address.
 *
 * @param haddr A pointer to the host virtual address to be converted.
 * @return The guest physical address corresponding to the host virtual address.
 */
paddr_t host_to_guest(uint8_t *haddr) { return haddr - pmem + CONFIG_MBASE; }

/**
 * Reads a word of specified length from the physical memory at the given address.
 *
 * This method translates the guest physical address to a host virtual address
 * using the `guest_to_host` function and then reads the data from the host memory
 * using the `host_read` function. The length of the word to be read is specified
 * by the `len` parameter.
 *
 * @param addr The physical address in the guest memory space to read from.
 * @param len The length of the word to read, typically in bytes.
 * @return The word read from the memory, as a `word_t` type.
 */
static word_t pmem_read(paddr_t addr, int len) {
  word_t ret = host_read(guest_to_host(addr), len);
  return ret;
}

/**
 * Writes data to the physical memory at the specified address.
 * 
 * This function writes a given amount of data to the physical memory location 
 * specified by the address. The address is first converted from the guest 
 * physical address space to the host physical address space using the 
 * `guest_to_host` function. The data is then written to the converted address 
 * using the `host_write` function.
 * 
 * @param addr The physical address in the guest address space where the data 
 *             should be written.
 * @param len  The length of the data to be written, in bytes.
 * @param data The data to be written to the specified memory location.
 */
static void pmem_write(paddr_t addr, int len, word_t data) {
  host_write(guest_to_host(addr), len, data);
}

/**
 * @brief Handles an out-of-bound memory access by triggering a panic.
 *
 * This method is called when a memory access is attempted at an address that
 * is outside the valid range of physical memory. It logs the invalid address,
 * the valid memory range, and the program counter (PC) at the time of the
 * access, then triggers a system panic to halt execution.
 *
 * @param addr The physical address that was accessed, which is out of bounds.
 */
static void out_of_bound(paddr_t addr) {
  panic("address = " FMT_PADDR " is out of bound of pmem [" FMT_PADDR ", " FMT_PADDR "] at pc = " FMT_WORD,
      addr, PMEM_LEFT, PMEM_RIGHT, cpu.pc);
}

/**
 * Initializes the physical memory based on the configuration settings.
 *
 * This function allocates memory for the physical memory area if the `CONFIG_PMEM_MALLOC`
 * macro is defined. It ensures the allocation is successful by asserting the pointer.
 * If the `CONFIG_MEM_RANDOM` macro is defined, the allocated memory is filled with random
 * values using the `rand()` function. Finally, it logs the physical memory address range
 * using the `Log` function.
 *
 * The memory size is determined by the `CONFIG_MSIZE` macro, and the address range is
 * represented by `PMEM_LEFT` and `PMEM_RIGHT`.
 */
void init_mem() {
#if   defined(CONFIG_PMEM_MALLOC)
  pmem = malloc(CONFIG_MSIZE);
  assert(pmem);
#endif
  IFDEF(CONFIG_MEM_RANDOM, memset(pmem, rand(), CONFIG_MSIZE));
  Log("physical memory area [" FMT_PADDR ", " FMT_PADDR "]", PMEM_LEFT, PMEM_RIGHT);
}

/**
 * Reads a word of data from a specified physical address.
 *
 * This function reads a word of data from the given physical address `addr` with the specified length `len`.
 * It first checks if the address is within the valid physical memory range using the `in_pmem` function.
 * If the address is within the physical memory range, it reads the data using the `pmem_read` function.
 * If the address is not within the physical memory range and the `CONFIG_DEVICE` macro is defined, it attempts
 * to read the data using the `mmio_read` function, which is typically used for memory-mapped I/O operations.
 * If the address is out of bounds and `CONFIG_DEVICE` is not defined, it calls the `out_of_bound` function to
 * handle the error and returns 0.
 *
 * @param addr The physical address from which to read the data.
 * @param len The length of the data to read.
 * @return The word of data read from the address, or 0 if the address is out of bounds.
 */
word_t paddr_read(paddr_t addr, int len) {
  if (likely(in_pmem(addr))) return pmem_read(addr, len);
  IFDEF(CONFIG_DEVICE, return mmio_read(addr, len));
  out_of_bound(addr);
  return 0;
}

/**
 * Writes data to a specified physical address with a given length.
 * 
 * This function handles writing data to a physical address by first checking if the address
 * is within the physical memory range using `in_pmem(addr)`. If the address is within the
 * physical memory range, the data is written using `pmem_write(addr, len, data)`.
 * 
 * If the address is not within the physical memory range and the `CONFIG_DEVICE` macro is
 * defined, the function attempts to write the data to a memory-mapped I/O (MMIO) device
 * using `mmio_write(addr, len, data)`.
 * 
 * If the address is neither in physical memory nor mapped to an MMIO device, the function
 * calls `out_of_bound(addr)` to handle the out-of-bounds access.
 * 
 * @param addr The physical address to write to.
 * @param len The length of the data to write.
 * @param data The data to write to the specified address.
 */
void paddr_write(paddr_t addr, int len, word_t data) {
  if (likely(in_pmem(addr))) { pmem_write(addr, len, data); return; }
  IFDEF(CONFIG_DEVICE, mmio_write(addr, len, data); return);
  out_of_bound(addr);
}
