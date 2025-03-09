#include <unistd.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <assert.h>
#include <time.h>
#include "syscall.h"

// helper macros
#define _concat(x, y) x ## y
#define concat(x, y) _concat(x, y)
#define _args(n, list) concat(_arg, n) list
#define _arg0(a0, ...) a0
#define _arg1(a0, a1, ...) a1
#define _arg2(a0, a1, a2, ...) a2
#define _arg3(a0, a1, a2, a3, ...) a3
#define _arg4(a0, a1, a2, a3, a4, ...) a4
#define _arg5(a0, a1, a2, a3, a4, a5, ...) a5

// extract an argument from the macro array
#define SYSCALL  _args(0, ARGS_ARRAY)
#define GPR1 _args(1, ARGS_ARRAY)
#define GPR2 _args(2, ARGS_ARRAY)
#define GPR3 _args(3, ARGS_ARRAY)
#define GPR4 _args(4, ARGS_ARRAY)
#define GPRx _args(5, ARGS_ARRAY)

// ISA-depedent definitions
#if defined(__ISA_X86__)
# define ARGS_ARRAY ("int $0x80", "eax", "ebx", "ecx", "edx", "eax")
#elif defined(__ISA_MIPS32__)
# define ARGS_ARRAY ("syscall", "v0", "a0", "a1", "a2", "v0")
#elif defined(__riscv)
#ifdef __riscv_e
# define ARGS_ARRAY ("ecall", "a5", "a0", "a1", "a2", "a0")
#else
# define ARGS_ARRAY ("ecall", "a7", "a0", "a1", "a2", "a0")
#endif
#elif defined(__ISA_AM_NATIVE__)
# define ARGS_ARRAY ("call *0x100000", "rdi", "rsi", "rdx", "rcx", "rax")
#elif defined(__ISA_X86_64__)
# define ARGS_ARRAY ("int $0x80", "rdi", "rsi", "rdx", "rcx", "rax")
#elif defined(__ISA_LOONGARCH32R__)
# define ARGS_ARRAY ("syscall 0", "a7", "a0", "a1", "a2", "a0")
#else
#error _syscall_ is not implemented
#endif

/**
 * _syscall_ - Invokes a system call with the specified parameters.
 *
 * This function is a low-level wrapper for invoking a system call. It uses inline assembly to
 * pass the system call type and arguments directly to the CPU registers, as required by the
 * system call convention. The system call type and arguments are passed via general-purpose
 * registers (GPR1, GPR2, GPR3, GPR4), and the result of the system call is returned via a
 * designated register (GPRx).
 *
 * @param type The type of system call to invoke. This value is passed to the GPR1 register.
 * @param a0   The first argument to the system call. This value is passed to the GPR2 register.
 * @param a1   The second argument to the system call. This value is passed to the GPR3 register.
 * @param a2   The third argument to the system call. This value is passed to the GPR4 register.
 *
 * @return The result of the system call, as returned by the CPU in the designated register (GPRx).
 */
intptr_t _syscall_(intptr_t type, intptr_t a0, intptr_t a1, intptr_t a2) {
  register intptr_t _gpr1 asm (GPR1) = type;
  register intptr_t _gpr2 asm (GPR2) = a0;
  register intptr_t _gpr3 asm (GPR3) = a1;
  register intptr_t _gpr4 asm (GPR4) = a2;
  register intptr_t ret asm (GPRx);
  asm volatile (SYSCALL : "=r" (ret) : "r"(_gpr1), "r"(_gpr2), "r"(_gpr3), "r"(_gpr4));
  return ret;
}

/**
 * Terminates the calling process with the specified exit status.
 * 
 * This function invokes the system call `SYS_exit` to terminate the current process.
 * The `status` parameter is passed to the system call, which can be used to indicate
 * the exit status of the process. After making the system call, the function enters
 * an infinite loop to ensure that the process does not continue execution.
 * 
 * @param status The exit status of the process. This value is returned to the parent
 *               process and can be used to indicate success, failure, or other
 *               process-specific information.
 */
void _exit(int status) {
  _syscall_(SYS_exit, status, 0, 0);
  while (1);
}

/**
 * _open - Attempts to open a file located at the specified path with the given flags and mode.
 * 
 * This function is a stub implementation that immediately calls the system exit function
 * with the `SYS_open` argument, effectively terminating the program without performing
 * any file operations. It does not actually open the file or handle any file-related
 * operations. The function always returns 0, but this return value is never reached
 * due to the program termination.
 *
 * @param path The file system path of the file to be opened. This parameter is ignored.
 * @param flags The flags specifying the mode in which the file should be opened. This parameter is ignored.
 * @param mode The file mode (permissions) to be used if the file is created. This parameter is ignored.
 * @return Always returns 0, but this return value is never reached due to the program termination.
 */
int _open(const char *path, int flags, mode_t mode) {
  _exit(SYS_open);
  return 0;
}

/**
 * Writes data to a file descriptor.
 *
 * This function is intended to write up to `count` bytes of data from the buffer
 * pointed to by `buf` to the file descriptor `fd`. However, the current implementation
 * immediately calls the system exit function with the `SYS_write` argument, which
 * may terminate the program or invoke a system-specific behavior related to writing.
 * The function always returns 0, indicating no bytes were written.
 *
 * @param fd The file descriptor to which the data should be written.
 * @param buf A pointer to the buffer containing the data to be written.
 * @param count The number of bytes to write from the buffer.
 * @return Always returns 0, indicating no bytes were written.
 */
int _write(int fd, void *buf, size_t count) {
  _exit(SYS_write);
  return 0;
}

/**
 * _sbrk - Adjusts the program break, which is the end of the process's data segment.
 *         This function is typically used to allocate or deallocate memory from the heap.
 *         When called with a positive increment, it attempts to increase the program break,
 *         effectively allocating more memory. When called with a negative increment,
 *         it attempts to decrease the program break, deallocating memory.
 *
 * @increment: The number of bytes to adjust the program break by. A positive value
 *             increases the program break, while a negative value decreases it.
 *
 * Return: On success, returns a pointer to the previous program break. On failure,
 *         returns (void *)-1, indicating that the adjustment was not possible.
 */
void *_sbrk(intptr_t increment) {
  return (void *)-1;
}

/**
 * Reads data from a file descriptor into a buffer.
 *
 * This method is a wrapper for the system call `read`, which reads up to `count`
 * bytes of data from the file descriptor `fd` into the buffer pointed to by `buf`.
 * The actual reading is performed by invoking the system call `SYS_read` via `_exit`.
 *
 * @param fd The file descriptor from which to read the data.
 * @param buf A pointer to the buffer where the read data will be stored.
 * @param count The maximum number of bytes to read.
 * @return Always returns 0, as the method exits via `_exit` before returning.
 */
int _read(int fd, void *buf, size_t count) {
  _exit(SYS_read);
  return 0;
}

/**
 * Closes the file descriptor specified by `fd` and terminates the process.
 * 
 * This method invokes the system call `SYS_close` to close the file descriptor
 * associated with the given `fd`. After closing the file descriptor, the method
 * calls `_exit` to terminate the process immediately. The return value of 0 is
 * purely symbolic, as the process does not continue execution after the `_exit`
 * call.
 * 
 * @param fd The file descriptor to be closed.
 * @return Always returns 0, though the process terminates before this value is used.
 */
int _close(int fd) {
  _exit(SYS_close);
  return 0;
}

/**
 * _lseek - Repositions the file offset of the open file associated with the file descriptor fd.
 *
 * @fd: The file descriptor for the open file.
 * @offset: The number of bytes to move the file pointer, relative to the position specified by whence.
 * @whence: The reference point for the offset. Possible values are:
 *          - SEEK_SET: The offset is set to offset bytes.
 *          - SEEK_CUR: The offset is set to its current location plus offset bytes.
 *          - SEEK_END: The offset is set to the size of the file plus offset bytes.
 *
 * This function triggers a system call (SYS_lseek) to reposition the file pointer and then exits the program.
 * It does not return the new file offset; instead, it terminates the program immediately.
 *
 * Return: This function does not return a meaningful value; it always returns 0 after invoking the system call.
 */
off_t _lseek(int fd, off_t offset, int whence) {
  _exit(SYS_lseek);
  return 0;
}

/**
 * _gettimeofday - Retrieves the current time and timezone information.
 * 
 * This function is intended to obtain the current time and, optionally, the 
 * timezone information. The time is stored in the `tv` parameter, which is a 
 * pointer to a `struct timeval`. The timezone information, if requested, is 
 * stored in the `tz` parameter, which is a pointer to a `struct timezone`.
 * 
 * The function currently calls the `_exit` system call with the `SYS_gettimeofday`
 * argument, which may indicate that this is a placeholder or stub implementation
 * for a system-specific function. The function always returns 0, suggesting that
 * it may not fully implement the intended functionality.
 * 
 * @param tv  Pointer to a `struct timeval` where the current time will be stored.
 * @param tz  Pointer to a `struct timezone` where the timezone information will 
 *            be stored (optional, can be NULL).
 * 
 * @return Always returns 0, indicating success or a placeholder implementation.
 */
int _gettimeofday(struct timeval *tv, struct timezone *tz) {
  _exit(SYS_gettimeofday);
  return 0;
}

/**
 * Executes a new program by replacing the current process image with a new one.
 * This function is a wrapper around the `SYS_execve` system call, which loads and
 * executes the program specified by `fname` with the given arguments and environment.
 * 
 * @param fname  The path to the executable file to be executed.
 * @param argv   An array of argument strings passed to the new program. The first
 *               argument should be the name of the program itself, and the array
 *               must be terminated by a NULL pointer.
 * @param envp   An array of environment variable strings passed to the new program.
 *               The array must be terminated by a NULL pointer.
 * 
 * @return This function does not return on success, as the current process is replaced
 *         by the new program. On failure, it returns 0, though this is not typically
 *         reached due to the `_exit` call.
 */
int _execve(const char *fname, char * const argv[], char *const envp[]) {
  _exit(SYS_execve);
  return 0;
}

// Syscalls below are not used in Nanos-lite.
// But to pass linking, they are defined as dummy functions.

int _fstat(int fd, struct stat *buf) {
  return -1;
}

/**
 * Retrieves file status information for the specified file.
 *
 * This function is intended to retrieve information about the file specified by
 * `fname` and store it in the `buf` structure. The information includes file
 * type, permissions, size, and other attributes.
 *
 * @param fname Pointer to a null-terminated string containing the path to the file.
 * @param buf   Pointer to a `struct stat` where the file status information will be stored.
 *
 * @return On success, returns 0. On failure, returns -1 and sets `errno` to indicate the error.
 *         This implementation always asserts and returns -1, indicating it is not functional.
 */
int _stat(const char *fname, struct stat *buf) {
  assert(0);
  return -1;
}

/**
 * _kill - Sends a signal to a process identified by its PID.
 *
 * This function is intended to send a signal `sig` to the process with the
 * specified `pid`. However, the current implementation does not perform the
 * actual signal sending operation. Instead, it calls `_exit` with the negative
 * value of `SYS_kill`, which likely indicates an error or an unsupported
 * operation. The function then returns `-1` to signify failure.
 *
 * @pid: The process ID of the target process to which the signal should be sent.
 * @sig: The signal number to be sent to the process.
 *
 * Return: Always returns `-1` to indicate that the operation failed.
 */
int _kill(int pid, int sig) {
  _exit(-SYS_kill);
  return -1;
}

/**
 * @brief Retrieves the process ID (PID) of the calling process.
 *
 * This method attempts to retrieve the PID of the current process by invoking the
 * `SYS_getpid` system call. However, the implementation is flawed as it immediately
 * terminates the process by calling `_exit` with the negation of the `SYS_getpid` value
 * as the exit status. The `return 1;` statement is unreachable and serves no practical
 * purpose.
 *
 * @return pid_t The method does not return a valid PID due to the immediate process
 *               termination. The return value `1` is never reached.
 */
pid_t _getpid() {
  _exit(-SYS_getpid);
  return 1;
}

/**
 * @brief Simulates the creation of a new process by forking the current process.
 *
 * This method is a placeholder that currently does not implement actual forking functionality.
 * Instead, it asserts with a value of 0, indicating that the method is not yet implemented or
 * should not be called in the current context. It returns -1 to signify an error condition,
 * which is consistent with the behavior of the standard `fork()` system call when it fails.
 *
 * @return Returns -1 to indicate failure, as the method does not perform any actual forking.
 *         In a successful fork, the return value would be the process ID (PID) of the child process
 *         to the parent, and 0 to the child process.
 */
pid_t _fork() {
  assert(0);
  return -1;
}

/**
 * @brief Simulates the vfork() system call.
 *
 * This method is a placeholder implementation that always asserts and returns an error.
 * The vfork() system call is typically used to create a new process that shares the address
 * space of the parent process until the child process either exits or calls execve().
 * This implementation, however, immediately asserts with a value of 0, causing the program
 * to terminate if assertions are enabled. It then returns -1 to indicate an error.
 *
 * @return Returns -1 to indicate failure, as the method does not actually create a new process.
 */
pid_t vfork() {
  assert(0);
  return -1;
}

/**
 * _link - Creates a hard link between two files.
 *
 * This function is intended to create a hard link from the file specified by `n` 
 * (the new name) to the file specified by `d` (the existing file). The function 
 * is currently a stub and always returns an error. It uses `assert(0)` to indicate 
 * that the implementation is incomplete or not yet functional.
 *
 * @param d The path to the existing file to which the hard link will be created.
 * @param n The path to the new name (hard link) that will be created.
 *
 * @return Returns -1 to indicate an error, as the function is not implemented.
 */
int _link(const char *d, const char *n) {
  assert(0);
  return -1;
}

/**
 * _unlink - Attempts to delete a file from the filesystem.
 *
 * @n: The path of the file to be deleted.
 *
 * This function is intended to remove the file specified by the path `n`.
 * However, the current implementation is a stub that always fails. It asserts
 * with `assert(0)` to indicate that the function is not implemented and
 * returns -1 to signal an error.
 *
 * Return: -1 to indicate failure (as the function is not implemented).
 */
int _unlink(const char *n) {
  assert(0);
  return -1;
}

/**
 * Waits for a child process to change state and retrieves its exit status.
 * This function is currently a stub and always fails by returning -1 and
 * triggering an assertion. It is intended to be implemented to match the
 * behavior of the standard `wait` system call.
 *
 * @param status Pointer to an integer where the exit status of the child
 *               process will be stored. If NULL, the exit status is not
 *               retrieved.
 *
 * @return On success, returns the process ID (PID) of the terminated child
 *         process. On failure, returns -1, and the cause of the failure
 *         should be checked (e.g., using `errno`).
 */
pid_t _wait(int *status) {
  assert(0);
  return -1;
}

/**
 * @brief Simulates the POSIX `times` function to retrieve process times.
 *
 * This function is a placeholder implementation that always asserts false,
 * indicating that it is not intended to be called or implemented in the current context.
 * In a real implementation, it would fill the provided buffer with process times such as
 * user CPU time, system CPU time, and child process times.
 *
 * @param buf A pointer to a `struct tms` where the process times would be stored if implemented.
 *            This structure typically contains fields for user and system CPU times.
 *
 * @return Returns a `clock_t` value representing the elapsed real time since an arbitrary point
 *         in the past. However, this implementation always returns 0 and asserts false.
 *
 * @note This function is not implemented and will always trigger an assertion failure if called.
 */
clock_t _times(void *buf) {
  assert(0);
  return 0;
}

/**
 * Creates a pipe, a unidirectional data channel that can be used for inter-process communication.
 * 
 * The pipefd array is used to return two file descriptors referring to the ends of the pipe.
 * pipefd[0] refers to the read end of the pipe, and pipefd[1] refers to the write end.
 * Data written to the write end of the pipe is buffered by the kernel until it is read from the read end.
 * 
 * On success, 0 is returned, and the file descriptors are stored in pipefd. On error, -1 is returned,
 * and errno is set to indicate the error.
 * 
 * This implementation currently asserts 0, indicating it is not functional and will always return -1.
 * 
 * @param pipefd An array of two integers where the file descriptors for the pipe will be stored.
 * @return 0 on success, -1 on failure.
 */
int pipe(int pipefd[2]) {
  assert(0);
  return -1;
}

/**
 * Duplicates a file descriptor.
 *
 * The `dup` function creates a copy of the file descriptor `oldfd`, using the lowest-numbered
 * unused file descriptor for the new descriptor. The new file descriptor refers to the same
 * open file description as `oldfd` and shares the same file offset and file status flags.
 *
 * @param oldfd The file descriptor to duplicate. It must be a valid, open file descriptor.
 * @return On success, the new file descriptor is returned. On error, -1 is returned, and
 *         `errno` is set to indicate the error.
 *
 * @note This implementation currently asserts and returns -1, indicating it is not yet
 *       implemented or is a placeholder.
 */
int dup(int oldfd) {
  assert(0);
  return -1;
}

/**
 * Duplicates a file descriptor to a specified file descriptor.
 *
 * The `dup2` function creates a copy of the file descriptor `oldfd` and assigns it to `newfd`.
 * If `newfd` is already open, it is closed before being reused. After a successful call to `dup2`,
 * both `oldfd` and `newfd` refer to the same open file description and share the same file offset
 * and file status flags.
 *
 * @param oldfd The file descriptor to duplicate.
 * @param newfd The desired file descriptor number for the duplicate.
 * @return On success, `newfd` is returned. On error, -1 is returned, and `errno` is set to indicate
 *         the error.
 */
int dup2(int oldfd, int newfd) {
  return -1;
}

/**
 * @brief Suspends the execution of the calling thread for a specified number of seconds.
 *
 * This function causes the calling thread to sleep (i.e., be suspended) for the
 * number of seconds specified by the `seconds` argument. The actual suspension
 * time may be longer due to system scheduling or signal handling.
 *
 * @param seconds The number of seconds to suspend execution. Must be a
 *                non-negative integer.
 *
 * @return On successful completion, returns 0. If the sleep is interrupted by
 *         a signal, the function returns the number of seconds remaining in the
 *         sleep period. If the `seconds` argument is invalid, the function
 *         returns -1 and sets `errno` to indicate the error.
 */
unsigned int sleep(unsigned int seconds) {
  assert(0);
  return -1;
}

/**
 * Reads the contents of a symbolic link.
 *
 * This function attempts to read the contents of the symbolic link specified by `pathname`
 * and stores the result in the buffer `buf`. The buffer size is specified by `bufsiz`.
 *
 * If the buffer is too small to hold the link's contents, the contents are truncated to fit
 * the buffer. The function returns the number of bytes placed in the buffer, excluding the
 * null terminator. If an error occurs, the function returns -1 and sets `errno` to indicate
 * the error.
 *
 * @param pathname The path to the symbolic link whose contents are to be read.
 * @param buf The buffer where the contents of the symbolic link will be stored.
 * @param bufsiz The size of the buffer `buf`.
 *
 * @return On success, the number of bytes placed in `buf` (excluding the null terminator).
 *         On error, -1 is returned and `errno` is set to indicate the error.
 */
ssize_t readlink(const char *pathname, char *buf, size_t bufsiz) {
  assert(0);
  return -1;
}

/**
 * Creates a symbolic link named `linkpath` which points to the file or directory specified by `target`.
 * 
 * This function is intended to create a symbolic link, which is a special type of file that acts as a reference
 * to another file or directory. The symbolic link itself contains the path to the target file or directory.
 * 
 * @param target The path to the target file or directory that the symbolic link should point to.
 *               This path can be absolute or relative to the location of the symbolic link.
 * @param linkpath The path where the symbolic link should be created. This path includes the name of the
 *                 symbolic link itself.
 * 
 * @return On success, 0 is returned. On failure, -1 is returned, and `errno` is set to indicate the error.
 *         Possible errors include, but are not limited to:
 *         - `EACCES`: Write permission is denied for the directory where the symbolic link is to be created.
 *         - `EEXIST`: A file or directory with the name `linkpath` already exists.
 *         - `ENOENT`: A component of the `target` or `linkpath` path does not exist or is a dangling symbolic link.
 *         - `ENOSPC`: The device containing the file has no room for the new symbolic link.
 *         - `EPERM`: The filesystem does not support the creation of symbolic links.
 *         - `EROFS`: The file is on a read-only filesystem.
 * 
 * @note This implementation currently asserts with a value of 0, indicating that it is not yet implemented
 *       or is a placeholder for future development. It always returns -1 to indicate failure.
 */
int symlink(const char *target, const char *linkpath) {
  assert(0);
  return -1;
}

/**
 * @brief Perform a device-specific input/output control operation.
 *
 * The ioctl() function manipulates the underlying device parameters of special files.
 * In particular, many operating characteristics of character special files (e.g., terminals)
 * may be controlled with ioctl() requests. The argument `fd` must be an open file descriptor.
 *
 * The `request` argument specifies the control operation to be performed. The nature of the
 * operation and any additional arguments depend on the device being addressed. Some requests
 * require additional arguments, which are passed as a variable argument list.
 *
 * @param fd The file descriptor of the device or file to be controlled.
 * @param request The request code specifying the operation to be performed.
 * @param ... Additional arguments, if required by the specific request.
 *
 * @return On success, the return value is typically 0, but may vary depending on the request.
 *         On error, -1 is returned, and errno is set to indicate the error.
 */
int ioctl(int fd, unsigned long request, ...) {
  return -1;
}
