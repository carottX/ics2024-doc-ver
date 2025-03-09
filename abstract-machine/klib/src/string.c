#include <klib.h>
#include <klib-macros.h>
#include <stdint.h>

#if !defined(__ISA_NATIVE__) || defined(__NATIVE_USE_KLIB__)

/**
 * Calculates the length of a null-terminated string.
 *
 * This function takes a pointer to a null-terminated string and returns the
 * number of characters in the string, excluding the null terminator.
 *
 * @param s A pointer to the null-terminated string whose length is to be calculated.
 * @return The length of the string as a value of type size_t.
 *
 * @note This function is not yet implemented and will panic if called.
 */
size_t strlen(const char *s) {
  panic("Not implemented");
}

/**
 * Copies the string pointed to by `src` (including the null-terminator) to the
 * buffer pointed to by `dst`. The destination buffer must be large enough to
 * hold the entire string, including the null-terminator, to avoid buffer
 * overflow. The behavior is undefined if the source and destination buffers
 * overlap.
 *
 * @param dst Pointer to the destination buffer where the string is to be copied.
 * @param src Pointer to the null-terminated string to be copied.
 * @return A pointer to the destination buffer `dst`.
 */
char *strcpy(char *dst, const char *src) {
  panic("Not implemented");
}

/**
 * Copies up to `n` characters from the string pointed to by `src` to the buffer
 * pointed to by `dst`. If the length of `src` is less than `n`, the remaining
 * characters in `dst` are filled with null bytes. If the length of `src` is
 * greater than or equal to `n`, the string in `dst` will not be null-terminated.
 *
 * @param dst Pointer to the destination buffer where the content is to be copied.
 * @param src Pointer to the null-terminated string to be copied.
 * @param n Maximum number of characters to copy from `src`.
 * @return A pointer to the destination buffer `dst`.
 *
 * @note If `src` and `dst` overlap, the behavior is undefined.
 * @warning This function does not check for buffer overflows in `dst`.
 */
char *strncpy(char *dst, const char *src, size_t n) {
  panic("Not implemented");
}

/**
 * Concatenates the source string `src` to the end of the destination string `dst`.
 * The destination string must have enough space to hold the result, including the
 * null-terminator. The source string is not modified. The behavior is undefined
 * if the destination buffer is not large enough to accommodate the concatenated
 * result.
 *
 * @param dst Pointer to the destination string, which must be null-terminated.
 * @param src Pointer to the source string, which must be null-terminated.
 * @return A pointer to the destination string `dst` after concatenation.
 *
 * @note This function does not allocate memory. The caller is responsible for
 * ensuring that the destination buffer is large enough to hold the concatenated
 * result.
 * @warning If the destination buffer is too small, this function may cause
 * buffer overflow, leading to undefined behavior.
 */
char *strcat(char *dst, const char *src) {
  panic("Not implemented");
}

/**
 * Compares two null-terminated strings lexicographically.
 *
 * This function compares the string pointed to by `s1` to the string pointed to by `s2`.
 * The comparison is done on a character-by-character basis, using the ASCII values of the characters.
 *
 * @param s1 Pointer to the first null-terminated string to compare.
 * @param s2 Pointer to the second null-terminated string to compare.
 * @return An integer less than, equal to, or greater than zero if `s1` is found,
 *         respectively, to be less than, to match, or be greater than `s2`.
 *         Specifically:
 *         - A negative value if `s1` is less than `s2`.
 *         - Zero if `s1` is equal to `s2`.
 *         - A positive value if `s1` is greater than `s2`.
 */
int strcmp(const char *s1, const char *s2) {
  panic("Not implemented");
}

/**
 * Compares up to the first `n` bytes of two strings.
 * 
 * This function compares the first `n` bytes of the strings `s1` and `s2`. 
 * It returns an integer less than, equal to, or greater than zero if `s1` 
 * is found, respectively, to be less than, to match, or be greater than `s2`.
 * The comparison is done lexicographically based on the ASCII values of the characters.
 * 
 * @param s1 Pointer to the first string to compare.
 * @param s2 Pointer to the second string to compare.
 * @param n Maximum number of bytes to compare.
 * @return A negative integer if `s1` is less than `s2`, 
 *         zero if `s1` is equal to `s2`, 
 *         or a positive integer if `s1` is greater than `s2`.
 */
int strncmp(const char *s1, const char *s2, size_t n) {
  panic("Not implemented");
}

/**
 * Fills the first n bytes of the memory area pointed to by s with the constant byte c.
 * 
 * This function is typically used to initialize a block of memory to a specific value.
 * It is commonly employed to set a region of memory to zero or another constant value.
 * 
 * @param s Pointer to the memory area to be filled.
 * @param c The value to be set. The value is passed as an int, but the function fills
 *          the block of memory using the unsigned char conversion of this value.
 * @param n Number of bytes to be set to the value c.
 * 
 * @return A pointer to the memory area s.
 * 
 * @note This implementation currently panics with the message "Not implemented" as it
 *       is a placeholder and not yet fully implemented.
 */
void *memset(void *s, int c, size_t n) {
  panic("Not implemented");
}

/**
 * Copies `n` bytes from the memory area pointed to by `src` to the memory area
 * pointed to by `dst`. The memory areas may overlap; the copy is performed in
 * a non-destructive manner, ensuring that the original data in `src` is not
 * corrupted before it is copied. This function is useful for safely copying
 * data between overlapping memory regions.
 *
 * @param dst Pointer to the destination memory area where the content is to be copied.
 * @param src Pointer to the source memory area from which the data is to be copied.
 * @param n   Number of bytes to copy.
 * @return    Returns a pointer to the destination memory area (`dst`).
 */
void *memmove(void *dst, const void *src, size_t n) {
  panic("Not implemented");
}

/**
 * Copies `n` bytes from the memory area pointed to by `in` to the memory area
 * pointed to by `out`. The memory areas must not overlap; if they do, the
 * behavior is undefined. Use `memmove` for overlapping memory areas.
 *
 * @param out Pointer to the destination memory area where the content is to be copied.
 * @param in Pointer to the source memory area from which the data is to be copied.
 * @param n Number of bytes to copy.
 * @return Returns a pointer to the destination memory area `out`.
 *
 * @note This implementation currently panics with the message "Not implemented".
 *       It is intended to be replaced with a functional implementation.
 */
void *memcpy(void *out, const void *in, size_t n) {
  panic("Not implemented");
}

/**
 * Compares the first `n` bytes of the memory areas pointed to by `s1` and `s2`.
 * 
 * This function performs a byte-by-byte comparison of the memory blocks `s1` and `s2`
 * up to `n` bytes. It returns an integer less than, equal to, or greater than zero
 * if the first `n` bytes of `s1` are found, respectively, to be less than, to match,
 * or be greater than the first `n` bytes of `s2`.
 * 
 * @param s1 Pointer to the first memory block to compare.
 * @param s2 Pointer to the second memory block to compare.
 * @param n Number of bytes to compare.
 * 
 * @return An integer less than, equal to, or greater than zero if the first `n` bytes
 *         of `s1` are found, respectively, to be less than, to match, or be greater
 *         than the first `n` bytes of `s2`.
 */
int memcmp(const void *s1, const void *s2, size_t n) {
  panic("Not implemented");
}

#endif
