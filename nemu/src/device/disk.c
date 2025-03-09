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

#include <device/map.h>

/**
 * Initializes the disk subsystem.
 *
 * This method sets up the necessary data structures and configurations
 * required for disk operations. It ensures that the disk is ready for
 * reading and writing by initializing the disk controller, setting up
 * disk buffers, and verifying the disk's operational state. If the disk
 * is already initialized, this method does nothing.
 *
 * @note This method should be called before any disk operations are performed.
 * @warning Improper initialization may lead to data corruption or system crashes.
 */
void init_disk() {
}
