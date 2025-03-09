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
  TYPE_I, TYPE_U, TYPE_S,
  TYPE_N, // none
};

#define src1R() do { *src1 = R(rs1); } while (0)
#define src2R() do { *src2 = R(rs2); } while (0)
#define immI() do { *imm = SEXT(BITS(i, 31, 20), 12); } while(0)
#define immU() do { *imm = SEXT(BITS(i, 31, 12), 20) << 12; } while(0)
#define immS() do { *imm = (SEXT(BITS(i, 31, 25), 7) << 5) | BITS(i, 11, 7); } while(0)

/**
 * Decodes the operand of an instruction based on the given type.
 *
 * This function extracts the source registers, destination register, and immediate value
 * from the instruction word based on the specified operand type. The extracted values
 * are stored in the provided pointers.
 *
 * @param s     Pointer to the Decode structure containing the instruction to decode.
 * @param rd    Pointer to store the destination register index.
 * @param src1  Pointer to store the value of the first source register.
 * @param src2  Pointer to store the value of the second source register.
 * @param imm   Pointer to store the immediate value.
 * @param type  The type of the operand to decode (e.g., TYPE_I, TYPE_U, TYPE_S, TYPE_N).
 *
 * The function performs the following steps:
 * 1. Extracts the instruction word from the Decode structure.
 * 2. Determines the source registers (rs1 and rs2) and destination register (rd) from the instruction word.
 * 3. Depending on the operand type, it calls the appropriate helper functions to extract the immediate value
 *    and source register values.
 * 4. If the operand type is unsupported, the function triggers a panic with an error message.
 */
static void decode_operand(Decode *s, int *rd, word_t *src1, word_t *src2, word_t *imm, int type) {
  uint32_t i = s->isa.inst;
  int rs1 = BITS(i, 19, 15);
  int rs2 = BITS(i, 24, 20);
  *rd     = BITS(i, 11, 7);
  switch (type) {
    case TYPE_I: src1R();          immI(); break;
    case TYPE_U:                   immU(); break;
    case TYPE_S: src1R(); src2R(); immS(); break;
    case TYPE_N: break;
    default: panic("unsupported type = %d", type);
  }
}

/**
 * Decodes and executes an instruction based on the current state of the Decode structure.
 * This method updates the next program counter (dnpc) to the current sequential program counter (snpc).
 * It then uses a series of instruction patterns (INSTPAT) to match the current instruction in the Decode structure
 * and execute the corresponding operation. The method handles various instruction types, including:
 * - AUIPC (Add Upper Immediate to PC): Updates the destination register with the sum of the PC and an immediate value.
 * - LBU (Load Byte Unsigned): Loads an unsigned byte from memory into the destination register.
 * - SB (Store Byte): Stores a byte from a source register into memory.
 * - EBREAK (Breakpoint): Triggers a trap for debugging purposes.
 * - Invalid Instruction: Handles unrecognized instructions by triggering an invalid instruction trap.
 * After executing the instruction, the method ensures that the zero register (R(0)) is reset to 0.
 *
 * @param s Pointer to the Decode structure containing the current instruction and state.
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
  INSTPAT("??????? ????? ????? ??? ????? 00101 11", auipc  , U, R(rd) = s->pc + imm);
  INSTPAT("??????? ????? ????? 100 ????? 00000 11", lbu    , I, R(rd) = Mr(src1 + imm, 1));
  INSTPAT("??????? ????? ????? 000 ????? 01000 11", sb     , S, Mw(src1 + imm, 1, src2));

  INSTPAT("0000000 00001 00000 000 00000 11100 11", ebreak , N, NEMUTRAP(s->pc, R(10))); // R(10) is $a0
  INSTPAT("??????? ????? ????? ??? ????? ????? ??", inv    , N, INV(s->pc));
  INSTPAT_END();

  R(0) = 0; // reset $zero to 0

  return 0;
}

/**
 * Executes a single instruction in the ISA (Instruction Set Architecture) context.
 * 
 * This method fetches the next instruction from memory using the current value of the 
 * simulated next program counter (s->snpc) and stores it in the instruction field of 
 * the ISA context (s->isa.inst). The instruction is fetched as a 4-byte value. 
 * After fetching the instruction, the method decodes and executes it by calling 
 * the decode_exec function with the Decode context (s) as an argument.
 * 
 * @param s A pointer to the Decode context containing the ISA state and the 
 *          simulated next program counter (snpc).
 * @return The result of the decode_exec function, which typically indicates the 
 *         success or failure of the instruction execution.
 */
int isa_exec_once(Decode *s) {
  s->isa.inst = inst_fetch(&s->snpc, 4);
  return decode_exec(s);
}
