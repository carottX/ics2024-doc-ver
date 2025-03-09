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
  TYPE_2RI12, TYPE_1RI20,
  TYPE_N, // none
};

#define src1R()  do { *src1 = R(rj); } while (0)
#define simm12() do { *imm = SEXT(BITS(i, 21, 10), 12); } while (0)
#define simm20() do { *imm = SEXT(BITS(i, 24, 5), 20) << 12; } while (0)

/**
 * Decodes the operand of an instruction based on the specified type.
 *
 * This method extracts the relevant fields from the instruction word and
 * populates the provided pointers with the decoded values. The decoding
 * process depends on the type of the operand, which determines how the
 * instruction word is interpreted.
 *
 * @param s      Pointer to the Decode structure containing the instruction word.
 * @param rd_    Pointer to store the decoded destination register index.
 * @param src1   Pointer to store the decoded value of the first source operand.
 * @param src2   Pointer to store the decoded value of the second source operand.
 * @param imm    Pointer to store the decoded immediate value.
 * @param type   The type of the operand, which determines the decoding logic.
 *
 * The method performs the following steps:
 * 1. Extracts the instruction word from the Decode structure.
 * 2. Decodes the destination register index (`rd_`) from the instruction word.
 * 3. Depending on the operand type, it may decode immediate values (`imm`) or
 *    source register indices (`src1`, `src2`).
 * 4. If the operand type is not supported, the method triggers a panic with an
 *    error message.
 */
static void decode_operand(Decode *s, int *rd_, word_t *src1, word_t *src2, word_t *imm, int type) {
  uint32_t i = s->isa.inst;
  int rj = BITS(i, 9, 5);
  *rd_ = BITS(i, 4, 0);
  switch (type) {
    case TYPE_1RI20: simm20(); src1R(); break;
    case TYPE_2RI12: simm12(); src1R(); break;
    case TYPE_N: break;
    default: panic("Unsupport type = %d", type);
  }
}

/**
 * Decodes and executes the instruction stored in the Decode structure.
 * 
 * This method updates the next program counter (dnpc) to the current sequential 
 * program counter (snpc) and then processes the instruction based on the 
 * instruction pattern matching. It uses the INSTPAT macro to match the instruction 
 * pattern and execute the corresponding operation. The method supports various 
 * instruction types including pcaddu12i, ld.w, st.w, break, and inv. After 
 * executing the instruction, it resets the zero register ($zero) to 0.
 * 
 * @param s Pointer to the Decode structure containing the instruction and 
 *          associated state.
 * @return Always returns 0, indicating successful execution.
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
  INSTPAT("0001110 ????? ????? ????? ????? ?????" , pcaddu12i, 1RI20 , R(rd) = s->pc + imm);
  INSTPAT("0010100010 ???????????? ????? ?????"   , ld.w     , 2RI12 , R(rd) = Mr(src1 + imm, 4));
  INSTPAT("0010100110 ???????????? ????? ?????"   , st.w     , 2RI12 , Mw(src1 + imm, 4, R(rd)));

  INSTPAT("0000 0000 0010 10100 ????? ????? ?????", break    , N     , NEMUTRAP(s->pc, R(4))); // R(4) is $a0
  INSTPAT("????????????????? ????? ????? ?????"   , inv      , N     , INV(s->pc));
  INSTPAT_END();

  R(0) = 0; // reset $zero to 0

  return 0;
}

/**
 * Executes a single instruction by fetching it from memory and decoding/executing it.
 *
 * This function fetches a 4-byte instruction from memory starting at the address
 * specified by `s->snpc`. The fetched instruction is stored in the `s->isa.inst`
 * field. After fetching, the function decodes and executes the instruction using
 * the `decode_exec` function.
 *
 * @param s A pointer to a `Decode` structure containing the current state of the
 *          decoder, including the program counter (`snpc`) and the instruction
 *          storage (`isa.inst`).
 *
 * @return The result of the `decode_exec` function, which typically indicates
 *         the status or outcome of the instruction execution.
 */
int isa_exec_once(Decode *s) {
  s->isa.inst = inst_fetch(&s->snpc, 4);
  return decode_exec(s);
}
