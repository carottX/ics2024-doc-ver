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

#define R(i) gpr(i)
#define Mr vaddr_read
#define Mw vaddr_write

enum {
  TYPE_I, TYPE_U,
  TYPE_N, // none
};

#define src1R() do { *src1 = R(rs); } while (0)
#define src2R() do { *src2 = R(rt); } while (0)
#define immI() do { *imm = SEXT(BITS(i, 15, 0), 16); } while(0)
#define immU() do { *imm = BITS(i, 15, 0); } while(0)

/**
 * Decodes the operand of an instruction based on the specified type.
 *
 * This method extracts and decodes the operand values from the instruction
 * stored in the Decode structure. Depending on the operand type, it sets the
 * destination register (`rd`), source registers (`src1`, `src2`), and immediate
 * value (`imm`). The decoding logic varies based on the instruction type.
 *
 * @param s     Pointer to the Decode structure containing the instruction.
 * @param rd    Pointer to store the destination register index.
 * @param src1  Pointer to store the first source operand value.
 * @param src2  Pointer to store the second source operand value.
 * @param imm   Pointer to store the immediate value.
 * @param type  The type of the operand (e.g., TYPE_I, TYPE_U, TYPE_N).
 */
static void decode_operand(Decode *s, int *rd, word_t *src1, word_t *src2, word_t *imm, int type) {
  uint32_t i = s->isa.inst;
  int rt = BITS(i, 20, 16);
  int rs = BITS(i, 25, 21);
  *rd = (type == TYPE_U || type == TYPE_I) ? rt : BITS(i, 15, 11);
  switch (type) {
    case TYPE_I: src1R(); immI(); break;
    case TYPE_U: src1R(); immU(); break;
    case TYPE_N: break;
    default: panic("unsupported type = %d", type);
  }
}

/**
 * @brief Decodes and executes an instruction based on the given Decode structure.
 *
 * This method takes a Decode structure `s` as input, which contains the current
 * instruction and related state. It updates the next program counter (`dnpc`)
 * to the current sequential program counter (`snpc`). The method then uses the
 * `INSTPAT` macro to match the instruction pattern and execute the corresponding
 * operation. The instruction patterns include:
 * - `lui`: Load Upper Immediate
 * - `lw`: Load Word
 * - `sw`: Store Word
 * - `sdbbp`: Software Debug Breakpoint
 * - `inv`: Invalid Instruction
 *
 * The method decodes the operands (register destination `rd`, source registers
 * `src1` and `src2`, and immediate value `imm`) and executes the instruction
 * based on its type. After execution, it resets the zero register (`$zero`) to 0.
 *
 * @param s Pointer to the Decode structure containing the instruction and state.
 * @return int Always returns 0, indicating successful execution.
 */
static int decode_exec(Decode *s) {
  s->dnpc = s->snpc;

#define INSTPAT_INST(s) ((s)->isa.inst)
#define INSTPAT_MATCH(s, name, type, ... /* execute body */ ) { \
  int rd = 0; \
  word_t src1 = 0, src2 = 0, imm = 0; \
  decode_operand(s, &rd, &src1, &src2, &imm, concat(TYPE_, type)); \
  __VA_ARGS__ ; \
}

  INSTPAT_START();
  INSTPAT("001111 ????? ????? ????? ????? ??????", lui    , U, R(rd) = imm << 16);
  INSTPAT("100011 ????? ????? ????? ????? ??????", lw     , I, R(rd) = Mr(src1 + imm, 4));
  INSTPAT("101011 ????? ????? ????? ????? ??????", sw     , I, Mw(src1 + imm, 4, R(rd)));

  INSTPAT("011100 ????? ????? ????? ????? 111111", sdbbp  , N, NEMUTRAP(s->pc, R(2))); // R(2) is $v0;
  INSTPAT("?????? ????? ????? ????? ????? ??????", inv    , N, INV(s->pc));
  INSTPAT_END();

  R(0) = 0; // reset $zero to 0

  return 0;
}

/**
 * Executes a single instruction by fetching it from memory and decoding/executing it.
 * 
 * This method fetches a 4-byte instruction from memory starting at the address specified 
 * by `s->snpc`. The fetched instruction is stored in the `inst` field of the `isa` structure 
 * within the `Decode` object `s`. After fetching the instruction, the method calls 
 * `decode_exec(s)` to decode and execute the fetched instruction.
 *
 * @param s A pointer to the `Decode` object containing the current state of the processor.
 * @return The result of the `decode_exec(s)` call, which typically indicates the success 
 *         or failure of the instruction execution.
 */
int isa_exec_once(Decode *s) {
  s->isa.inst = inst_fetch(&s->snpc, 4);
  return decode_exec(s);
}
