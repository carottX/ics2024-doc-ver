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
 * @brief Triggers an interrupt or exception with the specified interrupt number and returns the address of the interrupt/exception vector.
 *
 * This function is responsible for raising an interrupt or exception identified by the parameter `NO`. 
 * The function simulates the behavior of triggering an interrupt/exception in a system, typically 
 * by setting the appropriate hardware flags or invoking the corresponding interrupt handler. 
 * The `epc` parameter represents the program counter (PC) value at the time the interrupt/exception 
 * was triggered, which is often used to save the context or determine the return address.
 *
 * @param NO The interrupt/exception number to be triggered. This value uniquely identifies the type 
 *           of interrupt or exception being raised.
 * @param epc The program counter (PC) value at the time the interrupt/exception was triggered. This 
 *            is typically used to save the context or determine the return address.
 * @return The address of the interrupt/exception vector, which is the entry point for the corresponding 
 *         interrupt/exception handler. In the current implementation, this function returns 0, 
 *         indicating that the functionality is not yet implemented.
 */
word_t isa_raise_intr(word_t NO, vaddr_t epc) {
  /* TODO: Trigger an interrupt/exception with ``NO''.
   * Then return the address of the interrupt/exception vector.
   */

  return 0;
}

/**
 * Queries the current interrupt status of the ISA (Instruction Set Architecture).
 * This method checks the interrupt state and returns a value indicating whether
 * an interrupt is pending or not. The returned value is of type `word_t`, which
 * represents the interrupt status. In this implementation, the method always
 * returns `INTR_EMPTY`, indicating that no interrupt is currently pending.
 *
 * @return word_t The interrupt status, where `INTR_EMPTY` signifies no pending interrupt.
 */
word_t isa_query_intr() {
  return INTR_EMPTY;
}
