#include <proc.h>
#include <elf.h>

#ifdef __LP64__
# define Elf_Ehdr Elf64_Ehdr
# define Elf_Phdr Elf64_Phdr
#else
# define Elf_Ehdr Elf32_Ehdr
# define Elf_Phdr Elf32_Phdr
#endif

/**
 * Loads a program into the memory of the specified process control block (PCB).
 * This method is responsible for reading the executable file specified by `filename`,
 * allocating the necessary memory, and setting up the initial state of the process
 * within the PCB. It should handle any necessary relocations or address translations
 * to ensure the program is loaded correctly. The method returns the entry point
 * address of the loaded program, which can be used to start execution.
 *
 * @param pcb A pointer to the process control block (PCB) where the program will be loaded.
 * @param filename The name of the executable file to be loaded.
 * @return The entry point address of the loaded program as a uintptr_t. Returns 0 if loading fails.
 */
static uintptr_t loader(PCB *pcb, const char *filename) {
  TODO();
  return 0;
}

/**
 * Loads a program into the memory of the specified process control block (PCB)
 * and transfers execution to the program's entry point.
 *
 * This function uses a naive loader to load the program specified by `filename`
 * into the memory associated with the PCB. After loading, it retrieves the
 * entry point address of the program and jumps to it, effectively starting
 * the execution of the loaded program.
 *
 * @param pcb Pointer to the process control block (PCB) where the program
 *            will be loaded and executed.
 * @param filename The name of the file containing the program to be loaded.
 *
 * @note This function assumes that the loader function correctly loads the
 *       program and returns a valid entry point address. It also assumes
 *       that the program is compatible with the system's execution environment.
 */
void naive_uload(PCB *pcb, const char *filename) {
  uintptr_t entry = loader(pcb, filename);
  Log("Jump to entry = %p", entry);
  ((void(*)())entry) ();
}

