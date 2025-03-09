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
 * @brief Raises an interrupt signal to the device.
 *
 * This method is used to trigger an interrupt on the device, signaling that
 * an event or condition requiring attention has occurred. The specific behavior
 * of the interrupt, including how it is handled, depends on the device's
 * interrupt handling mechanism and the system's configuration.
 *
 * @note This function does not take any parameters or return any value. It is
 * expected that the device's interrupt handler will process the interrupt
 * appropriately once it is raised.
 */
void dev_raise_intr() {
}
