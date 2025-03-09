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

#include <common.h>
#include <device/alarm.h>
#include <sys/time.h>
#include <signal.h>

#define MAX_HANDLER 8

static alarm_handler_t handler[MAX_HANDLER] = {};
static int idx = 0;

/**
 * Adds an alarm handler to the list of handlers.
 * 
 * This function appends the provided alarm handler `h` to the internal array of handlers.
 * The handler is stored at the current index `idx`, which is then incremented. The function
 * asserts that the index `idx` is within the valid range (less than `MAX_HANDLER`) to prevent
 * buffer overflow.
 * 
 * @param h The alarm handler to be added to the list of handlers.
 */
void add_alarm_handle(alarm_handler_t h) {
  assert(idx < MAX_HANDLER);
  handler[idx ++] = h;
}

/**
 * @brief Signal handler for alarm signals.
 *
 * This function is invoked when an alarm signal (SIGALRM) is received. It iterates
 * through the registered handler functions up to the current index (`idx`) and
 * executes each one sequentially. This allows multiple functions to be triggered
 * in response to the alarm signal.
 *
 * @param signum The signal number received. This is expected to be SIGALRM.
 */
static void alarm_sig_handler(int signum) {
  int i;
  for (i = 0; i < idx; i ++) {
    handler[i]();
  }
}

/**
 * Initializes a virtual timer alarm that triggers at a specified frequency.
 * 
 * This function sets up a signal handler for the SIGVTALRM signal, which is
 * invoked when the virtual timer expires. The timer is configured to trigger
 * initially after a delay of (1,000,000 / TIMER_HZ) microseconds and then
 * repeatedly at the same interval. The signal handler `alarm_sig_handler` is
 * responsible for processing the SIGVTALRM signal.
 *
 * The function performs the following steps:
 * 1. Initializes a `sigaction` structure to define the signal handler for SIGVTALRM.
 * 2. Registers the signal handler using `sigaction`.
 * 3. Configures an `itimerval` structure to set the initial delay and interval
 *    for the virtual timer.
 * 4. Activates the timer using `setitimer`.
 *
 * If any step fails, the program asserts with an error message indicating the
 * failure.
 *
 * @note The `TIMER_HZ` macro must be defined to specify the timer frequency.
 * @note The `alarm_sig_handler` function must be implemented to handle SIGVTALRM.
 * @note This function does not return any value but asserts on failure.
 */
void init_alarm() {
  struct sigaction s;
  memset(&s, 0, sizeof(s));
  s.sa_handler = alarm_sig_handler;
  int ret = sigaction(SIGVTALRM, &s, NULL);
  Assert(ret == 0, "Can not set signal handler");

  struct itimerval it = {};
  it.it_value.tv_sec = 0;
  it.it_value.tv_usec = 1000000 / TIMER_HZ;
  it.it_interval = it.it_value;
  ret = setitimer(ITIMER_VIRTUAL, &it, NULL);
  Assert(ret == 0, "Can not set timer");
}
