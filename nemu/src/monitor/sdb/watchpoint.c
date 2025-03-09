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

#include "sdb.h"

#define NR_WP 32

typedef struct watchpoint {
  int NO;
  struct watchpoint *next;

  /* TODO: Add more members if necessary */

} WP;

static WP wp_pool[NR_WP] = {};
static WP *head = NULL, *free_ = NULL;

/**
 * Initializes the watchpoint pool by setting up the linked list structure
 * and initializing the necessary pointers.
 *
 * This function iterates through the watchpoint pool array (`wp_pool`) and
 * assigns a unique number (`NO`) to each watchpoint. It also sets up the
 * `next` pointer for each watchpoint to point to the next watchpoint in the
 * array, creating a linked list. The last watchpoint's `next` pointer is set
 * to `NULL` to indicate the end of the list.
 *
 * After initializing the watchpoint pool, the `head` pointer is set to `NULL`
 * to indicate that no watchpoints are currently in use, and the `free_` pointer
 * is set to the beginning of the watchpoint pool, indicating that all
 * watchpoints are available for allocation.
 */
void init_wp_pool() {
  int i;
  for (i = 0; i < NR_WP; i ++) {
    wp_pool[i].NO = i;
    wp_pool[i].next = (i == NR_WP - 1 ? NULL : &wp_pool[i + 1]);
  }

  head = NULL;
  free_ = wp_pool;
}

/* TODO: Implement the functionality of watchpoint */

