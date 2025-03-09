#include <vorbis.h>
#include <fixedptc.h>
// global configuration settings (e.g. set these in the project/makefile),
// or just set them in this file at the top (although ideally the first few
// should be visible when the header file is compiled too, although it's not
// crucial)

// STB_VORBIS_NO_PUSHDATA_API
//     does not compile the code for the various stb_vorbis_*_pushdata()
//     functions
// #define STB_VORBIS_NO_PUSHDATA_API

// STB_VORBIS_NO_PULLDATA_API
//     does not compile the code for the non-pushdata APIs
// #define STB_VORBIS_NO_PULLDATA_API

// STB_VORBIS_NO_STDIO
//     does not compile the code for the APIs that use FILE *s internally
//     or externally (implied by STB_VORBIS_NO_PULLDATA_API)
// #define STB_VORBIS_NO_STDIO

// STB_VORBIS_NO_INTEGER_CONVERSION
//     does not compile the code for converting audio sample data from
//     float to integer (implied by STB_VORBIS_NO_PULLDATA_API)
// #define STB_VORBIS_NO_INTEGER_CONVERSION

// STB_VORBIS_NO_FAST_SCALED_FLOAT
//      does not use a fast float-to-int trick to accelerate float-to-int on
//      most platforms which requires endianness be defined correctly.
//#define STB_VORBIS_NO_FAST_SCALED_FLOAT


// STB_VORBIS_MAX_CHANNELS [number]
//     globally define this to the maximum number of channels you need.
//     The spec does not put a restriction on channels except that
//     the count is stored in a byte, so 255 is the hard limit.
//     Reducing this saves about 16 bytes per value, so using 16 saves
//     (255-16)*16 or around 4KB. Plus anything other memory usage
//     I forgot to account for. Can probably go as low as 8 (7.1 audio),
//     6 (5.1 audio), or 2 (stereo only).
#ifndef STB_VORBIS_MAX_CHANNELS
#define STB_VORBIS_MAX_CHANNELS    16  // enough for anyone?
#endif

// STB_VORBIS_PUSHDATA_CRC_COUNT [number]
//     after a flush_pushdata(), stb_vorbis begins scanning for the
//     next valid page, without backtracking. when it finds something
//     that looks like a page, it streams through it and verifies its
//     CRC32. Should that validation fail, it keeps scanning. But it's
//     possible that _while_ streaming through to check the CRC32 of
//     one candidate page, it sees another candidate page. This #define
//     determines how many "overlapping" candidate pages it can search
//     at once. Note that "real" pages are typically ~4KB to ~8KB, whereas
//     garbage pages could be as big as 64KB, but probably average ~16KB.
//     So don't hose ourselves by scanning an apparent 64KB page and
//     missing a ton of real ones in the interim; so minimum of 2
#ifndef STB_VORBIS_PUSHDATA_CRC_COUNT
#define STB_VORBIS_PUSHDATA_CRC_COUNT  4
#endif

// STB_VORBIS_FAST_HUFFMAN_LENGTH [number]
//     sets the log size of the huffman-acceleration table.  Maximum
//     supported value is 24. with larger numbers, more decodings are O(1),
//     but the table size is larger so worse cache missing, so you'll have
//     to probe (and try multiple ogg vorbis files) to find the sweet spot.
#ifndef STB_VORBIS_FAST_HUFFMAN_LENGTH
#define STB_VORBIS_FAST_HUFFMAN_LENGTH   10
#endif

// STB_VORBIS_FAST_BINARY_LENGTH [number]
//     sets the log size of the binary-search acceleration table. this
//     is used in similar fashion to the fast-huffman size to set initial
//     parameters for the binary search

// STB_VORBIS_FAST_HUFFMAN_INT
//     The fast huffman tables are much more efficient if they can be
//     stored as 16-bit results instead of 32-bit results. This restricts
//     the codebooks to having only 65535 possible outcomes, though.
//     (At least, accelerated by the huffman table.)
#ifndef STB_VORBIS_FAST_HUFFMAN_INT
#define STB_VORBIS_FAST_HUFFMAN_SHORT
#endif

// STB_VORBIS_NO_HUFFMAN_BINARY_SEARCH
//     If the 'fast huffman' search doesn't succeed, then stb_vorbis falls
//     back on binary searching for the correct one. This requires storing
//     extra tables with the huffman codes in sorted order. Defining this
//     symbol trades off space for speed by forcing a linear search in the
//     non-fast case, except for "sparse" codebooks.
// #define STB_VORBIS_NO_HUFFMAN_BINARY_SEARCH

// STB_VORBIS_DIVIDES_IN_RESIDUE
//     stb_vorbis precomputes the result of the scalar residue decoding
//     that would otherwise require a divide per chunk. you can trade off
//     space for time by defining this symbol.
// #define STB_VORBIS_DIVIDES_IN_RESIDUE

// STB_VORBIS_DIVIDES_IN_CODEBOOK
//     vorbis VQ codebooks can be encoded two ways: with every case explicitly
//     stored, or with all elements being chosen from a small range of values,
//     and all values possible in all elements. By default, stb_vorbis expands
//     this latter kind out to look like the former kind for ease of decoding,
//     because otherwise an integer divide-per-vector-element is required to
//     unpack the index. If you define STB_VORBIS_DIVIDES_IN_CODEBOOK, you can
//     trade off storage for speed.
//#define STB_VORBIS_DIVIDES_IN_CODEBOOK

#ifdef STB_VORBIS_CODEBOOK_SHORTS
#error "STB_VORBIS_CODEBOOK_SHORTS is no longer supported as it produced incorrect results for some input formats"
#endif

// STB_VORBIS_DIVIDE_TABLE
//     this replaces small integer divides in the floor decode loop with
//     table lookups. made less than 1% difference, so disabled by default.

// STB_VORBIS_NO_INLINE_DECODE
//     disables the inlining of the scalar codebook fast-huffman decode.
//     might save a little codespace; useful for debugging
// #define STB_VORBIS_NO_INLINE_DECODE

// STB_VORBIS_NO_DEFER_FLOOR
//     Normally we only decode the floor without synthesizing the actual
//     full curve. We can instead synthesize the curve immediately. This
//     requires more memory and is very likely slower, so I don't think
//     you'd ever want to do it except for debugging.
// #define STB_VORBIS_NO_DEFER_FLOOR




//////////////////////////////////////////////////////////////////////////////

#ifdef STB_VORBIS_NO_PULLDATA_API
   #define STB_VORBIS_NO_INTEGER_CONVERSION
   #define STB_VORBIS_NO_STDIO
#endif

#if defined(STB_VORBIS_NO_CRT) && !defined(STB_VORBIS_NO_STDIO)
   #define STB_VORBIS_NO_STDIO 1
#endif

#ifndef STB_VORBIS_NO_INTEGER_CONVERSION
#ifndef STB_VORBIS_NO_FAST_SCALED_FLOAT

   // only need endianness for fast-float-to-int, which we don't
   // use for pushdata

   #ifndef STB_VORBIS_BIG_ENDIAN
     #define STB_VORBIS_ENDIAN  0
   #else
     #define STB_VORBIS_ENDIAN  1
   #endif

#endif
#endif


#ifndef STB_VORBIS_NO_STDIO
#include <stdio.h>
#endif

#ifndef STB_VORBIS_NO_CRT
   #include <stdlib.h>
   #include <string.h>
   #include <assert.h>
   #include <math.h>

   // find definition of alloca if it's not in stdlib.h:
   #if defined(_MSC_VER) || defined(__MINGW32__)
      #include <malloc.h>
   #endif
   #if defined(__linux__) || defined(__linux) || defined(__EMSCRIPTEN__) || defined(__NEWLIB__)
      #include <alloca.h>
   #endif
#else // STB_VORBIS_NO_CRT
   #define NULL 0
   #define malloc(s)   0
   #define free(s)     ((void) 0)
   #define realloc(s)  0
#endif // STB_VORBIS_NO_CRT

#include <limits.h>

#ifdef __MINGW32__
   // eff you mingw:
   //     "fixed":
   //         http://sourceforge.net/p/mingw-w64/mailman/message/32882927/
   //     "no that broke the build, reverted, who cares about C":
   //         http://sourceforge.net/p/mingw-w64/mailman/message/32890381/
   #ifdef __forceinline
   #undef __forceinline
   #endif
   #define __forceinline
   #ifndef alloca
   #define alloca __builtin_alloca
   #endif
#elif !defined(_MSC_VER)
   #if __GNUC__
      #define __forceinline inline
   #else
      #define __forceinline
   #endif
#endif

#if STB_VORBIS_MAX_CHANNELS > 256
#error "Value of STB_VORBIS_MAX_CHANNELS outside of allowed range"
#endif

#if STB_VORBIS_FAST_HUFFMAN_LENGTH > 24
#error "Value of STB_VORBIS_FAST_HUFFMAN_LENGTH outside of allowed range"
#endif


#if 0
#include <crtdbg.h>
#define CHECK(f)   _CrtIsValidHeapPointer(f->channel_buffers[1])
#else
#define CHECK(f)   ((void) 0)
#endif

#define MAX_BLOCKSIZE_LOG  13   // from specification
#define MAX_BLOCKSIZE      (1 << MAX_BLOCKSIZE_LOG)


typedef unsigned char  uint8;
typedef   signed char   int8;
typedef unsigned short uint16;
typedef   signed short  int16;
typedef unsigned int   uint32;
typedef   signed int    int32;

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

typedef fixedpt codetype;

// @NOTE
//
// Some arrays below are tagged "//varies", which means it's actually
// a variable-sized piece of data, but rather than malloc I assume it's
// small enough it's better to just allocate it all together with the
// main thing
//
// Most of the variables are specified with the smallest size I could pack
// them into. It might give better performance to make them all full-sized
// integers. It should be safe to freely rearrange the structures or change
// the sizes larger--nothing relies on silently truncating etc., nor the
// order of variables.

#define FAST_HUFFMAN_TABLE_SIZE   (1 << STB_VORBIS_FAST_HUFFMAN_LENGTH)
#define FAST_HUFFMAN_TABLE_MASK   (FAST_HUFFMAN_TABLE_SIZE - 1)

typedef struct
{
   int dimensions, entries;
   uint8 *codeword_lengths;
   fixedpt  minimum_value;
   fixedpt  delta_value;
   uint8  value_bits;
   uint8  lookup_type;
   uint8  sequence_p;
   uint8  sparse;
   uint32 lookup_values;
   codetype *multiplicands;
   uint32 *codewords;
   #ifdef STB_VORBIS_FAST_HUFFMAN_SHORT
    int16  fast_huffman[FAST_HUFFMAN_TABLE_SIZE];
   #else
    int32  fast_huffman[FAST_HUFFMAN_TABLE_SIZE];
   #endif
   uint32 *sorted_codewords;
   int    *sorted_values;
   int     sorted_entries;
} Codebook;

typedef struct
{
   uint8 order;
   uint16 rate;
   uint16 bark_map_size;
   uint8 amplitude_bits;
   uint8 amplitude_offset;
   uint8 number_of_books;
   uint8 book_list[16]; // varies
} Floor0;

typedef struct
{
   uint8 partitions;
   uint8 partition_class_list[32]; // varies
   uint8 class_dimensions[16]; // varies
   uint8 class_subclasses[16]; // varies
   uint8 class_masterbooks[16]; // varies
   int16 subclass_books[16][8]; // varies
   uint16 Xlist[31*8+2]; // varies
   uint8 sorted_order[31*8+2];
   uint8 neighbors[31*8+2][2];
   uint8 floor1_multiplier;
   uint8 rangebits;
   int values;
} Floor1;

typedef union
{
   Floor0 floor0;
   Floor1 floor1;
} Floor;

typedef struct
{
   uint32 begin, end;
   uint32 part_size;
   uint8 classifications;
   uint8 classbook;
   uint8 **classdata;
   int16 (*residue_books)[8];
} Residue;

typedef struct
{
   uint8 magnitude;
   uint8 angle;
   uint8 mux;
} MappingChannel;

typedef struct
{
   uint16 coupling_steps;
   MappingChannel *chan;
   uint8  submaps;
   uint8  submap_floor[15]; // varies
   uint8  submap_residue[15]; // varies
} Mapping;

typedef struct
{
   uint8 blockflag;
   uint8 mapping;
   uint16 windowtype;
   uint16 transformtype;
} Mode;

typedef struct
{
   uint32  goal_crc;    // expected crc if match
   int     bytes_left;  // bytes left in packet
   uint32  crc_so_far;  // running crc
   int     bytes_done;  // bytes processed in _current_ chunk
   uint32  sample_loc;  // granule pos encoded in page
} CRCscan;

typedef struct
{
   uint32 page_start, page_end;
   uint32 last_decoded_sample;
} ProbedPage;

struct stb_vorbis
{
  // user-accessible info
   unsigned int sample_rate;
   int channels;

   unsigned int setup_memory_required;
   unsigned int temp_memory_required;
   unsigned int setup_temp_memory_required;

   char *vendor;
   int comment_list_length;
   char **comment_list;

  // input config
#ifndef STB_VORBIS_NO_STDIO
   FILE *f;
   uint32 f_start;
   int close_on_free;
#endif

   uint8 *stream;
   uint8 *stream_start;
   uint8 *stream_end;

   uint32 stream_len;

   uint8  push_mode;

   // the page to seek to when seeking to start, may be zero
   uint32 first_audio_page_offset;

   // p_first is the page on which the first audio packet ends
   // (but not necessarily the page on which it starts)
   ProbedPage p_first, p_last;

  // memory management
   stb_vorbis_alloc alloc;
   int setup_offset;
   int temp_offset;

  // run-time results
   int eof;
   enum STBVorbisError error;

  // user-useful data

  // header info
   int blocksize[2];
   int blocksize_0, blocksize_1;
   int codebook_count;
   Codebook *codebooks;
   int floor_count;
   uint16 floor_types[64]; // varies
   Floor *floor_config;
   int residue_count;
   uint16 residue_types[64]; // varies
   Residue *residue_config;
   int mapping_count;
   Mapping *mapping;
   int mode_count;
   Mode mode_config[64];  // varies

   uint32 total_samples;

  // decode buffer
   fixedpt *channel_buffers[STB_VORBIS_MAX_CHANNELS];
   fixedpt *outputs        [STB_VORBIS_MAX_CHANNELS];

   fixedpt *previous_window[STB_VORBIS_MAX_CHANNELS];
   int previous_length;

   #ifndef STB_VORBIS_NO_DEFER_FLOOR
   int16 *finalY[STB_VORBIS_MAX_CHANNELS];
   #else
   fixedpt *floor_buffers[STB_VORBIS_MAX_CHANNELS];
   #endif

   uint32 current_loc; // sample location of next frame to decode
   int    current_loc_valid;

  // per-blocksize precomputed data

   // twiddle factors
   fixedpt *A[2],*B[2],*C[2];
   fixedpt *window[2];
   uint16 *bit_reverse[2];

  // current page/packet/segment streaming info
   uint32 serial; // stream serial number for verification
   int last_page;
   int segment_count;
   uint8 segments[255];
   uint8 page_flag;
   uint8 bytes_in_seg;
   uint8 first_decode;
   int next_seg;
   int last_seg;  // flag that we're on the last segment
   int last_seg_which; // what was the segment number of the last seg?
   uint32 acc;
   int valid_bits;
   int packet_bytes;
   int end_seg_with_known_loc;
   uint32 known_loc_for_packet;
   int discard_samples_deferred;
   uint32 samples_output;

  // push mode scanning
   int page_crc_tests; // only in push_mode: number of tests active; -1 if not searching
#ifndef STB_VORBIS_NO_PUSHDATA_API
   CRCscan scan[STB_VORBIS_PUSHDATA_CRC_COUNT];
#endif

  // sample-access
   int channel_buffer_start;
   int channel_buffer_end;
};

#if defined(STB_VORBIS_NO_PUSHDATA_API)
   #define IS_PUSH_MODE(f)   FALSE
#elif defined(STB_VORBIS_NO_PULLDATA_API)
   #define IS_PUSH_MODE(f)   TRUE
#else
   #define IS_PUSH_MODE(f)   ((f)->push_mode)
#endif

typedef struct stb_vorbis vorb;

/**
 * Sets the error state of the given Vorbis decoder instance and handles
 * specific error conditions.
 *
 * This function assigns the provided error code `e` to the `error` field of
 * the `vorb` structure `f`. If the decoder has not reached the end of the
 * input data (`f->eof` is false) and the error is not `VORBIS_need_more_data`,
 * the error is set again (useful for debugging purposes, e.g., setting a
 * breakpoint). The function always returns `0`.
 *
 * @param f Pointer to the `vorb` structure representing the Vorbis decoder instance.
 * @param e The error code to set, of type `enum STBVorbisError`.
 * @return Always returns `0`.
 */
static int error(vorb *f, enum STBVorbisError e)
{
   f->error = e;
   if (!f->eof && e != VORBIS_need_more_data) {
      f->error=e; // breakpoint for debugging
   }
   return 0;
}


// these functions are used for allocating temporary memory
// while decoding. if you can afford the stack space, use
// alloca(); otherwise, provide a temp buffer and it will
// allocate out of those.

#define array_size_required(count,size)  (count*(sizeof(void *)+(size)))

#define temp_alloc(f,size)              (f->alloc.alloc_buffer ? setup_temp_malloc(f,size) : alloca(size))
#define temp_free(f,p)                  (void)0
#define temp_alloc_save(f)              ((f)->temp_offset)
#define temp_alloc_restore(f,p)         ((f)->temp_offset = (p))

#define temp_block_array(f,count,size)  make_block_array(temp_alloc(f,array_size_required(count,size)), count, size)

// given a sufficiently large block of memory, make an array of pointers to subblocks of it
static void *make_block_array(void *mem, int count, int size)
{
   int i;
   void ** p = (void **) mem;
   char *q = (char *) (p + count);
   for (i=0; i < count; ++i) {
      p[i] = q;
      q += size;
   }
   return p;
}

/**
 * Allocates memory for setup purposes in a vorbis context, ensuring proper alignment.
 * 
 * This function allocates memory of the specified size, rounded up to the nearest multiple of 8
 * to ensure proper alignment for future allocations. The memory is either allocated from a
 * pre-existing buffer (if `alloc_buffer` is set) or dynamically using `malloc`. The function
 * also updates the `setup_memory_required` field in the `vorb` structure to track the total
 * memory required for setup operations.
 *
 * @param f Pointer to the `vorb` structure containing setup and allocation context.
 * @param sz The size of the memory block to allocate, in bytes.
 *
 * @return A pointer to the allocated memory block, or `NULL` if the allocation fails or if
 *         the requested size exceeds the available buffer space.
 */
static void *setup_malloc(vorb *f, int sz)
{
   sz = (sz+7) & ~7; // round up to nearest 8 for alignment of future allocs.
   f->setup_memory_required += sz;
   if (f->alloc.alloc_buffer) {
      void *p = (char *) f->alloc.alloc_buffer + f->setup_offset;
      if (f->setup_offset + sz > f->temp_offset) return NULL;
      f->setup_offset += sz;
      return p;
   }
   return sz ? malloc(sz) : NULL;
}

/**
 * Frees the allocated memory pointed to by `p` if the allocator buffer in the `vorb` structure is not set.
 * This function is designed to handle memory cleanup for setup memory that is not managed by a stack-based allocator.
 * If the `alloc_buffer` in the `vorb` structure is set, the function does nothing, assuming the memory is managed
 * by a stack allocator and does not need to be freed explicitly.
 *
 * @param f Pointer to the `vorb` structure containing the allocator configuration.
 * @param p Pointer to the memory block to be freed.
 */
static void setup_free(vorb *f, void *p)
{
   if (f->alloc.alloc_buffer) return; // do nothing; setup mem is a stack
   free(p);
}

/**
 * Allocates a temporary memory block of the specified size, ensuring alignment to 8 bytes.
 * If the alloc_buffer in the vorb context is available, it uses the buffer for allocation,
 * otherwise, it falls back to standard malloc. The allocation is performed from the end of
 * the buffer, reducing the temp_offset to mark the allocated space. If the requested size
 * exceeds the available space in the buffer, the function returns NULL.
 *
 * @param f  Pointer to the vorb context containing the alloc_buffer and offsets.
 * @param sz The size of the memory block to allocate.
 * @return   Pointer to the allocated memory block, or NULL if allocation fails.
 */
static void *setup_temp_malloc(vorb *f, int sz)
{
   sz = (sz+7) & ~7; // round up to nearest 8 for alignment of future allocs.
   if (f->alloc.alloc_buffer) {
      if (f->temp_offset - sz < f->setup_offset) return NULL;
      f->temp_offset -= sz;
      return (char *) f->alloc.alloc_buffer + f->temp_offset;
   }
   return malloc(sz);
}

/**
 * Manages the temporary memory allocation or deallocation for a Vorbis decoder context.
 * If the Vorbis decoder context (`f`) is using a custom allocator buffer (`alloc.alloc_buffer`),
 * this function adjusts the temporary offset to account for the requested size (`sz`), ensuring
 * proper alignment by rounding up to the nearest multiple of 8. If no custom allocator buffer is
 * in use, the function directly frees the provided memory block (`p`).
 *
 * @param f Pointer to the Vorbis decoder context.
 * @param p Pointer to the memory block to be freed (if no custom allocator buffer is used).
 * @param sz Size of the memory block to be managed or freed.
 */
static void setup_temp_free(vorb *f, void *p, int sz)
{
   if (f->alloc.alloc_buffer) {
      f->temp_offset += (sz+7)&~7;
      return;
   }
   free(p);
}

#define CRC32_POLY    0x04c11db7   // from spec

static uint32 crc_table[256];
/**
 * Initializes the CRC-32 lookup table used for CRC-32 computation.
 * 
 * This method generates a 256-entry lookup table (`crc_table`) where each entry
 * corresponds to the CRC-32 value for a single byte of input data. The table is
 * computed using the CRC-32 polynomial (`CRC32_POLY`) and the standard CRC-32
 * algorithm. The polynomial is applied whenever the most significant bit of the
 * shifting value is set.
 * 
 * The method iterates over all possible byte values (0-255) and computes their
 * corresponding CRC-32 value by shifting and XORing with the polynomial. The
 * result is stored in the `crc_table` array, which can later be used for
 * efficient CRC-32 computation.
 */
static void crc32_init(void)
{
   int i,j;
   uint32 s;
   for(i=0; i < 256; i++) {
      for (s=(uint32) i << 24, j=0; j < 8; ++j)
         s = (s << 1) ^ (s >= (1U<<31) ? CRC32_POLY : 0);
      crc_table[i] = s;
   }
}

/**
 * Updates a CRC-32 checksum with a new byte of data.
 *
 * This function computes the updated CRC-32 value by combining the current CRC-32 checksum
 * with the provided byte. The calculation is performed using a precomputed CRC-32 lookup table
 * (crc_table). The algorithm shifts the current CRC value left by 8 bits and XORs it with the
 * appropriate entry in the lookup table, which is indexed by the XOR of the byte and the high
 * byte of the current CRC value.
 *
 * @param crc The current CRC-32 checksum value.
 * @param byte The new byte of data to incorporate into the CRC-32 checksum.
 * @return The updated CRC-32 checksum after processing the new byte.
 */
static __forceinline uint32 crc32_update(uint32 crc, uint8 byte)
{
   return (crc << 8) ^ crc_table[byte ^ (crc >> 24)];
}


// used in setup, and for huffman that doesn't go fast path
static unsigned int bit_reverse(unsigned int n)
{
  n = ((n & 0xAAAAAAAA) >>  1) | ((n & 0x55555555) << 1);
  n = ((n & 0xCCCCCCCC) >>  2) | ((n & 0x33333333) << 2);
  n = ((n & 0xF0F0F0F0) >>  4) | ((n & 0x0F0F0F0F) << 4);
  n = ((n & 0xFF00FF00) >>  8) | ((n & 0x00FF00FF) << 8);
  return (n >> 16) | (n << 16);
}

/**
 * Computes the square of a fixed-point number.
 *
 * This method takes a fixed-point number `x` and returns its square by multiplying
 * `x` by itself using the `fixedpt_mul` function. The result is also a fixed-point
 * number.
 *
 * @param x The fixed-point number to be squared.
 * @return The square of the input fixed-point number.
 */
static fixedpt square(fixedpt x)
{
   return fixedpt_mul(x,x);
}

// this is a weird definition of log2() for which log2(1) = 1, log2(2) = 2, log2(4) = 3
// as required by the specification. fast(?) implementation from stb.h
// @OPTIMIZE: called multiple times per-packet with "constants"; move to setup
static int ilog(int32 n)
{
   static signed char log2_4[16] = { 0,1,2,2,3,3,3,3,4,4,4,4,4,4,4,4 };

   if (n < 0) return 0; // signed n returns 0

   // 2 compares if n < 16, 3 compares otherwise (4 if signed or n > 1<<29)
   if (n < (1 << 14))
        if (n < (1 <<  4))            return  0 + log2_4[n      ];
        else if (n < (1 <<  9))       return  5 + log2_4[n >>  5];
             else                     return 10 + log2_4[n >> 10];
   else if (n < (1 << 24))
             if (n < (1 << 19))       return 15 + log2_4[n >> 15];
             else                     return 20 + log2_4[n >> 20];
        else if (n < (1 << 29))       return 25 + log2_4[n >> 25];
             else                     return 30 + log2_4[n >> 30];
}

#ifndef M_PI
  #define M_PI  3.14159265358979323846264f  // from CRC
#endif

// code length assigned to a value with no huffman encoding
#define NO_CODE   255

/////////////////////// LEAF SETUP FUNCTIONS //////////////////////////
//
// these functions are only called at setup, and only a few times
// per file

static fixedpt float32_unpack(uint32 x)
{
   // from the specification, note that this is not IEEE 754
   uint32 mantissa = x & 0x1fffff;
   uint32 sign = x & 0x80000000;
   uint32 exp = (x & 0x7fe00000) >> 21;
   int res = sign ? -mantissa : mantissa;
   //return ldexp(res, exp-788);
   int shift = exp - 788 + FIXEDPT_FBITS;
   fixedpt r = (shift > 0 ? res << shift : res >> (-shift));
   return r;
}


// zlib & jpeg huffman tables assume that the output symbols
// can either be arbitrarily arranged, or have monotonically
// increasing frequencies--they rely on the lengths being sorted;
// this makes for a very simple generation algorithm.
// vorbis allows a huffman table with non-sorted lengths. This
// requires a more sophisticated construction, since symbols in
// order do not map to huffman codes "in order".
static void add_entry(Codebook *c, uint32 huff_code, int symbol, int count, int len, uint32 *values)
{
   if (!c->sparse) {
      c->codewords      [symbol] = huff_code;
   } else {
      c->codewords       [count] = huff_code;
      c->codeword_lengths[count] = len;
      values             [count] = symbol;
   }
}

/**
 * Computes codewords for a given codebook based on the provided code lengths and values.
 *
 * This function generates codewords for a Huffman-like codebook using the lengths of the codes
 * and the associated values. It ensures that the codewords are valid and unique by managing
 * available slots in a binary tree structure. The function initializes the available slots,
 * processes the first valid entry, and then iterates through the remaining entries to assign
 * codewords. It propagates availability up the tree to maintain the uniqueness of codewords.
 *
 * @param c Pointer to the Codebook structure where the codewords will be stored.
 * @param len Array of code lengths for each entry. A length of NO_CODE indicates an invalid entry.
 * @param n Number of entries in the `len` and `values` arrays.
 * @param values Array of values associated with each entry.
 * @return TRUE if codewords were successfully computed for all valid entries, FALSE otherwise.
 */
static int compute_codewords(Codebook *c, uint8 *len, int n, uint32 *values)
{
   int i,k,m=0;
   uint32 available[32];

   memset(available, 0, sizeof(available));
   // find the first entry
   for (k=0; k < n; ++k) if (len[k] < NO_CODE) break;
   if (k == n) { assert(c->sorted_entries == 0); return TRUE; }
   // add to the list
   add_entry(c, 0, k, m++, len[k], values);
   // add all available leaves
   for (i=1; i <= len[k]; ++i)
      available[i] = 1U << (32-i);
   // note that the above code treats the first case specially,
   // but it's really the same as the following code, so they
   // could probably be combined (except the initial code is 0,
   // and I use 0 in available[] to mean 'empty')
   for (i=k+1; i < n; ++i) {
      uint32 res;
      int z = len[i], y;
      if (z == NO_CODE) continue;
      // find lowest available leaf (should always be earliest,
      // which is what the specification calls for)
      // note that this property, and the fact we can never
      // have more than one free leaf at a given level, isn't totally
      // trivial to prove, but it seems true and the assert never
      // fires, so!
      while (z > 0 && !available[z]) --z;
      if (z == 0) { return FALSE; }
      res = available[z];
      assert(z >= 0 && z < 32);
      available[z] = 0;
      add_entry(c, bit_reverse(res), i, m++, len[i], values);
      // propagate availability up the tree
      if (z != len[i]) {
         assert(len[i] >= 0 && len[i] < 32);
         for (y=len[i]; y > z; --y) {
            assert(available[y] == 0);
            available[y] = res + (1 << (32-y));
         }
      }
   }
   return TRUE;
}

// accelerated huffman table allows fast O(1) match of all symbols
// of length <= STB_VORBIS_FAST_HUFFMAN_LENGTH
static void compute_accelerated_huffman(Codebook *c)
{
   int i, len;
   for (i=0; i < FAST_HUFFMAN_TABLE_SIZE; ++i)
      c->fast_huffman[i] = -1;

   len = c->sparse ? c->sorted_entries : c->entries;
   #ifdef STB_VORBIS_FAST_HUFFMAN_SHORT
   if (len > 32767) len = 32767; // largest possible value we can encode!
   #endif
   for (i=0; i < len; ++i) {
      if (c->codeword_lengths[i] <= STB_VORBIS_FAST_HUFFMAN_LENGTH) {
         uint32 z = c->sparse ? bit_reverse(c->sorted_codewords[i]) : c->codewords[i];
         // set table entries for all bit combinations in the higher bits
         while (z < FAST_HUFFMAN_TABLE_SIZE) {
             c->fast_huffman[z] = i;
             z += 1 << c->codeword_lengths[i];
         }
      }
   }
}

#ifdef _MSC_VER
#define STBV_CDECL __cdecl
#else
#define STBV_CDECL
#endif

/**
 * Compares two unsigned 32-bit integers pointed to by `p` and `q`.
 * 
 * This function is designed to be used as a comparator in functions like `qsort`.
 * It takes two void pointers, casts them to `uint32` pointers, and dereferences them
 * to obtain the actual values. It then compares the two values:
 * - Returns -1 if the value pointed to by `p` is less than the value pointed to by `q`.
 * - Returns 1 if the value pointed to by `p` is greater than the value pointed to by `q`.
 * - Returns 0 if the values are equal.
 *
 * @param p Pointer to the first unsigned 32-bit integer.
 * @param q Pointer to the second unsigned 32-bit integer.
 * @return -1 if *p < *q, 1 if *p > *q, or 0 if *p == *q.
 */
static int STBV_CDECL uint32_compare(const void *p, const void *q)
{
   uint32 x = * (uint32 *) p;
   uint32 y = * (uint32 *) q;
   return x < y ? -1 : x > y;
}

/**
 * Determines whether a code of a given length should be included in the sorting process.
 * 
 * This method checks the conditions under which a code of a specified length should be
 * included in the sorting process based on the properties of the codebook and the code length.
 * 
 * @param c A pointer to the Codebook structure, which contains information about the codebook,
 *          including whether it is sparse.
 * @param len The length of the code to be checked.
 * 
 * @return TRUE if the code should be included in the sorting process, FALSE otherwise.
 * 
 * The method follows these rules:
 * - If the codebook is sparse, the code is always included in the sorting process, provided
 *   that the length is not NO_CODE (an assertion is used to enforce this).
 * - If the code length is NO_CODE, the code is not included in the sorting process.
 * - If the code length is greater than STB_VORBIS_FAST_HUFFMAN_LENGTH, the code is included
 *   in the sorting process.
 * - Otherwise, the code is not included in the sorting process.
 */
static int include_in_sort(Codebook *c, uint8 len)
{
   if (c->sparse) { assert(len != NO_CODE); return TRUE; }
   if (len == NO_CODE) return FALSE;
   if (len > STB_VORBIS_FAST_HUFFMAN_LENGTH) return TRUE;
   return FALSE;
}

// if the fast table above doesn't work, we want to binary
// search them... need to reverse the bits
static void compute_sorted_huffman(Codebook *c, uint8 *lengths, uint32 *values)
{
   int i, len;
   // build a list of all the entries
   // OPTIMIZATION: don't include the short ones, since they'll be caught by FAST_HUFFMAN.
   // this is kind of a frivolous optimization--I don't see any performance improvement,
   // but it's like 4 extra lines of code, so.
   if (!c->sparse) {
      int k = 0;
      for (i=0; i < c->entries; ++i)
         if (include_in_sort(c, lengths[i]))
            c->sorted_codewords[k++] = bit_reverse(c->codewords[i]);
      assert(k == c->sorted_entries);
   } else {
      for (i=0; i < c->sorted_entries; ++i)
         c->sorted_codewords[i] = bit_reverse(c->codewords[i]);
   }

   qsort(c->sorted_codewords, c->sorted_entries, sizeof(c->sorted_codewords[0]), uint32_compare);
   c->sorted_codewords[c->sorted_entries] = 0xffffffff;

   len = c->sparse ? c->sorted_entries : c->entries;
   // now we need to indicate how they correspond; we could either
   //   #1: sort a different data structure that says who they correspond to
   //   #2: for each sorted entry, search the original list to find who corresponds
   //   #3: for each original entry, find the sorted entry
   // #1 requires extra storage, #2 is slow, #3 can use binary search!
   for (i=0; i < len; ++i) {
      int huff_len = c->sparse ? lengths[values[i]] : lengths[i];
      if (include_in_sort(c,huff_len)) {
         uint32 code = bit_reverse(c->codewords[i]);
         int x=0, n=c->sorted_entries;
         while (n > 1) {
            // invariant: sc[x] <= code < sc[x+n]
            int m = x + (n >> 1);
            if (c->sorted_codewords[m] <= code) {
               x = m;
               n -= (n>>1);
            } else {
               n >>= 1;
            }
         }
         assert(c->sorted_codewords[x] == code);
         if (c->sparse) {
            c->sorted_values[x] = values[i];
            c->codeword_lengths[x] = huff_len;
         } else {
            c->sorted_values[x] = i;
         }
      }
   }
}

// only run while parsing the header (3 times)
static int vorbis_validate(uint8 *data)
{
   static uint8 vorbis[6] = { 'v', 'o', 'r', 'b', 'i', 's' };
   return memcmp(data, vorbis, 6) == 0;
}

/**
 * Computes the power of an integer x raised to the power of y.
 *
 * This function calculates x^y by iteratively multiplying x by itself y times.
 * The result is returned as a 64-bit signed integer (int64_t) to accommodate
 * large values that may result from the exponentiation.
 *
 * @param x The base integer to be raised to the power of y.
 * @param y The exponent indicating the power to which x is raised.
 *           Must be a non-negative integer.
 *
 * @return The result of x^y as a 64-bit signed integer (int64_t).
 *         Returns 1 if y is 0, as any number raised to the power of 0 is 1.
 */
static int64_t powi(int x, int y) {
  int64_t s = 1;
  int i;
  for (i = 0; i < y; i ++) s *= x;
  return s;
}

// called from setup only, once per code book
// (formula implied by specification)
static int lookup1_values(int entries, int dim)
{
   int r = fixedpt_toint(fixedpt_floor(fixedpt_exp(fixedpt_divi(fixedpt_ln(fixedpt_fromint(entries)), dim))));
   if ((int) powi(r+1, dim) <= entries)   // (int) cast for MinGW warning;
      ++r;                                // floor() to avoid _ftol() when non-CRT
   if (powi(r+1, dim) <= entries)
      return -1;
   if (powi(r, dim) > entries)
      return -1;
   return r;
}

// called twice per file
static void compute_twiddle_factors(int n, fixedpt *A, fixedpt *B, fixedpt *C)
{
   int n4 = n >> 2, n8 = n >> 3;
   int k,k2;

   for (k=k2=0; k < n4; ++k,k2+=2) {
      A[k2  ] =  fixedpt_cos(fixedpt_divi(fixedpt_muli(FIXEDPT_PI,4*k),n));
      A[k2+1] = -fixedpt_sin(fixedpt_divi(fixedpt_muli(FIXEDPT_PI,4*k),n));
      B[k2  ] =  fixedpt_divi(fixedpt_cos(fixedpt_divi(fixedpt_muli(FIXEDPT_PI,k2+1),n*2)), 2);
      B[k2+1] =  fixedpt_divi(fixedpt_sin(fixedpt_divi(fixedpt_muli(FIXEDPT_PI,k2+1),n*2)), 2);
   }
   for (k=k2=0; k < n8; ++k,k2+=2) {
      C[k2  ] =  fixedpt_cos(fixedpt_divi(fixedpt_muli(FIXEDPT_PI,2*(k2+1)),n));
      C[k2+1] = -fixedpt_sin(fixedpt_divi(fixedpt_muli(FIXEDPT_PI,2*(k2+1)),n));
   }
}

/**
 * Computes a window function based on a sine wave approximation.
 *
 * This function generates a window of size `n` by computing a sine-based window function.
 * The window is symmetric, so only the first half of the window is computed, and the
 * second half is implicitly mirrored.
 *
 * The window is computed as follows:
 * 1. For each index `i` in the first half of the window, a temporary value `tmp` is calculated
 *    as the sine of a scaled angle derived from the index and the window size.
 * 2. The window value at index `i` is then computed as the sine of a scaled version of the
 *    square of `tmp`.
 *
 * The fixed-point arithmetic functions `fixedpt_sin`, `fixedpt_divi`, `fixedpt_muli`, and
 * `fixedpt_mul` are used to perform the calculations with fixed-point precision.
 *
 * @param n The size of the window to compute. Must be a positive even integer.
 * @param window A pointer to an array of fixedpt values where the computed window will be stored.
 *               The array must have at least `n` elements.
 */
static void compute_window(int n, fixedpt *window)
{
   int n2 = n >> 1, i;
   for (i=0; i < n2; ++i) {
     fixedpt tmp = fixedpt_sin(fixedpt_divi(fixedpt_muli(FIXEDPT_HALF_PI, 2 * i + 1), n2 * 2));
     window[i] = fixedpt_sin(fixedpt_mul(FIXEDPT_HALF_PI, square(tmp)));
   }
}

/**
 * Computes the bit-reversed indices for a given size `n` and stores them in the array `rev`.
 * The method is designed to work with sizes that are powers of two. It calculates the bit-reversed
 * indices for the first `n/8` elements of the array. The bit-reversed indices are computed by
 * reversing the bits of the index `i` and then shifting and aligning the result to fit within the
 * desired range.
 *
 * @param n The size of the problem, typically a power of two. The method computes bit-reversed
 *          indices for the first `n/8` elements.
 * @param rev An array of type `uint16` where the computed bit-reversed indices will be stored.
 *            The array must have at least `n/8` elements.
 *
 * @note The `ilog` function is assumed to return the logarithm base 2 of `n`, but it is off-by-one
 *       compared to standard definitions (i.e., `ilog(n) = log2(n) + 1`). The bit-reversed indices
 *       are shifted and aligned to ensure they fit within the range of `n`.
 */
static void compute_bitreverse(int n, uint16 *rev)
{
   int ld = ilog(n) - 1; // ilog is off-by-one from normal definitions
   int i, n8 = n >> 3;
   for (i=0; i < n8; ++i)
      rev[i] = (bit_reverse(i) >> (32-ld+3)) << 2;
}

/**
 * Initializes the block size for a given Vorbis context by allocating memory and computing
 * necessary factors for the specified block size. This includes:
 * - Allocating memory for twiddle factors (A, B, and C arrays) used in FFT calculations.
 * - Computing the twiddle factors using the `compute_twiddle_factors` function.
 * - Allocating memory for the window function and computing it using `compute_window`.
 * - Allocating memory for the bit-reverse table and computing it using `compute_bitreverse`.
 *
 * @param f Pointer to the Vorbis context.
 * @param b The block size index to initialize.
 * @param n The size of the block, which is used to determine the sizes of the allocated arrays.
 * @return Returns `TRUE` on success. If memory allocation fails, returns an error code
 *         indicating out-of-memory (VORBIS_outofmem) via the `error` function.
 */
static int init_blocksize(vorb *f, int b, int n)
{
   int n2 = n >> 1, n4 = n >> 2, n8 = n >> 3;
   f->A[b] = (fixedpt *) setup_malloc(f, sizeof(fixedpt) * n2);
   f->B[b] = (fixedpt *) setup_malloc(f, sizeof(fixedpt) * n2);
   f->C[b] = (fixedpt *) setup_malloc(f, sizeof(fixedpt) * n4);
   if (!f->A[b] || !f->B[b] || !f->C[b]) return error(f, VORBIS_outofmem);
   compute_twiddle_factors(n, f->A[b], f->B[b], f->C[b]);
   f->window[b] = (fixedpt *) setup_malloc(f, sizeof(float) * n2);
   if (!f->window[b]) return error(f, VORBIS_outofmem);
   compute_window(n, f->window[b]);
   f->bit_reverse[b] = (uint16 *) setup_malloc(f, sizeof(uint16) * n8);
   if (!f->bit_reverse[b]) return error(f, VORBIS_outofmem);
   compute_bitreverse(n, f->bit_reverse[b]);
   return TRUE;
}

/**
 * Finds the indices of the closest lower and higher neighbors of the element at index `n` in the array `x`.
 * 
 * This function iterates through the first `n` elements of the array `x` to determine the indices of the 
 * elements that are closest to `x[n]` but are strictly lower and higher than `x[n]`, respectively. 
 * The indices of these neighbors are stored in the variables pointed to by `plow` and `phigh`.
 * 
 * @param x     Pointer to the array of uint16 values to search.
 * @param n     The index of the element in the array for which neighbors are to be found.
 * @param plow  Pointer to an integer where the index of the closest lower neighbor will be stored.
 * @param phigh Pointer to an integer where the index of the closest higher neighbor will be stored.
 * 
 * @note If no lower neighbor is found, `plow` is not modified. Similarly, if no higher neighbor is found, 
 *       `phigh` is not modified. The initial values of `plow` and `phigh` should be set appropriately 
 *       before calling this function.
 */
static void neighbors(uint16 *x, int n, int *plow, int *phigh)
{
   int low = -1;
   int high = 65536;
   int i;
   for (i=0; i < n; ++i) {
      if (x[i] > low  && x[i] < x[n]) { *plow  = i; low = x[i]; }
      if (x[i] < high && x[i] > x[n]) { *phigh = i; high = x[i]; }
   }
}

// this has been repurposed so y is now the original index instead of y
typedef struct
{
   uint16 x,id;
} stbv__floor_ordering;

/**
 * Compares two `stbv__floor_ordering` objects based on their `x` values.
 *
 * This function is designed to be used as a comparator for sorting or searching
 * operations, such as with `qsort`. It takes two pointers to `stbv__floor_ordering`
 * objects and compares their `x` fields.
 *
 * @param p A pointer to the first `stbv__floor_ordering` object to compare.
 * @param q A pointer to the second `stbv__floor_ordering` object to compare.
 * @return -1 if the `x` value of the first object is less than the `x` value of
 *         the second object, 1 if the `x` value of the first object is greater
 *         than the `x` value of the second object, and 0 if the `x` values are equal.
 */
static int STBV_CDECL point_compare(const void *p, const void *q)
{
   stbv__floor_ordering *a = (stbv__floor_ordering *) p;
   stbv__floor_ordering *b = (stbv__floor_ordering *) q;
   return a->x < b->x ? -1 : a->x > b->x;
}

//
/////////////////////// END LEAF SETUP FUNCTIONS //////////////////////////


#if defined(STB_VORBIS_NO_STDIO)
   #define USE_MEMORY(z)    TRUE
#else
   #define USE_MEMORY(z)    ((z)->stream)
#endif

/**
 * @brief Reads a single byte (8 bits) from the input stream associated with the vorb object.
 *
 * This method retrieves the next byte from the input stream. The behavior depends on the configuration:
 * - If the vorb object is configured to use memory (USE_MEMORY(z) is true), it reads the byte directly from the memory buffer.
 *   If the stream pointer reaches or exceeds the end of the buffer, the EOF flag is set, and 0 is returned.
 * - If the vorb object is configured to use a file (and STB_VORBIS_NO_STDIO is not defined), it reads the byte using fgetc().
 *   If fgetc() returns EOF, the EOF flag is set, and 0 is returned.
 *
 * @param z Pointer to the vorb object from which the byte is read.
 * @return The next byte from the input stream, or 0 if the end of the stream is reached.
 */
static uint8 get8(vorb *z)
{
   if (USE_MEMORY(z)) {
      if (z->stream >= z->stream_end) { z->eof = TRUE; return 0; }
      return *z->stream++;
   }

   #ifndef STB_VORBIS_NO_STDIO
   {
   int c = fgetc(z->f);
   if (c == EOF) { z->eof = TRUE; return 0; }
   return c;
   }
   #endif
}

/**
 * Reads a 32-bit unsigned integer from the provided vorb file stream.
 * The integer is constructed by reading four consecutive bytes from the stream
 * and combining them in little-endian order. The first byte read becomes the
 * least significant byte (LSB) of the resulting integer, and the fourth byte
 * becomes the most significant byte (MSB).
 *
 * @param f A pointer to the vorb file stream from which to read the bytes.
 * @return The 32-bit unsigned integer constructed from the four bytes read.
 */
static uint32 get32(vorb *f)
{
   uint32 x;
   x = get8(f);
   x += get8(f) << 8;
   x += get8(f) << 16;
   x += (uint32) get8(f) << 24;
   return x;
}

/**
 * Retrieves 'n' bytes of data from the vorbis stream and stores them in the provided buffer.
 * 
 * This function reads 'n' bytes from the vorbis stream 'z' and copies them into the buffer 'data'.
 * If the stream is memory-based (USE_MEMORY(z) is true), it directly copies the data from the
 * memory buffer. If the stream is file-based, it uses the standard I/O function 'fread' to read
 * the data from the file. If the end of the stream is reached during the operation, the 'eof'
 * flag in the vorbis structure is set to 1, and the function returns 0. Otherwise, it returns 1
 * to indicate success.
 * 
 * @param z Pointer to the vorbis stream structure.
 * @param data Pointer to the buffer where the read data will be stored.
 * @param n The number of bytes to read from the stream.
 * @return 1 if the operation is successful, 0 if the end of the stream is reached or an error occurs.
 */
static int getn(vorb *z, uint8 *data, int n)
{
   if (USE_MEMORY(z)) {
      if (z->stream+n > z->stream_end) { z->eof = 1; return 0; }
      memcpy(data, z->stream, n);
      z->stream += n;
      return 1;
   }

   #ifndef STB_VORBIS_NO_STDIO
   if (fread(data, n, 1, z->f) == 1)
      return 1;
   else {
      z->eof = 1;
      return 0;
   }
   #endif
}

/**
 * Skips `n` bytes in the input stream associated with the `vorb` object.
 * 
 * This function advances the stream pointer by `n` bytes. If the stream is memory-based
 * (i.e., `USE_MEMORY(z)` is true), it directly increments the `stream` pointer and checks
 * if the pointer has reached or exceeded the `stream_end`, setting the `eof` flag to 1 if
 * so. If the stream is file-based and `STB_VORBIS_NO_STDIO` is not defined, it uses `fseek`
 * to move the file pointer by `n` bytes from the current position.
 *
 * @param z Pointer to the `vorb` object representing the stream.
 * @param n Number of bytes to skip in the stream.
 */
static void skip(vorb *z, int n)
{
   if (USE_MEMORY(z)) {
      z->stream += n;
      if (z->stream >= z->stream_end) z->eof = 1;
      return;
   }
   #ifndef STB_VORBIS_NO_STDIO
   {
      long x = ftell(z->f);
      fseek(z->f, x+n, SEEK_SET);
   }
   #endif
}

/**
 * Sets the file offset for the given `stb_vorbis` instance to the specified location.
 * This function is used to reposition the file pointer or memory stream to a specific offset.
 * 
 * If the instance is in push mode (i.e., `f->push_mode` is true), the function returns 0 immediately
 * without performing any action. For memory-based streams, it checks if the requested location is
 * within valid bounds. If the location is valid, the stream pointer is updated, and the function
 * returns 1. If the location is invalid, the stream pointer is set to the end, the EOF flag is set,
 * and the function returns 0.
 * 
 * For file-based streams, the function adjusts the location relative to the file's start position
 * (`f->f_start`). If the adjusted location is invalid (e.g., exceeds 0x7fffffff), the EOF flag is set,
 * and the function returns 0. Otherwise, it attempts to seek to the adjusted location using `fseek`.
 * If the seek operation succeeds, the function returns 1. If it fails, the EOF flag is set, the file
 * pointer is moved to the end of the file, and the function returns 0.
 * 
 * @param f   Pointer to the `stb_vorbis` instance.
 * @param loc The desired offset to set the file or memory stream to.
 * @return    1 if the offset was successfully set, 0 otherwise.
 */
static int set_file_offset(stb_vorbis *f, unsigned int loc)
{
   #ifndef STB_VORBIS_NO_PUSHDATA_API
   if (f->push_mode) return 0;
   #endif
   f->eof = 0;
   if (USE_MEMORY(f)) {
      if (f->stream_start + loc >= f->stream_end || f->stream_start + loc < f->stream_start) {
         f->stream = f->stream_end;
         f->eof = 1;
         return 0;
      } else {
         f->stream = f->stream_start + loc;
         return 1;
      }
   }
   #ifndef STB_VORBIS_NO_STDIO
   if (loc + f->f_start < loc || loc >= 0x80000000) {
      loc = 0x7fffffff;
      f->eof = 1;
   } else {
      loc += f->f_start;
   }
   if (!fseek(f->f, loc, SEEK_SET))
      return 1;
   f->eof = 1;
   fseek(f->f, f->f_start, SEEK_END);
   return 0;
   #endif
}


static uint8 ogg_page_header[4] = { 0x4f, 0x67, 0x67, 0x53 };

/**
 * @brief Captures and validates the OggS pattern from the given Vorbis file.
 *
 * This method reads the first four bytes from the provided Vorbis file (`f`) and checks
 * if they match the OggS pattern, which is the signature of an Ogg container. The expected
 * pattern is the sequence of bytes: 0x4f, 0x67, 0x67, 0x53 (which corresponds to "OggS" in ASCII).
 *
 * @param f A pointer to the Vorbis file structure from which bytes are read.
 * @return int Returns `TRUE` (1) if the first four bytes match the OggS pattern, otherwise
 *             returns `FALSE` (0).
 */
static int capture_pattern(vorb *f)
{
   if (0x4f != get8(f)) return FALSE;
   if (0x67 != get8(f)) return FALSE;
   if (0x67 != get8(f)) return FALSE;
   if (0x53 != get8(f)) return FALSE;
   return TRUE;
}

#define PAGEFLAG_continued_packet   1
#define PAGEFLAG_first_page         2
#define PAGEFLAG_last_page          4

/**
 * Processes the start of a page in a Vorbis stream, capturing necessary metadata and validating the stream structure.
 * This function is responsible for reading and interpreting the header of a Vorbis page, including the page flag,
 * absolute granule position, stream serial number, page sequence number, CRC32, and segment count. It also handles
 * the first decode scenario by setting the page start and end positions, as well as the last decoded sample.
 * 
 * @param f A pointer to the vorbis structure representing the Vorbis stream.
 * 
 * @return Returns TRUE if the page start is successfully processed, otherwise returns an error code indicating
 *         the specific issue encountered (e.g., invalid stream structure version or unexpected end of file).
 */
static int start_page_no_capturepattern(vorb *f)
{
   uint32 loc0,loc1,n;
   if (f->first_decode && !IS_PUSH_MODE(f)) {
      f->p_first.page_start = stb_vorbis_get_file_offset(f) - 4;
   }
   // stream structure version
   if (0 != get8(f)) return error(f, VORBIS_invalid_stream_structure_version);
   // header flag
   f->page_flag = get8(f);
   // absolute granule position
   loc0 = get32(f);
   loc1 = get32(f);
   // @TODO: validate loc0,loc1 as valid positions?
   // stream serial number -- vorbis doesn't interleave, so discard
   get32(f);
   //if (f->serial != get32(f)) return error(f, VORBIS_incorrect_stream_serial_number);
   // page sequence number
   n = get32(f);
   f->last_page = n;
   // CRC32
   get32(f);
   // page_segments
   f->segment_count = get8(f);
   if (!getn(f, f->segments, f->segment_count))
      return error(f, VORBIS_unexpected_eof);
   // assume we _don't_ know any the sample position of any segments
   f->end_seg_with_known_loc = -2;
   if (loc0 != ~0U || loc1 != ~0U) {
      int i;
      // determine which packet is the last one that will complete
      for (i=f->segment_count-1; i >= 0; --i)
         if (f->segments[i] < 255)
            break;
      // 'i' is now the index of the _last_ segment of a packet that ends
      if (i >= 0) {
         f->end_seg_with_known_loc = i;
         f->known_loc_for_packet   = loc0;
      }
   }
   if (f->first_decode) {
      int i,len;
      len = 0;
      for (i=0; i < f->segment_count; ++i)
         len += f->segments[i];
      len += 27 + f->segment_count;
      f->p_first.page_end = f->p_first.page_start + len;
      f->p_first.last_decoded_sample = loc0;
   }
   f->next_seg = 0;
   return TRUE;
}

/**
 * Starts processing a new page in the Vorbis stream.
 * 
 * This method begins by attempting to capture the Vorbis capture pattern, which is a unique identifier
 * that marks the start of a valid Vorbis page. If the capture pattern is not found, the method returns
 * an error indicating that the capture pattern is missing. If the capture pattern is successfully
 * identified, the method proceeds to process the page without further checking for the capture pattern.
 * 
 * @param f A pointer to the Vorbis stream context.
 * @return Returns 0 on success. If the capture pattern is missing, returns an error code indicating
 *         the absence of the capture pattern (VORBIS_missing_capture_pattern).
 */
static int start_page(vorb *f)
{
   if (!capture_pattern(f)) return error(f, VORBIS_missing_capture_pattern);
   return start_page_no_capturepattern(f);
}

/**
 * Starts processing a new packet in the Vorbis stream.
 *
 * This method ensures that the stream is positioned at the start of a valid packet.
 * It handles the case where the next segment is not yet available by attempting to
 * start a new page. If the page flag indicates a continued packet, an error is returned
 * as this is considered invalid. Once the next segment is valid, the method initializes
 * various state variables related to packet processing and returns TRUE to indicate
 * successful initialization.
 *
 * @param f Pointer to the Vorbis stream context.
 * @return TRUE if the packet was successfully started, FALSE if an error occurred
 *         or if the stream could not be positioned at the start of a valid packet.
 */
static int start_packet(vorb *f)
{
   while (f->next_seg == -1) {
      if (!start_page(f)) return FALSE;
      if (f->page_flag & PAGEFLAG_continued_packet)
         return error(f, VORBIS_continued_packet_flag_invalid);
   }
   f->last_seg = FALSE;
   f->valid_bits = 0;
   f->packet_bytes = 0;
   f->bytes_in_seg = 0;
   // f->next_seg is now valid
   return TRUE;
}

/**
 * Attempts to start reading a new packet from the Vorbis stream.
 *
 * This method checks if the next segment is uninitialized (indicated by `f->next_seg == -1`).
 * If so, it reads the next byte from the stream and verifies the presence of the Vorbis capture
 * pattern (0x4f, 0x67, 0x67, 0x53). If the capture pattern is missing, it returns an error.
 * If the capture pattern is valid, it starts reading the page without the capture pattern.
 * If the page flag indicates a continued packet, it sets up the necessary state to read the packet
 * and returns an error indicating the continued packet flag is invalid.
 * If the next segment is already initialized, it proceeds to start reading the packet directly.
 *
 * @param f Pointer to the Vorbis stream context.
 * @return TRUE if the packet was successfully started, FALSE if an error occurred or EOF was reached.
 */
static int maybe_start_packet(vorb *f)
{
   if (f->next_seg == -1) {
      int x = get8(f);
      if (f->eof) return FALSE; // EOF at page boundary is not an error!
      if (0x4f != x      ) return error(f, VORBIS_missing_capture_pattern);
      if (0x67 != get8(f)) return error(f, VORBIS_missing_capture_pattern);
      if (0x67 != get8(f)) return error(f, VORBIS_missing_capture_pattern);
      if (0x53 != get8(f)) return error(f, VORBIS_missing_capture_pattern);
      if (!start_page_no_capturepattern(f)) return FALSE;
      if (f->page_flag & PAGEFLAG_continued_packet) {
         // set up enough state that we can read this packet if we want,
         // e.g. during recovery
         f->last_seg = FALSE;
         f->bytes_in_seg = 0;
         return error(f, VORBIS_continued_packet_flag_invalid);
      }
   }
   return start_packet(f);
}

/**
 * Retrieves the length of the next segment in the Vorbis stream.
 *
 * This method processes the next segment in the Vorbis stream, updating the internal state of the
 * Vorbis context (`vorb *f`). It handles the transition between pages and segments, ensuring that
 * the stream is correctly parsed. If the current segment is the last segment of the page, it marks
 * the stream as such and returns 0. If the next segment is not available, it attempts to start a new
 * page and validates the continuation flag. The method also updates the segment count and the number
 * of bytes in the current segment.
 *
 * @param f Pointer to the Vorbis context.
 * @return The length of the next segment. Returns 0 if the segment is the last segment or if an error
 *         occurs (e.g., invalid continuation flag).
 */
static int next_segment(vorb *f)
{
   int len;
   if (f->last_seg) return 0;
   if (f->next_seg == -1) {
      f->last_seg_which = f->segment_count-1; // in case start_page fails
      if (!start_page(f)) { f->last_seg = 1; return 0; }
      if (!(f->page_flag & PAGEFLAG_continued_packet)) return error(f, VORBIS_continued_packet_flag_invalid);
   }
   len = f->segments[f->next_seg++];
   if (len < 255) {
      f->last_seg = TRUE;
      f->last_seg_which = f->next_seg-1;
   }
   if (f->next_seg >= f->segment_count)
      f->next_seg = -1;
   assert(f->bytes_in_seg == 0);
   f->bytes_in_seg = len;
   return len;
}

#define EOP    (-1)
#define INVALID_BITS  (-1)

/**
 * Reads a single byte from the current packet in the Vorbis stream.
 *
 * This method handles the logic for reading a byte from the current segment of the Vorbis stream.
 * If the current segment has no remaining bytes (`bytes_in_seg` is 0), it attempts to move to the
 * next segment using `next_segment(f)`. If no more segments are available (`last_seg` is true), it
 * returns `EOP` (End of Packet) to indicate the end of the packet. Otherwise, it decrements the
 * `bytes_in_seg` counter, increments the `packet_bytes` counter, and returns the next byte from the
 * stream using `get8(f)`.
 *
 * @param f Pointer to the Vorbis stream context (`vorb` structure).
 * @return The next byte from the stream, or `EOP` if the end of the packet is reached.
 */
static int get8_packet_raw(vorb *f)
{
   if (!f->bytes_in_seg) {  // CLANG!
      if (f->last_seg) return EOP;
      else if (!next_segment(f)) return EOP;
   }
   assert(f->bytes_in_seg > 0);
   --f->bytes_in_seg;
   ++f->packet_bytes;
   return get8(f);
}

/**
 * Reads a single byte from the packet data of the Vorbis stream.
 * This function retrieves the next byte from the packet data using `get8_packet_raw`
 * and resets the valid bits counter in the Vorbis stream context to zero.
 * 
 * @param f Pointer to the Vorbis stream context (`vorb` structure).
 * @return The byte read from the packet data as an integer.
 */
static int get8_packet(vorb *f)
{
   int x = get8_packet_raw(f);
   f->valid_bits = 0;
   return x;
}

/**
 * Reads a 32-bit unsigned integer from a packet in little-endian format.
 * 
 * This method reads four bytes from the packet using the `get8_packet` function,
 * and combines them into a single 32-bit unsigned integer. The bytes are read
 * in sequence, with the first byte representing the least significant byte (LSB)
 * and the fourth byte representing the most significant byte (MSB). This ensures
 * that the integer is correctly interpreted in little-endian format.
 *
 * @param f A pointer to the `vorb` structure representing the packet source.
 * @return The 32-bit unsigned integer read from the packet.
 */
static int get32_packet(vorb *f)
{
   uint32 x;
   x = get8_packet(f);
   x += get8_packet(f) << 8;
   x += get8_packet(f) << 16;
   x += (uint32) get8_packet(f) << 24;
   return x;
}

/**
 * Flushes the current packet in the Vorbis stream until the End of Packet (EOP) marker is encountered.
 * This function reads bytes from the packet using `get8_packet_raw` in a loop, discarding them until
 * the EOP marker is found. This ensures that the stream is properly synchronized and ready for the next packet.
 *
 * @param f A pointer to the `vorb` structure representing the Vorbis stream.
 */
static void flush_packet(vorb *f)
{
    while (get8_packet_raw(f) != EOP);
}

// @OPTIMIZE: this is the secondary bit decoder, so it's probably not as important
// as the huffman decoder?
static uint32 get_bits(vorb *f, int n)
{
   uint32 z;

   if (f->valid_bits < 0) return 0;
   if (f->valid_bits < n) {
      if (n > 24) {
         // the accumulator technique below would not work correctly in this case
         z = get_bits(f, 24);
         z += get_bits(f, n-24) << 24;
         return z;
      }
      if (f->valid_bits == 0) f->acc = 0;
      while (f->valid_bits < n) {
         int z = get8_packet_raw(f);
         if (z == EOP) {
            f->valid_bits = INVALID_BITS;
            return 0;
         }
         f->acc += z << f->valid_bits;
         f->valid_bits += 8;
      }
   }

   assert(f->valid_bits >= n);
   z = f->acc & ((1 << n)-1);
   f->acc >>= n;
   f->valid_bits -= n;
   return z;
}

// @OPTIMIZE: primary accumulator for huffman
// expand the buffer to as many bits as possible without reading off end of packet
// it might be nice to allow f->valid_bits and f->acc to be stored in registers,
// e.g. cache them locally and decode locally
static __forceinline void prep_huffman(vorb *f)
{
   if (f->valid_bits <= 24) {
      if (f->valid_bits == 0) f->acc = 0;
      do {
         int z;
         if (f->last_seg && !f->bytes_in_seg) return;
         z = get8_packet_raw(f);
         if (z == EOP) return;
         f->acc += (unsigned) z << f->valid_bits;
         f->valid_bits += 8;
      } while (f->valid_bits <= 24);
   }
}

enum
{
   VORBIS_packet_id = 1,
   VORBIS_packet_comment = 3,
   VORBIS_packet_setup = 5
};

/**
 * Decodes a scalar value from a Vorbis codebook using raw Huffman decoding.
 *
 * This function decodes a scalar value from the provided Vorbis codebook `c` using the
 * bitstream context `f`. It supports both binary search and linear search strategies
 * depending on the structure of the codebook. Binary search is used when the codebook
 * has sorted codewords and either the codewords array is not provided or the number of
 * entries exceeds 8. Linear search is used for smaller codebooks or when sorted codewords
 * are not available.
 *
 * @param f Pointer to the Vorbis bitstream context.
 * @param c Pointer to the Codebook structure containing Huffman codewords and metadata.
 * @return The decoded scalar value on success, or -1 if decoding fails (e.g., invalid
 *         bitstream, insufficient bits, or malformed codebook).
 */
static int codebook_decode_scalar_raw(vorb *f, Codebook *c)
{
   int i;
   prep_huffman(f);

   if (c->codewords == NULL && c->sorted_codewords == NULL)
      return -1;

   // cases to use binary search: sorted_codewords && !c->codewords
   //                             sorted_codewords && c->entries > 8
   if (c->entries > 8 ? c->sorted_codewords!=NULL : !c->codewords) {
      // binary search
      uint32 code = bit_reverse(f->acc);
      int x=0, n=c->sorted_entries, len;

      while (n > 1) {
         // invariant: sc[x] <= code < sc[x+n]
         int m = x + (n >> 1);
         if (c->sorted_codewords[m] <= code) {
            x = m;
            n -= (n>>1);
         } else {
            n >>= 1;
         }
      }
      // x is now the sorted index
      if (!c->sparse) x = c->sorted_values[x];
      // x is now sorted index if sparse, or symbol otherwise
      len = c->codeword_lengths[x];
      if (f->valid_bits >= len) {
         f->acc >>= len;
         f->valid_bits -= len;
         return x;
      }

      f->valid_bits = 0;
      return -1;
   }

   // if small, linear search
   assert(!c->sparse);
   for (i=0; i < c->entries; ++i) {
      if (c->codeword_lengths[i] == NO_CODE) continue;
      if (c->codewords[i] == (f->acc & ((1 << c->codeword_lengths[i])-1))) {
         if (f->valid_bits >= c->codeword_lengths[i]) {
            f->acc >>= c->codeword_lengths[i];
            f->valid_bits -= c->codeword_lengths[i];
            return i;
         }
         f->valid_bits = 0;
         return -1;
      }
   }

   error(f, VORBIS_invalid_stream);
   f->valid_bits = 0;
   return -1;
}

#ifndef STB_VORBIS_NO_INLINE_DECODE

#define DECODE_RAW(var, f,c)                                  \
   if (f->valid_bits < STB_VORBIS_FAST_HUFFMAN_LENGTH)        \
      prep_huffman(f);                                        \
   var = f->acc & FAST_HUFFMAN_TABLE_MASK;                    \
   var = c->fast_huffman[var];                                \
   if (var >= 0) {                                            \
      int n = c->codeword_lengths[var];                       \
      f->acc >>= n;                                           \
      f->valid_bits -= n;                                     \
      if (f->valid_bits < 0) { f->valid_bits = 0; var = -1; } \
   } else {                                                   \
      var = codebook_decode_scalar_raw(f,c);                  \
   }

#else

/**
 * Decodes a scalar value from the Vorbis bitstream using the provided codebook.
 * This method first attempts to use a fast Huffman table lookup for efficiency.
 * If the fast lookup fails (indicated by a negative value), it falls back to a
 * raw decoding method.
 *
 * @param f Pointer to the Vorbis context (`vorb` structure) containing the bitstream
 *          and related state.
 * @param c Pointer to the `Codebook` structure containing the Huffman codes and
 *          related metadata.
 * @return The decoded scalar value. If the decoding fails due to insufficient
 *         valid bits in the bitstream, returns -1.
 */
static int codebook_decode_scalar(vorb *f, Codebook *c)
{
   int i;
   if (f->valid_bits < STB_VORBIS_FAST_HUFFMAN_LENGTH)
      prep_huffman(f);
   // fast huffman table lookup
   i = f->acc & FAST_HUFFMAN_TABLE_MASK;
   i = c->fast_huffman[i];
   if (i >= 0) {
      f->acc >>= c->codeword_lengths[i];
      f->valid_bits -= c->codeword_lengths[i];
      if (f->valid_bits < 0) { f->valid_bits = 0; return -1; }
      return i;
   }
   return codebook_decode_scalar_raw(f,c);
}

#define DECODE_RAW(var,f,c)    var = codebook_decode_scalar(f,c);

#endif

#define DECODE(var,f,c)                                       \
   DECODE_RAW(var,f,c)                                        \
   if (c->sparse) var = c->sorted_values[var];

#ifndef STB_VORBIS_DIVIDES_IN_CODEBOOK
  #define DECODE_VQ(var,f,c)   DECODE_RAW(var,f,c)
#else
  #define DECODE_VQ(var,f,c)   DECODE(var,f,c)
#endif






// CODEBOOK_ELEMENT_FAST is an optimization for the CODEBOOK_FLOATS case
// where we avoid one addition
#define CODEBOOK_ELEMENT(c,off)          (c->multiplicands[off])
#define CODEBOOK_ELEMENT_FAST(c,off)     (c->multiplicands[off])
#define CODEBOOK_ELEMENT_BASE(c)         (0)

/**
 * Decodes the starting point for a codebook in a Vorbis stream.
 *
 * This method is responsible for decoding the initial value from a codebook 
 * in a Vorbis stream. It ensures that the codebook's lookup type is valid 
 * (type 0 is only legal in a scalar context) and decodes the value using 
 * the `DECODE_VQ` macro. If the codebook is sparse, it asserts that the 
 * decoded value is within the bounds of the sorted entries. Additionally, 
 * it checks for the end-of-packet (EOP) condition and returns an error if 
 * the stream is invalid.
 *
 * @param f A pointer to the Vorbis stream context.
 * @param c A pointer to the codebook being decoded.
 * @return The decoded starting value (z), or -1 if an error occurs or 
 *         the end of the packet is reached.
 */
static int codebook_decode_start(vorb *f, Codebook *c)
{
   int z = -1;

   // type 0 is only legal in a scalar context
   if (c->lookup_type == 0)
      error(f, VORBIS_invalid_stream);
   else {
      DECODE_VQ(z,f,c);
      if (c->sparse) assert(z < c->sorted_entries);
      if (z < 0) {  // check for EOP
         if (!f->bytes_in_seg)
            if (f->last_seg)
               return z;
         error(f, VORBIS_invalid_stream);
      }
   }
   return z;
}

/**
 * Decodes a codebook entry from the Vorbis stream and writes the decoded values to the output array.
 *
 * This method reads a codebook entry from the Vorbis stream `f` using the provided codebook `c`. 
 * The decoded values are added to the `output` array, which has a length of `len`. If `len` exceeds 
 * the number of dimensions in the codebook, it is truncated to the codebook's dimensions.
 *
 * The decoding process depends on the codebook's lookup type and whether the codebook uses sequence 
 * prediction. If the codebook uses a lookup type of 1 and the `STB_VORBIS_DIVIDES_IN_CODEBOOK` macro 
 * is defined, the method performs a division-based lookup. Otherwise, it uses a direct lookup.
 *
 * @param f       Pointer to the Vorbis stream from which to read the codebook entry.
 * @param c       Pointer to the codebook used for decoding.
 * @param output  Array where the decoded values will be written. The values are added to the existing 
 *                contents of the array.
 * @param len     The number of values to decode. If greater than the codebook's dimensions, it is 
 *                truncated to the codebook's dimensions.
 *
 * @return        Returns `TRUE` if decoding is successful, otherwise returns `FALSE` if an error 
 *                occurs during the initial decoding step.
 */
static int codebook_decode(vorb *f, Codebook *c, fixedpt *output, int len)
{
   int i,z = codebook_decode_start(f,c);
   if (z < 0) return FALSE;
   if (len > c->dimensions) len = c->dimensions;

#ifdef STB_VORBIS_DIVIDES_IN_CODEBOOK
   if (c->lookup_type == 1) {
      fixedpt last = CODEBOOK_ELEMENT_BASE(c);
      int div = 1;
      for (i=0; i < len; ++i) {
         int off = (z / div) % c->lookup_values;
         fixedpt val = CODEBOOK_ELEMENT_FAST(c,off) + last;
         output[i] += val;
         if (c->sequence_p) last = val + c->minimum_value;
         div *= c->lookup_values;
      }
      return TRUE;
   }
#endif

   z *= c->dimensions;
   if (c->sequence_p) {
      fixedpt last = CODEBOOK_ELEMENT_BASE(c);
      for (i=0; i < len; ++i) {
         fixedpt val = CODEBOOK_ELEMENT_FAST(c,z+i) + last;
         output[i] += val;
         last = val + c->minimum_value;
      }
   } else {
      fixedpt last = CODEBOOK_ELEMENT_BASE(c);
      for (i=0; i < len; ++i) {
         output[i] += CODEBOOK_ELEMENT_FAST(c,z+i) + last;
      }
   }

   return TRUE;
}

/**
 * Decodes a sequence of values from a Vorbis codebook and writes them to the output buffer.
 * 
 * This function decodes a sequence of values from the provided codebook `c` using the Vorbis
 * bitstream `f`. The decoded values are added to the `output` buffer at intervals specified by
 * `step`. The number of values to decode is determined by `len`, but is limited to the
 * dimensionality of the codebook. The function handles both lookup types defined in the Vorbis
 * specification, including the case where the codebook uses division-based lookup (if
 * `STB_VORBIS_DIVIDES_IN_CODEBOOK` is defined).
 *
 * @param f        Pointer to the Vorbis bitstream context.
 * @param c        Pointer to the Codebook structure containing the codebook data.
 * @param output   Pointer to the output buffer where decoded values will be stored.
 * @param len      The number of values to decode (limited by the codebook's dimensionality).
 * @param step     The step size for writing values to the output buffer.
 * @return         Returns `TRUE` if decoding was successful, or `FALSE` if an error occurred
 *                 (e.g., if the initial decode step failed).
 */
static int codebook_decode_step(vorb *f, Codebook *c, fixedpt *output, int len, int step)
{
   int i,z = codebook_decode_start(f,c);
   fixedpt last = CODEBOOK_ELEMENT_BASE(c);
   if (z < 0) return FALSE;
   if (len > c->dimensions) len = c->dimensions;

#ifdef STB_VORBIS_DIVIDES_IN_CODEBOOK
   if (c->lookup_type == 1) {
      int div = 1;
      for (i=0; i < len; ++i) {
         int off = (z / div) % c->lookup_values;
         fixedpt val = CODEBOOK_ELEMENT_FAST(c,off) + last;
         output[i*step] += val;
         if (c->sequence_p) last = val;
         div *= c->lookup_values;
      }
      return TRUE;
   }
#endif

   z *= c->dimensions;
   for (i=0; i < len; ++i) {
      fixedpt val = CODEBOOK_ELEMENT_FAST(c,z+i) + last;
      output[i*step] += val;
      if (c->sequence_p) last = val;
   }

   return TRUE;
}

/**
 * Decodes and deinterleaves data from a Vorbis codebook, applying repeat logic as necessary.
 *
 * This method processes the input data from the Vorbis stream (`f`) using the provided codebook (`c`).
 * It decodes the data, deinterleaves it across multiple output channels (`outputs`), and handles
 * repeat logic for multi-dimensional codebook entries. The method updates the interleave counters
 * (`c_inter_p` and `p_inter_p`) to track the current position in the output buffers.
 *
 * @param f                Pointer to the Vorbis stream context.
 * @param c                Pointer to the codebook used for decoding.
 * @param outputs          Array of pointers to output buffers for each channel.
 * @param ch               Number of channels in the output buffers.
 * @param c_inter_p        Pointer to the current interleave counter for channels.
 * @param p_inter_p        Pointer to the current interleave counter for positions.
 * @param len              Length of the output buffers.
 * @param total_decode     Total number of elements to decode.
 *
 * @return                 Returns `TRUE` if decoding is successful, otherwise `FALSE` or an error code
 *                         if an invalid stream or other error is encountered.
 */
static int codebook_decode_deinterleave_repeat(vorb *f, Codebook *c, fixedpt **outputs, int ch, int *c_inter_p, int *p_inter_p, int len, int total_decode)
{
   int c_inter = *c_inter_p;
   int p_inter = *p_inter_p;
   int i,z, effective = c->dimensions;

   // type 0 is only legal in a scalar context
   if (c->lookup_type == 0)   return error(f, VORBIS_invalid_stream);

   while (total_decode > 0) {
      fixedpt last = CODEBOOK_ELEMENT_BASE(c);
      DECODE_VQ(z,f,c);
      #ifndef STB_VORBIS_DIVIDES_IN_CODEBOOK
      assert(!c->sparse || z < c->sorted_entries);
      #endif
      if (z < 0) {
         if (!f->bytes_in_seg)
            if (f->last_seg) return FALSE;
         return error(f, VORBIS_invalid_stream);
      }

      // if this will take us off the end of the buffers, stop short!
      // we check by computing the length of the virtual interleaved
      // buffer (len*ch), our current offset within it (p_inter*ch)+(c_inter),
      // and the length we'll be using (effective)
      if (c_inter + p_inter*ch + effective > len * ch) {
         effective = len*ch - (p_inter*ch - c_inter);
      }

   #ifdef STB_VORBIS_DIVIDES_IN_CODEBOOK
      if (c->lookup_type == 1) {
         int div = 1;
         for (i=0; i < effective; ++i) {
            int off = (z / div) % c->lookup_values;
            fixedpt val = CODEBOOK_ELEMENT_FAST(c,off) + last;
            if (outputs[c_inter])
               outputs[c_inter][p_inter] += val;
            if (++c_inter == ch) { c_inter = 0; ++p_inter; }
            if (c->sequence_p) last = val;
            div *= c->lookup_values;
         }
      } else
   #endif
      {
         z *= c->dimensions;
         if (c->sequence_p) {
            for (i=0; i < effective; ++i) {
               fixedpt val = CODEBOOK_ELEMENT_FAST(c,z+i) + last;
               if (outputs[c_inter])
                  outputs[c_inter][p_inter] += val;
               if (++c_inter == ch) { c_inter = 0; ++p_inter; }
               last = val;
            }
         } else {
            for (i=0; i < effective; ++i) {
               fixedpt val = CODEBOOK_ELEMENT_FAST(c,z+i) + last;
               if (outputs[c_inter])
                  outputs[c_inter][p_inter] += val;
               if (++c_inter == ch) { c_inter = 0; ++p_inter; }
            }
         }
      }

      total_decode -= effective;
   }
   *c_inter_p = c_inter;
   *p_inter_p = p_inter;
   return TRUE;
}

/**
 * Predicts the y-coordinate of a point on a line given its x-coordinate and two known points on the line.
 * The method uses linear interpolation to estimate the y-coordinate based on the slope of the line
 * defined by the points (x0, y0) and (x1, y1).
 *
 * @param x The x-coordinate of the point for which the y-coordinate is to be predicted.
 * @param x0 The x-coordinate of the first known point on the line.
 * @param x1 The x-coordinate of the second known point on the line.
 * @param y0 The y-coordinate of the first known point on the line.
 * @param y1 The y-coordinate of the second known point on the line.
 * @return The predicted y-coordinate of the point on the line corresponding to the given x-coordinate.
 *         The result is calculated based on the direction of the slope (dy) and the position of x
 *         relative to x0 and x1.
 */
static int predict_point(int x, int x0, int x1, int y0, int y1)
{
   int dy = y1 - y0;
   int adx = x1 - x0;
   // @OPTIMIZE: force int division to round in the right direction... is this necessary on x86?
   int err = abs(dy) * (x - x0);
   int off = err / adx;
   return dy < 0 ? y0 - off : y0 + off;
}

#define SCALE_SHIFT 13
#define SCALE (1 << (SCALE_SHIFT))
// the following table is block-copied from the specification
static fixedpt inverse_db_table[256] =
{
  fixedpt_rconst(1.0649863e-07f * SCALE), fixedpt_rconst(1.1341951e-07f * SCALE), fixedpt_rconst(1.2079015e-07f * SCALE), fixedpt_rconst(1.2863978e-07f * SCALE),
  fixedpt_rconst(1.3699951e-07f * SCALE), fixedpt_rconst(1.4590251e-07f * SCALE), fixedpt_rconst(1.5538408e-07f * SCALE), fixedpt_rconst(1.6548181e-07f * SCALE),
  fixedpt_rconst(1.7623575e-07f * SCALE), fixedpt_rconst(1.8768855e-07f * SCALE), fixedpt_rconst(1.9988561e-07f * SCALE), fixedpt_rconst(2.1287530e-07f * SCALE),
  fixedpt_rconst(2.2670913e-07f * SCALE), fixedpt_rconst(2.4144197e-07f * SCALE), fixedpt_rconst(2.5713223e-07f * SCALE), fixedpt_rconst(2.7384213e-07f * SCALE),
  fixedpt_rconst(2.9163793e-07f * SCALE), fixedpt_rconst(3.1059021e-07f * SCALE), fixedpt_rconst(3.3077411e-07f * SCALE), fixedpt_rconst(3.5226968e-07f * SCALE),
  fixedpt_rconst(3.7516214e-07f * SCALE), fixedpt_rconst(3.9954229e-07f * SCALE), fixedpt_rconst(4.2550680e-07f * SCALE), fixedpt_rconst(4.5315863e-07f * SCALE),
  fixedpt_rconst(4.8260743e-07f * SCALE), fixedpt_rconst(5.1396998e-07f * SCALE), fixedpt_rconst(5.4737065e-07f * SCALE), fixedpt_rconst(5.8294187e-07f * SCALE),
  fixedpt_rconst(6.2082472e-07f * SCALE), fixedpt_rconst(6.6116941e-07f * SCALE), fixedpt_rconst(7.0413592e-07f * SCALE), fixedpt_rconst(7.4989464e-07f * SCALE),
  fixedpt_rconst(7.9862701e-07f * SCALE), fixedpt_rconst(8.5052630e-07f * SCALE), fixedpt_rconst(9.0579828e-07f * SCALE), fixedpt_rconst(9.6466216e-07f * SCALE),
  fixedpt_rconst(1.0273513e-06f * SCALE), fixedpt_rconst(1.0941144e-06f * SCALE), fixedpt_rconst(1.1652161e-06f * SCALE), fixedpt_rconst(1.2409384e-06f * SCALE),
  fixedpt_rconst(1.3215816e-06f * SCALE), fixedpt_rconst(1.4074654e-06f * SCALE), fixedpt_rconst(1.4989305e-06f * SCALE), fixedpt_rconst(1.5963394e-06f * SCALE),
  fixedpt_rconst(1.7000785e-06f * SCALE), fixedpt_rconst(1.8105592e-06f * SCALE), fixedpt_rconst(1.9282195e-06f * SCALE), fixedpt_rconst(2.0535261e-06f * SCALE),
  fixedpt_rconst(2.1869758e-06f * SCALE), fixedpt_rconst(2.3290978e-06f * SCALE), fixedpt_rconst(2.4804557e-06f * SCALE), fixedpt_rconst(2.6416497e-06f * SCALE),
  fixedpt_rconst(2.8133190e-06f * SCALE), fixedpt_rconst(2.9961443e-06f * SCALE), fixedpt_rconst(3.1908506e-06f * SCALE), fixedpt_rconst(3.3982101e-06f * SCALE),
  fixedpt_rconst(3.6190449e-06f * SCALE), fixedpt_rconst(3.8542308e-06f * SCALE), fixedpt_rconst(4.1047004e-06f * SCALE), fixedpt_rconst(4.3714470e-06f * SCALE),
  fixedpt_rconst(4.6555282e-06f * SCALE), fixedpt_rconst(4.9580707e-06f * SCALE), fixedpt_rconst(5.2802740e-06f * SCALE), fixedpt_rconst(5.6234160e-06f * SCALE),
  fixedpt_rconst(5.9888572e-06f * SCALE), fixedpt_rconst(6.3780469e-06f * SCALE), fixedpt_rconst(6.7925283e-06f * SCALE), fixedpt_rconst(7.2339451e-06f * SCALE),
  fixedpt_rconst(7.7040476e-06f * SCALE), fixedpt_rconst(8.2047000e-06f * SCALE), fixedpt_rconst(8.7378876e-06f * SCALE), fixedpt_rconst(9.3057248e-06f * SCALE),
  fixedpt_rconst(9.9104632e-06f * SCALE), fixedpt_rconst(1.0554501e-05f * SCALE), fixedpt_rconst(1.1240392e-05f * SCALE), fixedpt_rconst(1.1970856e-05f * SCALE),
  fixedpt_rconst(1.2748789e-05f * SCALE), fixedpt_rconst(1.3577278e-05f * SCALE), fixedpt_rconst(1.4459606e-05f * SCALE), fixedpt_rconst(1.5399272e-05f * SCALE),
  fixedpt_rconst(1.6400004e-05f * SCALE), fixedpt_rconst(1.7465768e-05f * SCALE), fixedpt_rconst(1.8600792e-05f * SCALE), fixedpt_rconst(1.9809576e-05f * SCALE),
  fixedpt_rconst(2.1096914e-05f * SCALE), fixedpt_rconst(2.2467911e-05f * SCALE), fixedpt_rconst(2.3928002e-05f * SCALE), fixedpt_rconst(2.5482978e-05f * SCALE),
  fixedpt_rconst(2.7139006e-05f * SCALE), fixedpt_rconst(2.8902651e-05f * SCALE), fixedpt_rconst(3.0780908e-05f * SCALE), fixedpt_rconst(3.2781225e-05f * SCALE),
  fixedpt_rconst(3.4911534e-05f * SCALE), fixedpt_rconst(3.7180282e-05f * SCALE), fixedpt_rconst(3.9596466e-05f * SCALE), fixedpt_rconst(4.2169667e-05f * SCALE),
  fixedpt_rconst(4.4910090e-05f * SCALE), fixedpt_rconst(4.7828601e-05f * SCALE), fixedpt_rconst(5.0936773e-05f * SCALE), fixedpt_rconst(5.4246931e-05f * SCALE),
  fixedpt_rconst(5.7772202e-05f * SCALE), fixedpt_rconst(6.1526565e-05f * SCALE), fixedpt_rconst(6.5524908e-05f * SCALE), fixedpt_rconst(6.9783085e-05f * SCALE),
  fixedpt_rconst(7.4317983e-05f * SCALE), fixedpt_rconst(7.9147585e-05f * SCALE), fixedpt_rconst(8.4291040e-05f * SCALE), fixedpt_rconst(8.9768747e-05f * SCALE),
  fixedpt_rconst(9.5602426e-05f * SCALE), fixedpt_rconst(0.00010181521f * SCALE), fixedpt_rconst(0.00010843174f * SCALE), fixedpt_rconst(0.00011547824f * SCALE),
  fixedpt_rconst(0.00012298267f * SCALE), fixedpt_rconst(0.00013097477f * SCALE), fixedpt_rconst(0.00013948625f * SCALE), fixedpt_rconst(0.00014855085f * SCALE),
  fixedpt_rconst(0.00015820453f * SCALE), fixedpt_rconst(0.00016848555f * SCALE), fixedpt_rconst(0.00017943469f * SCALE), fixedpt_rconst(0.00019109536f * SCALE),
  fixedpt_rconst(0.00020351382f * SCALE), fixedpt_rconst(0.00021673929f * SCALE), fixedpt_rconst(0.00023082423f * SCALE), fixedpt_rconst(0.00024582449f * SCALE),
  fixedpt_rconst(0.00026179955f * SCALE), fixedpt_rconst(0.00027881276f * SCALE), fixedpt_rconst(0.00029693158f * SCALE), fixedpt_rconst(0.00031622787f * SCALE),
  fixedpt_rconst(0.00033677814f * SCALE), fixedpt_rconst(0.00035866388f * SCALE), fixedpt_rconst(0.00038197188f * SCALE), fixedpt_rconst(0.00040679456f * SCALE),
  fixedpt_rconst(0.00043323036f * SCALE), fixedpt_rconst(0.00046138411f * SCALE), fixedpt_rconst(0.00049136745f * SCALE), fixedpt_rconst(0.00052329927f * SCALE),
  fixedpt_rconst(0.00055730621f * SCALE), fixedpt_rconst(0.00059352311f * SCALE), fixedpt_rconst(0.00063209358f * SCALE), fixedpt_rconst(0.00067317058f * SCALE),
  fixedpt_rconst(0.00071691700f * SCALE), fixedpt_rconst(0.00076350630f * SCALE), fixedpt_rconst(0.00081312324f * SCALE), fixedpt_rconst(0.00086596457f * SCALE),
  fixedpt_rconst(0.00092223983f * SCALE), fixedpt_rconst(0.00098217216f * SCALE), fixedpt_rconst(0.0010459992f * SCALE),  fixedpt_rconst(0.0011139742f * SCALE),
  fixedpt_rconst(0.0011863665f * SCALE),  fixedpt_rconst(0.0012634633f * SCALE),  fixedpt_rconst(0.0013455702f * SCALE),  fixedpt_rconst(0.0014330129f * SCALE),
  fixedpt_rconst(0.0015261382f * SCALE),  fixedpt_rconst(0.0016253153f * SCALE),  fixedpt_rconst(0.0017309374f * SCALE),  fixedpt_rconst(0.0018434235f * SCALE),
  fixedpt_rconst(0.0019632195f * SCALE),  fixedpt_rconst(0.0020908006f * SCALE),  fixedpt_rconst(0.0022266726f * SCALE),  fixedpt_rconst(0.0023713743f * SCALE),
  fixedpt_rconst(0.0025254795f * SCALE),  fixedpt_rconst(0.0026895994f * SCALE),  fixedpt_rconst(0.0028643847f * SCALE),  fixedpt_rconst(0.0030505286f * SCALE),
  fixedpt_rconst(0.0032487691f * SCALE),  fixedpt_rconst(0.0034598925f * SCALE),  fixedpt_rconst(0.0036847358f * SCALE),  fixedpt_rconst(0.0039241906f * SCALE),
  fixedpt_rconst(0.0041792066f * SCALE),  fixedpt_rconst(0.0044507950f * SCALE),  fixedpt_rconst(0.0047400328f * SCALE),  fixedpt_rconst(0.0050480668f * SCALE),
  fixedpt_rconst(0.0053761186f * SCALE),  fixedpt_rconst(0.0057254891f * SCALE),  fixedpt_rconst(0.0060975636f * SCALE),  fixedpt_rconst(0.0064938176f * SCALE),
  fixedpt_rconst(0.0069158225f * SCALE),  fixedpt_rconst(0.0073652516f * SCALE),  fixedpt_rconst(0.0078438871f * SCALE),  fixedpt_rconst(0.0083536271f * SCALE),
  fixedpt_rconst(0.0088964928f * SCALE),  fixedpt_rconst(0.009474637f * SCALE),   fixedpt_rconst(0.010090352f * SCALE),   fixedpt_rconst(0.010746080f * SCALE),
  fixedpt_rconst(0.011444421f * SCALE),   fixedpt_rconst(0.012188144f * SCALE),   fixedpt_rconst(0.012980198f * SCALE),   fixedpt_rconst(0.013823725f * SCALE),
  fixedpt_rconst(0.014722068f * SCALE),   fixedpt_rconst(0.015678791f * SCALE),   fixedpt_rconst(0.016697687f * SCALE),   fixedpt_rconst(0.017782797f * SCALE),
  fixedpt_rconst(0.018938423f * SCALE),   fixedpt_rconst(0.020169149f * SCALE),   fixedpt_rconst(0.021479854f * SCALE),   fixedpt_rconst(0.022875735f * SCALE),
  fixedpt_rconst(0.024362330f * SCALE),   fixedpt_rconst(0.025945531f * SCALE),   fixedpt_rconst(0.027631618f * SCALE),   fixedpt_rconst(0.029427276f * SCALE),
  fixedpt_rconst(0.031339626f * SCALE),   fixedpt_rconst(0.033376252f * SCALE),   fixedpt_rconst(0.035545228f * SCALE),   fixedpt_rconst(0.037855157f * SCALE),
  fixedpt_rconst(0.040315199f * SCALE),   fixedpt_rconst(0.042935108f * SCALE),   fixedpt_rconst(0.045725273f * SCALE),   fixedpt_rconst(0.048696758f * SCALE),
  fixedpt_rconst(0.051861348f * SCALE),   fixedpt_rconst(0.055231591f * SCALE),   fixedpt_rconst(0.058820850f * SCALE),   fixedpt_rconst(0.062643361f * SCALE),
  fixedpt_rconst(0.066714279f * SCALE),   fixedpt_rconst(0.071049749f * SCALE),   fixedpt_rconst(0.075666962f * SCALE),   fixedpt_rconst(0.080584227f * SCALE),
  fixedpt_rconst(0.085821044f * SCALE),   fixedpt_rconst(0.091398179f * SCALE),   fixedpt_rconst(0.097337747f * SCALE),   fixedpt_rconst(0.10366330f * SCALE),
  fixedpt_rconst(0.11039993f * SCALE),    fixedpt_rconst(0.11757434f * SCALE),    fixedpt_rconst(0.12521498f * SCALE),    fixedpt_rconst(0.13335215f * SCALE),
  fixedpt_rconst(0.14201813f * SCALE),    fixedpt_rconst(0.15124727f * SCALE),    fixedpt_rconst(0.16107617f * SCALE),    fixedpt_rconst(0.17154380f * SCALE),
  fixedpt_rconst(0.18269168f * SCALE),    fixedpt_rconst(0.19456402f * SCALE),    fixedpt_rconst(0.20720788f * SCALE),    fixedpt_rconst(0.22067342f * SCALE),
  fixedpt_rconst(0.23501402f * SCALE),    fixedpt_rconst(0.25028656f * SCALE),    fixedpt_rconst(0.26655159f * SCALE),    fixedpt_rconst(0.28387361f * SCALE),
  fixedpt_rconst(0.30232132f * SCALE),    fixedpt_rconst(0.32196786f * SCALE),    fixedpt_rconst(0.34289114f * SCALE),    fixedpt_rconst(0.36517414f * SCALE),
  fixedpt_rconst(0.38890521f * SCALE),    fixedpt_rconst(0.41417847f * SCALE),    fixedpt_rconst(0.44109412f * SCALE),    fixedpt_rconst(0.46975890f * SCALE),
  fixedpt_rconst(0.50028648f * SCALE),    fixedpt_rconst(0.53279791f * SCALE),    fixedpt_rconst(0.56742212f * SCALE),    fixedpt_rconst(0.60429640f * SCALE),
  fixedpt_rconst(0.64356699f * SCALE),    fixedpt_rconst(0.68538959f * SCALE),    fixedpt_rconst(0.72993007f * SCALE),    fixedpt_rconst(0.77736504f * SCALE),
  fixedpt_rconst(0.82788260f * SCALE),    fixedpt_rconst(0.88168307f * SCALE),    fixedpt_rconst(0.9389798f * SCALE),     fixedpt_rconst(1.0f * SCALE)
};


// @OPTIMIZE: if you want to replace this bresenham line-drawing routine,
// note that you must produce bit-identical output to decode correctly;
// this specific sequence of operations is specified in the spec (it's
// drawing integer-quantized frequency-space lines that the encoder
// expects to be exactly the same)
//     ... also, isn't the whole point of Bresenham's algorithm to NOT
// have to divide in the setup? sigh.
#ifndef STB_VORBIS_NO_DEFER_FLOOR
#define LINE_OP(a,b)   a = fixedpt_mul(a, b)
#else
#define LINE_OP(a,b)   a = b
#endif

#ifdef STB_VORBIS_DIVIDE_TABLE
#define DIVTAB_NUMER   32
#define DIVTAB_DENOM   64
int8 integer_divide_table[DIVTAB_NUMER][DIVTAB_DENOM]; // 2KB
#endif

/**
 * @brief Draws a line on a fixed-point output buffer using a modified Bresenham's line algorithm.
 *
 * This function draws a line from the starting point (x0, y0) to the ending point (x1, y1) on a fixed-point output buffer.
 * The line is drawn by iterating over the x-axis and calculating the corresponding y-coordinate for each x.
 * The function uses a modified Bresenham's algorithm to handle both positive and negative slopes efficiently.
 *
 * @param output Pointer to the fixed-point output buffer where the line will be drawn.
 * @param x0 The x-coordinate of the starting point of the line.
 * @param y0 The y-coordinate of the starting point of the line.
 * @param x1 The x-coordinate of the ending point of the line.
 * @param y1 The y-coordinate of the ending point of the line.
 * @param n The maximum x-coordinate value to prevent drawing beyond the buffer's bounds.
 *
 * @note The function uses a precomputed inverse_db_table to map y-coordinates to fixed-point values.
 * @note The function supports both positive and negative slopes and handles division efficiently using a precomputed table if STB_VORBIS_DIVIDE_TABLE is defined.
 */
static __forceinline void draw_line(fixedpt *output, int x0, int y0, int x1, int y1, int n)
{
   int dy = y1 - y0;
   int adx = x1 - x0;
   int ady = abs(dy);
   int base;
   int x=x0,y=y0;
   int err = 0;
   int sy;

#ifdef STB_VORBIS_DIVIDE_TABLE
   if (adx < DIVTAB_DENOM && ady < DIVTAB_NUMER) {
      if (dy < 0) {
         base = -integer_divide_table[ady][adx];
         sy = base-1;
      } else {
         base =  integer_divide_table[ady][adx];
         sy = base+1;
      }
   } else {
      base = dy / adx;
      if (dy < 0)
         sy = base - 1;
      else
         sy = base+1;
   }
#else
   base = dy / adx;
   if (dy < 0)
      sy = base - 1;
   else
      sy = base+1;
#endif
   ady -= abs(base) * adx;
   if (x1 > n) x1 = n;
   if (x < x1) {
      LINE_OP(output[x], inverse_db_table[y&255]);
      for (++x; x < x1; ++x) {
         err += ady;
         if (err >= adx) {
            err -= adx;
            y += sy;
         } else
            y += base;
         LINE_OP(output[x], inverse_db_table[y&255]);
      }
   }
}

/**
 * Decodes residue data using the specified codebook and stores the result in the target array.
 *
 * This method processes residue data based on the provided residue type (rtype). It supports two
 * decoding strategies:
 * - If `rtype` is 0, the method decodes the residue in a step-by-step manner, where each step
 *   corresponds to a portion of the residue data determined by the codebook dimensions.
 * - If `rtype` is non-zero, the method decodes the residue in a continuous manner, iterating
 *   through the residue data in chunks equal to the codebook dimensions.
 *
 * @param f         Pointer to the vorb context, which contains the input data and state.
 * @param book      Pointer to the Codebook used for decoding the residue data.
 * @param target    Pointer to the target array where the decoded data will be stored.
 * @param offset    The starting offset in the target array where the decoded data should be written.
 * @param n         The total number of samples to decode.
 * @param rtype     The residue type, which determines the decoding strategy (0 for step-by-step, non-zero for continuous).
 *
 * @return          Returns TRUE if the decoding process succeeds for all steps or chunks. Returns FALSE if any step or chunk fails to decode.
 */
static int residue_decode(vorb *f, Codebook *book, fixedpt *target, int offset, int n, int rtype)
{
   int k;
   if (rtype == 0) {
      int step = n / book->dimensions;
      for (k=0; k < step; ++k)
         if (!codebook_decode_step(f, book, target+offset+k, n-offset-k, step))
            return FALSE;
   } else {
      for (k=0; k < n; ) {
         if (!codebook_decode(f, book, target+offset, n-k))
            return FALSE;
         k += book->dimensions;
         offset += book->dimensions;
      }
   }
   return TRUE;
}

// n is 1/2 of the blocksize --
// specification: "Correct per-vector decode length is [n]/2"
static void decode_residue(vorb *f, fixedpt *residue_buffers[], int ch, int n, int rn, uint8 *do_not_decode)
{
   int i,j,pass;
   Residue *r = f->residue_config + rn;
   int rtype = f->residue_types[rn];
   int c = r->classbook;
   int classwords = f->codebooks[c].dimensions;
   unsigned int actual_size = rtype == 2 ? n*2 : n;
   unsigned int limit_r_begin = (r->begin < actual_size ? r->begin : actual_size);
   unsigned int limit_r_end   = (r->end   < actual_size ? r->end   : actual_size);
   int n_read = limit_r_end - limit_r_begin;
   int part_read = n_read / r->part_size;
   int temp_alloc_point = temp_alloc_save(f);
   #ifndef STB_VORBIS_DIVIDES_IN_RESIDUE
   uint8 ***part_classdata = (uint8 ***) temp_block_array(f,f->channels, part_read * sizeof(**part_classdata));
   #else
   int **classifications = (int **) temp_block_array(f,f->channels, part_read * sizeof(**classifications));
   #endif

   CHECK(f);

   for (i=0; i < ch; ++i)
      if (!do_not_decode[i])
         memset(residue_buffers[i], 0, sizeof(residue_buffers[0][0]) * n);

   if (rtype == 2 && ch != 1) {
      for (j=0; j < ch; ++j)
         if (!do_not_decode[j])
            break;
      if (j == ch)
         goto done;

      for (pass=0; pass < 8; ++pass) {
         int pcount = 0, class_set = 0;
         if (ch == 2) {
            while (pcount < part_read) {
               int z = r->begin + pcount*r->part_size;
               int c_inter = (z & 1), p_inter = z>>1;
               if (pass == 0) {
                  Codebook *c = f->codebooks+r->classbook;
                  int q;
                  DECODE(q,f,c);
                  if (q == EOP) goto done;
                  #ifndef STB_VORBIS_DIVIDES_IN_RESIDUE
                  part_classdata[0][class_set] = r->classdata[q];
                  #else
                  for (i=classwords-1; i >= 0; --i) {
                     classifications[0][i+pcount] = q % r->classifications;
                     q /= r->classifications;
                  }
                  #endif
               }
               for (i=0; i < classwords && pcount < part_read; ++i, ++pcount) {
                  int z = r->begin + pcount*r->part_size;
                  #ifndef STB_VORBIS_DIVIDES_IN_RESIDUE
                  int c = part_classdata[0][class_set][i];
                  #else
                  int c = classifications[0][pcount];
                  #endif
                  int b = r->residue_books[c][pass];
                  if (b >= 0) {
                     Codebook *book = f->codebooks + b;
                     #ifdef STB_VORBIS_DIVIDES_IN_CODEBOOK
                     if (!codebook_decode_deinterleave_repeat(f, book, residue_buffers, ch, &c_inter, &p_inter, n, r->part_size))
                        goto done;
                     #else
                     // saves 1%
                     if (!codebook_decode_deinterleave_repeat(f, book, residue_buffers, ch, &c_inter, &p_inter, n, r->part_size))
                        goto done;
                     #endif
                  } else {
                     z += r->part_size;
                     c_inter = z & 1;
                     p_inter = z >> 1;
                  }
               }
               #ifndef STB_VORBIS_DIVIDES_IN_RESIDUE
               ++class_set;
               #endif
            }
         } else if (ch > 2) {
            while (pcount < part_read) {
               int z = r->begin + pcount*r->part_size;
               int c_inter = z % ch, p_inter = z/ch;
               if (pass == 0) {
                  Codebook *c = f->codebooks+r->classbook;
                  int q;
                  DECODE(q,f,c);
                  if (q == EOP) goto done;
                  #ifndef STB_VORBIS_DIVIDES_IN_RESIDUE
                  part_classdata[0][class_set] = r->classdata[q];
                  #else
                  for (i=classwords-1; i >= 0; --i) {
                     classifications[0][i+pcount] = q % r->classifications;
                     q /= r->classifications;
                  }
                  #endif
               }
               for (i=0; i < classwords && pcount < part_read; ++i, ++pcount) {
                  int z = r->begin + pcount*r->part_size;
                  #ifndef STB_VORBIS_DIVIDES_IN_RESIDUE
                  int c = part_classdata[0][class_set][i];
                  #else
                  int c = classifications[0][pcount];
                  #endif
                  int b = r->residue_books[c][pass];
                  if (b >= 0) {
                     Codebook *book = f->codebooks + b;
                     if (!codebook_decode_deinterleave_repeat(f, book, residue_buffers, ch, &c_inter, &p_inter, n, r->part_size))
                        goto done;
                  } else {
                     z += r->part_size;
                     c_inter = z % ch;
                     p_inter = z / ch;
                  }
               }
               #ifndef STB_VORBIS_DIVIDES_IN_RESIDUE
               ++class_set;
               #endif
            }
         }
      }
      goto done;
   }
   CHECK(f);

   for (pass=0; pass < 8; ++pass) {
      int pcount = 0, class_set=0;
      while (pcount < part_read) {
         if (pass == 0) {
            for (j=0; j < ch; ++j) {
               if (!do_not_decode[j]) {
                  Codebook *c = f->codebooks+r->classbook;
                  int temp;
                  DECODE(temp,f,c);
                  if (temp == EOP) goto done;
                  #ifndef STB_VORBIS_DIVIDES_IN_RESIDUE
                  part_classdata[j][class_set] = r->classdata[temp];
                  #else
                  for (i=classwords-1; i >= 0; --i) {
                     classifications[j][i+pcount] = temp % r->classifications;
                     temp /= r->classifications;
                  }
                  #endif
               }
            }
         }
         for (i=0; i < classwords && pcount < part_read; ++i, ++pcount) {
            for (j=0; j < ch; ++j) {
               if (!do_not_decode[j]) {
                  #ifndef STB_VORBIS_DIVIDES_IN_RESIDUE
                  int c = part_classdata[j][class_set][i];
                  #else
                  int c = classifications[j][pcount];
                  #endif
                  int b = r->residue_books[c][pass];
                  if (b >= 0) {
                     fixedpt *target = residue_buffers[j];
                     int offset = r->begin + pcount * r->part_size;
                     int n = r->part_size;
                     Codebook *book = f->codebooks + b;
                     if (!residue_decode(f, book, target, offset, n, rtype))
                        goto done;
                  }
               }
            }
         }
         #ifndef STB_VORBIS_DIVIDES_IN_RESIDUE
         ++class_set;
         #endif
      }
   }
  done:
   CHECK(f);
   #ifndef STB_VORBIS_DIVIDES_IN_RESIDUE
   temp_free(f,part_classdata);
   #else
   temp_free(f,classifications);
   #endif
   temp_alloc_restore(f,temp_alloc_point);
}


#if 0
// slow way for debugging
void inverse_mdct_slow(float *buffer, int n)
{
   int i,j;
   int n2 = n >> 1;
   float *x = (float *) malloc(sizeof(*x) * n2);
   memcpy(x, buffer, sizeof(*x) * n2);
   for (i=0; i < n; ++i) {
      float acc = 0;
      for (j=0; j < n2; ++j)
         // formula from paper:
         //acc += n/4.0f * x[j] * (float) cos(M_PI / 2 / n * (2 * i + 1 + n/2.0)*(2*j+1));
         // formula from wikipedia
         //acc += 2.0f / n2 * x[j] * (float) cos(M_PI/n2 * (i + 0.5 + n2/2)*(j + 0.5));
         // these are equivalent, except the formula from the paper inverts the multiplier!
         // however, what actually works is NO MULTIPLIER!?!
         //acc += 64 * 2.0f / n2 * x[j] * (float) cos(M_PI/n2 * (i + 0.5 + n2/2)*(j + 0.5));
         acc += x[j] * (float) cos(M_PI / 2 / n * (2 * i + 1 + n/2.0)*(2*j+1));
      buffer[i] = acc;
   }
   free(x);
}
#elif 0
// same as above, but just barely able to run in real time on modern machines
void inverse_mdct_slow(float *buffer, int n, vorb *f, int blocktype)
{
   float mcos[16384];
   int i,j;
   int n2 = n >> 1, nmask = (n << 2) -1;
   float *x = (float *) malloc(sizeof(*x) * n2);
   memcpy(x, buffer, sizeof(*x) * n2);
   for (i=0; i < 4*n; ++i)
      mcos[i] = (float) cos(M_PI / 2 * i / n);

   for (i=0; i < n; ++i) {
      float acc = 0;
      for (j=0; j < n2; ++j)
         acc += x[j] * mcos[(2 * i + 1 + n2)*(2*j+1) & nmask];
      buffer[i] = acc;
   }
   free(x);
}
#elif 0
// transform to use a slow dct-iv; this is STILL basically trivial,
// but only requires half as many ops
void dct_iv_slow(float *buffer, int n)
{
   float mcos[16384];
   float x[2048];
   int i,j;
   int n2 = n >> 1, nmask = (n << 3) - 1;
   memcpy(x, buffer, sizeof(*x) * n);
   for (i=0; i < 8*n; ++i)
      mcos[i] = (float) cos(M_PI / 4 * i / n);
   for (i=0; i < n; ++i) {
      float acc = 0;
      for (j=0; j < n; ++j)
         acc += x[j] * mcos[((2 * i + 1)*(2*j+1)) & nmask];
      buffer[i] = acc;
   }
}

/**
 * Performs an inverse Modified Discrete Cosine Transform (MDCT) on the given buffer.
 * This method is a slower, more straightforward implementation of the inverse MDCT.
 * 
 * The MDCT is commonly used in audio compression algorithms to transform time-domain
 * data into frequency-domain data. The inverse MDCT transforms the frequency-domain
 * data back into the time-domain.
 *
 * @param buffer     Pointer to the input/output buffer containing the frequency-domain data.
 *                   The result of the inverse MDCT will be stored in this buffer.
 * @param n          The size of the transform, typically the window size. Must be a power of two.
 * @param f          Pointer to a vorb structure, which may contain context or configuration
 *                   data for the transform. This is not used in this implementation.
 * @param blocktype  The type of block being processed, which may influence the transform.
 *                   This is not used in this implementation.
 *
 * The method works as follows:
 * 1. Copies the first half of the input buffer into a temporary array.
 * 2. Applies a type-IV DCT (Discrete Cosine Transform) to the temporary array.
 * 3. Rearranges the transformed data back into the original buffer, applying the necessary
 *    sign changes and shifts to complete the inverse MDCT.
 *
 * The result is the time-domain representation of the input frequency-domain data.
 */
void inverse_mdct_slow(float *buffer, int n, vorb *f, int blocktype)
{
   int i, n4 = n >> 2, n2 = n >> 1, n3_4 = n - n4;
   float temp[4096];

   memcpy(temp, buffer, n2 * sizeof(float));
   dct_iv_slow(temp, n2);  // returns -c'-d, a-b'

   for (i=0; i < n4  ; ++i) buffer[i] = temp[i+n4];            // a-b'
   for (   ; i < n3_4; ++i) buffer[i] = -temp[n3_4 - i - 1];   // b-a', c+d'
   for (   ; i < n   ; ++i) buffer[i] = -temp[i - n3_4];       // c'+d
}
#endif

#ifndef LIBVORBIS_MDCT
#define LIBVORBIS_MDCT 0
#endif

#if LIBVORBIS_MDCT
// directly call the vorbis MDCT using an interface documented
// by Jeff Roberts... useful for performance comparison
typedef struct
{
  int n;
  int log2n;

  float *trig;
  int   *bitrev;

  float scale;
} mdct_lookup;

extern void mdct_init(mdct_lookup *lookup, int n);
extern void mdct_clear(mdct_lookup *l);
extern void mdct_backward(mdct_lookup *init, float *in, float *out);

mdct_lookup M1,M2;

/**
 * Performs the inverse Modified Discrete Cosine Transform (IMDCT) on the given buffer.
 * The IMDCT is used to transform frequency-domain data back to the time-domain, commonly
 * employed in audio decoding processes.
 *
 * This function selects an appropriate MDCT lookup table (`M1` or `M2`) based on the
 * transform size `n`. If neither lookup table is initialized with the required size,
 * it initializes one of them and uses it for the transformation. The function then
 * applies the inverse MDCT to the input buffer using the selected lookup table.
 *
 * @param buffer Pointer to the input/output buffer containing the frequency-domain data.
 *               After the transformation, this buffer will hold the time-domain data.
 * @param n      The size of the transform, which must match the size of the MDCT lookup table.
 * @param f      Pointer to the Vorbis context, which may contain additional configuration
 *               or state information (unused in this implementation).
 * @param blocktype The type of the current block, which may influence the MDCT behavior
 *                  (unused in this implementation).
 */
void inverse_mdct(float *buffer, int n, vorb *f, int blocktype)
{
   mdct_lookup *M;
   if (M1.n == n) M = &M1;
   else if (M2.n == n) M = &M2;
   else if (M1.n == 0) { mdct_init(&M1, n); M = &M1; }
   else {
      if (M2.n) __asm int 3;
      mdct_init(&M2, n);
      M = &M2;
   }

   mdct_backward(M, buffer, buffer);
}
#endif


// the following were split out into separate functions while optimizing;
// they could be pushed back up but eh. __forceinline showed no change;
// they're probably already being inlined.
static void imdct_step3_iter0_loop(int n, fixedpt *e, int i_off, int k_off, fixedpt *A)
{
   fixedpt *ee0 = e + i_off;
   fixedpt *ee2 = ee0 + k_off;
   int i;

   assert((n & 3) == 0);
   for (i=(n>>2); i > 0; --i) {
      fixedpt k00_20, k01_21;
      k00_20  = ee0[ 0] - ee2[ 0];
      k01_21  = ee0[-1] - ee2[-1];
      ee0[ 0] += ee2[ 0];//ee0[ 0] = ee0[ 0] + ee2[ 0];
      ee0[-1] += ee2[-1];//ee0[-1] = ee0[-1] + ee2[-1];
      ee2[ 0] = fixedpt_mul(k00_20,A[0]) - fixedpt_mul(k01_21,A[1]);
      ee2[-1] = fixedpt_mul(k01_21,A[0]) + fixedpt_mul(k00_20,A[1]);
      A += 8;

      k00_20  = ee0[-2] - ee2[-2];
      k01_21  = ee0[-3] - ee2[-3];
      ee0[-2] += ee2[-2];//ee0[-2] = ee0[-2] + ee2[-2];
      ee0[-3] += ee2[-3];//ee0[-3] = ee0[-3] + ee2[-3];
      ee2[-2] = fixedpt_mul(k00_20,A[0]) - fixedpt_mul(k01_21,A[1]);
      ee2[-3] = fixedpt_mul(k01_21,A[0]) + fixedpt_mul(k00_20,A[1]);
      A += 8;

      k00_20  = ee0[-4] - ee2[-4];
      k01_21  = ee0[-5] - ee2[-5];
      ee0[-4] += ee2[-4];//ee0[-4] = ee0[-4] + ee2[-4];
      ee0[-5] += ee2[-5];//ee0[-5] = ee0[-5] + ee2[-5];
      ee2[-4] = fixedpt_mul(k00_20,A[0]) - fixedpt_mul(k01_21,A[1]);
      ee2[-5] = fixedpt_mul(k01_21,A[0]) + fixedpt_mul(k00_20,A[1]);
      A += 8;

      k00_20  = ee0[-6] - ee2[-6];
      k01_21  = ee0[-7] - ee2[-7];
      ee0[-6] += ee2[-6];//ee0[-6] = ee0[-6] + ee2[-6];
      ee0[-7] += ee2[-7];//ee0[-7] = ee0[-7] + ee2[-7];
      ee2[-6] = fixedpt_mul(k00_20,A[0]) - fixedpt_mul(k01_21,A[1]);
      ee2[-7] = fixedpt_mul(k01_21,A[0]) + fixedpt_mul(k00_20,A[1]);
      A += 8;
      ee0 -= 8;
      ee2 -= 8;
   }
}

/**
 * Performs the inner loop of the third step of the Inverse Modified Discrete Cosine Transform (IMDCT).
 * This method processes a block of fixed-point data by applying a series of arithmetic operations
 * to the input arrays `e` and `A`. The method iterates over the input data in chunks of 8 elements,
 * updating the values in the arrays `e0` and `e2` based on the values from the array `A`.
 *
 * @param lim The limit for the loop, typically derived from the size of the input data.
 * @param e Pointer to the input array of fixed-point data.
 * @param d0 Offset applied to the pointer `e` to get the starting position of `e0`.
 * @param k_off Offset applied to the pointer `e0` to get the starting position of `e2`.
 * @param A Pointer to the array of fixed-point coefficients used in the calculations.
 * @param k1 The step size for incrementing the pointer `A` after each iteration.
 */
static void imdct_step3_inner_r_loop(int lim, fixedpt *e, int d0, int k_off, fixedpt *A, int k1)
{
   int i;
   fixedpt k00_20, k01_21;

   fixedpt *e0 = e + d0;
   fixedpt *e2 = e0 + k_off;

   for (i=lim >> 2; i > 0; --i) {
      k00_20 = e0[-0] - e2[-0];
      k01_21 = e0[-1] - e2[-1];
      e0[-0] += e2[-0];//e0[-0] = e0[-0] + e2[-0];
      e0[-1] += e2[-1];//e0[-1] = e0[-1] + e2[-1];
      e2[-0] = fixedpt_mul(k00_20,A[0]) - fixedpt_mul((k01_21),A[1]);
      e2[-1] = fixedpt_mul(k01_21,A[0]) + fixedpt_mul((k00_20),A[1]);

      A += k1;

      k00_20 = e0[-2] - e2[-2];
      k01_21 = e0[-3] - e2[-3];
      e0[-2] += e2[-2];//e0[-2] = e0[-2] + e2[-2];
      e0[-3] += e2[-3];//e0[-3] = e0[-3] + e2[-3];
      e2[-2] = fixedpt_mul(k00_20,A[0]) - fixedpt_mul((k01_21),A[1]);
      e2[-3] = fixedpt_mul(k01_21,A[0]) + fixedpt_mul((k00_20),A[1]);

      A += k1;

      k00_20 = e0[-4] - e2[-4];
      k01_21 = e0[-5] - e2[-5];
      e0[-4] += e2[-4];//e0[-4] = e0[-4] + e2[-4];
      e0[-5] += e2[-5];//e0[-5] = e0[-5] + e2[-5];
      e2[-4] = fixedpt_mul(k00_20,A[0]) - fixedpt_mul((k01_21),A[1]);
      e2[-5] = fixedpt_mul(k01_21,A[0]) + fixedpt_mul((k00_20),A[1]);

      A += k1;

      k00_20 = e0[-6] - e2[-6];
      k01_21 = e0[-7] - e2[-7];
      e0[-6] += e2[-6];//e0[-6] = e0[-6] + e2[-6];
      e0[-7] += e2[-7];//e0[-7] = e0[-7] + e2[-7];
      e2[-6] = fixedpt_mul(k00_20,A[0]) - fixedpt_mul((k01_21),A[1]);
      e2[-7] = fixedpt_mul(k01_21,A[0]) + fixedpt_mul((k00_20),A[1]);

      e0 -= 8;
      e2 -= 8;

      A += k1;
   }
}

/**
 * Performs the inner loop of the third step of the Inverse Modified Discrete Cosine Transform (IMDCT).
 * This method processes the input array `e` in chunks, applying a series of fixed-point arithmetic
 * operations to compute intermediate values. The operations involve adding, subtracting, and multiplying
 * elements of the input array with precomputed coefficients from array `A`.
 *
 * @param n The number of iterations to perform in the loop.
 * @param e The input array containing the data to be processed.
 * @param i_off The initial offset into the input array `e`.
 * @param k_off The offset between the two pointers `ee0` and `ee2` used in the loop.
 * @param A The array of precomputed coefficients used in the multiplication operations.
 * @param a_off The offset used to access elements in the coefficient array `A`.
 * @param k0 The step size used to decrement the pointers `ee0` and `ee2` after each iteration.
 */
static void imdct_step3_inner_s_loop(int n, fixedpt *e, int i_off, int k_off, fixedpt *A, int a_off, int k0)
{
   int i;
   fixedpt A0 = A[0];
   fixedpt A1 = A[0+1];
   fixedpt A2 = A[0+a_off];
   fixedpt A3 = A[0+a_off+1];
   fixedpt A4 = A[0+a_off*2+0];
   fixedpt A5 = A[0+a_off*2+1];
   fixedpt A6 = A[0+a_off*3+0];
   fixedpt A7 = A[0+a_off*3+1];

   fixedpt k00,k11;

   fixedpt *ee0 = e  +i_off;
   fixedpt *ee2 = ee0+k_off;

   for (i=n; i > 0; --i) {
      k00     = ee0[ 0] - ee2[ 0];
      k11     = ee0[-1] - ee2[-1];
      ee0[ 0] =  ee0[ 0] + ee2[ 0];
      ee0[-1] =  ee0[-1] + ee2[-1];
      ee2[ 0] = fixedpt_mul(k00, A0) - fixedpt_mul(k11, A1);
      ee2[-1] = fixedpt_mul(k11, A0) + fixedpt_mul(k00, A1);

      k00     = ee0[-2] - ee2[-2];
      k11     = ee0[-3] - ee2[-3];
      ee0[-2] =  ee0[-2] + ee2[-2];
      ee0[-3] =  ee0[-3] + ee2[-3];
      ee2[-2] = fixedpt_mul(k00, A2) - fixedpt_mul(k11, A3);
      ee2[-3] = fixedpt_mul(k11, A2) + fixedpt_mul(k00, A3);

      k00     = ee0[-4] - ee2[-4];
      k11     = ee0[-5] - ee2[-5];
      ee0[-4] =  ee0[-4] + ee2[-4];
      ee0[-5] =  ee0[-5] + ee2[-5];
      ee2[-4] = fixedpt_mul(k00, A4) - fixedpt_mul(k11, A5);
      ee2[-5] = fixedpt_mul(k11, A4) + fixedpt_mul(k00, A5);

      k00     = ee0[-6] - ee2[-6];
      k11     = ee0[-7] - ee2[-7];
      ee0[-6] =  ee0[-6] + ee2[-6];
      ee0[-7] =  ee0[-7] + ee2[-7];
      ee2[-6] = fixedpt_mul(k00, A6) - fixedpt_mul(k11, A7);
      ee2[-7] = fixedpt_mul(k11, A6) + fixedpt_mul(k00, A7);

      ee0 -= k0;
      ee2 -= k0;
   }
}

/**
 * Performs a specific iteration step (54) on an array of fixed-point values.
 * This method processes the input array `z` in place, performing a series of
 * arithmetic operations to compute intermediate values and update the array.
 * The operations include additions, subtractions, and combinations of these
 * to produce new values for specific indices in the array.
 *
 * The method operates on the following indices of the array `z`:
 * - `z[0]`, `z[-1]`, `z[-2]`, `z[-3]`, `z[-4]`, `z[-5]`, `z[-6]`, `z[-7]`.
 *
 * The steps involve:
 * 1. Computing intermediate values `k00`, `k11`, `k22`, `k33` using differences
 *    between specific elements of `z`.
 * 2. Computing intermediate values `y0`, `y1`, `y2`, `y3` using sums of specific
 *    elements of `z`.
 * 3. Updating the array `z` with new values based on the computed intermediates.
 *
 * The method is optimized for performance using the `__forceinline` directive,
 * ensuring it is inlined by the compiler.
 *
 * @param z Pointer to the array of fixed-point values to be processed.
 *          The array must have at least 8 elements accessible from the pointer.
 */
static __forceinline void iter_54(fixedpt *z)
{
   fixedpt k00,k11,k22,k33;
   fixedpt y0,y1,y2,y3;

   k00  = z[ 0] - z[-4];
   y0   = z[ 0] + z[-4];
   y2   = z[-2] + z[-6];
   k22  = z[-2] - z[-6];

   z[-0] = y0 + y2;      // z0 + z4 + z2 + z6
   z[-2] = y0 - y2;      // z0 + z4 - z2 - z6

   // done with y0,y2

   k33  = z[-3] - z[-7];

   z[-4] = k00 + k33;    // z0 - z4 + z3 - z7
   z[-6] = k00 - k33;    // z0 - z4 - z3 + z7

   // done with k33

   k11  = z[-1] - z[-5];
   y1   = z[-1] + z[-5];
   y3   = z[-3] + z[-7];

   z[-1] = y1 + y3;      // z1 + z5 + z3 + z7
   z[-3] = y1 - y3;      // z1 + z5 - z3 - z7
   z[-5] = k11 - k22;    // z1 - z5 + z2 - z6
   z[-7] = k11 + k22;    // z1 - z5 - z2 + z6
}

/**
 * Performs the inner loop of the IMDCT (Inverse Modified Discrete Cosine Transform) step 3 for the LD65.4 mode.
 * This method processes a block of fixed-point data by applying a series of arithmetic operations and transformations,
 * including additions, subtractions, and multiplications with precomputed coefficients. The method operates on 16-element
 * chunks of the input array, updating the values in place.
 *
 * @param n        The size parameter influencing the loop's range and the number of iterations.
 * @param e        Pointer to the input/output array containing the fixed-point data to be processed.
 * @param i_off    The initial offset into the array `e` where processing begins.
 * @param A        Pointer to the array of precomputed coefficients used in the transformations.
 * @param base_n   The base size parameter used to compute the offset into the coefficient array `A`.
 */
static void imdct_step3_inner_s_loop_ld654(int n, fixedpt *e, int i_off, fixedpt *A, int base_n)
{
   int a_off = base_n >> 3;
   fixedpt A2 = A[0+a_off];
   fixedpt *z = e + i_off;
   fixedpt *base = z - 16 * n;

   while (z > base) {
      fixedpt k00,k11;

      k00   = z[-0] - z[-8];
      k11   = z[-1] - z[-9];
      z[-0] = z[-0] + z[-8];
      z[-1] = z[-1] + z[-9];
      z[-8] =  k00;
      z[-9] =  k11 ;

      k00    = z[ -2] - z[-10];
      k11    = z[ -3] - z[-11];
      z[ -2] = z[ -2] + z[-10];
      z[ -3] = z[ -3] + z[-11];
      z[-10] = fixedpt_mul(k00+k11, A2);
      z[-11] = fixedpt_mul(k11-k00, A2);

      k00    = z[-12] - z[ -4];  // reverse to avoid a unary negation
      k11    = z[ -5] - z[-13];
      z[ -4] = z[ -4] + z[-12];
      z[ -5] = z[ -5] + z[-13];
      z[-12] = k11;
      z[-13] = k00;

      k00    = z[-14] - z[ -6];  // reverse to avoid a unary negation
      k11    = z[ -7] - z[-15];
      z[ -6] = z[ -6] + z[-14];
      z[ -7] = z[ -7] + z[-15];
      z[-14] = fixedpt_mul(k00+k11, A2);
      z[-15] = fixedpt_mul(k00-k11, A2);

      iter_54(z);
      iter_54(z-8);
      z -= 16;
   }
}

/**
 * Performs the inverse Modified Discrete Cosine Transform (IMDCT) on the given buffer.
 * This function is used in audio decoding to transform frequency-domain data back to the time domain.
 *
 * The IMDCT algorithm is based on the paper "The use of multirate filter banks for coding of high quality digital audio".
 * The implementation includes optimizations to reduce computational complexity and improve efficiency.
 *
 * @param buffer    Pointer to the input/output buffer containing the frequency-domain data.
 *                  The transformed time-domain data will be stored here.
 * @param n         The size of the transform. Must be a power of two.
 * @param f         Pointer to the vorb structure containing necessary precomputed data (e.g., twiddle factors).
 * @param blocktype The block type (0 or 1) used to select the appropriate precomputed data.
 *
 * The function performs the following steps:
 * 1. Prepares the buffer and allocates temporary memory.
 * 2. Applies the IMDCT kernel to the input data, including reflection and multiplication with twiddle factors.
 * 3. Executes a series of optimized steps (steps 2-6) to further process the data.
 * 4. Performs bit-reversal and final transformations to produce the time-domain output.
 * 5. Frees temporary memory and restores the allocator state.
 *
 * Note: The function assumes that the input buffer is properly aligned and that the vorb structure
 * contains valid precomputed data for the given block type.
 */
static void inverse_mdct(fixedpt *buffer, int n, vorb *f, int blocktype)
{
   int n2 = n >> 1, n4 = n >> 2, n8 = n >> 3, l;
   int ld;
   // @OPTIMIZE: reduce register pressure by using fewer variables?
   int save_point = temp_alloc_save(f);
   fixedpt *buf2 = (fixedpt *) temp_alloc(f, n2 * sizeof(*buf2));
   fixedpt *u=NULL,*v=NULL;
   // twiddle factors
   fixedpt *A = f->A[blocktype];

   // IMDCT algorithm from "The use of multirate filter banks for coding of high quality digital audio"
   // See notes about bugs in that paper in less-optimal implementation 'inverse_mdct_old' after this function.

   // kernel from paper


   // merged:
   //   copy and reflect spectral data
   //   step 0

   // note that it turns out that the items added together during
   // this step are, in fact, being added to themselves (as reflected
   // by step 0). inexplicable inefficiency! this became obvious
   // once I combined the passes.

   // so there's a missing 'times 2' here (for adding X to itself).
   // this propagates through linearly to the end, where the numbers
   // are 1/2 too small, and need to be compensated for.

   {
      fixedpt *d,*e, *e_stop;
      fixedpt *AA;
      d = &buf2[n2-2];
      AA = A;
      e = &buffer[0];
      e_stop = &buffer[n2];
      while (e != e_stop) {
         d[1] = fixedpt_mul(e[0], AA[0]) - fixedpt_mul(e[2], AA[1]);
         d[0] = fixedpt_mul(e[0], AA[1]) + fixedpt_mul(e[2], AA[0]);
         d -= 2;
         AA += 2;
         e += 4;
      }

      e = &buffer[n2-3];
      while (d >= buf2) {
         d[1] = fixedpt_mul(-e[2], AA[0]) - fixedpt_mul(-e[0], AA[1]);
         d[0] = fixedpt_mul(-e[2], AA[1]) + fixedpt_mul(-e[0], AA[0]);
         d -= 2;
         AA += 2;
         e -= 4;
      }
   }

   // now we use symbolic names for these, so that we can
   // possibly swap their meaning as we change which operations
   // are in place

   u = buffer;
   v = buf2;

   // step 2    (paper output is w, now u)
   // this could be in place, but the data ends up in the wrong
   // place... _somebody_'s got to swap it, so this is nominated
   {
      fixedpt *AA = &A[n2-8];
      fixedpt *d0,*d1, *e0, *e1;

      e0 = &v[n4];
      e1 = &v[0];

      d0 = &u[n4];
      d1 = &u[0];

      while (AA >= A) {
         fixedpt v40_20, v41_21;

         v41_21 = e0[1] - e1[1];
         v40_20 = e0[0] - e1[0];
         d0[1]  = e0[1] + e1[1];
         d0[0]  = e0[0] + e1[0];
         d1[1]  = fixedpt_mul(v41_21, AA[4]) - fixedpt_mul(v40_20, AA[5]);
         d1[0]  = fixedpt_mul(v40_20, AA[4]) + fixedpt_mul(v41_21, AA[5]);

         v41_21 = e0[3] - e1[3];
         v40_20 = e0[2] - e1[2];
         d0[3]  = e0[3] + e1[3];
         d0[2]  = e0[2] + e1[2];
         d1[3]  = fixedpt_mul(v41_21, AA[0]) - fixedpt_mul(v40_20, AA[1]);
         d1[2]  = fixedpt_mul(v40_20, AA[0]) + fixedpt_mul(v41_21, AA[1]);

         AA -= 8;

         d0 += 4;
         d1 += 4;
         e0 += 4;
         e1 += 4;
      }
   }

   // step 3
   ld = ilog(n) - 1; // ilog is off-by-one from normal definitions

   // optimized step 3:

   // the original step3 loop can be nested r inside s or s inside r;
   // it's written originally as s inside r, but this is dumb when r
   // iterates many times, and s few. So I have two copies of it and
   // switch between them halfway.

   // this is iteration 0 of step 3
   imdct_step3_iter0_loop(n >> 4, u, n2-1-n4*0, -(n >> 3), A);
   imdct_step3_iter0_loop(n >> 4, u, n2-1-n4*1, -(n >> 3), A);

   // this is iteration 1 of step 3
   imdct_step3_inner_r_loop(n >> 5, u, n2-1 - n8*0, -(n >> 4), A, 16);
   imdct_step3_inner_r_loop(n >> 5, u, n2-1 - n8*1, -(n >> 4), A, 16);
   imdct_step3_inner_r_loop(n >> 5, u, n2-1 - n8*2, -(n >> 4), A, 16);
   imdct_step3_inner_r_loop(n >> 5, u, n2-1 - n8*3, -(n >> 4), A, 16);

   l=2;
   for (; l < (ld-3)>>1; ++l) {
      int k0 = n >> (l+2), k0_2 = k0>>1;
      int lim = 1 << (l+1);
      int i;
      for (i=0; i < lim; ++i)
         imdct_step3_inner_r_loop(n >> (l+4), u, n2-1 - k0*i, -k0_2, A, 1 << (l+3));
   }

   for (; l < ld-6; ++l) {
      int k0 = n >> (l+2), k1 = 1 << (l+3), k0_2 = k0>>1;
      int rlim = n >> (l+6), r;
      int lim = 1 << (l+1);
      int i_off;
      fixedpt *A0 = A;
      i_off = n2-1;
      for (r=rlim; r > 0; --r) {
         imdct_step3_inner_s_loop(lim, u, i_off, -k0_2, A0, k1, k0);
         A0 += k1*4;
         i_off -= 8;
      }
   }

   // iterations with count:
   //   ld-6,-5,-4 all interleaved together
   //       the big win comes from getting rid of needless flops
   //         due to the constants on pass 5 & 4 being all 1 and 0;
   //       combining them to be simultaneous to improve cache made little difference
   imdct_step3_inner_s_loop_ld654(n >> 5, u, n2-1, A, n);

   // output is u

   // step 4, 5, and 6
   // cannot be in-place because of step 5
   {
      uint16 *bitrev = f->bit_reverse[blocktype];
      // weirdly, I'd have thought reading sequentially and writing
      // erratically would have been better than vice-versa, but in
      // fact that's not what my testing showed. (That is, with
      // j = bitreverse(i), do you read i and write j, or read j and write i.)

      fixedpt *d0 = &v[n4-4];
      fixedpt *d1 = &v[n2-4];
      while (d0 >= v) {
         int k4;

         k4 = bitrev[0];
         d1[3] = u[k4+0];
         d1[2] = u[k4+1];
         d0[3] = u[k4+2];
         d0[2] = u[k4+3];

         k4 = bitrev[1];
         d1[1] = u[k4+0];
         d1[0] = u[k4+1];
         d0[1] = u[k4+2];
         d0[0] = u[k4+3];

         d0 -= 4;
         d1 -= 4;
         bitrev += 2;
      }
   }
   // (paper output is u, now v)


   // data must be in buf2
   assert(v == buf2);

   // step 7   (paper output is v, now v)
   // this is now in place
   {
      fixedpt *C = f->C[blocktype];
      fixedpt *d, *e;

      d = v;
      e = v + n2 - 4;

      while (d < e) {
         fixedpt a02,a11,b0,b1,b2,b3;

         a02 = d[0] - e[2];
         a11 = d[1] + e[3];

         b0 = fixedpt_mul(C[1], a02) + fixedpt_mul(C[0], a11);
         b1 = fixedpt_mul(C[1], a11) - fixedpt_mul(C[0], a02);

         b2 = d[0] + e[ 2];
         b3 = d[1] - e[ 3];

         d[0] = b2 + b0;
         d[1] = b3 + b1;
         e[2] = b2 - b0;
         e[3] = b1 - b3;

         a02 = d[2] - e[0];
         a11 = d[3] + e[1];

         b0 = fixedpt_mul(C[3], a02) + fixedpt_mul(C[2], a11);
         b1 = fixedpt_mul(C[3], a11) - fixedpt_mul(C[2], a02);

         b2 = d[2] + e[ 0];
         b3 = d[3] - e[ 1];

         d[2] = b2 + b0;
         d[3] = b3 + b1;
         e[0] = b2 - b0;
         e[1] = b1 - b3;

         C += 4;
         d += 4;
         e -= 4;
      }
   }

   // data must be in buf2


   // step 8+decode   (paper output is X, now buffer)
   // this generates pairs of data a la 8 and pushes them directly through
   // the decode kernel (pushing rather than pulling) to avoid having
   // to make another pass later

   // this cannot POSSIBLY be in place, so we refer to the buffers directly

   {
      fixedpt *d0,*d1,*d2,*d3;

      fixedpt *B = f->B[blocktype] + n2 - 8;
      fixedpt *e = buf2 + n2 - 8;
      d0 = &buffer[0];
      d1 = &buffer[n2-4];
      d2 = &buffer[n2];
      d3 = &buffer[n-4];
      while (e >= v) {
         fixedpt p0,p1,p2,p3;

         p3 =  fixedpt_mul(e[6], B[7]) - fixedpt_mul(e[7], B[6]);
         p2 = -fixedpt_mul(e[6], B[6]) - fixedpt_mul(e[7], B[7]);

         d0[0] =   p3;
         d1[3] = - p3;
         d2[0] =   p2;
         d3[3] =   p2;

         p1 =  fixedpt_mul(e[4], B[5]) - fixedpt_mul(e[5], B[4]);
         p0 = -fixedpt_mul(e[4], B[4]) - fixedpt_mul(e[5], B[5]);

         d0[1] =   p1;
         d1[2] = - p1;
         d2[1] =   p0;
         d3[2] =   p0;

         p3 =  fixedpt_mul(e[2], B[3]) - fixedpt_mul(e[3], B[2]);
         p2 = -fixedpt_mul(e[2], B[2]) - fixedpt_mul(e[3], B[3]);

         d0[2] =   p3;
         d1[1] = - p3;
         d2[2] =   p2;
         d3[1] =   p2;

         p1 =  fixedpt_mul(e[0], B[1]) - fixedpt_mul(e[1], B[0]);
         p0 = -fixedpt_mul(e[0], B[0]) - fixedpt_mul(e[1], B[1]);

         d0[3] =   p1;
         d1[0] = - p1;
         d2[3] =   p0;
         d3[0] =   p0;

         B -= 8;
         e -= 8;
         d0 += 4;
         d2 += 4;
         d1 -= 4;
         d3 -= 4;
      }
   }

   temp_free(f,buf2);
   temp_alloc_restore(f,save_point);
}

#if 0
// this is the original version of the above code, if you want to optimize it from scratch
void inverse_mdct_naive(float *buffer, int n)
{
   float s;
   float A[1 << 12], B[1 << 12], C[1 << 11];
   int i,k,k2,k4, n2 = n >> 1, n4 = n >> 2, n8 = n >> 3, l;
   int n3_4 = n - n4, ld;
   // how can they claim this only uses N words?!
   // oh, because they're only used sparsely, whoops
   float u[1 << 13], X[1 << 13], v[1 << 13], w[1 << 13];
   // set up twiddle factors

   for (k=k2=0; k < n4; ++k,k2+=2) {
      A[k2  ] = (float)  cos(4*k*M_PI/n);
      A[k2+1] = (float) -sin(4*k*M_PI/n);
      B[k2  ] = (float)  cos((k2+1)*M_PI/n/2);
      B[k2+1] = (float)  sin((k2+1)*M_PI/n/2);
   }
   for (k=k2=0; k < n8; ++k,k2+=2) {
      C[k2  ] = (float)  cos(2*(k2+1)*M_PI/n);
      C[k2+1] = (float) -sin(2*(k2+1)*M_PI/n);
   }

   // IMDCT algorithm from "The use of multirate filter banks for coding of high quality digital audio"
   // Note there are bugs in that pseudocode, presumably due to them attempting
   // to rename the arrays nicely rather than representing the way their actual
   // implementation bounces buffers back and forth. As a result, even in the
   // "some formulars corrected" version, a direct implementation fails. These
   // are noted below as "paper bug".

   // copy and reflect spectral data
   for (k=0; k < n2; ++k) u[k] = buffer[k];
   for (   ; k < n ; ++k) u[k] = -buffer[n - k - 1];
   // kernel from paper
   // step 1
   for (k=k2=k4=0; k < n4; k+=1, k2+=2, k4+=4) {
      v[n-k4-1] = (u[k4] - u[n-k4-1]) * A[k2]   - (u[k4+2] - u[n-k4-3])*A[k2+1];
      v[n-k4-3] = (u[k4] - u[n-k4-1]) * A[k2+1] + (u[k4+2] - u[n-k4-3])*A[k2];
   }
   // step 2
   for (k=k4=0; k < n8; k+=1, k4+=4) {
      w[n2+3+k4] = v[n2+3+k4] + v[k4+3];
      w[n2+1+k4] = v[n2+1+k4] + v[k4+1];
      w[k4+3]    = (v[n2+3+k4] - v[k4+3])*A[n2-4-k4] - (v[n2+1+k4]-v[k4+1])*A[n2-3-k4];
      w[k4+1]    = (v[n2+1+k4] - v[k4+1])*A[n2-4-k4] + (v[n2+3+k4]-v[k4+3])*A[n2-3-k4];
   }
   // step 3
   ld = ilog(n) - 1; // ilog is off-by-one from normal definitions
   for (l=0; l < ld-3; ++l) {
      int k0 = n >> (l+2), k1 = 1 << (l+3);
      int rlim = n >> (l+4), r4, r;
      int s2lim = 1 << (l+2), s2;
      for (r=r4=0; r < rlim; r4+=4,++r) {
         for (s2=0; s2 < s2lim; s2+=2) {
            u[n-1-k0*s2-r4] = w[n-1-k0*s2-r4] + w[n-1-k0*(s2+1)-r4];
            u[n-3-k0*s2-r4] = w[n-3-k0*s2-r4] + w[n-3-k0*(s2+1)-r4];
            u[n-1-k0*(s2+1)-r4] = (w[n-1-k0*s2-r4] - w[n-1-k0*(s2+1)-r4]) * A[r*k1]
                                - (w[n-3-k0*s2-r4] - w[n-3-k0*(s2+1)-r4]) * A[r*k1+1];
            u[n-3-k0*(s2+1)-r4] = (w[n-3-k0*s2-r4] - w[n-3-k0*(s2+1)-r4]) * A[r*k1]
                                + (w[n-1-k0*s2-r4] - w[n-1-k0*(s2+1)-r4]) * A[r*k1+1];
         }
      }
      if (l+1 < ld-3) {
         // paper bug: ping-ponging of u&w here is omitted
         memcpy(w, u, sizeof(u));
      }
   }

   // step 4
   for (i=0; i < n8; ++i) {
      int j = bit_reverse(i) >> (32-ld+3);
      assert(j < n8);
      if (i == j) {
         // paper bug: original code probably swapped in place; if copying,
         //            need to directly copy in this case
         int i8 = i << 3;
         v[i8+1] = u[i8+1];
         v[i8+3] = u[i8+3];
         v[i8+5] = u[i8+5];
         v[i8+7] = u[i8+7];
      } else if (i < j) {
         int i8 = i << 3, j8 = j << 3;
         v[j8+1] = u[i8+1], v[i8+1] = u[j8 + 1];
         v[j8+3] = u[i8+3], v[i8+3] = u[j8 + 3];
         v[j8+5] = u[i8+5], v[i8+5] = u[j8 + 5];
         v[j8+7] = u[i8+7], v[i8+7] = u[j8 + 7];
      }
   }
   // step 5
   for (k=0; k < n2; ++k) {
      w[k] = v[k*2+1];
   }
   // step 6
   for (k=k2=k4=0; k < n8; ++k, k2 += 2, k4 += 4) {
      u[n-1-k2] = w[k4];
      u[n-2-k2] = w[k4+1];
      u[n3_4 - 1 - k2] = w[k4+2];
      u[n3_4 - 2 - k2] = w[k4+3];
   }
   // step 7
   for (k=k2=0; k < n8; ++k, k2 += 2) {
      v[n2 + k2 ] = ( u[n2 + k2] + u[n-2-k2] + C[k2+1]*(u[n2+k2]-u[n-2-k2]) + C[k2]*(u[n2+k2+1]+u[n-2-k2+1]))/2;
      v[n-2 - k2] = ( u[n2 + k2] + u[n-2-k2] - C[k2+1]*(u[n2+k2]-u[n-2-k2]) - C[k2]*(u[n2+k2+1]+u[n-2-k2+1]))/2;
      v[n2+1+ k2] = ( u[n2+1+k2] - u[n-1-k2] + C[k2+1]*(u[n2+1+k2]+u[n-1-k2]) - C[k2]*(u[n2+k2]-u[n-2-k2]))/2;
      v[n-1 - k2] = (-u[n2+1+k2] + u[n-1-k2] + C[k2+1]*(u[n2+1+k2]+u[n-1-k2]) - C[k2]*(u[n2+k2]-u[n-2-k2]))/2;
   }
   // step 8
   for (k=k2=0; k < n4; ++k,k2 += 2) {
      X[k]      = v[k2+n2]*B[k2  ] + v[k2+1+n2]*B[k2+1];
      X[n2-1-k] = v[k2+n2]*B[k2+1] - v[k2+1+n2]*B[k2  ];
   }

   // decode kernel to output
   // determined the following value experimentally
   // (by first figuring out what made inverse_mdct_slow work); then matching that here
   // (probably vorbis encoder premultiplies by n or n/2, to save it on the decoder?)
   s = 0.5; // theoretically would be n4

   // [[[ note! the s value of 0.5 is compensated for by the B[] in the current code,
   //     so it needs to use the "old" B values to behave correctly, or else
   //     set s to 1.0 ]]]
   for (i=0; i < n4  ; ++i) buffer[i] = s * X[i+n4];
   for (   ; i < n3_4; ++i) buffer[i] = -s * X[n3_4 - i - 1];
   for (   ; i < n   ; ++i) buffer[i] = -s * X[i - n3_4];
}
#endif

/**
 * Retrieves the appropriate window buffer for a given length.
 *
 * This function checks if the provided length (after being doubled) matches either
 * of the predefined block sizes (`blocksize_0` or `blocksize_1`) in the `vorb` structure.
 * If a match is found, it returns the corresponding window buffer. If no match is found,
 * it returns `NULL`.
 *
 * @param f Pointer to the `vorb` structure containing the window buffers and block sizes.
 * @param len The length for which the window buffer is requested. This value is doubled
 *            internally before comparison with the block sizes.
 * @return A pointer to the window buffer if a match is found; otherwise, `NULL`.
 */
static fixedpt *get_window(vorb *f, int len)
{
   len <<= 1;
   if (len == f->blocksize_0) return f->window[0];
   if (len == f->blocksize_1) return f->window[1];
   return NULL;
}

#ifndef STB_VORBIS_NO_DEFER_FLOOR
typedef int16 YTYPE;
#else
typedef int YTYPE;
#endif
/**
 * Applies floor curve decoding for a specific channel in a Vorbis audio stream.
 * 
 * This method processes the floor curve data for a given channel `i` in the Vorbis stream `f`. 
 * It decodes the floor curve based on the provided `map` and `n` (number of samples), and updates 
 * the `target` array with the decoded floor curve values. The `finalY` array contains the 
 * decoded floor values for each point, and `step2_flag` indicates which points are valid 
 * for processing. The method handles both deferred and non-deferred floor curve decoding 
 * based on the `STB_VORBIS_NO_DEFER_FLOOR` flag.
 *
 * @param f           Pointer to the Vorbis stream context.
 * @param map         Pointer to the mapping configuration for the current stream.
 * @param i           Index of the channel being processed.
 * @param n           Number of samples to process (half the size of the `target` array).
 * @param target      Array to store the decoded floor curve values.
 * @param finalY      Array of decoded floor values for each point.
 * @param step2_flag  Array indicating which points are valid for processing.
 * @return            Returns `TRUE` on success. If the floor type is invalid, returns an error 
 *                    code via `error()`.
 */
static int do_floor(vorb *f, Mapping *map, int i, int n, fixedpt *target, YTYPE *finalY, uint8 *step2_flag)
{
   int n2 = n >> 1;
   int s = map->chan[i].mux, floor;
   floor = map->submap_floor[s];
   if (f->floor_types[floor] == 0) {
      return error(f, VORBIS_invalid_stream);
   } else {
      Floor1 *g = &f->floor_config[floor].floor1;
      int j,q;
      int lx = 0, ly = finalY[0] * g->floor1_multiplier;
      for (q=1; q < g->values; ++q) {
         j = g->sorted_order[q];
         #ifndef STB_VORBIS_NO_DEFER_FLOOR
         if (finalY[j] >= 0)
         #else
         if (step2_flag[j])
         #endif
         {
            int hy = finalY[j] * g->floor1_multiplier;
            int hx = g->Xlist[j];
            if (lx != hx)
               draw_line(target, lx,ly, hx,hy, n2);
            CHECK(f);
            lx = hx, ly = hy;
         }
      }
      if (lx < n2) {
         // optimization of: draw_line(target, lx,ly, n,ly, n2);
         for (j=lx; j < n2; ++j)
            LINE_OP(target[j], inverse_db_table[ly]);
         CHECK(f);
      }
   }
   return TRUE;
}

// The meaning of "left" and "right"
//
// For a given frame:
//     we compute samples from 0..n
//     window_center is n/2
//     we'll window and mix the samples from left_start to left_end with data from the previous frame
//     all of the samples from left_end to right_start can be output without mixing; however,
//        this interval is 0-length except when transitioning between short and long frames
//     all of the samples from right_start to right_end need to be mixed with the next frame,
//        which we don't have, so those get saved in a buffer
//     frame N's right_end-right_start, the number of samples to mix with the next frame,
//        has to be the same as frame N+1's left_end-left_start (which they are by
//        construction)

static int vorbis_decode_initial(vorb *f, int *p_left_start, int *p_left_end, int *p_right_start, int *p_right_end, int *mode)
{
   Mode *m;
   int i, n, prev, next, window_center;
   f->channel_buffer_start = f->channel_buffer_end = 0;

  retry:
   if (f->eof) return FALSE;
   if (!maybe_start_packet(f))
      return FALSE;
   // check packet type
   if (get_bits(f,1) != 0) {
      if (IS_PUSH_MODE(f))
         return error(f,VORBIS_bad_packet_type);
      while (EOP != get8_packet(f));
      goto retry;
   }

   if (f->alloc.alloc_buffer)
      assert(f->alloc.alloc_buffer_length_in_bytes == f->temp_offset);

   i = get_bits(f, ilog(f->mode_count-1));
   if (i == EOP) return FALSE;
   if (i >= f->mode_count) return FALSE;
   *mode = i;
   m = f->mode_config + i;
   if (m->blockflag) {
      n = f->blocksize_1;
      prev = get_bits(f,1);
      next = get_bits(f,1);
   } else {
      prev = next = 0;
      n = f->blocksize_0;
   }

// WINDOWING

   window_center = n >> 1;
   if (m->blockflag && !prev) {
      *p_left_start = (n - f->blocksize_0) >> 2;
      *p_left_end   = (n + f->blocksize_0) >> 2;
   } else {
      *p_left_start = 0;
      *p_left_end   = window_center;
   }
   if (m->blockflag && !next) {
      *p_right_start = (n*3 - f->blocksize_0) >> 2;
      *p_right_end   = (n*3 + f->blocksize_0) >> 2;
   } else {
      *p_right_start = window_center;
      *p_right_end   = n;
   }

   return TRUE;
}

/**
 * Decodes the remaining part of a Vorbis audio packet after the initial setup.
 * This method handles the decoding of floor data, residue data, inverse coupling,
 * and inverse MDCT (Modified Discrete Cosine Transform) to reconstruct the audio signal.
 * It also manages sample discarding, packet flushing, and position tracking for
 * synchronization with the Ogg container.
 *
 * @param f Pointer to the Vorbis file context.
 * @param len Pointer to store the length of the decoded audio data.
 * @param m Pointer to the current mode configuration.
 * @param left_start Start index of the left window for discarding samples.
 * @param left_end End index of the left window for discarding samples.
 * @param right_start Start index of the right window for discarding samples.
 * @param right_end End index of the right window for discarding samples.
 * @param p_left Pointer to store the updated left window start index.
 * @return TRUE if decoding was successful, otherwise FALSE.
 */
static int vorbis_decode_packet_rest(vorb *f, int *len, Mode *m, int left_start, int left_end, int right_start, int right_end, int *p_left)
{
   Mapping *map;
   int i,j,k,n,n2;
   int zero_channel[256];
   int really_zero_channel[256];

// WINDOWING

   n = f->blocksize[m->blockflag];
   map = &f->mapping[m->mapping];

// FLOORS
   n2 = n >> 1;

   CHECK(f);

   for (i=0; i < f->channels; ++i) {
      int s = map->chan[i].mux, floor;
      zero_channel[i] = FALSE;
      floor = map->submap_floor[s];
      if (f->floor_types[floor] == 0) {
         return error(f, VORBIS_invalid_stream);
      } else {
         Floor1 *g = &f->floor_config[floor].floor1;
         if (get_bits(f, 1)) {
            short *finalY;
            uint8 step2_flag[256];
            static int range_list[4] = { 256, 128, 86, 64 };
            int range = range_list[g->floor1_multiplier-1];
            int offset = 2;
            finalY = f->finalY[i];
            finalY[0] = get_bits(f, ilog(range)-1);
            finalY[1] = get_bits(f, ilog(range)-1);
            for (j=0; j < g->partitions; ++j) {
               int pclass = g->partition_class_list[j];
               int cdim = g->class_dimensions[pclass];
               int cbits = g->class_subclasses[pclass];
               int csub = (1 << cbits)-1;
               int cval = 0;
               if (cbits) {
                  Codebook *c = f->codebooks + g->class_masterbooks[pclass];
                  DECODE(cval,f,c);
               }
               for (k=0; k < cdim; ++k) {
                  int book = g->subclass_books[pclass][cval & csub];
                  cval = cval >> cbits;
                  if (book >= 0) {
                     int temp;
                     Codebook *c = f->codebooks + book;
                     DECODE(temp,f,c);
                     finalY[offset++] = temp;
                  } else
                     finalY[offset++] = 0;
               }
            }
            if (f->valid_bits == INVALID_BITS) goto error; // behavior according to spec
            step2_flag[0] = step2_flag[1] = 1;
            for (j=2; j < g->values; ++j) {
               int low, high, pred, highroom, lowroom, room, val;
               low = g->neighbors[j][0];
               high = g->neighbors[j][1];
               //neighbors(g->Xlist, j, &low, &high);
               pred = predict_point(g->Xlist[j], g->Xlist[low], g->Xlist[high], finalY[low], finalY[high]);
               val = finalY[j];
               highroom = range - pred;
               lowroom = pred;
               if (highroom < lowroom)
                  room = highroom * 2;
               else
                  room = lowroom * 2;
               if (val) {
                  step2_flag[low] = step2_flag[high] = 1;
                  step2_flag[j] = 1;
                  if (val >= room)
                     if (highroom > lowroom)
                        finalY[j] = val - lowroom + pred;
                     else
                        finalY[j] = pred - val + highroom - 1;
                  else
                     if (val & 1)
                        finalY[j] = pred - ((val+1)>>1);
                     else
                        finalY[j] = pred + (val>>1);
               } else {
                  step2_flag[j] = 0;
                  finalY[j] = pred;
               }
            }

#ifdef STB_VORBIS_NO_DEFER_FLOOR
            do_floor(f, map, i, n, f->floor_buffers[i], finalY, step2_flag);
#else
            // defer final floor computation until _after_ residue
            for (j=0; j < g->values; ++j) {
               if (!step2_flag[j])
                  finalY[j] = -1;
            }
#endif
         } else {
           error:
            zero_channel[i] = TRUE;
         }
         // So we just defer everything else to later

         // at this point we've decoded the floor into buffer
      }
   }
   CHECK(f);
   // at this point we've decoded all floors

   if (f->alloc.alloc_buffer)
      assert(f->alloc.alloc_buffer_length_in_bytes == f->temp_offset);

   // re-enable coupled channels if necessary
   memcpy(really_zero_channel, zero_channel, sizeof(really_zero_channel[0]) * f->channels);
   for (i=0; i < map->coupling_steps; ++i)
      if (!zero_channel[map->chan[i].magnitude] || !zero_channel[map->chan[i].angle]) {
         zero_channel[map->chan[i].magnitude] = zero_channel[map->chan[i].angle] = FALSE;
      }

   CHECK(f);
// RESIDUE DECODE
   for (i=0; i < map->submaps; ++i) {
      fixedpt *residue_buffers[STB_VORBIS_MAX_CHANNELS];
      int r;
      uint8 do_not_decode[256];
      int ch = 0;
      for (j=0; j < f->channels; ++j) {
         if (map->chan[j].mux == i) {
            if (zero_channel[j]) {
               do_not_decode[ch] = TRUE;
               residue_buffers[ch] = NULL;
            } else {
               do_not_decode[ch] = FALSE;
               residue_buffers[ch] = f->channel_buffers[j];
            }
            ++ch;
         }
      }
      r = map->submap_residue[i];
      decode_residue(f, residue_buffers, ch, n2, r, do_not_decode);
   }

   if (f->alloc.alloc_buffer)
      assert(f->alloc.alloc_buffer_length_in_bytes == f->temp_offset);
   CHECK(f);

// INVERSE COUPLING
   for (i = map->coupling_steps-1; i >= 0; --i) {
      int n2 = n >> 1;
      fixedpt *m = f->channel_buffers[map->chan[i].magnitude];
      fixedpt *a = f->channel_buffers[map->chan[i].angle    ];
      for (j=0; j < n2; ++j) {
         fixedpt a2,m2;
         if (m[j] > 0)
            if (a[j] > 0)
               m2 = m[j], a2 = m[j] - a[j];
            else
               a2 = m[j], m2 = m[j] + a[j];
         else
            if (a[j] > 0)
               m2 = m[j], a2 = m[j] + a[j];
            else
               a2 = m[j], m2 = m[j] - a[j];
         m[j] = m2;
         a[j] = a2;
      }
   }
   CHECK(f);

   // finish decoding the floors
#ifndef STB_VORBIS_NO_DEFER_FLOOR
   for (i=0; i < f->channels; ++i) {
      if (really_zero_channel[i]) {
         memset(f->channel_buffers[i], 0, sizeof(*f->channel_buffers[i]) * n2);
      } else {
         do_floor(f, map, i, n, f->channel_buffers[i], f->finalY[i], NULL);
      }
   }
#else
   for (i=0; i < f->channels; ++i) {
      if (really_zero_channel[i]) {
         memset(f->channel_buffers[i], 0, sizeof(*f->channel_buffers[i]) * n2);
      } else {
         for (j=0; j < n2; ++j)
            f->channel_buffers[i][j] = fixedpt_mul(f->channel_buffers[i][j], f->floor_buffers[i][j]);
      }
   }
#endif

// INVERSE MDCT
   CHECK(f);
   for (i=0; i < f->channels; ++i)
      inverse_mdct(f->channel_buffers[i], n, f, m->blockflag);
   CHECK(f);

   // this shouldn't be necessary, unless we exited on an error
   // and want to flush to get to the next packet
   flush_packet(f);

   if (f->first_decode) {
      // assume we start so first non-discarded sample is sample 0
      // this isn't to spec, but spec would require us to read ahead
      // and decode the size of all current frames--could be done,
      // but presumably it's not a commonly used feature
      f->current_loc = -n2; // start of first frame is positioned for discard
      // we might have to discard samples "from" the next frame too,
      // if we're lapping a large block then a small at the start?
      f->discard_samples_deferred = n - right_end;
      f->current_loc_valid = TRUE;
      f->first_decode = FALSE;
   } else if (f->discard_samples_deferred) {
      if (f->discard_samples_deferred >= right_start - left_start) {
         f->discard_samples_deferred -= (right_start - left_start);
         left_start = right_start;
         *p_left = left_start;
      } else {
         left_start += f->discard_samples_deferred;
         *p_left = left_start;
         f->discard_samples_deferred = 0;
      }
   } else if (f->previous_length == 0 && f->current_loc_valid) {
      // we're recovering from a seek... that means we're going to discard
      // the samples from this packet even though we know our position from
      // the last page header, so we need to update the position based on
      // the discarded samples here
      // but wait, the code below is going to add this in itself even
      // on a discard, so we don't need to do it here...
   }

   // check if we have ogg information about the sample # for this packet
   if (f->last_seg_which == f->end_seg_with_known_loc) {
      // if we have a valid current loc, and this is final:
      if (f->current_loc_valid && (f->page_flag & PAGEFLAG_last_page)) {
         uint32 current_end = f->known_loc_for_packet;
         // then let's infer the size of the (probably) short final frame
         if (current_end < f->current_loc + (right_end-left_start)) {
            if (current_end < f->current_loc) {
               // negative truncation, that's impossible!
               *len = 0;
            } else {
               *len = current_end - f->current_loc;
            }
            *len += left_start; // this doesn't seem right, but has no ill effect on my test files
            if (*len > right_end) *len = right_end; // this should never happen
            f->current_loc += *len;
            return TRUE;
         }
      }
      // otherwise, just set our sample loc
      // guess that the ogg granule pos refers to the _middle_ of the
      // last frame?
      // set f->current_loc to the position of left_start
      f->current_loc = f->known_loc_for_packet - (n2-left_start);
      f->current_loc_valid = TRUE;
   }
   if (f->current_loc_valid)
      f->current_loc += (right_start - left_start);

   if (f->alloc.alloc_buffer)
      assert(f->alloc.alloc_buffer_length_in_bytes == f->temp_offset);
   *len = right_end;  // ignore samples after the window goes to 0
   CHECK(f);

   return TRUE;
}

/**
 * Decodes a Vorbis audio packet into PCM samples.
 *
 * This method processes a single Vorbis packet, extracting the audio data and converting it into
 * PCM format. It first initializes the decoding process by determining the mode, left end, and right end
 * of the packet. If initialization is successful, it proceeds to decode the rest of the packet using the
 * specified mode configuration.
 *
 * @param f Pointer to the Vorbis file context, containing the necessary state and configuration.
 * @param len Pointer to an integer where the length of the decoded PCM data will be stored.
 * @param p_left Pointer to an integer representing the left channel's starting position.
 * @param p_right Pointer to an integer representing the right channel's starting position.
 *
 * @return Returns 1 if the packet was successfully decoded, and 0 if the decoding process failed
 *         during initialization or packet decoding.
 */
static int vorbis_decode_packet(vorb *f, int *len, int *p_left, int *p_right)
{
   int mode, left_end, right_end;
   if (!vorbis_decode_initial(f, p_left, &left_end, p_right, &right_end, &mode)) return 0;
   return vorbis_decode_packet_rest(f, len, f->mode_config + mode, *p_left, left_end, *p_right, right_end, p_left);
}

/**
 * Finalizes the processing of a Vorbis audio frame by performing windowing and mixing operations.
 * This method handles the transition between consecutive frames by applying a windowing function
 * to smoothly blend the end of the previous frame with the start of the current frame. It also
 * prepares the current frame's data for the next frame by saving the relevant portion of the
 * current frame as the "previous window."
 *
 * @param f         Pointer to the stb_vorbis structure containing the decoder state and buffers.
 * @param len       The length of the current frame's data.
 * @param left      The starting index of the left-window sin() region, indicating where to begin
 *                  mixing from the previous frame's data.
 * @param right     The starting index of the right-window sin() region, indicating where to end
 *                  the returned data and where to start saving the current frame's data for the
 *                  next frame.
 *
 * @return          The number of valid samples produced by this frame (right - left). Returns 0
 *                  if there was no previous frame or if an error occurred during processing.
 */
static int vorbis_finish_frame(stb_vorbis *f, int len, int left, int right)
{
   int prev,i,j;
   // we use right&left (the start of the right- and left-window sin()-regions)
   // to determine how much to return, rather than inferring from the rules
   // (same result, clearer code); 'left' indicates where our sin() window
   // starts, therefore where the previous window's right edge starts, and
   // therefore where to start mixing from the previous buffer. 'right'
   // indicates where our sin() ending-window starts, therefore that's where
   // we start saving, and where our returned-data ends.

   // mixin from previous window
   if (f->previous_length) {
      int i,j, n = f->previous_length;
      fixedpt *w = get_window(f, n);
      if (w == NULL) return 0;
      for (i=0; i < f->channels; ++i) {
         for (j=0; j < n; ++j)
            f->channel_buffers[i][left+j] =
               fixedpt_mul(f->channel_buffers[i][left+j], w[    j]) +
               fixedpt_mul(f->previous_window[i][     j], w[n-1-j]);
      }
   }

   prev = f->previous_length;

   // last half of this data becomes previous window
   f->previous_length = len - right;

   // @OPTIMIZE: could avoid this copy by double-buffering the
   // output (flipping previous_window with channel_buffers), but
   // then previous_window would have to be 2x as large, and
   // channel_buffers couldn't be temp mem (although they're NOT
   // currently temp mem, they could be (unless we want to level
   // performance by spreading out the computation))
   for (i=0; i < f->channels; ++i)
      for (j=0; right+j < len; ++j)
         f->previous_window[i][j] = f->channel_buffers[i][right+j];

   if (!prev)
      // there was no previous packet, so this data isn't valid...
      // this isn't entirely true, only the would-have-overlapped data
      // isn't valid, but this seems to be what the spec requires
      return 0;

   // truncate a short frame
   if (len < right) right = len;

   f->samples_output += right-left;

   return right - left;
}

/**
 * Decodes the first frame of a Vorbis audio stream and processes it.
 *
 * This method attempts to decode the first frame of the Vorbis audio stream
 * associated with the given `stb_vorbis` context. It calls `vorbis_decode_packet`
 * to decode the packet, which returns the length of the decoded data and the
 * left and right boundaries of the frame. If the decoding is successful (i.e.,
 * `res` is non-zero), the method finalizes the frame by calling `vorbis_finish_frame`
 * with the decoded length and boundaries.
 *
 * @param f Pointer to the `stb_vorbis` context representing the Vorbis stream.
 * @return Returns the result of the packet decoding. A non-zero value indicates
 *         success, while zero indicates failure or the end of the stream.
 */
static int vorbis_pump_first_frame(stb_vorbis *f)
{
   int len, right, left, res;
   res = vorbis_decode_packet(f, &len, &left, &right);
   if (res)
      vorbis_finish_frame(f, len, left, right);
   return res;
}

#ifndef STB_VORBIS_NO_PUSHDATA_API
/**
 * Checks if a complete Vorbis packet is present in the given `stb_vorbis` stream.
 * This function parses the Ogg stream to ensure that the entire packet is available
 * before further processing. It handles cases where packets span multiple Ogg pages
 * and validates the stream's integrity.
 *
 * The function iterates through the segments of the current page and checks if the
 * packet ends within the page or continues onto the next page. It ensures that the
 * stream contains enough data to process the packet and validates the Ogg page header
 * and segment table. If the packet crosses page boundaries, it continues parsing the
 * next page to ensure the entire packet is present.
 *
 * @param f Pointer to the `stb_vorbis` structure representing the Vorbis stream.
 * @return Returns `TRUE` if a complete packet is present, or an error code if the
 *         stream is invalid or more data is needed.
 */
static int is_whole_packet_present(stb_vorbis *f)
{
   // make sure that we have the packet available before continuing...
   // this requires a full ogg parse, but we know we can fetch from f->stream

   // instead of coding this out explicitly, we could save the current read state,
   // read the next packet with get8() until end-of-packet, check f->eof, then
   // reset the state? but that would be slower, esp. since we'd have over 256 bytes
   // of state to restore (primarily the page segment table)

   int s = f->next_seg, first = TRUE;
   uint8 *p = f->stream;

   if (s != -1) { // if we're not starting the packet with a 'continue on next page' flag
      for (; s < f->segment_count; ++s) {
         p += f->segments[s];
         if (f->segments[s] < 255)               // stop at first short segment
            break;
      }
      // either this continues, or it ends it...
      if (s == f->segment_count)
         s = -1; // set 'crosses page' flag
      if (p > f->stream_end)                     return error(f, VORBIS_need_more_data);
      first = FALSE;
   }
   for (; s == -1;) {
      uint8 *q;
      int n;

      // check that we have the page header ready
      if (p + 26 >= f->stream_end)               return error(f, VORBIS_need_more_data);
      // validate the page
      if (memcmp(p, ogg_page_header, 4))         return error(f, VORBIS_invalid_stream);
      if (p[4] != 0)                             return error(f, VORBIS_invalid_stream);
      if (first) { // the first segment must NOT have 'continued_packet', later ones MUST
         if (f->previous_length)
            if ((p[5] & PAGEFLAG_continued_packet))  return error(f, VORBIS_invalid_stream);
         // if no previous length, we're resynching, so we can come in on a continued-packet,
         // which we'll just drop
      } else {
         if (!(p[5] & PAGEFLAG_continued_packet)) return error(f, VORBIS_invalid_stream);
      }
      n = p[26]; // segment counts
      q = p+27;  // q points to segment table
      p = q + n; // advance past header
      // make sure we've read the segment table
      if (p > f->stream_end)                     return error(f, VORBIS_need_more_data);
      for (s=0; s < n; ++s) {
         p += q[s];
         if (q[s] < 255)
            break;
      }
      if (s == n)
         s = -1; // set 'crosses page' flag
      if (p > f->stream_end)                     return error(f, VORBIS_need_more_data);
      first = FALSE;
   }
   return TRUE;
}
#endif // !STB_VORBIS_NO_PUSHDATA_API

static int start_decoder(vorb *f)
{
   uint8 header[6], x,y;
   int len,i,j,k, max_submaps = 0;
   int longest_floorlist=0;

   // first page, first packet
   f->first_decode = TRUE;

   if (!start_page(f))                              return FALSE;
   // validate page flag
   if (!(f->page_flag & PAGEFLAG_first_page))       return error(f, VORBIS_invalid_first_page);
   if (f->page_flag & PAGEFLAG_last_page)           return error(f, VORBIS_invalid_first_page);
   if (f->page_flag & PAGEFLAG_continued_packet)    return error(f, VORBIS_invalid_first_page);
   // check for expected packet length
   if (f->segment_count != 1)                       return error(f, VORBIS_invalid_first_page);
   if (f->segments[0] != 30) {
      // check for the Ogg skeleton fishead identifying header to refine our error
      if (f->segments[0] == 64 &&
          getn(f, header, 6) &&
          header[0] == 'f' &&
          header[1] == 'i' &&
          header[2] == 's' &&
          header[3] == 'h' &&
          header[4] == 'e' &&
          header[5] == 'a' &&
          get8(f)   == 'd' &&
          get8(f)   == '\0')                        return error(f, VORBIS_ogg_skeleton_not_supported);
      else
                                                    return error(f, VORBIS_invalid_first_page);
   }

   // read packet
   // check packet header
   if (get8(f) != VORBIS_packet_id)                 return error(f, VORBIS_invalid_first_page);
   if (!getn(f, header, 6))                         return error(f, VORBIS_unexpected_eof);
   if (!vorbis_validate(header))                    return error(f, VORBIS_invalid_first_page);
   // vorbis_version
   if (get32(f) != 0)                               return error(f, VORBIS_invalid_first_page);
   f->channels = get8(f); if (!f->channels)         return error(f, VORBIS_invalid_first_page);
   if (f->channels > STB_VORBIS_MAX_CHANNELS)       return error(f, VORBIS_too_many_channels);
   f->sample_rate = get32(f); if (!f->sample_rate)  return error(f, VORBIS_invalid_first_page);
   get32(f); // bitrate_maximum
   get32(f); // bitrate_nominal
   get32(f); // bitrate_minimum
   x = get8(f);
   {
      int log0,log1;
      log0 = x & 15;
      log1 = x >> 4;
      f->blocksize_0 = 1 << log0;
      f->blocksize_1 = 1 << log1;
      if (log0 < 6 || log0 > 13)                       return error(f, VORBIS_invalid_setup);
      if (log1 < 6 || log1 > 13)                       return error(f, VORBIS_invalid_setup);
      if (log0 > log1)                                 return error(f, VORBIS_invalid_setup);
   }

   // framing_flag
   x = get8(f);
   if (!(x & 1))                                    return error(f, VORBIS_invalid_first_page);

   // second packet!
   if (!start_page(f))                              return FALSE;

   if (!start_packet(f))                            return FALSE;

   if (!next_segment(f))                            return FALSE;

   if (get8_packet(f) != VORBIS_packet_comment)            return error(f, VORBIS_invalid_setup);
   for (i=0; i < 6; ++i) header[i] = get8_packet(f);
   if (!vorbis_validate(header))                    return error(f, VORBIS_invalid_setup);
   //file vendor
   len = get32_packet(f);
   f->vendor = (char*)setup_malloc(f, sizeof(char) * (len+1));
   if (f->vendor == NULL)                           return error(f, VORBIS_outofmem);
   for(i=0; i < len; ++i) {
      f->vendor[i] = get8_packet(f);
   }
   f->vendor[len] = (char)'\0';
   //user comments
   f->comment_list_length = get32_packet(f);
   f->comment_list = (char**)setup_malloc(f, sizeof(char*) * (f->comment_list_length + 1));
   if (f->comment_list == NULL)                     return error(f, VORBIS_outofmem);

   for(i=0; i < f->comment_list_length; ++i) {
      len = get32_packet(f);
      f->comment_list[i] = (char*)setup_malloc(f, sizeof(char) * (len+1));
      if (f->comment_list[i] == NULL)               return error(f, VORBIS_outofmem);

      for(j=0; j < len; ++j) {
         f->comment_list[i][j] = get8_packet(f);
      }
      f->comment_list[i][len] = (char)'\0';
   }

   // framing_flag
   x = get8_packet(f);
   if (!(x & 1))                                    return error(f, VORBIS_invalid_setup);


   skip(f, f->bytes_in_seg);
   f->bytes_in_seg = 0;

   do {
      len = next_segment(f);
      skip(f, len);
      f->bytes_in_seg = 0;
   } while (len);

   // third packet!
   if (!start_packet(f))                            return FALSE;

   #ifndef STB_VORBIS_NO_PUSHDATA_API
   if (IS_PUSH_MODE(f)) {
      if (!is_whole_packet_present(f)) {
         // convert error in ogg header to write type
         if (f->error == VORBIS_invalid_stream)
            f->error = VORBIS_invalid_setup;
         return FALSE;
      }
   }
   #endif

   crc32_init(); // always init it, to avoid multithread race conditions

   if (get8_packet(f) != VORBIS_packet_setup)       return error(f, VORBIS_invalid_setup);
   for (i=0; i < 6; ++i) header[i] = get8_packet(f);
   if (!vorbis_validate(header))                    return error(f, VORBIS_invalid_setup);

   // codebooks

   f->codebook_count = get_bits(f,8) + 1;
   f->codebooks = (Codebook *) setup_malloc(f, sizeof(*f->codebooks) * f->codebook_count);
   if (f->codebooks == NULL)                        return error(f, VORBIS_outofmem);
   memset(f->codebooks, 0, sizeof(*f->codebooks) * f->codebook_count);
   for (i=0; i < f->codebook_count; ++i) {
      uint32 *values;
      int ordered, sorted_count;
      int total=0;
      uint8 *lengths;
      Codebook *c = f->codebooks+i;
      CHECK(f);
      x = get_bits(f, 8); if (x != 0x42)            return error(f, VORBIS_invalid_setup);
      x = get_bits(f, 8); if (x != 0x43)            return error(f, VORBIS_invalid_setup);
      x = get_bits(f, 8); if (x != 0x56)            return error(f, VORBIS_invalid_setup);
      x = get_bits(f, 8);
      c->dimensions = (get_bits(f, 8)<<8) + x;
      x = get_bits(f, 8);
      y = get_bits(f, 8);
      c->entries = (get_bits(f, 8)<<16) + (y<<8) + x;
      ordered = get_bits(f,1);
      c->sparse = ordered ? 0 : get_bits(f,1);

      if (c->dimensions == 0 && c->entries != 0)    return error(f, VORBIS_invalid_setup);

      if (c->sparse)
         lengths = (uint8 *) setup_temp_malloc(f, c->entries);
      else
         lengths = c->codeword_lengths = (uint8 *) setup_malloc(f, c->entries);

      if (!lengths) return error(f, VORBIS_outofmem);

      if (ordered) {
         int current_entry = 0;
         int current_length = get_bits(f,5) + 1;
         while (current_entry < c->entries) {
            int limit = c->entries - current_entry;
            int n = get_bits(f, ilog(limit));
            if (current_length >= 32) return error(f, VORBIS_invalid_setup);
            if (current_entry + n > (int) c->entries) { return error(f, VORBIS_invalid_setup); }
            memset(lengths + current_entry, current_length, n);
            current_entry += n;
            ++current_length;
         }
      } else {
         for (j=0; j < c->entries; ++j) {
            int present = c->sparse ? get_bits(f,1) : 1;
            if (present) {
               lengths[j] = get_bits(f, 5) + 1;
               ++total;
               if (lengths[j] == 32)
                  return error(f, VORBIS_invalid_setup);
            } else {
               lengths[j] = NO_CODE;
            }
         }
      }

      if (c->sparse && total >= c->entries >> 2) {
         // convert sparse items to non-sparse!
         if (c->entries > (int) f->setup_temp_memory_required)
            f->setup_temp_memory_required = c->entries;

         c->codeword_lengths = (uint8 *) setup_malloc(f, c->entries);
         if (c->codeword_lengths == NULL) return error(f, VORBIS_outofmem);
         memcpy(c->codeword_lengths, lengths, c->entries);
         setup_temp_free(f, lengths, c->entries); // note this is only safe if there have been no intervening temp mallocs!
         lengths = c->codeword_lengths;
         c->sparse = 0;
      }

      // compute the size of the sorted tables
      if (c->sparse) {
         sorted_count = total;
      } else {
         sorted_count = 0;
         #ifndef STB_VORBIS_NO_HUFFMAN_BINARY_SEARCH
         for (j=0; j < c->entries; ++j)
            if (lengths[j] > STB_VORBIS_FAST_HUFFMAN_LENGTH && lengths[j] != NO_CODE)
               ++sorted_count;
         #endif
      }

      c->sorted_entries = sorted_count;
      values = NULL;

      CHECK(f);
      if (!c->sparse) {
         c->codewords = (uint32 *) setup_malloc(f, sizeof(c->codewords[0]) * c->entries);
         if (!c->codewords)                  return error(f, VORBIS_outofmem);
      } else {
         unsigned int size;
         if (c->sorted_entries) {
            c->codeword_lengths = (uint8 *) setup_malloc(f, c->sorted_entries);
            if (!c->codeword_lengths)           return error(f, VORBIS_outofmem);
            c->codewords = (uint32 *) setup_temp_malloc(f, sizeof(*c->codewords) * c->sorted_entries);
            if (!c->codewords)                  return error(f, VORBIS_outofmem);
            values = (uint32 *) setup_temp_malloc(f, sizeof(*values) * c->sorted_entries);
            if (!values)                        return error(f, VORBIS_outofmem);
         }
         size = c->entries + (sizeof(*c->codewords) + sizeof(*values)) * c->sorted_entries;
         if (size > f->setup_temp_memory_required)
            f->setup_temp_memory_required = size;
      }

      if (!compute_codewords(c, lengths, c->entries, values)) {
         if (c->sparse) setup_temp_free(f, values, 0);
         return error(f, VORBIS_invalid_setup);
      }

      if (c->sorted_entries) {
         // allocate an extra slot for sentinels
         c->sorted_codewords = (uint32 *) setup_malloc(f, sizeof(*c->sorted_codewords) * (c->sorted_entries+1));
         if (c->sorted_codewords == NULL) return error(f, VORBIS_outofmem);
         // allocate an extra slot at the front so that c->sorted_values[-1] is defined
         // so that we can catch that case without an extra if
         c->sorted_values    = ( int   *) setup_malloc(f, sizeof(*c->sorted_values   ) * (c->sorted_entries+1));
         if (c->sorted_values == NULL) return error(f, VORBIS_outofmem);
         ++c->sorted_values;
         c->sorted_values[-1] = -1;
         compute_sorted_huffman(c, lengths, values);
      }

      if (c->sparse) {
         setup_temp_free(f, values, sizeof(*values)*c->sorted_entries);
         setup_temp_free(f, c->codewords, sizeof(*c->codewords)*c->sorted_entries);
         setup_temp_free(f, lengths, c->entries);
         c->codewords = NULL;
      }

      compute_accelerated_huffman(c);

      CHECK(f);
      c->lookup_type = get_bits(f, 4);
      if (c->lookup_type > 2) return error(f, VORBIS_invalid_setup);
      if (c->lookup_type > 0) {
         uint16 *mults;
         c->minimum_value = float32_unpack(get_bits(f, 32));
         c->delta_value = float32_unpack(get_bits(f, 32));
         c->value_bits = get_bits(f, 4)+1;
         c->sequence_p = get_bits(f,1);
         if (c->lookup_type == 1) {
            int values = lookup1_values(c->entries, c->dimensions);
            if (values < 0) return error(f, VORBIS_invalid_setup);
            c->lookup_values = (uint32) values;
         } else {
            c->lookup_values = c->entries * c->dimensions;
         }
         if (c->lookup_values == 0) return error(f, VORBIS_invalid_setup);
         mults = (uint16 *) setup_temp_malloc(f, sizeof(mults[0]) * c->lookup_values);
         if (mults == NULL) return error(f, VORBIS_outofmem);
         for (j=0; j < (int) c->lookup_values; ++j) {
            int q = get_bits(f, c->value_bits);
            if (q == EOP) { setup_temp_free(f,mults,sizeof(mults[0])*c->lookup_values); return error(f, VORBIS_invalid_setup); }
            mults[j] = q;
         }

#ifndef STB_VORBIS_DIVIDES_IN_CODEBOOK
         if (c->lookup_type == 1) {
            int len, sparse = c->sparse;
            fixedpt last=0;
            // pre-expand the lookup1-style multiplicands, to avoid a divide in the inner loop
            if (sparse) {
               if (c->sorted_entries == 0) goto skip;
               c->multiplicands = (codetype *) setup_malloc(f, sizeof(c->multiplicands[0]) * c->sorted_entries * c->dimensions);
            } else
               c->multiplicands = (codetype *) setup_malloc(f, sizeof(c->multiplicands[0]) * c->entries        * c->dimensions);
            if (c->multiplicands == NULL) { setup_temp_free(f,mults,sizeof(mults[0])*c->lookup_values); return error(f, VORBIS_outofmem); }
            len = sparse ? c->sorted_entries : c->entries;
            for (j=0; j < len; ++j) {
               unsigned int z = sparse ? c->sorted_values[j] : j;
               unsigned int div=1;
               for (k=0; k < c->dimensions; ++k) {
                  int off = (z / div) % c->lookup_values;
                  fixedpt val = fixedpt_muli(c->delta_value, mults[off]) + c->minimum_value + last;

                  c->multiplicands[j*c->dimensions + k] = val;
                  if (c->sequence_p)
                     last = val;
                  if (k+1 < c->dimensions) {
                     if (div > UINT_MAX / (unsigned int) c->lookup_values) {
                        setup_temp_free(f, mults,sizeof(mults[0])*c->lookup_values);
                        return error(f, VORBIS_invalid_setup);
                     }
                     div *= c->lookup_values;
                  }
               }
            }
            c->lookup_type = 2;
         }
         else
#endif
         {
            fixedpt last=0;
            CHECK(f);
            c->multiplicands = (codetype *) setup_malloc(f, sizeof(c->multiplicands[0]) * c->lookup_values);
            if (c->multiplicands == NULL) { setup_temp_free(f, mults,sizeof(mults[0])*c->lookup_values); return error(f, VORBIS_outofmem); }
            for (j=0; j < (int) c->lookup_values; ++j) {
               fixedpt val = fixedpt_muli(c->delta_value, mults[j]) + c->minimum_value + last;
               c->multiplicands[j] = val;
               if (c->sequence_p)
                  last = val;
            }
         }
#ifndef STB_VORBIS_DIVIDES_IN_CODEBOOK
        skip:;
#endif
         setup_temp_free(f, mults, sizeof(mults[0])*c->lookup_values);

         CHECK(f);
      }
      CHECK(f);
   }

   // time domain transfers (notused)

   x = get_bits(f, 6) + 1;
   for (i=0; i < x; ++i) {
      uint32 z = get_bits(f, 16);
      if (z != 0) return error(f, VORBIS_invalid_setup);
   }

   // Floors
   f->floor_count = get_bits(f, 6)+1;
   f->floor_config = (Floor *)  setup_malloc(f, f->floor_count * sizeof(*f->floor_config));
   if (f->floor_config == NULL) return error(f, VORBIS_outofmem);
   for (i=0; i < f->floor_count; ++i) {
      f->floor_types[i] = get_bits(f, 16);
      if (f->floor_types[i] > 1) return error(f, VORBIS_invalid_setup);
      if (f->floor_types[i] == 0) {
         Floor0 *g = &f->floor_config[i].floor0;
         g->order = get_bits(f,8);
         g->rate = get_bits(f,16);
         g->bark_map_size = get_bits(f,16);
         g->amplitude_bits = get_bits(f,6);
         g->amplitude_offset = get_bits(f,8);
         g->number_of_books = get_bits(f,4) + 1;
         for (j=0; j < g->number_of_books; ++j)
            g->book_list[j] = get_bits(f,8);
         return error(f, VORBIS_feature_not_supported);
      } else {
         stbv__floor_ordering p[31*8+2];
         Floor1 *g = &f->floor_config[i].floor1;
         int max_class = -1;
         g->partitions = get_bits(f, 5);
         for (j=0; j < g->partitions; ++j) {
            g->partition_class_list[j] = get_bits(f, 4);
            if (g->partition_class_list[j] > max_class)
               max_class = g->partition_class_list[j];
         }
         for (j=0; j <= max_class; ++j) {
            g->class_dimensions[j] = get_bits(f, 3)+1;
            g->class_subclasses[j] = get_bits(f, 2);
            if (g->class_subclasses[j]) {
               g->class_masterbooks[j] = get_bits(f, 8);
               if (g->class_masterbooks[j] >= f->codebook_count) return error(f, VORBIS_invalid_setup);
            }
            for (k=0; k < 1 << g->class_subclasses[j]; ++k) {
               g->subclass_books[j][k] = get_bits(f,8)-1;
               if (g->subclass_books[j][k] >= f->codebook_count) return error(f, VORBIS_invalid_setup);
            }
         }
         g->floor1_multiplier = get_bits(f,2)+1;
         g->rangebits = get_bits(f,4);
         g->Xlist[0] = 0;
         g->Xlist[1] = 1 << g->rangebits;
         g->values = 2;
         for (j=0; j < g->partitions; ++j) {
            int c = g->partition_class_list[j];
            for (k=0; k < g->class_dimensions[c]; ++k) {
               g->Xlist[g->values] = get_bits(f, g->rangebits);
               ++g->values;
            }
         }
         // precompute the sorting
         for (j=0; j < g->values; ++j) {
            p[j].x = g->Xlist[j];
            p[j].id = j;
         }
         qsort(p, g->values, sizeof(p[0]), point_compare);
         for (j=0; j < g->values-1; ++j)
            if (p[j].x == p[j+1].x)
               return error(f, VORBIS_invalid_setup);
         for (j=0; j < g->values; ++j)
            g->sorted_order[j] = (uint8) p[j].id;
         // precompute the neighbors
         for (j=2; j < g->values; ++j) {
            int low = 0,hi = 0;
            neighbors(g->Xlist, j, &low,&hi);
            g->neighbors[j][0] = low;
            g->neighbors[j][1] = hi;
         }

         if (g->values > longest_floorlist)
            longest_floorlist = g->values;
      }
   }

   // Residue
   f->residue_count = get_bits(f, 6)+1;
   f->residue_config = (Residue *) setup_malloc(f, f->residue_count * sizeof(f->residue_config[0]));
   if (f->residue_config == NULL) return error(f, VORBIS_outofmem);
   memset(f->residue_config, 0, f->residue_count * sizeof(f->residue_config[0]));
   for (i=0; i < f->residue_count; ++i) {
      uint8 residue_cascade[64];
      Residue *r = f->residue_config+i;
      f->residue_types[i] = get_bits(f, 16);
      if (f->residue_types[i] > 2) return error(f, VORBIS_invalid_setup);
      r->begin = get_bits(f, 24);
      r->end = get_bits(f, 24);
      if (r->end < r->begin) return error(f, VORBIS_invalid_setup);
      r->part_size = get_bits(f,24)+1;
      r->classifications = get_bits(f,6)+1;
      r->classbook = get_bits(f,8);
      if (r->classbook >= f->codebook_count) return error(f, VORBIS_invalid_setup);
      for (j=0; j < r->classifications; ++j) {
         uint8 high_bits=0;
         uint8 low_bits=get_bits(f,3);
         if (get_bits(f,1))
            high_bits = get_bits(f,5);
         residue_cascade[j] = high_bits*8 + low_bits;
      }
      r->residue_books = (short (*)[8]) setup_malloc(f, sizeof(r->residue_books[0]) * r->classifications);
      if (r->residue_books == NULL) return error(f, VORBIS_outofmem);
      for (j=0; j < r->classifications; ++j) {
         for (k=0; k < 8; ++k) {
            if (residue_cascade[j] & (1 << k)) {
               r->residue_books[j][k] = get_bits(f, 8);
               if (r->residue_books[j][k] >= f->codebook_count) return error(f, VORBIS_invalid_setup);
            } else {
               r->residue_books[j][k] = -1;
            }
         }
      }
      // precompute the classifications[] array to avoid inner-loop mod/divide
      // call it 'classdata' since we already have r->classifications
      r->classdata = (uint8 **) setup_malloc(f, sizeof(*r->classdata) * f->codebooks[r->classbook].entries);
      if (!r->classdata) return error(f, VORBIS_outofmem);
      memset(r->classdata, 0, sizeof(*r->classdata) * f->codebooks[r->classbook].entries);
      for (j=0; j < f->codebooks[r->classbook].entries; ++j) {
         int classwords = f->codebooks[r->classbook].dimensions;
         int temp = j;
         r->classdata[j] = (uint8 *) setup_malloc(f, sizeof(r->classdata[j][0]) * classwords);
         if (r->classdata[j] == NULL) return error(f, VORBIS_outofmem);
         for (k=classwords-1; k >= 0; --k) {
            r->classdata[j][k] = temp % r->classifications;
            temp /= r->classifications;
         }
      }
   }

   f->mapping_count = get_bits(f,6)+1;
   f->mapping = (Mapping *) setup_malloc(f, f->mapping_count * sizeof(*f->mapping));
   if (f->mapping == NULL) return error(f, VORBIS_outofmem);
   memset(f->mapping, 0, f->mapping_count * sizeof(*f->mapping));
   for (i=0; i < f->mapping_count; ++i) {
      Mapping *m = f->mapping + i;
      int mapping_type = get_bits(f,16);
      if (mapping_type != 0) return error(f, VORBIS_invalid_setup);
      m->chan = (MappingChannel *) setup_malloc(f, f->channels * sizeof(*m->chan));
      if (m->chan == NULL) return error(f, VORBIS_outofmem);
      if (get_bits(f,1))
         m->submaps = get_bits(f,4)+1;
      else
         m->submaps = 1;
      if (m->submaps > max_submaps)
         max_submaps = m->submaps;
      if (get_bits(f,1)) {
         m->coupling_steps = get_bits(f,8)+1;
         if (m->coupling_steps > f->channels) return error(f, VORBIS_invalid_setup);
         for (k=0; k < m->coupling_steps; ++k) {
            m->chan[k].magnitude = get_bits(f, ilog(f->channels-1));
            m->chan[k].angle = get_bits(f, ilog(f->channels-1));
            if (m->chan[k].magnitude >= f->channels)        return error(f, VORBIS_invalid_setup);
            if (m->chan[k].angle     >= f->channels)        return error(f, VORBIS_invalid_setup);
            if (m->chan[k].magnitude == m->chan[k].angle)   return error(f, VORBIS_invalid_setup);
         }
      } else
         m->coupling_steps = 0;

      // reserved field
      if (get_bits(f,2)) return error(f, VORBIS_invalid_setup);
      if (m->submaps > 1) {
         for (j=0; j < f->channels; ++j) {
            m->chan[j].mux = get_bits(f, 4);
            if (m->chan[j].mux >= m->submaps)                return error(f, VORBIS_invalid_setup);
         }
      } else
         // @SPECIFICATION: this case is missing from the spec
         for (j=0; j < f->channels; ++j)
            m->chan[j].mux = 0;

      for (j=0; j < m->submaps; ++j) {
         get_bits(f,8); // discard
         m->submap_floor[j] = get_bits(f,8);
         m->submap_residue[j] = get_bits(f,8);
         if (m->submap_floor[j] >= f->floor_count)      return error(f, VORBIS_invalid_setup);
         if (m->submap_residue[j] >= f->residue_count)  return error(f, VORBIS_invalid_setup);
      }
   }

   // Modes
   f->mode_count = get_bits(f, 6)+1;
   for (i=0; i < f->mode_count; ++i) {
      Mode *m = f->mode_config+i;
      m->blockflag = get_bits(f,1);
      m->windowtype = get_bits(f,16);
      m->transformtype = get_bits(f,16);
      m->mapping = get_bits(f,8);
      if (m->windowtype != 0)                 return error(f, VORBIS_invalid_setup);
      if (m->transformtype != 0)              return error(f, VORBIS_invalid_setup);
      if (m->mapping >= f->mapping_count)     return error(f, VORBIS_invalid_setup);
   }

   flush_packet(f);

   f->previous_length = 0;

   for (i=0; i < f->channels; ++i) {
      f->channel_buffers[i] = (fixedpt *) setup_malloc(f, sizeof(fixedpt) * f->blocksize_1);
      f->previous_window[i] = (fixedpt *) setup_malloc(f, sizeof(fixedpt) * f->blocksize_1/2);
      f->finalY[i]          = (int16 *) setup_malloc(f, sizeof(int16) * longest_floorlist);
      if (f->channel_buffers[i] == NULL || f->previous_window[i] == NULL || f->finalY[i] == NULL) return error(f, VORBIS_outofmem);
      memset(f->channel_buffers[i], 0, sizeof(float) * f->blocksize_1);
      #ifdef STB_VORBIS_NO_DEFER_FLOOR
      f->floor_buffers[i]   = (fixedpt *) setup_malloc(f, sizeof(f->floor_buffers[0][0]) * f->blocksize_1/2);
      if (f->floor_buffers[i] == NULL) return error(f, VORBIS_outofmem);
      #endif
   }

   if (!init_blocksize(f, 0, f->blocksize_0)) return FALSE;
   if (!init_blocksize(f, 1, f->blocksize_1)) return FALSE;
   f->blocksize[0] = f->blocksize_0;
   f->blocksize[1] = f->blocksize_1;

#ifdef STB_VORBIS_DIVIDE_TABLE
   if (integer_divide_table[1][1]==0)
      for (i=0; i < DIVTAB_NUMER; ++i)
         for (j=1; j < DIVTAB_DENOM; ++j)
            integer_divide_table[i][j] = i / j;
#endif

   // compute how much temporary memory is needed

   // 1.
   {
      uint32 imdct_mem = (f->blocksize_1 * sizeof(float) >> 1);
      uint32 classify_mem;
      int i,max_part_read=0;
      for (i=0; i < f->residue_count; ++i) {
         Residue *r = f->residue_config + i;
         unsigned int actual_size = f->blocksize_1 / 2;
         unsigned int limit_r_begin = r->begin < actual_size ? r->begin : actual_size;
         unsigned int limit_r_end   = r->end   < actual_size ? r->end   : actual_size;
         int n_read = limit_r_end - limit_r_begin;
         int part_read = n_read / r->part_size;
         if (part_read > max_part_read)
            max_part_read = part_read;
      }
      #ifndef STB_VORBIS_DIVIDES_IN_RESIDUE
      classify_mem = f->channels * (sizeof(void*) + max_part_read * sizeof(uint8 *));
      #else
      classify_mem = f->channels * (sizeof(void*) + max_part_read * sizeof(int *));
      #endif

      // maximum reasonable partition size is f->blocksize_1

      f->temp_memory_required = classify_mem;
      if (imdct_mem > f->temp_memory_required)
         f->temp_memory_required = imdct_mem;
   }


   if (f->alloc.alloc_buffer) {
      assert(f->temp_offset == f->alloc.alloc_buffer_length_in_bytes);
      // check if there's enough temp memory so we don't error later
      if (f->setup_offset + sizeof(*f) + f->temp_memory_required > (unsigned) f->temp_offset)
         return error(f, VORBIS_outofmem);
   }

   // @TODO: stb_vorbis_seek_start expects first_audio_page_offset to point to a page
   // without PAGEFLAG_continued_packet, so this either points to the first page, or
   // the page after the end of the headers. It might be cleaner to point to a page
   // in the middle of the headers, when that's the page where the first audio packet
   // starts, but we'd have to also correctly skip the end of any continued packet in
   // stb_vorbis_seek_start.
   if (f->next_seg == -1) {
      f->first_audio_page_offset = stb_vorbis_get_file_offset(f);
   } else {
      f->first_audio_page_offset = 0;
   }

   return TRUE;
}

/**
 * @brief Deinitializes and frees all dynamically allocated memory associated with a `stb_vorbis` instance.
 *
 * This function is responsible for cleaning up the resources allocated during the initialization
 * and decoding process of a Vorbis audio stream. It frees memory for various components, including:
 * - The vendor string and comment list.
 * - Residue configurations and their associated class data.
 * - Codebooks, including codewords, multiplicands, and sorted values.
 * - Floor and residue configurations.
 * - Mapping configurations and channel buffers.
 * - Internal buffers such as `channel_buffers`, `previous_window`, `finalY`, and others.
 * - Additional buffers like `A`, `B`, `C`, `window`, and `bit_reverse`.
 * If the `STB_VORBIS_NO_STDIO` macro is not defined, it also closes the associated file if the
 * `close_on_free` flag is set.
 *
 * @param p Pointer to the `stb_vorbis` instance to deinitialize.
 */
static void vorbis_deinit(stb_vorbis *p)
{
   int i,j;

   setup_free(p, p->vendor);
   for (i=0; i < p->comment_list_length; ++i) {
      setup_free(p, p->comment_list[i]);
   }
   setup_free(p, p->comment_list);

   if (p->residue_config) {
      for (i=0; i < p->residue_count; ++i) {
         Residue *r = p->residue_config+i;
         if (r->classdata) {
            for (j=0; j < p->codebooks[r->classbook].entries; ++j)
               setup_free(p, r->classdata[j]);
            setup_free(p, r->classdata);
         }
         setup_free(p, r->residue_books);
      }
   }

   if (p->codebooks) {
      CHECK(p);
      for (i=0; i < p->codebook_count; ++i) {
         Codebook *c = p->codebooks + i;
         setup_free(p, c->codeword_lengths);
         setup_free(p, c->multiplicands);
         setup_free(p, c->codewords);
         setup_free(p, c->sorted_codewords);
         // c->sorted_values[-1] is the first entry in the array
         setup_free(p, c->sorted_values ? c->sorted_values-1 : NULL);
      }
      setup_free(p, p->codebooks);
   }
   setup_free(p, p->floor_config);
   setup_free(p, p->residue_config);
   if (p->mapping) {
      for (i=0; i < p->mapping_count; ++i)
         setup_free(p, p->mapping[i].chan);
      setup_free(p, p->mapping);
   }
   CHECK(p);
   for (i=0; i < p->channels && i < STB_VORBIS_MAX_CHANNELS; ++i) {
      setup_free(p, p->channel_buffers[i]);
      setup_free(p, p->previous_window[i]);
      #ifdef STB_VORBIS_NO_DEFER_FLOOR
      setup_free(p, p->floor_buffers[i]);
      #endif
      setup_free(p, p->finalY[i]);
   }
   for (i=0; i < 2; ++i) {
      setup_free(p, p->A[i]);
      setup_free(p, p->B[i]);
      setup_free(p, p->C[i]);
      setup_free(p, p->window[i]);
      setup_free(p, p->bit_reverse[i]);
   }
   #ifndef STB_VORBIS_NO_STDIO
   if (p->close_on_free) fclose(p->f);
   #endif
}

/**
 * Closes and deallocates a stb_vorbis instance, releasing all associated resources.
 * 
 * This function first checks if the provided stb_vorbis pointer `p` is NULL. If it is,
 * the function returns immediately without performing any operations. Otherwise, it
 * calls `vorbis_deinit` to release internal Vorbis decoder resources and then calls
 * `setup_free` to free the memory allocated for the stb_vorbis instance itself.
 * 
 * @param p A pointer to the stb_vorbis instance to be closed and deallocated.
 *          If NULL, the function does nothing.
 */
void stb_vorbis_close(stb_vorbis *p)
{
   if (p == NULL) return;
   vorbis_deinit(p);
   setup_free(p,p);
}

/**
 * Initializes a stb_vorbis structure with default values and optional allocator settings.
 *
 * This function sets up a stb_vorbis instance by zeroing out all its fields, ensuring that
 * all pointers are initially NULL. If an allocator configuration (`z`) is provided, it is
 * copied into the stb_vorbis instance, and the allocator's buffer length is aligned to an
 * 8-byte boundary. The function also initializes internal state variables such as `eof`,
 * `error`, and `page_crc_tests` to their default values. If the `STB_VORBIS_NO_STDIO` macro
 * is not defined, it initializes file-related fields to their defaults.
 *
 * @param p Pointer to the stb_vorbis structure to be initialized.
 * @param z Pointer to a stb_vorbis_alloc structure containing allocator settings. If NULL,
 *          the allocator fields in `p` are not modified.
 */
static void vorbis_init(stb_vorbis *p, const stb_vorbis_alloc *z)
{
   memset(p, 0, sizeof(*p)); // NULL out all malloc'd pointers to start
   if (z) {
      p->alloc = *z;
      p->alloc.alloc_buffer_length_in_bytes &= ~7;
      p->temp_offset = p->alloc.alloc_buffer_length_in_bytes;
   }
   p->eof = 0;
   p->error = VORBIS__no_error;
   p->stream = NULL;
   p->codebooks = NULL;
   p->page_crc_tests = -1;
   #ifndef STB_VORBIS_NO_STDIO
   p->close_on_free = FALSE;
   p->f = NULL;
   #endif
}

/**
 * Retrieves the current sample offset within the Vorbis stream.
 *
 * This function returns the current sample position in the stream if the position is valid.
 * If the position is not valid (e.g., the stream is not being decoded or the position is unknown),
 * the function returns -1.
 *
 * @param f A pointer to the stb_vorbis structure representing the Vorbis stream.
 * @return The current sample offset if valid, otherwise -1.
 */
int stb_vorbis_get_sample_offset(stb_vorbis *f)
{
   if (f->current_loc_valid)
      return f->current_loc;
   else
      return -1;
}

/**
 * Retrieves the metadata and configuration information for a given Vorbis audio stream.
 * This function populates a `stb_vorbis_info` structure with details about the audio stream,
 * including the number of channels, sample rate, and memory requirements for decoding.
 *
 * @param f A pointer to the `stb_vorbis` structure representing the Vorbis stream.
 * @return A `stb_vorbis_info` structure containing the following fields:
 *         - `channels`: The number of audio channels in the stream.
 *         - `sample_rate`: The sample rate of the audio stream in Hz.
 *         - `setup_memory_required`: The amount of memory required for the initial setup.
 *         - `setup_temp_memory_required`: The amount of temporary memory required during setup.
 *         - `temp_memory_required`: The amount of temporary memory required during decoding.
 *         - `max_frame_size`: The maximum size of a single frame in the stream.
 */
stb_vorbis_info stb_vorbis_get_info(stb_vorbis *f)
{
   stb_vorbis_info d;
   d.channels = f->channels;
   d.sample_rate = f->sample_rate;
   d.setup_memory_required = f->setup_memory_required;
   d.setup_temp_memory_required = f->setup_temp_memory_required;
   d.temp_memory_required = f->temp_memory_required;
   d.max_frame_size = f->blocksize_1 >> 1;
   return d;
}

/**
 * Retrieves the comment metadata from a given stb_vorbis file.
 *
 * This function extracts the vendor string, the number of comments, and the list of comments
 * from the provided stb_vorbis file structure and returns them as a stb_vorbis_comment structure.
 * The returned structure contains a copy of the metadata, allowing the caller to access the
 * comment information without modifying the original file structure.
 *
 * @param f A pointer to the stb_vorbis file structure from which to retrieve the comments.
 * @return A stb_vorbis_comment structure containing the vendor string, the number of comments,
 *         and the list of comments associated with the file.
 */
stb_vorbis_comment stb_vorbis_get_comment(stb_vorbis *f)
{
   stb_vorbis_comment d;
   d.vendor = f->vendor;
   d.comment_list_length = f->comment_list_length;
   d.comment_list = f->comment_list;
   return d;
}

/**
 * Retrieves the last error code from the given `stb_vorbis` instance and resets the error state.
 *
 * This function checks the `error` field of the `stb_vorbis` structure pointed to by `f`, stores its value,
 * and then resets the `error` field to `VORBIS__no_error` to indicate that no error is currently present.
 * The stored error code is returned to the caller, allowing them to handle or log the error as needed.
 *
 * @param f A pointer to the `stb_vorbis` instance from which to retrieve the error code.
 * @return The last error code that was stored in the `stb_vorbis` instance. If no error occurred,
 *         the return value will be `VORBIS__no_error`.
 */
int stb_vorbis_get_error(stb_vorbis *f)
{
   int e = f->error;
   f->error = VORBIS__no_error;
   return e;
}

/**
 * Allocates memory for a new stb_vorbis structure.
 *
 * This function allocates memory for a new stb_vorbis structure using the provided
 * stb_vorbis instance `f` to determine the appropriate memory allocation settings.
 * The size of the allocated memory is equal to the size of the stb_vorbis structure.
 *
 * @param f A pointer to an existing stb_vorbis instance used for memory allocation settings.
 * @return A pointer to the newly allocated stb_vorbis structure, or NULL if the allocation fails.
 */
static stb_vorbis * vorbis_alloc(stb_vorbis *f)
{
   stb_vorbis *p = (stb_vorbis *) setup_malloc(f, sizeof(*p));
   return p;
}

#ifndef STB_VORBIS_NO_PUSHDATA_API

/**
 * Resets the internal state of the stb_vorbis decoder to prepare for new pushdata operations.
 * This function clears various internal fields, such as previous_length, page_crc_tests,
 * discard_samples_deferred, and others, ensuring that the decoder is in a clean state for
 * processing new data. It is typically called when the decoder needs to start processing
 * a new stream or when the existing stream is being reset. This ensures that no residual
 * state from previous operations affects the decoding of new data.
 *
 * @param f Pointer to the stb_vorbis instance to be reset.
 */
void stb_vorbis_flush_pushdata(stb_vorbis *f)
{
   f->previous_length = 0;
   f->page_crc_tests  = 0;
   f->discard_samples_deferred = 0;
   f->current_loc_valid = FALSE;
   f->first_decode = FALSE;
   f->samples_output = 0;
   f->channel_buffer_start = 0;
   f->channel_buffer_end = 0;
}

/**
 * Searches for a valid Ogg page within the provided data buffer and updates the internal state
 * of the Vorbis decoder accordingly. This function is used in push-data mode, where data is
 * provided incrementally, and the decoder needs to identify page boundaries and validate them
 * using CRC checks.
 *
 * The function performs the following steps:
 * 1. Resets the `bytes_done` counter for all active page CRC tests.
 * 2. Searches for the Ogg page header (starting with the byte 0x4f) in the provided data.
 * 3. If a potential page header is found, it verifies the header's completeness and computes
 *    the expected CRC value for the page.
 * 4. If the CRC matches the expected value, the function updates the decoder's state to reflect
 *    the new page, including the sample location and other metadata.
 * 5. If the CRC does not match, the function discards the page and continues searching.
 *
 * @param f Pointer to the Vorbis decoder context.
 * @param data Pointer to the data buffer containing the Ogg stream data.
 * @param data_len Length of the data buffer.
 * @return The number of bytes consumed from the data buffer. If a valid page is found, this
 *         value indicates the position where the page ends. If no valid page is found, the
 *         entire buffer length is returned.
 */
static int vorbis_search_for_page_pushdata(vorb *f, uint8 *data, int data_len)
{
   int i,n;
   for (i=0; i < f->page_crc_tests; ++i)
      f->scan[i].bytes_done = 0;

   // if we have room for more scans, search for them first, because
   // they may cause us to stop early if their header is incomplete
   if (f->page_crc_tests < STB_VORBIS_PUSHDATA_CRC_COUNT) {
      if (data_len < 4) return 0;
      data_len -= 3; // need to look for 4-byte sequence, so don't miss
                     // one that straddles a boundary
      for (i=0; i < data_len; ++i) {
         if (data[i] == 0x4f) {
            if (0==memcmp(data+i, ogg_page_header, 4)) {
               int j,len;
               uint32 crc;
               // make sure we have the whole page header
               if (i+26 >= data_len || i+27+data[i+26] >= data_len) {
                  // only read up to this page start, so hopefully we'll
                  // have the whole page header start next time
                  data_len = i;
                  break;
               }
               // ok, we have it all; compute the length of the page
               len = 27 + data[i+26];
               for (j=0; j < data[i+26]; ++j)
                  len += data[i+27+j];
               // scan everything up to the embedded crc (which we must 0)
               crc = 0;
               for (j=0; j < 22; ++j)
                  crc = crc32_update(crc, data[i+j]);
               // now process 4 0-bytes
               for (   ; j < 26; ++j)
                  crc = crc32_update(crc, 0);
               // len is the total number of bytes we need to scan
               n = f->page_crc_tests++;
               f->scan[n].bytes_left = len-j;
               f->scan[n].crc_so_far = crc;
               f->scan[n].goal_crc = data[i+22] + (data[i+23] << 8) + (data[i+24]<<16) + (data[i+25]<<24);
               // if the last frame on a page is continued to the next, then
               // we can't recover the sample_loc immediately
               if (data[i+27+data[i+26]-1] == 255)
                  f->scan[n].sample_loc = ~0;
               else
                  f->scan[n].sample_loc = data[i+6] + (data[i+7] << 8) + (data[i+ 8]<<16) + (data[i+ 9]<<24);
               f->scan[n].bytes_done = i+j;
               if (f->page_crc_tests == STB_VORBIS_PUSHDATA_CRC_COUNT)
                  break;
               // keep going if we still have room for more
            }
         }
      }
   }

   for (i=0; i < f->page_crc_tests;) {
      uint32 crc;
      int j;
      int n = f->scan[i].bytes_done;
      int m = f->scan[i].bytes_left;
      if (m > data_len - n) m = data_len - n;
      // m is the bytes to scan in the current chunk
      crc = f->scan[i].crc_so_far;
      for (j=0; j < m; ++j)
         crc = crc32_update(crc, data[n+j]);
      f->scan[i].bytes_left -= m;
      f->scan[i].crc_so_far = crc;
      if (f->scan[i].bytes_left == 0) {
         // does it match?
         if (f->scan[i].crc_so_far == f->scan[i].goal_crc) {
            // Houston, we have page
            data_len = n+m; // consumption amount is wherever that scan ended
            f->page_crc_tests = -1; // drop out of page scan mode
            f->previous_length = 0; // decode-but-don't-output one frame
            f->next_seg = -1;       // start a new page
            f->current_loc = f->scan[i].sample_loc; // set the current sample location
                                    // to the amount we'd have decoded had we decoded this page
            f->current_loc_valid = f->current_loc != ~0U;
            return data_len;
         }
         // delete entry
         f->scan[i] = f->scan[--f->page_crc_tests];
      } else {
         ++i;
      }
   }

   return data_len;
}

// return value: number of bytes we used
int stb_vorbis_decode_frame_pushdata(
         stb_vorbis *f,                   // the file we're decoding
         const uint8 *data, int data_len, // the memory available for decoding
         int *channels,                   // place to write number of float * buffers
         fixedpt ***output,                 // place to write float ** array of float * buffers
         int *samples                     // place to write number of output samples
     )
{
   int i;
   int len,right,left;

   if (!IS_PUSH_MODE(f)) return error(f, VORBIS_invalid_api_mixing);

   if (f->page_crc_tests >= 0) {
      *samples = 0;
      return vorbis_search_for_page_pushdata(f, (uint8 *) data, data_len);
   }

   f->stream     = (uint8 *) data;
   f->stream_end = (uint8 *) data + data_len;
   f->error      = VORBIS__no_error;

   // check that we have the entire packet in memory
   if (!is_whole_packet_present(f)) {
      *samples = 0;
      return 0;
   }

   if (!vorbis_decode_packet(f, &len, &left, &right)) {
      // save the actual error we encountered
      enum STBVorbisError error = f->error;
      if (error == VORBIS_bad_packet_type) {
         // flush and resynch
         f->error = VORBIS__no_error;
         while (get8_packet(f) != EOP)
            if (f->eof) break;
         *samples = 0;
         return (int) (f->stream - data);
      }
      if (error == VORBIS_continued_packet_flag_invalid) {
         if (f->previous_length == 0) {
            // we may be resynching, in which case it's ok to hit one
            // of these; just discard the packet
            f->error = VORBIS__no_error;
            while (get8_packet(f) != EOP)
               if (f->eof) break;
            *samples = 0;
            return (int) (f->stream - data);
         }
      }
      // if we get an error while parsing, what to do?
      // well, it DEFINITELY won't work to continue from where we are!
      stb_vorbis_flush_pushdata(f);
      // restore the error that actually made us bail
      f->error = error;
      *samples = 0;
      return 1;
   }

   // success!
   len = vorbis_finish_frame(f, len, left, right);
   for (i=0; i < f->channels; ++i)
      f->outputs[i] = f->channel_buffers[i] + left;

   if (channels) *channels = f->channels;
   *samples = len;
   *output = f->outputs;
   return (int) (f->stream - data);
}

/**
 * Initializes a stb_vorbis decoder in push mode from a given memory buffer.
 * 
 * This function sets up a stb_vorbis decoder to operate in push mode, where the decoder
 * processes data from a provided memory buffer. It initializes the decoder and attempts
 * to start the decoding process. If successful, it returns a pointer to the initialized
 * stb_vorbis object. If the decoder requires more data or encounters an error, it returns
 * NULL and sets the appropriate error code.
 *
 * @param data A pointer to the memory buffer containing the Vorbis data to be decoded.
 * @param data_len The length of the memory buffer in bytes.
 * @param data_used A pointer to an integer that will be set to the number of bytes consumed
 *                  from the input buffer if the function returns a non-NULL result.
 * @param error A pointer to an integer that will be set to an error code if the function
 *              fails. Possible error codes include VORBIS_need_more_data if the decoder
 *              requires more data, or other error codes specific to the decoder.
 * @param alloc A pointer to an stb_vorbis_alloc structure that specifies the memory
 *              allocation callbacks to be used by the decoder. If NULL, default allocation
 *              methods are used.
 *
 * @return On success, returns a pointer to the initialized stb_vorbis decoder. On failure,
 *         returns NULL and sets the error code in the `error` parameter.
 */
stb_vorbis *stb_vorbis_open_pushdata(
         const unsigned char *data, int data_len, // the memory available for decoding
         int *data_used,              // only defined if result is not NULL
         int *error, const stb_vorbis_alloc *alloc)
{
   stb_vorbis *f, p;
   vorbis_init(&p, alloc);
   p.stream     = (uint8 *) data;
   p.stream_end = (uint8 *) data + data_len;
   p.push_mode  = TRUE;
   if (!start_decoder(&p)) {
      if (p.eof)
         *error = VORBIS_need_more_data;
      else
         *error = p.error;
      return NULL;
   }
   f = vorbis_alloc(&p);
   if (f) {
      *f = p;
      *data_used = (int) (f->stream - data);
      *error = 0;
      return f;
   } else {
      vorbis_deinit(&p);
      return NULL;
   }
}
#endif // STB_VORBIS_NO_PUSHDATA_API

unsigned int stb_vorbis_get_file_offset(stb_vorbis *f)
{
   #ifndef STB_VORBIS_NO_PUSHDATA_API
   if (f->push_mode) return 0;
   #endif
   if (USE_MEMORY(f)) return (unsigned int) (f->stream - f->stream_start);
   #ifndef STB_VORBIS_NO_STDIO
   return (unsigned int) (ftell(f->f) - f->f_start);
   #endif
}

#ifndef STB_VORBIS_NO_PULLDATA_API
//
// DATA-PULLING API
//

static uint32 vorbis_find_page(stb_vorbis *f, uint32 *end, uint32 *last)
{
   for(;;) {
      int n;
      if (f->eof) return 0;
      n = get8(f);
      if (n == 0x4f) { // page header candidate
         unsigned int retry_loc = stb_vorbis_get_file_offset(f);
         int i;
         // check if we're off the end of a file_section stream
         if (retry_loc - 25 > f->stream_len)
            return 0;
         // check the rest of the header
         for (i=1; i < 4; ++i)
            if (get8(f) != ogg_page_header[i])
               break;
         if (f->eof) return 0;
         if (i == 4) {
            uint8 header[27];
            uint32 i, crc, goal, len;
            for (i=0; i < 4; ++i)
               header[i] = ogg_page_header[i];
            for (; i < 27; ++i)
               header[i] = get8(f);
            if (f->eof) return 0;
            if (header[4] != 0) goto invalid;
            goal = header[22] + (header[23] << 8) + (header[24]<<16) + (header[25]<<24);
            for (i=22; i < 26; ++i)
               header[i] = 0;
            crc = 0;
            for (i=0; i < 27; ++i)
               crc = crc32_update(crc, header[i]);
            len = 0;
            for (i=0; i < header[26]; ++i) {
               int s = get8(f);
               crc = crc32_update(crc, s);
               len += s;
            }
            if (len && f->eof) return 0;
            for (i=0; i < len; ++i)
               crc = crc32_update(crc, get8(f));
            // finished parsing probable page
            if (crc == goal) {
               // we could now check that it's either got the last
               // page flag set, OR it's followed by the capture
               // pattern, but I guess TECHNICALLY you could have
               // a file with garbage between each ogg page and recover
               // from it automatically? So even though that paranoia
               // might decrease the chance of an invalid decode by
               // another 2^32, not worth it since it would hose those
               // invalid-but-useful files?
               if (end)
                  *end = stb_vorbis_get_file_offset(f);
               if (last) {
                  if (header[5] & 0x04)
                     *last = 1;
                  else
                     *last = 0;
               }
               set_file_offset(f, retry_loc-1);
               return 1;
            }
         }
        invalid:
         // not a valid page, so rewind and look for next one
         set_file_offset(f, retry_loc);
      }
   }
}


#define SAMPLE_unknown  0xffffffff

// seeking is implemented with a binary search, which narrows down the range to
// 64K, before using a linear search (because finding the synchronization
// pattern can be expensive, and the chance we'd find the end page again is
// relatively high for small ranges)
//
// two initial interpolation-style probes are used at the start of the search
// to try to bound either side of the binary search sensibly, while still
// working in O(log n) time if they fail.

static int get_seek_page_info(stb_vorbis *f, ProbedPage *z)
{
   uint8 header[27], lacing[255];
   int i,len;

   // record where the page starts
   z->page_start = stb_vorbis_get_file_offset(f);

   // parse the header
   getn(f, header, 27);
   if (header[0] != 'O' || header[1] != 'g' || header[2] != 'g' || header[3] != 'S')
      return 0;
   getn(f, lacing, header[26]);

   // determine the length of the payload
   len = 0;
   for (i=0; i < header[26]; ++i)
      len += lacing[i];

   // this implies where the page ends
   z->page_end = z->page_start + 27 + header[26] + len;

   // read the last-decoded sample out of the data
   z->last_decoded_sample = header[6] + (header[7] << 8) + (header[8] << 16) + (header[9] << 24);

   // restore file state to where we were
   set_file_offset(f, z->page_start);
   return 1;
}

// rarely used function to seek back to the preceding page while finding the
// start of a packet
static int go_to_page_before(stb_vorbis *f, unsigned int limit_offset)
{
   unsigned int previous_safe, end;

   // now we want to seek back 64K from the limit
   if (limit_offset >= 65536 && limit_offset-65536 >= f->first_audio_page_offset)
      previous_safe = limit_offset - 65536;
   else
      previous_safe = f->first_audio_page_offset;

   set_file_offset(f, previous_safe);

   while (vorbis_find_page(f, &end, NULL)) {
      if (end >= limit_offset && stb_vorbis_get_file_offset(f) < limit_offset)
         return 1;
      set_file_offset(f, end);
   }

   return 0;
}

// implements the search logic for finding a page and starting decoding. if
// the function succeeds, current_loc_valid will be true and current_loc will
// be less than or equal to the provided sample number (the closer the
// better).
static int seek_to_sample_coarse(stb_vorbis *f, uint32 sample_number)
{
   ProbedPage left, right, mid;
   int i, start_seg_with_known_loc, end_pos, page_start;
   uint32 delta, stream_length, padding, last_sample_limit;
   double offset = 0.0, bytes_per_sample = 0.0;
   int probe = 0;

   // find the last page and validate the target sample
   stream_length = stb_vorbis_stream_length_in_samples(f);
   if (stream_length == 0)            return error(f, VORBIS_seek_without_length);
   if (sample_number > stream_length) return error(f, VORBIS_seek_invalid);

   // this is the maximum difference between the window-center (which is the
   // actual granule position value), and the right-start (which the spec
   // indicates should be the granule position (give or take one)).
   padding = ((f->blocksize_1 - f->blocksize_0) >> 2);
   if (sample_number < padding)
      last_sample_limit = 0;
   else
      last_sample_limit = sample_number - padding;

   left = f->p_first;
   while (left.last_decoded_sample == ~0U) {
      // (untested) the first page does not have a 'last_decoded_sample'
      set_file_offset(f, left.page_end);
      if (!get_seek_page_info(f, &left)) goto error;
   }

   right = f->p_last;
   assert(right.last_decoded_sample != ~0U);

   // starting from the start is handled differently
   if (last_sample_limit <= left.last_decoded_sample) {
      if (stb_vorbis_seek_start(f)) {
         if (f->current_loc > sample_number)
            return error(f, VORBIS_seek_failed);
         return 1;
      }
      return 0;
   }

   while (left.page_end != right.page_start) {
      assert(left.page_end < right.page_start);
      // search range in bytes
      delta = right.page_start - left.page_end;
      if (delta <= 65536) {
         // there's only 64K left to search - handle it linearly
         set_file_offset(f, left.page_end);
      } else {
         if (probe < 2) {
            if (probe == 0) {
               // first probe (interpolate)
               double data_bytes = right.page_end - left.page_start;
               bytes_per_sample = data_bytes / right.last_decoded_sample;
               offset = left.page_start + bytes_per_sample * (last_sample_limit - left.last_decoded_sample);
            } else {
               // second probe (try to bound the other side)
               double error = ((double) last_sample_limit - mid.last_decoded_sample) * bytes_per_sample;
               if (error >= 0 && error <  8000) error =  8000;
               if (error <  0 && error > -8000) error = -8000;
               offset += error * 2;
            }

            // ensure the offset is valid
            if (offset < left.page_end)
               offset = left.page_end;
            if (offset > right.page_start - 65536)
               offset = right.page_start - 65536;

            set_file_offset(f, (unsigned int) offset);
         } else {
            // binary search for large ranges (offset by 32K to ensure
            // we don't hit the right page)
            set_file_offset(f, left.page_end + (delta / 2) - 32768);
         }

         if (!vorbis_find_page(f, NULL, NULL)) goto error;
      }

      for (;;) {
         if (!get_seek_page_info(f, &mid)) goto error;
         if (mid.last_decoded_sample != ~0U) break;
         // (untested) no frames end on this page
         set_file_offset(f, mid.page_end);
         assert(mid.page_start < right.page_start);
      }

      // if we've just found the last page again then we're in a tricky file,
      // and we're close enough (if it wasn't an interpolation probe).
      if (mid.page_start == right.page_start) {
         if (probe >= 2 || delta <= 65536)
            break;
      } else {
         if (last_sample_limit < mid.last_decoded_sample)
            right = mid;
         else
            left = mid;
      }

      ++probe;
   }

   // seek back to start of the last packet
   page_start = left.page_start;
   set_file_offset(f, page_start);
   if (!start_page(f)) return error(f, VORBIS_seek_failed);
   end_pos = f->end_seg_with_known_loc;
   assert(end_pos >= 0);

   for (;;) {
      for (i = end_pos; i > 0; --i)
         if (f->segments[i-1] != 255)
            break;

      start_seg_with_known_loc = i;

      if (start_seg_with_known_loc > 0 || !(f->page_flag & PAGEFLAG_continued_packet))
         break;

      // (untested) the final packet begins on an earlier page
      if (!go_to_page_before(f, page_start))
         goto error;

      page_start = stb_vorbis_get_file_offset(f);
      if (!start_page(f)) goto error;
      end_pos = f->segment_count - 1;
   }

   // prepare to start decoding
   f->current_loc_valid = FALSE;
   f->last_seg = FALSE;
   f->valid_bits = 0;
   f->packet_bytes = 0;
   f->bytes_in_seg = 0;
   f->previous_length = 0;
   f->next_seg = start_seg_with_known_loc;

   for (i = 0; i < start_seg_with_known_loc; i++)
      skip(f, f->segments[i]);

   // start decoding (optimizable - this frame is generally discarded)
   if (!vorbis_pump_first_frame(f))
      return 0;
   if (f->current_loc > sample_number)
      return error(f, VORBIS_seek_failed);
   return 1;

error:
   // try to restore the file to a valid state
   stb_vorbis_seek_start(f);
   return error(f, VORBIS_seek_failed);
}

// the same as vorbis_decode_initial, but without advancing
static int peek_decode_initial(vorb *f, int *p_left_start, int *p_left_end, int *p_right_start, int *p_right_end, int *mode)
{
   int bits_read, bytes_read;

   if (!vorbis_decode_initial(f, p_left_start, p_left_end, p_right_start, p_right_end, mode))
      return 0;

   // either 1 or 2 bytes were read, figure out which so we can rewind
   bits_read = 1 + ilog(f->mode_count-1);
   if (f->mode_config[*mode].blockflag)
      bits_read += 2;
   bytes_read = (bits_read + 7) / 8;

   f->bytes_in_seg += bytes_read;
   f->packet_bytes -= bytes_read;
   skip(f, -bytes_read);
   if (f->next_seg == -1)
      f->next_seg = f->segment_count - 1;
   else
      f->next_seg--;
   f->valid_bits = 0;

   return 1;
}

/**
 * Seeks to the specified sample number in the Vorbis audio stream.
 *
 * This function attempts to position the decoder at the frame that contains the
 * given sample number. It first performs a fast page-level search to get close
 * to the desired location, then performs a linear search within the page to find
 * the exact frame containing the sample.
 *
 * @param f Pointer to the stb_vorbis instance representing the Vorbis stream.
 * @param sample_number The sample number to seek to.
 * @return 1 if the seek was successful, 0 if the seek failed.
 * @note This function is not supported in push mode and will return an error if
 *       called in that context.
 * @note The function assumes that the stream is seekable and that the sample
 *       number is valid within the stream.
 */
int stb_vorbis_seek_frame(stb_vorbis *f, unsigned int sample_number)
{
   uint32 max_frame_samples;

   if (IS_PUSH_MODE(f)) return error(f, VORBIS_invalid_api_mixing);

   // fast page-level search
   if (!seek_to_sample_coarse(f, sample_number))
      return 0;

   assert(f->current_loc_valid);
   assert(f->current_loc <= sample_number);

   // linear search for the relevant packet
   max_frame_samples = (f->blocksize_1*3 - f->blocksize_0) >> 2;
   while (f->current_loc < sample_number) {
      int left_start, left_end, right_start, right_end, mode, frame_samples;
      if (!peek_decode_initial(f, &left_start, &left_end, &right_start, &right_end, &mode))
         return error(f, VORBIS_seek_failed);
      // calculate the number of samples returned by the next frame
      frame_samples = right_start - left_start;
      if (f->current_loc + frame_samples > sample_number) {
         return 1; // the next frame will contain the sample
      } else if (f->current_loc + frame_samples + max_frame_samples > sample_number) {
         // there's a chance the frame after this could contain the sample
         vorbis_pump_first_frame(f);
      } else {
         // this frame is too early to be relevant
         f->current_loc += frame_samples;
         f->previous_length = 0;
         maybe_start_packet(f);
         flush_packet(f);
      }
   }
   // the next frame should start with the sample
   if (f->current_loc != sample_number) return error(f, VORBIS_seek_failed);
   return 1;
}

/**
 * Seeks to a specific sample position in the Vorbis audio stream.
 *
 * This function attempts to position the decoder at the specified sample number within the audio stream.
 * It first seeks to the frame containing the target sample using `stb_vorbis_seek_frame`. If successful,
 * it adjusts the internal buffer to point to the exact sample position within the frame.
 *
 * @param f Pointer to the `stb_vorbis` instance representing the Vorbis stream.
 * @param sample_number The target sample number to seek to.
 * @return Returns 1 if the seek operation was successful, otherwise returns 0.
 *
 * @note The function assumes that the provided `sample_number` is valid and within the bounds of the stream.
 *       If the target sample is not at the start of the frame, it decodes the frame and adjusts the buffer
 *       to point to the correct sample position.
 */
int stb_vorbis_seek(stb_vorbis *f, unsigned int sample_number)
{
   if (!stb_vorbis_seek_frame(f, sample_number))
      return 0;

   if (sample_number != f->current_loc) {
      int n;
      uint32 frame_start = f->current_loc;
      int stb_vorbis_get_frame_float(stb_vorbis *f, int *channels, fixedpt ***output);
      stb_vorbis_get_frame_float(f, &n, NULL);
      assert(sample_number > frame_start);
      assert(f->channel_buffer_start + (int) (sample_number-frame_start) <= f->channel_buffer_end);
      f->channel_buffer_start += (sample_number - frame_start);
   }

   return 1;
}

/**
 * Seeks to the start of the Vorbis audio stream.
 *
 * This function resets the decoder to the beginning of the audio stream by setting the file offset
 * to the first audio page offset. It also initializes internal state variables to prepare for decoding
 * the first frame. This function is not supported in push mode and will return an error if called in
 * that context.
 *
 * @param f A pointer to the `stb_vorbis` structure representing the Vorbis decoder instance.
 * @return Returns `TRUE` if the seek operation was successful and the first frame was pumped, or
 *         `FALSE` if an error occurred (e.g., if called in push mode).
 */
int stb_vorbis_seek_start(stb_vorbis *f)
{
   if (IS_PUSH_MODE(f)) { return error(f, VORBIS_invalid_api_mixing); }
   set_file_offset(f, f->first_audio_page_offset);
   f->previous_length = 0;
   f->first_decode = TRUE;
   f->next_seg = -1;
   return vorbis_pump_first_frame(f);
}

/**
 * @brief Calculates the total number of samples in a Vorbis audio stream.
 *
 * This function determines the length of a Vorbis audio stream in samples by seeking to the last page
 * of the stream and extracting the absolute granule position from the page header. If the stream length
 * is not already known (i.e., `f->total_samples` is zero), the function performs a backward seek from
 * the end of the stream to locate the last page. Once the last page is found, the granule position is
 * read and used to set the total number of samples in the stream. If the stream is in push mode or if
 * the last page cannot be found, an error is returned.
 *
 * @param f A pointer to the `stb_vorbis` structure representing the Vorbis stream.
 * @return The total number of samples in the stream. If the stream length cannot be determined, 
 *         the function returns 0. If the stream is in push mode, an error is returned.
 */
unsigned int stb_vorbis_stream_length_in_samples(stb_vorbis *f)
{
   unsigned int restore_offset, previous_safe;
   unsigned int end, last_page_loc;

   if (IS_PUSH_MODE(f)) return error(f, VORBIS_invalid_api_mixing);
   if (!f->total_samples) {
      unsigned int last;
      uint32 lo,hi;
      char header[6];

      // first, store the current decode position so we can restore it
      restore_offset = stb_vorbis_get_file_offset(f);

      // now we want to seek back 64K from the end (the last page must
      // be at most a little less than 64K, but let's allow a little slop)
      if (f->stream_len >= 65536 && f->stream_len-65536 >= f->first_audio_page_offset)
         previous_safe = f->stream_len - 65536;
      else
         previous_safe = f->first_audio_page_offset;

      set_file_offset(f, previous_safe);
      // previous_safe is now our candidate 'earliest known place that seeking
      // to will lead to the final page'

      if (!vorbis_find_page(f, &end, &last)) {
         // if we can't find a page, we're hosed!
         f->error = VORBIS_cant_find_last_page;
         f->total_samples = 0xffffffff;
         goto done;
      }

      // check if there are more pages
      last_page_loc = stb_vorbis_get_file_offset(f);

      // stop when the last_page flag is set, not when we reach eof;
      // this allows us to stop short of a 'file_section' end without
      // explicitly checking the length of the section
      while (!last) {
         set_file_offset(f, end);
         if (!vorbis_find_page(f, &end, &last)) {
            // the last page we found didn't have the 'last page' flag
            // set. whoops!
            break;
         }
         previous_safe = last_page_loc+1;
         last_page_loc = stb_vorbis_get_file_offset(f);
      }

      set_file_offset(f, last_page_loc);

      // parse the header
      getn(f, (unsigned char *)header, 6);
      // extract the absolute granule position
      lo = get32(f);
      hi = get32(f);
      if (lo == 0xffffffff && hi == 0xffffffff) {
         f->error = VORBIS_cant_find_last_page;
         f->total_samples = SAMPLE_unknown;
         goto done;
      }
      if (hi)
         lo = 0xfffffffe; // saturate
      f->total_samples = lo;

      f->p_last.page_start = last_page_loc;
      f->p_last.page_end   = end;
      f->p_last.last_decoded_sample = lo;

     done:
      set_file_offset(f, restore_offset);
   }
   return f->total_samples == SAMPLE_unknown ? 0 : f->total_samples;
}

/**
 * Calculates the total length of the Vorbis audio stream in seconds.
 *
 * This function determines the duration of the audio stream by dividing the total number of samples
 * in the stream by the sample rate. The result is returned as a floating-point value representing
 * the length in seconds.
 *
 * @param f A pointer to the `stb_vorbis` structure representing the Vorbis stream.
 * @return The length of the audio stream in seconds as a `float`.
 */
float stb_vorbis_stream_length_in_seconds(stb_vorbis *f)
{
   return stb_vorbis_stream_length_in_samples(f) / (float) f->sample_rate;
}



/**
 * Decodes a frame of audio data from a Vorbis stream into floating-point samples.
 *
 * This function decodes a single frame of audio data from the provided Vorbis stream `f` and stores
 * the resulting floating-point samples in the `output` buffer. It also returns the number of samples
 * per channel in the decoded frame. The function handles both stereo and mono streams, and the number
 * of channels is returned via the `channels` parameter if it is not NULL.
 *
 * @param f Pointer to the `stb_vorbis` structure representing the Vorbis stream.
 * @param channels Pointer to an integer where the number of channels in the decoded frame will be stored.
 *                 If NULL, the number of channels is not returned.
 * @param output Pointer to an array of pointers where the decoded floating-point samples will be stored.
 *               Each pointer in the array corresponds to a channel. If NULL, the samples are not returned.
 *
 * @return The number of samples per channel in the decoded frame. Returns 0 if no frame was decoded or
 *         if an error occurred.
 *
 * @note This function should not be used in push mode (i.e., when the stream is being decoded in chunks
 *       using `stb_vorbis_decode_frame_push`). If called in push mode, it returns an error.
 */
int stb_vorbis_get_frame_float(stb_vorbis *f, int *channels, fixedpt ***output)
{
   int len, right,left,i;
   if (IS_PUSH_MODE(f)) return error(f, VORBIS_invalid_api_mixing);

   if (!vorbis_decode_packet(f, &len, &left, &right)) {
      f->channel_buffer_start = f->channel_buffer_end = 0;
      return 0;
   }

   len = vorbis_finish_frame(f, len, left, right);
   for (i=0; i < f->channels; ++i)
      f->outputs[i] = f->channel_buffers[i] + left;

   f->channel_buffer_start = left;
   f->channel_buffer_end   = left+len;

   if (channels) *channels = f->channels;
   if (output)   *output = f->outputs;
   return len;
}

#ifndef STB_VORBIS_NO_STDIO

/**
 * Opens a Vorbis audio stream from a specified section of a file.
 *
 * This function initializes a Vorbis decoder for a specific section of a file, 
 * allowing for decoding of Vorbis audio data. The file section is defined by the 
 * current file position and the specified length. The decoder can be configured 
 * to automatically close the file when the decoder is freed.
 *
 * @param file           Pointer to the FILE object representing the file containing 
 *                       the Vorbis stream.
 * @param close_on_free  If non-zero, the file will be automatically closed when 
 *                       the decoder is freed.
 * @param error          Pointer to an integer where an error code will be stored 
 *                       if the function fails. Can be NULL if error handling is 
 *                       not needed.
 * @param alloc          Pointer to an stb_vorbis_alloc structure specifying the 
 *                       memory allocation callbacks. Can be NULL to use default 
 *                       allocation methods.
 * @param length         The length of the Vorbis stream section in bytes.
 *
 * @return               A pointer to the initialized stb_vorbis decoder object 
 *                       on success, or NULL on failure. If NULL is returned and 
 *                       `error` is not NULL, the error code will be stored in 
 *                       `error`.
 */
stb_vorbis * stb_vorbis_open_file_section(FILE *file, int close_on_free, int *error, const stb_vorbis_alloc *alloc, unsigned int length)
{
   stb_vorbis *f, p;
   vorbis_init(&p, alloc);
   p.f = file;
   p.f_start = (uint32) ftell(file);
   p.stream_len   = length;
   p.close_on_free = close_on_free;
   if (start_decoder(&p)) {
      f = vorbis_alloc(&p);
      if (f) {
         *f = p;
         vorbis_pump_first_frame(f);
         return f;
      }
   }
   if (error) *error = p.error;
   vorbis_deinit(&p);
   return NULL;
}

/**
 * Opens a Vorbis audio file for decoding using a FILE pointer.
 *
 * This function initializes a Vorbis decoder for the given file. It calculates the length of the file
 * by seeking to the end and then returning to the original position. The file can be automatically closed
 * when the decoder is freed, depending on the `close_on_free` parameter. If an error occurs, it is returned
 * via the `error` parameter. The memory allocation strategy can be customized using the `alloc` parameter.
 *
 * @param file          A FILE pointer to the Vorbis audio file to be decoded.
 * @param close_on_free If non-zero, the file will be automatically closed when the decoder is freed.
 * @param error         A pointer to an integer where any error code will be stored. If no error occurs,
 *                      this will be set to 0.
 * @param alloc         A pointer to an `stb_vorbis_alloc` structure specifying the memory allocation strategy.
 *                      If NULL, default allocation methods will be used.
 *
 * @return              A pointer to an `stb_vorbis` decoder instance on success, or NULL on failure.
 */
stb_vorbis * stb_vorbis_open_file(FILE *file, int close_on_free, int *error, const stb_vorbis_alloc *alloc)
{
   unsigned int len, start;
   start = (unsigned int) ftell(file);
   fseek(file, 0, SEEK_END);
   len = (unsigned int) (ftell(file) - start);
   fseek(file, start, SEEK_SET);
   return stb_vorbis_open_file_section(file, close_on_free, error, alloc, len);
}

/**
 * Opens a Vorbis audio file by its filename and initializes a stb_vorbis decoder.
 *
 * This function attempts to open the specified Vorbis audio file in binary read mode.
 * If the file is successfully opened, it initializes a stb_vorbis decoder using the
 * opened file handle. If the file cannot be opened, an error code is returned.
 *
 * @param filename The path to the Vorbis audio file to open.
 * @param error    A pointer to an integer where an error code will be stored if the
 *                 operation fails. If the function succeeds, this value is not modified.
 *                 Possible error codes include:
 *                 - VORBIS_file_open_failure: Indicates that the file could not be opened.
 * @param alloc    A pointer to a stb_vorbis_alloc structure specifying memory allocation
 *                 callbacks. If NULL, default allocation methods are used.
 *
 * @return         A pointer to the initialized stb_vorbis decoder on success, or NULL
 *                 if the file could not be opened or the decoder could not be initialized.
 */
stb_vorbis * stb_vorbis_open_filename(const char *filename, int *error, const stb_vorbis_alloc *alloc)
{
   FILE *f;
#if defined(_WIN32) && defined(__STDC_WANT_SECURE_LIB__)
   if (0 != fopen_s(&f, filename, "rb"))
      f = NULL;
#else
   f = fopen(filename, "rb");
#endif
   if (f)
      return stb_vorbis_open_file(f, TRUE, error, alloc);
   if (error) *error = VORBIS_file_open_failure;
   return NULL;
}
#endif // STB_VORBIS_NO_STDIO

stb_vorbis * stb_vorbis_open_memory(const unsigned char *data, int len, int *error, const stb_vorbis_alloc *alloc)
{
   stb_vorbis *f, p;
   if (data == NULL) return NULL;
   vorbis_init(&p, alloc);
   p.stream = (uint8 *) data;
   p.stream_end = (uint8 *) data + len;
   p.stream_start = (uint8 *) p.stream;
   p.stream_len = len;
   p.push_mode = FALSE;
   if (start_decoder(&p)) {
      f = vorbis_alloc(&p);
      if (f) {
         *f = p;
         vorbis_pump_first_frame(f);
         if (error) *error = VORBIS__no_error;
         return f;
      }
   }
   if (error) *error = p.error;
   vorbis_deinit(&p);
   return NULL;
}

#ifndef STB_VORBIS_NO_INTEGER_CONVERSION
#define PLAYBACK_MONO     1
#define PLAYBACK_LEFT     2
#define PLAYBACK_RIGHT    4

#define L  (PLAYBACK_LEFT  | PLAYBACK_MONO)
#define C  (PLAYBACK_LEFT  | PLAYBACK_RIGHT | PLAYBACK_MONO)
#define R  (PLAYBACK_RIGHT | PLAYBACK_MONO)

static int8 channel_position[7][6] =
{
   { 0 },
   { C },
   { L, R },
   { L, C, R },
   { L, R, L, R },
   { L, C, R, L, R },
   { L, C, R, L, R, C },
};


#ifndef STB_VORBIS_NO_FAST_SCALED_FLOAT
   typedef union {
      float f;
      int i;
   } float_conv;
   typedef char stb_vorbis_float_size_test[sizeof(float)==4 && sizeof(int) == 4];
   #define FASTDEF(x) float_conv x
   // add (1<<23) to convert to int, then divide by 2^SHIFT, then add 0.5/2^SHIFT to round
   #define MAGIC(SHIFT) (1.5f * (1 << (23-SHIFT)) + 0.5f/(1 << SHIFT))
   #define ADDEND(SHIFT) (((150-SHIFT) << 23) + (1 << 22))
   //#define FAST_SCALED_FLOAT_TO_INT(temp,x,s) (temp.f = (x) + MAGIC(s), temp.i - ADDEND(s))
   #define check_endianness()
#else
   //#define FAST_SCALED_FLOAT_TO_INT(temp,x,s) ((int) ((x) * (1 << (s))))
   #define check_endianness()
   #define FASTDEF(x)
#endif
#define FAST_SCALED_FLOAT_TO_INT(temp,x,s) (fixedpt_toint(fixedpt_muli(x, 1 << (s - SCALE_SHIFT))))

/**
 * Copies and converts an array of fixed-point samples to an array of 16-bit integer samples.
 * 
 * This function iterates over the source array `src`, which contains fixed-point samples, and
 * converts each sample to a 16-bit integer value. The converted samples are stored in the
 * destination array `dest`. The conversion process involves scaling the fixed-point values to
 * fit within the range of a 16-bit signed integer (-32768 to 32767). If the converted value
 * exceeds this range, it is clamped to the nearest valid value.
 *
 * Before processing the samples, the function checks the endianness of the system to ensure
 * proper handling of the data.
 *
 * @param dest Pointer to the destination array where the converted 16-bit integer samples
 *             will be stored.
 * @param src  Pointer to the source array containing the fixed-point samples to be converted.
 * @param len  The number of samples to convert and copy from `src` to `dest`.
 */
static void copy_samples(short *dest, fixedpt *src, int len)
{
   int i;
   check_endianness();
   for (i=0; i < len; ++i) {
      FASTDEF(temp);
      int v = FAST_SCALED_FLOAT_TO_INT(temp, src[i],15);
      if ((unsigned int) (v + 32768) > 65535)
         v = v < 0 ? -32768 : 32767;
      dest[i] = v;
   }
}

/**
 * Computes and stores the processed audio samples in the output buffer.
 *
 * This method processes audio data by summing up the values from multiple channels
 * based on a given mask, and then scaling the result to fit within the range of
 * a 16-bit signed integer. The processed samples are stored in the provided output buffer.
 *
 * @param mask A bitmask used to determine which channels to include in the computation.
 * @param output A pointer to the output buffer where the processed samples will be stored.
 * @param num_c The number of channels in the input data.
 * @param data A pointer to an array of pointers to the input data for each channel.
 * @param d_offset The offset within the input data from where to start processing.
 * @param len The number of samples to process.
 *
 * The method processes the data in chunks of size BUFFER_SIZE to optimize performance.
 * It first sums up the values from the selected channels for each sample, then scales
 * the result to fit within the range of a 16-bit signed integer, and finally stores
 * the result in the output buffer. If the scaled value exceeds the range of a 16-bit
 * signed integer, it is clamped to the nearest valid value.
 */
static void compute_samples(int mask, short *output, int num_c, fixedpt **data, int d_offset, int len)
{
   #define BUFFER_SIZE  32
   fixedpt buffer[BUFFER_SIZE];
   int i,j,o,n = BUFFER_SIZE;
   check_endianness();
   for (o = 0; o < len; o += BUFFER_SIZE) {
      memset(buffer, 0, sizeof(buffer));
      if (o + n > len) n = len - o;
      for (j=0; j < num_c; ++j) {
         if (channel_position[num_c][j] & mask) {
            for (i=0; i < n; ++i)
               buffer[i] += data[j][d_offset+o+i];
         }
      }
      for (i=0; i < n; ++i) {
         FASTDEF(temp);
         int v = FAST_SCALED_FLOAT_TO_INT(temp,buffer[i],15);
         if ((unsigned int) (v + 32768) > 65535)
            v = v < 0 ? -32768 : 32767;
         output[o+i] = v;
      }
   }
}

/**
 * Computes stereo audio samples from multi-channel audio data and writes them to the output buffer.
 *
 * This method processes fixed-point audio data from multiple channels, mixes them according to their
 * playback positions (left, right, or both), and converts the result into stereo samples. The stereo
 * samples are stored in the provided output buffer as 16-bit signed integers.
 *
 * @param output     Pointer to the output buffer where stereo samples will be stored.
 * @param num_c      Number of input audio channels.
 * @param data       Array of pointers to fixed-point audio data for each channel.
 * @param d_offset   Offset in the input audio data from where processing should start.
 * @param len        Number of samples to process from the input audio data.
 *
 * The method uses a temporary buffer of size `BUFFER_SIZE` to process the audio data in chunks. It
 * checks the endianness of the system to ensure proper handling of data. The stereo samples are
 * computed by summing the contributions of each channel based on their playback positions (left,
 * right, or both). The resulting samples are scaled and clamped to the range of a 16-bit signed
 * integer before being written to the output buffer.
 */
static void compute_stereo_samples(short *output, int num_c, fixedpt **data, int d_offset, int len)
{
   #define BUFFER_SIZE  32
   fixedpt buffer[BUFFER_SIZE];
   int i,j,o,n = BUFFER_SIZE >> 1;
   // o is the offset in the source data
   check_endianness();
   for (o = 0; o < len; o += BUFFER_SIZE >> 1) {
      // o2 is the offset in the output data
      int o2 = o << 1;
      memset(buffer, 0, sizeof(buffer));
      if (o + n > len) n = len - o;
      for (j=0; j < num_c; ++j) {
         int m = channel_position[num_c][j] & (PLAYBACK_LEFT | PLAYBACK_RIGHT);
         if (m == (PLAYBACK_LEFT | PLAYBACK_RIGHT)) {
            for (i=0; i < n; ++i) {
               buffer[i*2+0] += data[j][d_offset+o+i];
               buffer[i*2+1] += data[j][d_offset+o+i];
            }
         } else if (m == PLAYBACK_LEFT) {
            for (i=0; i < n; ++i) {
               buffer[i*2+0] += data[j][d_offset+o+i];
            }
         } else if (m == PLAYBACK_RIGHT) {
            for (i=0; i < n; ++i) {
               buffer[i*2+1] += data[j][d_offset+o+i];
            }
         }
      }
      for (i=0; i < (n<<1); ++i) {
         FASTDEF(temp);
         int v = FAST_SCALED_FLOAT_TO_INT(temp,buffer[i],15);
         if ((unsigned int) (v + 32768) > 65535)
            v = v < 0 ? -32768 : 32767;
         output[o2+i] = v;
      }
   }
}

/**
 * Converts audio samples from a fixed-point format to a short integer format.
 * The method handles different channel configurations and ensures proper sample conversion
 * or copying based on the input and output channel counts.
 *
 * @param buf_c      The number of channels in the output buffer (must be <= 2).
 * @param buffer     A pointer to an array of pointers to the output buffers (short integer format).
 * @param b_offset   The offset in the output buffers where the samples should be written.
 * @param data_c     The number of channels in the input data (must be <= 6).
 * @param data       A pointer to an array of pointers to the input data (fixed-point format).
 * @param d_offset   The offset in the input data from where the samples should be read.
 * @param samples    The number of samples to convert or copy.
 *
 * If the number of input channels (`data_c`) and output channels (`buf_c`) differ,
 * the method uses a channel selector to map the input channels to the output channels.
 * If the number of channels is the same, it directly copies the samples.
 * If there are more output channels than input channels, the extra output channels are zeroed out.
 */
static void convert_samples_short(int buf_c, short **buffer, int b_offset, int data_c, fixedpt **data, int d_offset, int samples)
{
   int i;
   if (buf_c != data_c && buf_c <= 2 && data_c <= 6) {
      static int channel_selector[3][2] = { {0}, {PLAYBACK_MONO}, {PLAYBACK_LEFT, PLAYBACK_RIGHT} };
      for (i=0; i < buf_c; ++i)
         compute_samples(channel_selector[buf_c][i], buffer[i]+b_offset, data_c, data, d_offset, samples);
   } else {
      int limit = buf_c < data_c ? buf_c : data_c;
      for (i=0; i < limit; ++i)
         copy_samples(buffer[i]+b_offset, data[i]+d_offset, samples);
      for (   ; i < buf_c; ++i)
         memset(buffer[i]+b_offset, 0, sizeof(short) * samples);
   }
}

/**
 * Decodes a frame of audio data from a Vorbis stream into a short integer buffer.
 *
 * This function retrieves a frame of audio data from the provided Vorbis stream `f` and converts
 * it into a buffer of 16-bit signed integers (short). The decoded audio is stored in the `buffer`
 * array, which is expected to have enough space to hold the specified number of samples.
 *
 * @param f Pointer to the `stb_vorbis` structure representing the Vorbis stream.
 * @param num_c The number of channels in the output buffer. If this is less than the number of
 *              channels in the Vorbis stream, the extra channels will be truncated.
 * @param buffer Pointer to an array of pointers to short integer buffers where the decoded audio
 *               data will be stored. Each pointer corresponds to a channel.
 * @param num_samples The maximum number of samples to decode per channel. If the frame contains
 *                    more samples than this, only the first `num_samples` will be decoded.
 *
 * @return The number of samples actually decoded per channel. This will be less than or equal to
 *         `num_samples`. If no samples are decoded (e.g., due to an error or end of stream), 0 is returned.
 */
int stb_vorbis_get_frame_short(stb_vorbis *f, int num_c, short **buffer, int num_samples)
{
   fixedpt **output = NULL;
   int len = stb_vorbis_get_frame_float(f, NULL, &output);
   if (len > num_samples) len = num_samples;
   if (len)
      convert_samples_short(num_c, buffer, 0, f->channels, output, 0, len);
   return len;
}

/**
 * Converts audio data from a fixed-point format to interleaved short format.
 * This method handles both mono and stereo input data and ensures proper
 * channel mapping and interleaving in the output buffer.
 *
 * @param buf_c      The number of channels in the output buffer (must be 1 or 2).
 * @param buffer     Pointer to the output buffer where interleaved short data will be stored.
 * @param data_c     The number of channels in the input fixed-point data (must be <= 6).
 * @param data       Array of pointers to the input fixed-point data, one per channel.
 * @param d_offset   Offset in the input data arrays from which to start reading.
 * @param len        The number of samples to process for each channel.
 *
 * The method performs the following steps:
 * 1. Checks the system's endianness to ensure proper byte ordering.
 * 2. If the input and output channel counts differ and both are <= 2, it assumes
 *    stereo processing and computes stereo samples using `compute_stereo_samples`.
 * 3. Otherwise, it processes each sample for the minimum number of channels
 *    between input and output, converting fixed-point values to short integers.
 *    Values are clamped to the range [-32768, 32767] to avoid overflow.
 * 4. If the output buffer has more channels than the input, the extra channels
 *    are filled with zeros.
 */
static void convert_channels_short_interleaved(int buf_c, short *buffer, int data_c, fixedpt **data, int d_offset, int len)
{
   int i;
   check_endianness();
   if (buf_c != data_c && buf_c <= 2 && data_c <= 6) {
      assert(buf_c == 2);
      for (i=0; i < buf_c; ++i)
         compute_stereo_samples(buffer, data_c, data, d_offset, len);
   } else {
      int limit = buf_c < data_c ? buf_c : data_c;
      int j;
      for (j=0; j < len; ++j) {
         for (i=0; i < limit; ++i) {
            FASTDEF(temp);
            fixedpt f = data[i][d_offset+j];
            int v = FAST_SCALED_FLOAT_TO_INT(temp, f,15);//data[i][d_offset+j],15);
            if ((unsigned int) (v + 32768) > 65535)
               v = v < 0 ? -32768 : 32767;
            *buffer++ = v;
         }
         for (   ; i < buf_c; ++i)
            *buffer++ = 0;
      }
   }
}

/**
 * Decodes a single frame of audio data from a Vorbis stream into an interleaved short buffer.
 *
 * This function reads a frame of audio data from the provided `stb_vorbis` instance and converts
 * it into interleaved 16-bit signed integer samples. The output is stored in the provided `buffer`.
 * The function supports multi-channel audio and ensures the output is interleaved by channel.
 *
 * @param f The `stb_vorbis` instance from which to decode the audio frame.
 * @param num_c The number of channels to decode. If set to 1, the function will decode a mono stream.
 * @param buffer The output buffer where the decoded interleaved samples will be stored.
 * @param num_shorts The maximum number of shorts (16-bit samples) that can be stored in the buffer.
 *
 * @return The number of samples decoded per channel. If the buffer is too small to hold all the
 *         decoded data, the function will truncate the output and return the number of samples
 *         that fit into the buffer. Returns 0 if no frame was decoded or an error occurred.
 */
int stb_vorbis_get_frame_short_interleaved(stb_vorbis *f, int num_c, short *buffer, int num_shorts)
{
   fixedpt **output;
   int len;
   if (num_c == 1) return stb_vorbis_get_frame_short(f,num_c,&buffer, num_shorts);
   len = stb_vorbis_get_frame_float(f, NULL, &output);
   if (len) {
      if (len*num_c > num_shorts) len = num_shorts / num_c;
      convert_channels_short_interleaved(num_c, buffer, f->channels, output, 0, len);
   }
   return len;
}

/**
 * @brief Decodes and retrieves a specified number of interleaved short samples from a Vorbis audio stream.
 *
 * This function reads and decodes audio data from a `stb_vorbis` instance, converting the samples
 * to interleaved 16-bit short format and storing them in the provided buffer. The function processes
 * the audio data in chunks, ensuring that the output buffer is filled with the requested number of
 * samples or until the end of the stream is reached.
 *
 * @param f Pointer to the `stb_vorbis` instance representing the Vorbis audio stream.
 * @param channels The number of interleaved channels in the output buffer. If the Vorbis stream
 *                 has more channels, only the first `channels` channels are used.
 * @param buffer Pointer to the buffer where the decoded interleaved short samples will be stored.
 * @param num_shorts The total number of short samples to retrieve. This should be a multiple of
 *                   the number of channels.
 * @return The total number of short samples actually retrieved and stored in the buffer. This may
 *         be less than `num_shorts` if the end of the stream is reached.
 */
int stb_vorbis_get_samples_short_interleaved(stb_vorbis *f, int channels, short *buffer, int num_shorts)
{
   fixedpt **outputs;
   int len = num_shorts / channels;
   int n=0;
   int z = f->channels;
   if (z > channels) z = channels;
   while (n < len) {
      int k = f->channel_buffer_end - f->channel_buffer_start;
      if (n+k >= len) k = len - n;
      if (k)
         convert_channels_short_interleaved(channels, buffer, f->channels, f->channel_buffers, f->channel_buffer_start, k);
      buffer += k*channels;
      n += k;
      f->channel_buffer_start += k;
      if (n == len) break;
      if (!stb_vorbis_get_frame_float(f, NULL, &outputs)) break;
   }
   return n;
}

/**
 * Retrieves a specified number of samples from a Vorbis audio stream and converts them to 16-bit signed short integers.
 * 
 * This function reads audio samples from the provided Vorbis stream (`f`) and stores them in the specified buffer (`buffer`).
 * The samples are converted to 16-bit signed short integers and interleaved according to the number of channels (`channels`).
 * The function continues to read samples until the requested number of samples (`len`) is reached or the end of the stream is encountered.
 * 
 * @param f         A pointer to the `stb_vorbis` structure representing the Vorbis stream.
 * @param channels  The number of channels in the output buffer. If the stream has more channels, only the first `channels` are used.
 * @param buffer    A pointer to an array of pointers to the output buffers where the converted samples will be stored.
 * @param len       The number of samples to retrieve. This is the total number of samples across all channels.
 * 
 * @return          The total number of samples successfully retrieved and stored in the buffer. This may be less than `len`
 *                  if the end of the stream is reached before the requested number of samples is read.
 */
int stb_vorbis_get_samples_short(stb_vorbis *f, int channels, short **buffer, int len)
{
   fixedpt **outputs;
   int n=0;
   int z = f->channels;
   if (z > channels) z = channels;
   while (n < len) {
      int k = f->channel_buffer_end - f->channel_buffer_start;
      if (n+k >= len) k = len - n;
      if (k)
         convert_samples_short(channels, buffer, n, f->channels, f->channel_buffers, f->channel_buffer_start, k);
      n += k;
      f->channel_buffer_start += k;
      if (n == len) break;
      if (!stb_vorbis_get_frame_float(f, NULL, &outputs)) break;
   }
   return n;
}

#ifndef STB_VORBIS_NO_STDIO
/**
 * Decodes a Vorbis audio file specified by the given filename and returns the decoded audio data.
 *
 * This function opens the specified Vorbis file, decodes its contents, and stores the decoded audio
 * data in a dynamically allocated buffer. The caller is responsible for freeing the allocated buffer.
 *
 * @param filename     The path to the Vorbis audio file to be decoded.
 * @param channels     A pointer to an integer where the number of audio channels will be stored.
 * @param sample_rate  A pointer to an integer where the sample rate of the audio will be stored.
 *                     This can be NULL if the sample rate is not needed.
 * @param output       A pointer to a short pointer where the decoded audio data will be stored.
 *                     The caller must free this memory when it is no longer needed.
 *
 * @return             The number of decoded audio samples per channel on success. If the file cannot
 *                     be opened or decoded, a negative value is returned:
 *                     - -1: Failed to open the file.
 *                     - -2: Memory allocation failed.
 */
int stb_vorbis_decode_filename(const char *filename, int *channels, int *sample_rate, short **output)
{
   int data_len, offset, total, limit, error;
   short *data;
   stb_vorbis *v = stb_vorbis_open_filename(filename, &error, NULL);
   if (v == NULL) return -1;
   limit = v->channels * 4096;
   *channels = v->channels;
   if (sample_rate)
      *sample_rate = v->sample_rate;
   offset = data_len = 0;
   total = limit;
   data = (short *) malloc(total * sizeof(*data));
   if (data == NULL) {
      stb_vorbis_close(v);
      return -2;
   }
   for (;;) {
      int n = stb_vorbis_get_frame_short_interleaved(v, v->channels, data+offset, total-offset);
      if (n == 0) break;
      data_len += n;
      offset += n * v->channels;
      if (offset + limit > total) {
         short *data2;
         total *= 2;
         data2 = (short *) realloc(data, total * sizeof(*data));
         if (data2 == NULL) {
            free(data);
            stb_vorbis_close(v);
            return -2;
         }
         data = data2;
      }
   }
   *output = data;
   stb_vorbis_close(v);
   return data_len;
}
#endif // NO_STDIO

int stb_vorbis_decode_memory(const uint8 *mem, int len, int *channels, int *sample_rate, short **output)
{
   int data_len, offset, total, limit, error;
   short *data;
   stb_vorbis *v = stb_vorbis_open_memory(mem, len, &error, NULL);
   if (v == NULL) return -1;
   limit = v->channels * 4096;
   *channels = v->channels;
   if (sample_rate)
      *sample_rate = v->sample_rate;
   offset = data_len = 0;
   total = limit;
   data = (short *) malloc(total * sizeof(*data));
   if (data == NULL) {
      stb_vorbis_close(v);
      return -2;
   }
   for (;;) {
      int n = stb_vorbis_get_frame_short_interleaved(v, v->channels, data+offset, total-offset);
      if (n == 0) break;
      data_len += n;
      offset += n * v->channels;
      if (offset + limit > total) {
         short *data2;
         total *= 2;
         data2 = (short *) realloc(data, total * sizeof(*data));
         if (data2 == NULL) {
            free(data);
            stb_vorbis_close(v);
            return -2;
         }
         data = data2;
      }
   }
   *output = data;
   stb_vorbis_close(v);
   return data_len;
}
#endif // STB_VORBIS_NO_INTEGER_CONVERSION

int stb_vorbis_get_samples_float_interleaved(stb_vorbis *f, int channels, fixedpt *buffer, int num_floats)
{
   fixedpt **outputs;
   int len = num_floats / channels;
   int n=0;
   int z = f->channels;
   if (z > channels) z = channels;
   while (n < len) {
      int i,j;
      int k = f->channel_buffer_end - f->channel_buffer_start;
      if (n+k >= len) k = len - n;
      for (j=0; j < k; ++j) {
         for (i=0; i < z; ++i)
            *buffer++ = f->channel_buffers[i][f->channel_buffer_start+j];
         for (   ; i < channels; ++i)
            *buffer++ = 0;
      }
      n += k;
      f->channel_buffer_start += k;
      if (n == len)
         break;
      if (!stb_vorbis_get_frame_float(f, NULL, &outputs))
         break;
   }
   return n;
}

/**
 * Retrieves a specified number of audio samples from a Vorbis stream and stores them in a buffer as floating-point values.
 *
 * This function reads up to `num_samples` samples from the Vorbis stream `f` and stores them in the provided `buffer`.
 * The samples are converted to floating-point format and stored in the buffer, with each channel's samples stored in a separate array.
 * If the stream has fewer channels than `channels`, the remaining channels in the buffer are filled with zeros.
 *
 * @param f            Pointer to the `stb_vorbis` structure representing the Vorbis stream.
 * @param channels     The number of channels to store in the buffer. If the stream has fewer channels, the remaining channels are zero-filled.
 * @param buffer       A pointer to an array of pointers, where each pointer points to a buffer for a single channel's samples.
 *                     The buffers should be large enough to hold `num_samples` samples.
 * @param num_samples  The number of samples to retrieve from the stream.
 *
 * @return The number of samples successfully retrieved and stored in the buffer. This may be less than `num_samples` if the end of the stream is reached.
 */
int stb_vorbis_get_samples_float(stb_vorbis *f, int channels, fixedpt **buffer, int num_samples)
{
   fixedpt **outputs;
   int n=0;
   int z = f->channels;
   if (z > channels) z = channels;
   while (n < num_samples) {
      int i;
      int k = f->channel_buffer_end - f->channel_buffer_start;
      if (n+k >= num_samples) k = num_samples - n;
      if (k) {
         for (i=0; i < z; ++i)
            memcpy(buffer[i]+n, f->channel_buffers[i]+f->channel_buffer_start, sizeof(fixedpt)*k);
         for (   ; i < channels; ++i)
            memset(buffer[i]+n, 0, sizeof(fixedpt) * k);
      }
      n += k;
      f->channel_buffer_start += k;
      if (n == num_samples)
         break;
      if (!stb_vorbis_get_frame_float(f, NULL, &outputs))
         break;
   }
   return n;
}
#endif // STB_VORBIS_NO_PULLDATA_API

/* Version history
    1.17    - 2019-07-08 - fix CVE-2019-13217, -13218, -13219, -13220, -13221, -13222, -13223
                           found with Mayhem by ForAllSecure
    1.16    - 2019-03-04 - fix warnings
    1.15    - 2019-02-07 - explicit failure if Ogg Skeleton data is found
    1.14    - 2018-02-11 - delete bogus dealloca usage
    1.13    - 2018-01-29 - fix truncation of last frame (hopefully)
    1.12    - 2017-11-21 - limit residue begin/end to blocksize/2 to avoid large temp allocs in bad/corrupt files
    1.11    - 2017-07-23 - fix MinGW compilation
    1.10    - 2017-03-03 - more robust seeking; fix negative ilog(); clear error in open_memory
    1.09    - 2016-04-04 - back out 'avoid discarding last frame' fix from previous version
    1.08    - 2016-04-02 - fixed multiple warnings; fix setup memory leaks;
                           avoid discarding last frame of audio data
    1.07    - 2015-01-16 - fixed some warnings, fix mingw, const-correct API
                           some more crash fixes when out of memory or with corrupt files
    1.06    - 2015-08-31 - full, correct support for seeking API (Dougall Johnson)
                           some crash fixes when out of memory or with corrupt files
    1.05    - 2015-04-19 - don't define __forceinline if it's redundant
    1.04    - 2014-08-27 - fix missing const-correct case in API
    1.03    - 2014-08-07 - Warning fixes
    1.02    - 2014-07-09 - Declare qsort compare function _cdecl on windows
    1.01    - 2014-06-18 - fix stb_vorbis_get_samples_float
    1.0     - 2014-05-26 - fix memory leaks; fix warnings; fix bugs in multichannel
                           (API change) report sample rate for decode-full-file funcs
    0.99996 - bracket #include <malloc.h> for macintosh compilation by Laurent Gomila
    0.99995 - use union instead of pointer-cast for fast-float-to-int to avoid alias-optimization problem
    0.99994 - change fast-float-to-int to work in single-precision FPU mode, remove endian-dependence
    0.99993 - remove assert that fired on legal files with empty tables
    0.99992 - rewind-to-start
    0.99991 - bugfix to stb_vorbis_get_samples_short by Bernhard Wodo
    0.9999 - (should have been 0.99990) fix no-CRT support, compiling as C++
    0.9998 - add a full-decode function with a memory source
    0.9997 - fix a bug in the read-from-FILE case in 0.9996 addition
    0.9996 - query length of vorbis stream in samples/seconds
    0.9995 - bugfix to another optimization that only happened in certain files
    0.9994 - bugfix to one of the optimizations that caused significant (but inaudible?) errors
    0.9993 - performance improvements; runs in 99% to 104% of time of reference implementation
    0.9992 - performance improvement of IMDCT; now performs close to reference implementation
    0.9991 - performance improvement of IMDCT
    0.999 - (should have been 0.9990) performance improvement of IMDCT
    0.998 - no-CRT support from Casey Muratori
    0.997 - bugfixes for bugs found by Terje Mathisen
    0.996 - bugfix: fast-huffman decode initialized incorrectly for sparse codebooks; fixing gives 10% speedup - found by Terje Mathisen
    0.995 - bugfix: fix to 'effective' overrun detection - found by Terje Mathisen
    0.994 - bugfix: garbage decode on final VQ symbol of a non-multiple - found by Terje Mathisen
    0.993 - bugfix: pushdata API required 1 extra byte for empty page (failed to consume final page if empty) - found by Terje Mathisen
    0.992 - fixes for MinGW warning
    0.991 - turn fast-float-conversion on by default
    0.990 - fix push-mode seek recovery if you seek into the headers
    0.98b - fix to bad release of 0.98
    0.98 - fix push-mode seek recovery; robustify float-to-int and support non-fast mode
    0.97 - builds under c++ (typecasting, don't use 'class' keyword)
    0.96 - somehow MY 0.95 was right, but the web one was wrong, so here's my 0.95 rereleased as 0.96, fixes a typo in the clamping code
    0.95 - clamping code for 16-bit functions
    0.94 - not publically released
    0.93 - fixed all-zero-floor case (was decoding garbage)
    0.92 - fixed a memory leak
    0.91 - conditional compiles to omit parts of the API and the infrastructure to support them: STB_VORBIS_NO_PULLDATA_API, STB_VORBIS_NO_PUSHDATA_API, STB_VORBIS_NO_STDIO, STB_VORBIS_NO_INTEGER_CONVERSION
    0.90 - first public release
*/


/*
------------------------------------------------------------------------------
This software is available under 2 licenses -- choose whichever you prefer.
------------------------------------------------------------------------------
ALTERNATIVE A - MIT License
Copyright (c) 2017 Sean Barrett
Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
------------------------------------------------------------------------------
ALTERNATIVE B - Public Domain (www.unlicense.org)
This is free and unencumbered software released into the public domain.
Anyone is free to copy, modify, publish, use, compile, sell, or distribute this
software, either in source code form or as a compiled binary, for any purpose,
commercial or non-commercial, and by any means.
In jurisdictions that recognize copyright laws, the author or authors of this
software dedicate any and all copyright interest in the software to the public
domain. We make this dedication for the benefit of the public at large and to
the detriment of our heirs and successors. We intend this dedication to be an
overt act of relinquishment in perpetuity of all present and future rights to
this software under copyright law.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
------------------------------------------------------------------------------
*/
