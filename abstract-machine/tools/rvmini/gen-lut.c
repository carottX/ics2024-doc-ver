#include <stdio.h>
#include <assert.h>

#define LUT_BIN "lut.bin"

/**
 * @brief Generates and writes lookup tables (LUTs) for various bitwise operations to a binary file.
 *
 * This method creates lookup tables for bitwise operations including AND, OR, XOR, 
 * shift left logical (SLL), shift right logical (SRL), and shift right arithmetic (SRA). 
 * The generated tables are written to a binary file specified by the macro `LUT_BIN`.
 *
 * The method uses the `gen_table` macro to iterate over rows and columns, compute the result 
 * of the specified bitwise operation, and write the result to the file. Each table is generated 
 * with specific dimensions and entry sizes based on the operation.
 *
 * The generated tables are:
 * - `_and8_table`: 256x256 table for 8-bit AND operation.
 * - `_or8_table`: 256x256 table for 8-bit OR operation.
 * - `_xor8_table`: 256x256 table for 8-bit XOR operation.
 * - `_sll8_table`: 32x256 table for 8-bit SLL operation with 4-byte entries.
 * - `_srl8_table`: 32x256 table for 8-bit SRL operation with 4-byte entries.
 * - `_sra8_table`: 32x256 table for 8-bit SRA operation with 4-byte entries.
 *
 * The method asserts that the file is successfully opened before proceeding with table generation.
 * After generating and writing all tables, the file is closed.
 *
 * @return int Returns 0 upon successful completion.
 */
int main() {
  FILE *fp = fopen(LUT_BIN, "w");
  assert(fp != NULL);

#define gen_table(name, row, col, entry_size, expr) do { \
  for (int j = 0; j < row; j ++) { \
    for (int i = 0; i < col; i ++) { \
      unsigned res = (expr); \
      fwrite(&res, entry_size, 1, fp); \
    } \
  } \
} while (0)

  gen_table(_and8_table, 256, 256, 1, j & i);
  gen_table(_or8_table,  256, 256, 1, j | i);
  gen_table(_xor8_table, 256, 256, 1, j ^ i);
  gen_table(_sll8_table, 32, 256, 4, (unsigned)i << j);
  gen_table(_srl8_table, 32, 256, 4, ((unsigned)i << 24) >> j);
  gen_table(_sra8_table, 32, 256, 4, (int)((unsigned)i << 24) >> j);

  fclose(fp);

  return 0;
}
