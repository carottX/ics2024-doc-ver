#define LIBGCC2_UNITS_PER_WORD (__riscv_xlen / 8)
#include "libgcc2.h"

/**
 * Performs an arithmetic left shift operation on a double-width integer.
 *
 * This function shifts the double-width integer `u` left by `b` bits. If `b` is zero,
 * the function returns `u` unchanged. The function handles cases where the shift count
 * `b` is greater than or equal to the word size (W_TYPE_SIZE) by appropriately shifting
 * the high and low parts of the double-width integer.
 *
 * @param u The double-width integer to be shifted.
 * @param b The number of bits to shift `u` left. Must be a non-negative value.
 *
 * @return The result of shifting `u` left by `b` bits. If `b` is zero, returns `u`.
 *
 * @note The function assumes that `DWtype` is a double-width integer type, `UWtype` is
 *       an unsigned word-sized integer type, and `DWunion` is a union that allows access
 *       to the double-width integer as two word-sized parts (high and low).
 */
DWtype __ashldi3 (DWtype u, shift_count_type b)
{
  if (b == 0)
    return u;

  const DWunion uu = {.ll = u};
  const shift_count_type bm = W_TYPE_SIZE - b;
  DWunion w;

  if (bm <= 0)
    {
      w.s.low = 0;
      w.s.high = (UWtype) uu.s.low << -bm;
    }
  else
    {
      const UWtype carries = (UWtype) uu.s.low >> bm;

      w.s.low = (UWtype) uu.s.low << b;
      w.s.high = ((UWtype) uu.s.high << b) | carries;
    }

  return w.ll;
}
