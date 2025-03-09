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
 * This function is responsible for raising an interrupt or exception identified by the parameter `NO`.
 * It simulates the behavior of triggering an interrupt/exception in the system and returns the address
 * of the corresponding interrupt/exception vector. The `epc` parameter represents the program counter
 * value at the time the interrupt/exception was triggered, which may be used for context handling.
 *
 * @param NO The interrupt/exception number to be raised.
 * @param epc The program counter value at the time of the interrupt/exception.
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
 * This method checks the current interrupt state and returns a value indicating whether
 * an interrupt is pending or if the interrupt queue is empty.
 *
 * @return word_t Returns the interrupt status. The value INTR_EMPTY is returned if
 *         no interrupts are pending, indicating that the interrupt queue is empty.
 */
word_t isa_query_intr() {
  return INTR_EMPTY;
}
