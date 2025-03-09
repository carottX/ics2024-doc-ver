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

#include <isa.h>

/**
 * @brief Triggers an interrupt or exception and returns the address of the interrupt/exception vector.
 *
 * This function is responsible for triggering an interrupt or exception identified by the parameter `NO`.
 * The `epc` parameter represents the program counter (PC) value at the point where the interrupt/exception
 * was triggered. After handling the interrupt/exception, the function returns the address of the
 * corresponding interrupt/exception vector, which is used to jump to the appropriate handler.
 *
 * @param NO The interrupt/exception number to be triggered.
 * @param epc The program counter (PC) value at the time of the interrupt/exception.
 * @return The address of the interrupt/exception vector. Currently, this function returns 0 as a placeholder.
 */
word_t isa_raise_intr(word_t NO, vaddr_t epc) {
  /* TODO: Trigger an interrupt/exception with ``NO''.
   * Then return the address of the interrupt/exception vector.
   */

  return 0;
}

/**
 * Queries the interrupt status of the ISA (Industry Standard Architecture) system.
 * This method checks the current interrupt status and returns a value indicating 
 * whether an interrupt is pending or not. In this implementation, it always returns 
 * `INTR_EMPTY`, indicating that no interrupt is currently pending.
 *
 * @return word_t Returns the interrupt status, specifically `INTR_EMPTY` to signify 
 *         that no interrupt is pending.
 */
word_t isa_query_intr() {
  return INTR_EMPTY;
}
