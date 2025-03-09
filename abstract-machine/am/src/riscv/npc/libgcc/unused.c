#include <klib-macros.h>
#include <am.h>

/**
 * __muldf3 - Multiplies two double-precision floating-point numbers.
 *
 * This function is intended to perform the multiplication of two double-precision
 * floating-point values (64-bit IEEE 754 format). It is typically used in
 * environments where floating-point arithmetic is not natively supported or
 * requires software emulation.
 *
 * @param a The first double-precision floating-point operand.
 * @param b The second double-precision floating-point operand.
 * @return The product of the two operands as a double-precision floating-point value.
 *
 * @note This implementation currently raises a panic with the message "Not implement"
 *       indicating that the function is not yet implemented. Proper implementation
 *       should handle the multiplication of the two double-precision values, including
 *       special cases such as infinities, NaNs, and denormals.
 */
double __muldf3 (double a, double b) { panic("Not implement"); }
/**
 * Converts a double-precision floating-point number to a signed 64-bit integer.
 * This function is intended to handle the conversion by truncating the fractional
 * part of the floating-point number and returning the resulting integer.
 * 
 * @param a The double-precision floating-point number to be converted.
 * @return The signed 64-bit integer representation of the truncated value.
 * @note This function is not implemented and will trigger a panic if called.
 */
long __fixdfdi (double a) { panic("Not implement"); }
