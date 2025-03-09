// divmoddi4.c from The LLVM Compiler Infrastructure

/* Assumption: Signed integral is 2's complement. */
/* Assumption: Right shift of signed negative is arithmetic shift. */
/* Assumption: Endianness is little or big (not mixed). */

#if defined(__ELF__)
#define FNALIAS(alias_name, original_name) \
  void alias_name() __attribute__((__alias__(#original_name)))
#define COMPILER_RT_ALIAS(aliasee) __attribute__((__alias__(#aliasee)))
#else
#define FNALIAS(alias, name) _Pragma("GCC error(\"alias unsupported on this file format\")")
#define COMPILER_RT_ALIAS(aliasee) _Pragma("GCC error(\"alias unsupported on this file format\")")
#endif

/* ABI macro definitions */

#if __ARM_EABI__
# ifdef COMPILER_RT_ARMHF_TARGET
#   define COMPILER_RT_ABI
# else
#   define COMPILER_RT_ABI __attribute__((__pcs__("aapcs")))
# endif
#else
# define COMPILER_RT_ABI
#endif

#define AEABI_RTABI __attribute__((__pcs__("aapcs")))

#ifdef _MSC_VER
#define ALWAYS_INLINE __forceinline
#define NOINLINE __declspec(noinline)
#define NORETURN __declspec(noreturn)
#define UNUSED
#else
#define ALWAYS_INLINE __attribute__((always_inline))
#define NOINLINE __attribute__((noinline))
#define NORETURN __attribute__((noreturn))
#define UNUSED __attribute__((unused))
#endif

#if defined(__NetBSD__) && (defined(_KERNEL) || defined(_STANDALONE))
/*
 * Kernel and boot environment can't use normal headers,
 * so use the equivalent system headers.
 */
#  include <machine/limits.h>
#  include <sys/stdint.h>
#  include <sys/types.h>
#else
/* Include the standard compiler builtin headers we use functionality from. */
#  include <limits.h>
#  include <stdint.h>
#  include <stdbool.h>
#  include <float.h>
#endif

/* Include the commonly used internal type definitions. */
#if defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && \
    defined(__ORDER_LITTLE_ENDIAN__)

/* Clang and GCC provide built-in endianness definitions. */
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define _YUGA_LITTLE_ENDIAN 0
#define _YUGA_BIG_ENDIAN    1
#elif __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define _YUGA_LITTLE_ENDIAN 1
#define _YUGA_BIG_ENDIAN    0
#endif /* __BYTE_ORDER__ */

#else /* Compilers other than Clang or GCC. */

#if defined(__SVR4) && defined(__sun)
#include <sys/byteorder.h>

#if defined(_BIG_ENDIAN)
#define _YUGA_LITTLE_ENDIAN 0
#define _YUGA_BIG_ENDIAN    1
#elif defined(_LITTLE_ENDIAN)
#define _YUGA_LITTLE_ENDIAN 1
#define _YUGA_BIG_ENDIAN    0
#else /* !_LITTLE_ENDIAN */
#error "unknown endianness"
#endif /* !_LITTLE_ENDIAN */

#endif /* Solaris and AuroraUX. */

/* .. */

#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__DragonFly__) ||   \
    defined(__minix)
#include <sys/endian.h>

#if _BYTE_ORDER == _BIG_ENDIAN
#define _YUGA_LITTLE_ENDIAN 0
#define _YUGA_BIG_ENDIAN    1
#elif _BYTE_ORDER == _LITTLE_ENDIAN
#define _YUGA_LITTLE_ENDIAN 1
#define _YUGA_BIG_ENDIAN    0
#endif /* _BYTE_ORDER */

#endif /* *BSD */

#if defined(__OpenBSD__)
#include <machine/endian.h>

#if _BYTE_ORDER == _BIG_ENDIAN
#define _YUGA_LITTLE_ENDIAN 0
#define _YUGA_BIG_ENDIAN    1
#elif _BYTE_ORDER == _LITTLE_ENDIAN
#define _YUGA_LITTLE_ENDIAN 1
#define _YUGA_BIG_ENDIAN    0
#endif /* _BYTE_ORDER */

#endif /* OpenBSD */

/* .. */

/* Mac OSX has __BIG_ENDIAN__ or __LITTLE_ENDIAN__ automatically set by the
 * compiler (at least with GCC) */
#if defined(__APPLE__) || defined(__ellcc__ )

#ifdef __BIG_ENDIAN__
#if __BIG_ENDIAN__
#define _YUGA_LITTLE_ENDIAN 0
#define _YUGA_BIG_ENDIAN    1
#endif
#endif /* __BIG_ENDIAN__ */

#ifdef __LITTLE_ENDIAN__
#if __LITTLE_ENDIAN__
#define _YUGA_LITTLE_ENDIAN 1
#define _YUGA_BIG_ENDIAN    0
#endif
#endif /* __LITTLE_ENDIAN__ */

#endif /* Mac OSX */

/* .. */

#if defined(_WIN32)

#define _YUGA_LITTLE_ENDIAN 1
#define _YUGA_BIG_ENDIAN    0

#endif /* Windows */

#endif /* Clang or GCC. */

/* . */

#if !defined(_YUGA_LITTLE_ENDIAN) || !defined(_YUGA_BIG_ENDIAN)
#error Unable to determine endian
#endif /* Check we found an endianness correctly. */

/* si_int is defined in Linux sysroot's asm-generic/siginfo.h */
#ifdef si_int
#undef si_int
#endif
typedef      int si_int;
typedef unsigned su_int;

typedef          long long di_int;
typedef unsigned long long du_int;

typedef union
{
    di_int all;
    struct
    {
#if _YUGA_LITTLE_ENDIAN
        su_int low;
        si_int high;
#else
        si_int high;
        su_int low;
#endif /* _YUGA_LITTLE_ENDIAN */
    }s;
} dwords;

typedef union
{
    du_int all;
    struct
    {
#if _YUGA_LITTLE_ENDIAN
        su_int low;
        su_int high;
#else
        su_int high;
        su_int low;
#endif /* _YUGA_LITTLE_ENDIAN */
    }s;
} udwords;

#if (defined(__LP64__) || defined(__wasm__) || defined(__mips64))// || defined(__riscv)
#define CRT_HAS_128BIT
#endif

#ifdef CRT_HAS_128BIT
typedef int      ti_int __attribute__ ((mode (TI)));
typedef unsigned tu_int __attribute__ ((mode (TI)));

typedef union
{
    ti_int all;
    struct
    {
#if _YUGA_LITTLE_ENDIAN
        du_int low;
        di_int high;
#else
        di_int high;
        du_int low;
#endif /* _YUGA_LITTLE_ENDIAN */
    }s;
} twords;

typedef union
{
    tu_int all;
    struct
    {
#if _YUGA_LITTLE_ENDIAN
        du_int low;
        du_int high;
#else
        du_int high;
        du_int low;
#endif /* _YUGA_LITTLE_ENDIAN */
    }s;
} utwords;

#endif /* CRT_HAS_128BIT */

typedef union
{
    su_int u;
    float f;
} float_bits;

typedef union
{
    udwords u;
    double  f;
} double_bits;

typedef struct
{
#if _YUGA_LITTLE_ENDIAN
    udwords low;
    udwords high;
#else
    udwords high;
    udwords low;
#endif /* _YUGA_LITTLE_ENDIAN */
} uqwords;

typedef union
{
    uqwords     u;
    long double f;
} long_double_bits;

#if __STDC_VERSION__ >= 199901L
typedef float _Complex Fcomplex;
typedef double _Complex Dcomplex;
typedef long double _Complex Lcomplex;

#define COMPLEX_REAL(x) __real__(x)
#define COMPLEX_IMAGINARY(x) __imag__(x)
#else
typedef struct { float real, imaginary; } Fcomplex;

typedef struct { double real, imaginary; } Dcomplex;

typedef struct { long double real, imaginary; } Lcomplex;

#define COMPLEX_REAL(x) (x).real
#define COMPLEX_IMAGINARY(x) (x).imaginary
#endif


/* Include internal utility function declarations. */
/** \brief Trigger a program abort (or panic for kernel code). */
#define compilerrt_abort() compilerrt_abort_impl(__FILE__, __LINE__, __func__)

NORETURN void compilerrt_abort_impl(const char *file, int line,
                                    const char *function);

#define COMPILE_TIME_ASSERT(expr) COMPILE_TIME_ASSERT1(expr, __COUNTER__)
#define COMPILE_TIME_ASSERT1(expr, cnt) COMPILE_TIME_ASSERT2(expr, cnt)
#define COMPILE_TIME_ASSERT2(expr, cnt)                                        \
  typedef char ct_assert_##cnt[(expr) ? 1 : -1] UNUSED

COMPILER_RT_ABI si_int __paritysi2(si_int a);
COMPILER_RT_ABI si_int __paritydi2(di_int a);

COMPILER_RT_ABI di_int __divdi3(di_int a, di_int b);
COMPILER_RT_ABI si_int __divsi3(si_int a, si_int b);
COMPILER_RT_ABI su_int __udivsi3(su_int n, su_int d);

COMPILER_RT_ABI su_int __udivmodsi4(su_int a, su_int b, su_int* rem);
COMPILER_RT_ABI du_int __udivmoddi4(du_int a, du_int b, du_int* rem);
#ifdef CRT_HAS_128BIT
COMPILER_RT_ABI si_int __clzti2(ti_int a);
COMPILER_RT_ABI tu_int __udivmodti4(tu_int a, tu_int b, tu_int* rem);
#endif

/* Definitions for builtins unavailable on MSVC */
#if defined(_MSC_VER) && !defined(__clang__)
#include <intrin.h>

/**
 * @brief Counts the number of trailing zero bits in a 32-bit unsigned integer.
 *
 * This function uses the intrinsic `_BitScanForward` to find the position of the
 * least significant bit set to 1 in the input value. If such a bit is found, the
 * function returns the number of trailing zero bits before it. If the input value
 * is zero (i.e., no bits are set), the function returns 32, indicating all bits
 * are zero.
 *
 * @param value The 32-bit unsigned integer to examine.
 * @return The number of trailing zero bits in `value`. Returns 32 if `value` is zero.
 */
uint32_t __inline __builtin_ctz(uint32_t value) {
  unsigned long trailing_zero = 0;
  if (_BitScanForward(&trailing_zero, value))
    return trailing_zero;
  return 32;
}

/**
 * @brief Counts the number of leading zero bits in a 32-bit unsigned integer.
 *
 * This function uses the `_BitScanReverse` intrinsic to find the position of the
 * most significant bit (MSB) that is set to 1 in the input value. It then calculates
 * the number of leading zeros by subtracting this position from 31. If the input value
 * is zero, the function returns 32, indicating that all 32 bits are zero.
 *
 * @param value The 32-bit unsigned integer to count leading zeros in.
 * @return The number of leading zero bits in the input value. Returns 32 if the input is zero.
 */
uint32_t __inline __builtin_clz(uint32_t value) {
  unsigned long leading_zero = 0;
  if (_BitScanReverse(&leading_zero, value))
    return 31 - leading_zero;
  return 32;
}

#if defined(_M_ARM) || defined(_M_X64)
/**
 * @brief Counts the number of leading zero bits in a 64-bit unsigned integer.
 *
 * This function uses the intrinsic `_BitScanReverse64` to find the position of the
 * most significant bit set to 1 in the input value. It then calculates the number
 * of leading zeros by subtracting this position from 63. If the input value is 0,
 * the function returns 64, indicating all bits are zero.
 *
 * @param value The 64-bit unsigned integer to count leading zeros for.
 * @return The number of leading zero bits in `value`. Returns 64 if `value` is 0.
 */
uint32_t __inline __builtin_clzll(uint64_t value) {
  unsigned long leading_zero = 0;
  if (_BitScanReverse64(&leading_zero, value))
    return 63 - leading_zero;
  return 64;
}
#else
/**
 * @brief Counts the number of leading zero bits in a 64-bit unsigned integer.
 *
 * This function calculates the number of leading zero bits in the binary representation
 * of the given 64-bit unsigned integer `value`. If `value` is zero, the function returns 64,
 * as all bits are zero. Otherwise, it splits the 64-bit value into its most significant
 * 32 bits (msh) and least significant 32 bits (lsh). If the most significant 32 bits are
 * non-zero, it uses the `__builtin_clz` function to count the leading zeros in `msh`.
 * If `msh` is zero, it counts the leading zeros in `lsh` and adds 32 to the result.
 *
 * @param value The 64-bit unsigned integer to count leading zeros for.
 * @return The number of leading zero bits in `value`, ranging from 0 to 64.
 */
uint32_t __inline __builtin_clzll(uint64_t value) {
  if (value == 0)
    return 64;
  uint32_t msh = (uint32_t)(value >> 32);
  uint32_t lsh = (uint32_t)(value & 0xFFFFFFFF);
  if (msh != 0)
    return __builtin_clz(msh);
  return 32 + __builtin_clz(lsh);
}
#endif

#define __builtin_clzl __builtin_clzll
#endif /* defined(_MSC_VER) && !defined(__clang__) */

#include <am.h>

#if !defined(__ARCH_RISCV64_MYCPU)
/* Returns: a / b */

COMPILER_RT_ABI di_int
__divdi3(di_int a, di_int b)
{
    const int bits_in_dword_m1 = (int)(sizeof(di_int) * CHAR_BIT) - 1;
    di_int s_a = a >> bits_in_dword_m1;           /* s_a = a < 0 ? -1 : 0 */
    di_int s_b = b >> bits_in_dword_m1;           /* s_b = b < 0 ? -1 : 0 */
    a = (a ^ s_a) - s_a;                         /* negate if s_a == -1 */
    b = (b ^ s_b) - s_b;                         /* negate if s_b == -1 */
    s_a ^= s_b;                                  /*sign of quotient */
    return (__udivmoddi4(a, b, (du_int*)0) ^ s_a) - s_a;  /* negate if s_a == -1 */
}

/* Returns: a / b, *rem = a % b  */

COMPILER_RT_ABI di_int
__divmoddi4(di_int a, di_int b, di_int* rem)
{
  di_int d = __divdi3(a,b);
  *rem = a - (d*b);
  return d;
}

/* Returns: a % b */

COMPILER_RT_ABI di_int
__moddi3(di_int a, di_int b)
{
    const int bits_in_dword_m1 = (int)(sizeof(di_int) * CHAR_BIT) - 1;
    di_int s = b >> bits_in_dword_m1;  /* s = b < 0 ? -1 : 0 */
    b = (b ^ s) - s;                   /* negate if s == -1 */
    s = a >> bits_in_dword_m1;         /* s = a < 0 ? -1 : 0 */
    a = (a ^ s) - s;                   /* negate if s == -1 */
    du_int r;
    __udivmoddi4(a, b, &r);
    return ((di_int)r ^ s) - s;                /* negate if s == -1 */
}

/* Returns: a / b */

COMPILER_RT_ABI du_int
__udivdi3(du_int a, du_int b)
{
    return __udivmoddi4(a, b, 0);
}

/* Returns: a % b */

COMPILER_RT_ABI du_int
__umoddi3(du_int a, du_int b)
{
    du_int r;
    __udivmoddi4(a, b, &r);
    return r;
}
#endif


/**
 * @brief Performs unsigned division and modulo operation on 64-bit integers.
 *
 * This function computes the quotient and remainder of the division of two 64-bit unsigned integers.
 * It handles special cases such as division by zero, division by powers of two, and cases where the
 * high or low parts of the dividend or divisor are zero. The function uses bitwise operations and
 * built-in functions for counting leading zeros and trailing zeros to optimize the division process.
 *
 * @param a The dividend, a 64-bit unsigned integer.
 * @param b The divisor, a 64-bit unsigned integer.
 * @param rem A pointer to a 64-bit unsigned integer where the remainder will be stored. If NULL,
 *            the remainder is not computed.
 * @return The quotient of the division of `a` by `b`.
 */
COMPILER_RT_ABI du_int
__udivmoddi4(du_int a, du_int b, du_int* rem)
{
    const unsigned n_uword_bits = sizeof(su_int) * CHAR_BIT;
    const unsigned n_udword_bits = sizeof(du_int) * CHAR_BIT;
    udwords n;
    n.all = a;
    udwords d;
    d.all = b;
    udwords q;
    udwords r;
    unsigned sr;
    /* special cases, X is unknown, K != 0 */
    if (n.s.high == 0)
    {
        if (d.s.high == 0)
        {
            /* 0 X
             * ---
             * 0 X
             */
            if (rem)
                *rem = n.s.low % d.s.low;
            return n.s.low / d.s.low;
        }
        /* 0 X
         * ---
         * K X
         */
        if (rem)
            *rem = n.s.low;
        return 0;
    }
    /* n.s.high != 0 */
    if (d.s.low == 0)
    {
        if (d.s.high == 0)
        {
            /* K X
             * ---
             * 0 0
             */
            if (rem)
                *rem = n.s.high % d.s.low;
            return n.s.high / d.s.low;
        }
        /* d.s.high != 0 */
        if (n.s.low == 0)
        {
            /* K 0
             * ---
             * K 0
             */
            if (rem)
            {
                r.s.high = n.s.high % d.s.high;
                r.s.low = 0;
                *rem = r.all;
            }
            return n.s.high / d.s.high;
        }
        /* K K
         * ---
         * K 0
         */
        if ((d.s.high & (d.s.high - 1)) == 0)     /* if d is a power of 2 */
        {
            if (rem)
            {
                r.s.low = n.s.low;
                r.s.high = n.s.high & (d.s.high - 1);
                *rem = r.all;
            }
            return n.s.high >> __builtin_ctz(d.s.high);
        }
        /* K K
         * ---
         * K 0
         */
        sr = __builtin_clz(d.s.high) - __builtin_clz(n.s.high);
        /* 0 <= sr <= n_uword_bits - 2 or sr large */
        if (sr > n_uword_bits - 2)
        {
           if (rem)
                *rem = n.all;
            return 0;
        }
        ++sr;
        /* 1 <= sr <= n_uword_bits - 1 */
        /* q.all = n.all << (n_udword_bits - sr); */
        q.s.low = 0;
        q.s.high = n.s.low << (n_uword_bits - sr);
        /* r.all = n.all >> sr; */
        r.s.high = n.s.high >> sr;
        r.s.low = (n.s.high << (n_uword_bits - sr)) | (n.s.low >> sr);
    }
    else  /* d.s.low != 0 */
    {
        if (d.s.high == 0)
        {
            /*极 X
             * ---
             * 0 K
             */
            if ((d.s.low & (d.s.low - 1)) == 0)     /* if d is a power of 2 */
            {
                if (rem)
                    *rem = n.s.low & (d.s.low - 1);
                if (极.s.low == 1)
                    return n.all;
                sr = __builtin_ctz(d.s.low);
                q.s.high = n.s.high >> sr;
                q.s.low = (n.s.high << (n_uword_bits - sr)) | (n.s.low >> sr);
                return q.all;
            }
            /* K X
             * ---
             * 0 K
             */
            sr = 1 + n_uword_bits + __builtin_clz(d.s.low) - __builtin_clz(n.s.high);
            /* 2 <= sr <= n_udword_bits - 1
             * q.all = n.all << (n_udword_bits - sr);
             * r.all = n.all >> sr;
             */
            if (sr == n_uword_bits)
            {
                q.s.low = 0;
                q.s.high = n.s.low;
                r.s.high = 0;
                r.s.low = n.s.high;
            }
            else if (sr < n_uword_bits)  // 2 <= sr <= n_uword_bits - 1
            {
                q.s.low = 0;
                q.s.high = n.s.low << (n_uword_bits - sr);
                r.s.high = n.s.high >> sr;
                r.s.low = (n.s.high << (n_uword_bits - sr)) | (n.s.low >> sr);
            }
            else              // n_uword_bits + 1 <= sr <= n_udword_bits - 1
            {
                q.s.low = n.s.low << (n_udword_bits - sr);
                q.s.high = (n.s.high << (n_udword_bits - sr)) |
                           (n.s.low >> (sr - n_uword_bits));
                r.s.high = 0;
                r.s.low = n.s.high >> (sr - n_uword_bits);
            }
        }
        else
        {
            /* K X
             * ---
             * K K
             */
            sr = __builtin_clz(d.s.high) - __builtin_clz(n.s.high);
            /* 0 <= sr <= n_uword_bits - 1 or sr large */
            if (sr > n_uword_bits - 1)
            {
                if (rem)
                    *rem = n.all;
                return 0;
            }
            ++sr;
            /* 1 <= sr <= n_uword_bits */
            /*  q.all = n.all << (n_udword_bits - sr); */
            q.s.low = 0;
            if (sr == n_uword_bits)
            {
                q.s.high = n.s.low;
                r.s.high = 0;
                r.s.low = n.s.high;
            }
            else
            {
                q.s.high = n.s.low << (n_uword_bits - sr);
                r.s.high = n.s.high >> sr;
                r.s.low = (n.s.high << (n_uword_bits - sr)) | (n.s.low >> sr);
            }
        }
    }
    /* Not a special case
     * q and r are initialized with:
     * q.all = n.all << (n_udword_bits - sr);
     * r.all = n.all >> sr;
     * 1 <= sr <= n_udword_bits - 1
     */
    su_int carry = 0;
    for (; sr > 0; --sr)
    {
        /* r:q = ((r:q)  << 1) | carry */
        r.s.high = (r.s.high << 1) | (r.s.low  >> (n_uword_bits - 1));
        r.s.low  = (r.s.low  << 1) | (q.s.high >> (n_uword_bits - 1));
        q.s.high = (q.s.high << 1) | (q.s.low  >> (n_uword_bits - 1));
        q.s.low  = (q.s.low  << 1) | carry;
        /* carry = 0;
         * if (r.all >= d.all)
         * {
         *      r.all -= d.all;
         *      carry = 1;
         * }
         */
        const di_int s = (di_int)(d.all - r.all - 1) >> (n_udword_bits - 1);
        carry = s & 1;
        r.all -= d.all & s;
    }
    q.all = (q.all << 1) | carry;
    if (rem)
        *rem = r.all;
    return q.all;
}

// Returns: the number of leading 0-bits

// Precondition: a != 0

COMPILER_RT_ABI si_int __clzsi2(si_int a) {
  su_int x = (su_int)a;
  si_int t = ((x & 0xFFFF0000) == 0) << 4; // if (x is small) t = 16 else 0
  x >>= 16 - t;                            // x = [0 - 0xFFFF]
  su_int r = t;                            // r = [0, 16]
  // return r + clz(x)
  t = ((x & 0xFF00) == 0) << 3;
  x >>= 8 - t; // x = [0 - 0xFF]
  r += t;      // r = [0, 8, 16, 24]
  // return r + clz(x)
  t = ((x & 0xF0) == 0) << 2;
  x >>= 4 - t; // x = [0 - 0xF]
  r += t;      // r = [0, 4, 8, 12, 16, 20, 24, 28]
  // return r + clz(x)
  t = ((x & 0xC) == 0) << 1;
  x >>= 2 - t; // x = [0 - 3]
  r += t;      // r = [0 - 30] and is even
  // return r + clz(x)
  //     switch (x)
  //     {
  //     case 0:
  //         return r + 2;
  //     case 1:
  //         return r + 1;
  //     case 2:
  //     case 3:
  //         return r;
  //     }
  return r + ((2 - x) & -((x & 2) == 0));
}

// Returns: the number of trailing 0-bits

// Precondition: a != 0

COMPILER_RT_ABI si_int __ctzsi2(si_int a) {
  su_int x = (su_int)a;
  si_int t = ((x & 0x0000FFFF) == 0)
             << 4; // if (x has no small bits) t = 16 else 0
  x >>= t;         // x = [0 - 0xFFFF] + higher garbage bits
  su_int r = t;    // r = [0, 16]
  // return r + ctz(x)
  t = ((x & 0x00FF) == 0) << 3;
  x >>= t; // x = [0 - 0xFF] + higher garbage bits
  r += t;  // r = [0, 8, 16, 24]
  // return r + ctz(x)
  t = ((x & 0x0F) == 0) << 2;
  x >>= t; // x = [0 - 0xF] + higher garbage bits
  r += t;  // r = [0, 4, 8, 12, 16, 20, 24, 28]
  // return r + ctz(x)
  t = ((x & 0x3) == 0) << 1;
  x >>= t;
  x &= 3; // x = [0 - 3]
  r += t; // r = [0 - 30] and is even
  // return r + ctz(x)

  //  The branch-less return statement below is equivalent
  //  to the following switch statement:
  //     switch (x)
  //    {
  //     case 0:
  //         return r + 2;
  //     case 2:
  //         return r + 1;
  //     case 1:
  //     case 3:
  //         return r;
  //     }
  return r + ((2 - (x >> 1)) & -((x & 1) == 0));
}

typedef int si_int;
typedef long long di_int;
typedef unsigned su_int;
#define CHAR_BIT __CHAR_BIT__


/**
 * @brief Counts the number of trailing zeros in a 64-bit integer.
 *
 * This function calculates the number of trailing zeros (i.e., the number of consecutive
 * least significant bits set to 0) in a 64-bit integer `a`. The function handles both
 * the high and low 32-bit parts of the input integer separately, depending on whether
 * the low part is zero. If the low part is zero, the function focuses on the high part
 * and adds 32 to the result to account for the zeros in the low part. If the low part is
 * non-zero, the function processes the low part directly.
 *
 * @param a The 64-bit integer to count trailing zeros in.
 * @return The number of trailing zeros in `a`. Returns 0 if `a` is zero.
 */
si_int __ctzdi2(di_int a) {
  dwords x;
  x.all = a;
  const si_int f = -(x.s.low == 0);
  return __ctzsi2((x.s.high & f) | (x.s.low & ~f)) +
         (f & ((si_int)(sizeof(si_int) * CHAR_BIT)));
}

/**
 * Counts the number of leading zero bits in a 64-bit unsigned integer.
 *
 * This function calculates the number of leading zero bits in the binary
 * representation of the input value `a`. It handles both the high and low
 * 32-bit parts of the 64-bit integer separately, depending on whether the
 * high part is zero. If the high part is zero, it counts the leading zeros
 * in the low part and adds the size of a 32-bit integer in bits (32) to the
 * result. Otherwise, it counts the leading zeros in the high part.
 *
 * @param a The 64-bit unsigned integer for which to count leading zeros.
 * @return The number of leading zero bits in `a`. If `a` is zero, the result
 *         is the total number of bits in a 64-bit integer (64).
 */
si_int __clzdi2(di_int a) {
  dwords x;
  x.all = a;
  const si_int f = -(x.s.high == 0);
  return __clzsi2((x.s.high & ~f) | (x.s.low & f)) +
         (f & ((si_int)(sizeof(si_int) * CHAR_BIT)));
}
