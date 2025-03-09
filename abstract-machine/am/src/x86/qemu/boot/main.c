#include <stdint.h>
#include <elf.h>
#include <x86/x86.h>

#define SECTSIZE 512
#define ARGSIZE  1024

/**
 * Waits for the disk to be ready for the next command.
 * 
 * This function continuously polls the status register of the disk controller
 * (located at I/O port 0x1F7) until the disk indicates it is ready. The disk is
 * considered ready when the status register's top two bits (0xC0) match the
 * value 0x40, which signifies that the disk is not busy and is ready to accept
 * a new command.
 * 
 * The function uses the `inb` instruction to read the status register and
 * checks the condition in a loop until the disk is ready. This is a blocking
 * operation and will not return until the disk is ready.
 */
static inline void wait_disk(void) {
  while ((inb(0x1f7) & 0xc0) != 0x40);
}

/**
 * Reads a sector from the disk into the provided buffer.
 *
 * This function performs the following steps:
 * 1. Waits for the disk to be ready using `wait_disk()`.
 * 2. Sends the necessary commands to the disk controller to specify the sector to be read:
 *    - Sets the number of sectors to read (1) by writing to port 0x1F2.
 *    - Writes the sector number to ports 0x1F3, 0x1F4, and 0x1F5, with the sector number being split into bytes.
 *    - Writes the most significant byte of the sector number and the drive selection bits to port 0x1F6.
 *    - Sends the read command (0x20) to port 0x1F7.
 * 3. Waits again for the disk to be ready using `wait_disk()`.
 * 4. Reads the sector data from the disk controller into the buffer by reading from port 0x1F0 in 32-bit chunks.
 *
 * @param buf Pointer to the buffer where the sector data will be stored.
 * @param sect The sector number to read from the disk.
 */
static inline void read_disk(void *buf, int sect) {
  wait_disk();
  outb(0x1f2, 1);
  outb(0x1f3, sect);
  outb(0x1f4, sect >> 8);
  outb(0x1f5, sect >> 16);
  outb(0x1f6, (sect >> 24) | 0xE0);
  outb(0x1f7, 0x20);
  wait_disk();
  for (int i = 0; i < SECTSIZE / 4; i ++) {
    ((uint32_t *)buf)[i] = inl(0x1f0);
  }
}

/**
 * Copies data from disk into a memory buffer.
 *
 * This function reads data from the disk in sectors and copies it into the specified memory buffer.
 * The function calculates the starting and ending memory addresses based on the provided buffer
 * pointer and the number of bytes to copy. It then reads data from the disk in sector-sized chunks
 * and stores it in the buffer.
 *
 * @param buf        Pointer to the memory buffer where the data will be copied.
 * @param nbytes     Number of bytes to copy from the disk to the buffer.
 * @param disk_offset Offset on the disk from where the data should be read.
 *
 * @note The function assumes that the buffer is aligned to sector boundaries and that the disk
 *       operations are handled by the `read_disk` function. The `SECTSIZE` and `ARGSIZE` constants
 *       are used to calculate the sector addresses.
 */
static inline void copy_from_disk(void *buf, int nbytes, int disk_offset) {
  uint32_t cur  = (uint32_t)buf & ~(SECTSIZE - 1);
  uint32_t ed   = (uint32_t)buf + nbytes;
  uint32_t sect = (disk_offset / SECTSIZE) + (ARGSIZE / SECTSIZE) + 1;
  for(; cur < ed; cur += SECTSIZE, sect ++)
    read_disk((void *)cur, sect);
}

/**
 * Loads a program into memory by copying its contents from disk and initializing the BSS section.
 *
 * This function performs two main tasks:
 * 1. Copies the program's data from disk into memory starting at the specified physical address (paddr).
 *    The data is read from the disk starting at the given offset and is of size `filesz`.
 * 2. Initializes the BSS (Block Started by Symbol) section of the program in memory. The BSS section
 *    is the portion of memory that follows the program's data and is used for uninitialized global
 *    and static variables. This section is initialized to zero.
 *
 * @param filesz The size of the program's data in bytes, as stored on disk.
 * @param memsz  The total size of the program in memory, including both the data and the BSS section.
 * @param paddr  The physical address in memory where the program should be loaded.
 * @param offset The offset on disk where the program's data begins.
 */
static void load_program(uint32_t filesz, uint32_t memsz, uint32_t paddr, uint32_t offset) {
  copy_from_disk((void *)paddr, filesz, offset);
  char *bss = (void *)(paddr + filesz);
  for (uint32_t i = filesz; i != memsz; i++) {
    *bss++ = 0;
  }
}

/**
 * Loads an ELF64 executable into memory by iterating over its program headers.
 * 
 * This method takes a pointer to the ELF64 header (`elf`) and processes each program
 * header to load the corresponding program segments into memory. It calculates the
 * address of the program headers using the `e_phoff` field from the ELF header. For
 * each program header, it extracts the file size (`p_filesz`), memory size (`p_memsz`),
 * physical address (`p_paddr`), and file offset (`p_offset`), and passes these values
 * to the `load_program` function to perform the actual loading.
 * 
 * @param elf Pointer to the ELF64 header of the executable to be loaded.
 */
static void load_elf64(Elf64_Ehdr *elf) {
  Elf64_Phdr *ph = (Elf64_Phdr *)((char *)elf + elf->e_phoff);
  for (int i = 0; i < elf->e_phnum; i++, ph++) {
    load_program(
      (uint32_t)ph->p_filesz,
      (uint32_t)ph->p_memsz,
      (uint32_t)ph->p_paddr,
      (uint32_t)ph->p_offset
    );
  }
}

/**
 * Loads an ELF32 binary into memory by processing its program headers.
 *
 * This method takes a pointer to an ELF32 header (`Elf32_Ehdr`) and iterates
 * over its program headers (`Elf32_Phdr`). For each program header, it calls
 * the `load_program` function to load the corresponding program segment into
 * memory. The program headers are located using the `e_phoff` field of the ELF
 * header, and the number of program headers is determined by the `e_phnum`
 * field.
 *
 * @param elf Pointer to the ELF32 header structure.
 */
static void load_elf32(Elf32_Ehdr *elf) {
  Elf32_Phdr *ph = (Elf32_Phdr *)((char *)elf + elf->e_phoff);
  for (int i = 0; i < elf->e_phnum; i++, ph++) {
    load_program(
      (uint32_t)ph->p_filesz,
      (uint32_t)ph->p_memsz,
      (uint32_t)ph->p_paddr,
      (uint32_t)ph->p_offset
    );
  }
}

/**
 * Loads the kernel from disk into memory and executes it. This function is responsible for
 * loading the kernel's ELF header and determining whether the kernel is 32-bit or 64-bit.
 * If the current CPU is the Application Processor (AP), it assumes the kernel is already
 * loaded and proceeds directly to execution. Otherwise, it loads the kernel's ELF header
 * and the main argument string into memory, then loads the appropriate ELF binary based
 * on the architecture. Finally, it jumps to the kernel's entry point to begin execution.
 *
 * The ELF header is expected to be located at memory address 0x8000. The main argument
 * string is loaded into memory at MAINARG_ADDR. The function uses the `copy_from_disk`
 * utility to read data from disk into memory.
 *
 * After loading the kernel, the function determines the entry point from the ELF header
 * and executes the kernel by jumping to that address.
 */
void load_kernel(void) {
  union {
    Elf32_Ehdr elf32;
    Elf64_Ehdr elf64;
  } *u = (void *)0x8000;
  int is_ap = boot_record()->is_ap;

  if (!is_ap) {
    // load argument (string) to memory
    copy_from_disk((void *)MAINARG_ADDR, 1024, -1024);
    // load elf header to memory
    copy_from_disk(u, 4096, 0);
    if (u->elf32.e_machine == EM_X86_64) {
      load_elf64(&u->elf64);
    } else {
      load_elf32(&u->elf32);
    }
  } else {
    // everything should be loaded
  }

  if (u->elf32.e_machine == EM_X86_64) {
    ((void(*)())(uint32_t)(u->elf64.e_entry))();
  } else {
    ((void(*)())(uint32_t)(u->elf32.e_entry))();
  }
}
