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

#include "local-include/reg.h"
#include <cpu/cpu.h>
#include <cpu/ifetch.h>
#include <cpu/decode.h>

typedef union {
  struct {
    uint8_t R_M		:3;
    uint8_t reg		:3;
    uint8_t mod		:2;
  };
  struct {
    uint8_t dont_care	:3;
    uint8_t opcode		:3;
  };
  uint8_t val;
} ModR_M;

typedef union {
  struct {
    uint8_t base	:3;
    uint8_t index	:3;
    uint8_t ss		:2;
  };
  uint8_t val;
} SIB;

/**
 * Fetches the next instruction from the simulated memory and updates the instruction trace or queue if enabled.
 * 
 * This function fetches a word of length `len` from the memory location pointed to by the simulated program counter (PC)
 * in the Decode structure `s`. If instruction tracing (CONFIG_ITRACE) or instruction queue (CONFIG_IQUEUE) is enabled,
 * the fetched instruction is also stored in the instruction buffer within the Decode structure for further analysis or processing.
 * 
 * @param s Pointer to the Decode structure containing the current state of the simulated CPU, including the PC and instruction buffer.
 * @param len The length of the instruction to fetch, in bytes.
 * 
 * @return The fetched instruction as a word_t value. If instruction tracing or queueing is enabled, the original fetched value
 *         is returned after updating the instruction buffer. Otherwise, the fetched value is returned directly.
 */
static word_t x86_inst_fetch(Decode *s, int len) {
#if defined(CONFIG_ITRACE) || defined(CONFIG_IQUEUE)
  uint8_t *p = &s->isa.inst[s->snpc - s->pc];
  word_t ret = inst_fetch(&s->snpc, len);
  word_t ret_save = ret;
  int i;
  assert(s->snpc - s->pc < sizeof(s->isa.inst));
  for (i = 0; i < len; i ++) {
    p[i] = ret & 0xff;
    ret >>= 8;
  }
  return ret_save;
#else
  return inst_fetch(&s->snpc, len);
#endif
}

/**
 * Reads a value from a register based on the specified index and width.
 *
 * This function reads a value from a register identified by the index `idx`.
 * The width of the value to be read is specified by the `width` parameter, which
 * determines the size of the data to be retrieved:
 * - If `width` is 4, the function reads a long word (32-bit) from the register using `reg_l`.
 * - If `width` is 1, the function reads a byte (8-bit) from the register using `reg_b`.
 * - If `width` is 2, the function reads a word (16-bit) from the register using `reg_w`.
 * 
 * If an unsupported width is provided, the function triggers an assertion failure.
 *
 * @param idx The index of the register to read from.
 * @param width The width of the data to read (1 for byte, 2 for word, 4 for long word).
 * @return The value read from the register as a `word_t`.
 */
word_t reg_read(int idx, int width) {
  switch (width) {
    case 4: return reg_l(idx);
    case 1: return reg_b(idx);
    case 2: return reg_w(idx);
    default: assert(0);
  }
}

/**
 * Writes a value to a register based on the specified width.
 *
 * This method writes the provided `data` value to a register identified by `idx`.
 * The width of the register (in bytes) determines which specific register function
 * is used for the write operation:
 * - If `width` is 4, the value is written using `reg_l()` (long/4-byte register).
 * - If `width` is 1, the value is written using `reg_b()` (byte/1-byte register).
 * - If `width` is 2, the value is written using `reg_w()` (word/2-byte register).
 *
 * If an unsupported width is provided, the method triggers an assertion failure.
 *
 * @param idx   The index of the register to write to.
 * @param width The width of the register in bytes (1, 2, or 4).
 * @param data  The value to write to the register.
 */
static void reg_write(int idx, int width, word_t data) {
  switch (width) {
    case 4: reg_l(idx) = data; return;
    case 1: reg_b(idx) = data; return;
    case 2: reg_w(idx) = data; return;
    default: assert(0);
  }
}

/**
 * Loads the effective address based on the ModR/M byte and SIB (Scale-Index-Base) byte in the x86 instruction.
 * 
 * This function calculates the effective address by interpreting the ModR/M and SIB bytes of an x86 instruction.
 * It handles the following cases:
 * - The ModR/M byte specifies the addressing mode (mod field) and the base register (R/M field).
 * - If the R/M field is R_ESP, the SIB byte is fetched to determine the base, index, and scale.
 * - The displacement (disp) is fetched based on the mod field and added to the calculated address.
 * - The final effective address is computed by adding the displacement, base register value, and scaled index register value.
 * 
 * @param s Pointer to the Decode structure containing the instruction stream.
 * @param m Pointer to the ModR_M structure containing the ModR/M byte.
 * @param rm_addr Pointer to the word_t variable where the computed effective address will be stored.
 * 
 * @note The function asserts that the mod field in the ModR/M byte is not 3, as this indicates a register addressing mode,
 *       which is not handled by this function.
 */
static void load_addr(Decode *s, ModR_M *m, word_t *rm_addr) {
  assert(m->mod != 3);

  sword_t disp = 0;
  int disp_size = 4;
  int base_reg = -1, index_reg = -1, scale = 0;

  if (m->R_M == R_ESP) {
    SIB sib;
    sib.val = x86_inst_fetch(s, 1);
    base_reg = sib.base;
    scale = sib.ss;

    if (sib.index != R_ESP) { index_reg = sib.index; }
  }
  else { base_reg = m->R_M; } /* no SIB */

  if (m->mod == 0) {
    if (base_reg == R_EBP) { base_reg = -1; }
    else { disp_size = 0; }
  }
  else if (m->mod == 1) { disp_size = 1; }

  if (disp_size != 0) { /* has disp */
    disp = x86_inst_fetch(s, disp_size);
    if (disp_size == 1) { disp = (int8_t)disp; }
  }

  word_t addr = disp;
  if (base_reg != -1)  addr += reg_l(base_reg);
  if (index_reg != -1) addr += reg_l(index_reg) << scale;
  *rm_addr = addr;
}

/**
 * Decodes the ModR/M byte of an x86 instruction and updates the provided registers and address.
 * 
 * This method fetches the ModR/M byte from the instruction stream and interprets it to determine
 * the register and addressing mode. Depending on the value of the ModR/M byte, it updates the
 * `rm_reg` and `rm_addr` parameters to reflect the decoded register or memory address. If the
 * `reg` parameter is not NULL, it also updates it with the register value from the ModR/M byte.
 *
 * @param s Pointer to the Decode structure containing the instruction stream.
 * @param rm_reg Pointer to an integer where the R/M register value will be stored. If the
 *               addressing mode is not register-direct, this will be set to -1.
 * @param rm_addr Pointer to a word_t where the computed memory address will be stored if the
 *                addressing mode is not register-direct.
 * @param reg Pointer to an integer where the register value from the ModR/M byte will be stored.
 *            If NULL, this value is ignored.
 * @param width The width of the operand, which is not directly used in this function but may be
 *              relevant in the context of the caller.
 */
static void decode_rm(Decode *s, int *rm_reg, word_t *rm_addr, int *reg, int width) {
  ModR_M m;
  m.val = x86_inst_fetch(s, 1);
  if (reg != NULL) *reg = m.reg;
  if (m.mod == 3) *rm_reg = m.R_M;
  else { load_addr(s, &m, rm_addr); *rm_reg = -1; }
}

#define Rr reg_read
#define Rw reg_write
#define Mr vaddr_read
#define Mw vaddr_write
#define RMr(reg, w)  (reg != -1 ? Rr(reg, w) : Mr(addr, w))
#define RMw(data) do { if (rd != -1) Rw(rd, w, data); else Mw(addr, w, data); } while (0)

#define destr(r)  do { *rd_ = (r); } while (0)
#define src1r(r)  do { *src1 = Rr(r, w); } while (0)
#define imm()     do { *imm = x86_inst_fetch(s, w); } while (0)
#define simm(w)   do { *imm = SEXT(x86_inst_fetch(s, w), w * 8); } while (0)

enum {
  TYPE_r, TYPE_I, TYPE_SI, TYPE_J, TYPE_E,
  TYPE_I2r,  // XX <- Ib / eXX <- Iv
  TYPE_I2a,  // AL <- Ib / eAX <- Iv
  TYPE_G2E,  // Eb <- Gb / Ev <- Gv
  TYPE_E2G,  // Gb <- Eb / Gv <- Ev
  TYPE_I2E,  // Eb <- Ib / Ev <- Iv
  TYPE_Ib2E, TYPE_cl2E, TYPE_1_E, TYPE_SI2E,
  TYPE_Eb2G, TYPE_Ew2G,
  TYPE_O2a, TYPE_a2O,
  TYPE_I_E2G,  // Gv <- EvIb / Gv <- EvIv // use for imul
  TYPE_SI_E2G,  // Gv <- EvIb / Gv <- EvIv // use for imul
  TYPE_Ib_G2E, // Ev <- GvIb // use for shld/shrd
  TYPE_cl_G2E, // Ev <- GvCL // use for shld/shrd
  TYPE_N, // none
};

#define INSTPAT_INST(s) opcode
#define INSTPAT_MATCH(s, name, type, width, ... /* execute body */ ) { \
  int rd = 0, rs = 0, gp_idx = 0; \
  word_t src1 = 0, addr = 0, imm = 0; \
  int w = width == 0 ? (is_operand_size_16 ? 2 : 4) : width; \
  decode_operand(s, opcode, &rd, &src1, &addr, &rs, &gp_idx, &imm, w, concat(TYPE_, type)); \
  s->dnpc = s->snpc; \
  __VA_ARGS__ ; \
}

/**
 * Decodes an operand based on the specified type and updates the corresponding
 * destination and source registers, address, and immediate value.
 *
 * @param s        Pointer to the Decode structure containing instruction context.
 * @param opcode   The opcode of the instruction being decoded.
 * @param rd_      Pointer to store the destination register index.
 * @param src1     Pointer to store the first source operand value.
 * @param addr     Pointer to store the computed memory address.
 * @param rs       Pointer to store the source register index.
 * @param gp_idx   Pointer to store the general-purpose register index.
 * @param imm      Pointer to store the immediate value.
 * @param w        Width flag indicating operand size (e.g., 0 for byte, 1 for word).
 * @param type     The type of operand decoding to perform (e.g., TYPE_I2r, TYPE_G2E).
 *
 * The function handles various operand types:
 * - TYPE_I2r: Decodes an immediate value to a register.
 * - TYPE_G2E: Decodes a general-purpose register to a memory operand.
 * - TYPE_E2G: Decodes a memory operand to a general-purpose register.
 * - TYPE_I2E: Decodes an immediate value to a memory operand.
 * - TYPE_O2a: Decodes an offset to the accumulator register (EAX).
 * - TYPE_a2O: Decodes the accumulator register (EAX) to an offset.
 * - TYPE_N: No operand decoding required.
 *
 * If an unsupported type is provided, the function triggers a panic.
 */
static void decode_operand(Decode *s, uint8_t opcode, int *rd_, word_t *src1,
    word_t *addr, int *rs, int *gp_idx, word_t *imm, int w, int type) {
  switch (type) {
    case TYPE_I2r:  destr(opcode & 0x7); imm(); break;
    case TYPE_G2E:  decode_rm(s, rd_, addr, rs, w); src1r(*rs); break;
    case TYPE_E2G:  decode_rm(s, rs, addr, rd_, w); break;
    case TYPE_I2E:  decode_rm(s, rd_, addr, gp_idx, w); imm(); break;
    case TYPE_O2a:  destr(R_EAX); *addr = x86_inst_fetch(s, 4); break;
    case TYPE_a2O:  *rs = R_EAX;  *addr = x86_inst_fetch(s, 4); break;
    case TYPE_N:    break;
    default: panic("Unsupported type = %d", type);
  }
}

#define gp1() do { \
  switch (gp_idx) { \
    default: INV(s->pc); \
  }; \
} while (0)

/**
 * Processes a 2-byte escape sequence in the x86 instruction decoding process.
 * This method is typically invoked when the instruction decoder encounters
 * an escape opcode prefix (e.g., 0x0F) in the instruction stream. It fetches
 * the subsequent opcode byte and uses the instruction pattern matching system
 * to determine the appropriate decoding behavior based on the fetched opcode.
 *
 * @param s Pointer to the Decode structure containing the current decoding state.
 * @param is_operand_size_16 A boolean flag indicating whether the operand size is 16 bits.
 *                           This can influence the decoding of certain instructions.
 */
void _2byte_esc(Decode *s, bool is_operand_size_16) {
  uint8_t opcode = x86_inst_fetch(s, 1);
  INSTPAT_START();
  INSTPAT("???? ????", inv,    N,    0, INV(s->pc));
  INSTPAT_END();
}

/**
 * Executes a single instruction based on the provided decoder state.
 * This method fetches the opcode of the instruction and matches it against
 * a set of predefined patterns to determine the appropriate action.
 * 
 * The method supports handling various x86 instructions, including:
 * - 2-byte escape sequences
 * - Data size overrides
 * - General-purpose instructions (e.g., MOV)
 * - Immediate-to-register/memory operations
 * - Special cases like NEMU traps and invalid instructions
 * 
 * The method uses a pattern-matching mechanism (INSTPAT) to dispatch
 * the correct handler for the fetched opcode. If the opcode indicates
 * a data size override (e.g., 0x66), the method adjusts the operand size
 * and re-fetches the opcode to process the next instruction.
 * 
 * @param s A pointer to the Decode structure containing the current
 *          instruction decoding state.
 * @return Returns 0 upon successful execution of the instruction.
 */
int isa_exec_once(Decode *s) {
  bool is_operand_size_16 = false;
  uint8_t opcode = 0;

again:
  opcode = x86_inst_fetch(s, 1);

  INSTPAT_START();

  INSTPAT("0000 1111", 2byte_esc, N,    0, _2byte_esc(s, is_operand_size_16));

  INSTPAT("0110 0110", data_size, N,    0, is_operand_size_16 = true; goto again;);

  INSTPAT("1000 0000", gp1,       I2E,  1, gp1());
  INSTPAT("1000 1000", mov,       G2E,  1, RMw(src1));
  INSTPAT("1000 1001", mov,       G2E,  0, RMw(src1));
  INSTPAT("1000 1010", mov,       E2G,  1, Rw(rd, w, RMr(rs, w)));
  INSTPAT("1000 1011", mov,       E2G,  0, Rw(rd, w, RMr(rs, w)));

  INSTPAT("1010 0000", mov,       O2a,  1, Rw(R_EAX, 1, Mr(addr, 1)));
  INSTPAT("1010 0001", mov,       O2a,  0, Rw(R_EAX, w, Mr(addr, w)));
  INSTPAT("1010 0010", mov,       a2O,  1, Mw(addr, 1, Rr(R_EAX, 1)));
  INSTPAT("1010 0011", mov,       a2O,  0, Mw(addr, w, Rr(R_EAX, w)));

  INSTPAT("1011 0???", mov,       I2r,  1, Rw(rd, 1, imm));
  INSTPAT("1011 1???", mov,       I2r,  0, Rw(rd, w, imm));

  INSTPAT("1100 0110", mov,       I2E,  1, RMw(imm));
  INSTPAT("1100 0111", mov,       I2E,  0, RMw(imm));
  INSTPAT("1100 1100", nemu_trap, N,    0, NEMUTRAP(s->pc, cpu.eax));
  INSTPAT("???? ????", inv,       N,    0, INV(s->pc));
  INSTPAT_END();

  return 0;
}
