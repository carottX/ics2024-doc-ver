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
#include MUXDEF(CONFIG_TIMER_GETTIMEOFDAY, <sys/time.h>, <time.h>)

IFDEF(CONFIG_TIMER_CLOCK_GETTIME,
    static_assert(CLOCKS_PER_SEC == 1000000, "CLOCKS_PER_SEC != 1000000"));
IFDEF(CONFIG_TIMER_CLOCK_GETTIME,
    static_assert(sizeof(clock_t) == 8, "sizeof(clock_t) != 8"));

static uint64_t boot_time = 0;

/**
 * @brief Retrieves the current time in microseconds.
 *
 * This function returns the current time in microseconds, using different
 * mechanisms depending on the configuration:
 * - If `CONFIG_TARGET_AM` is defined, it reads the uptime from the AM_TIMER_UPTIME
 *   register.
 * - If `CONFIG_TIMER_GETTIMEOFDAY` is defined, it uses the `gettimeofday` function
 *   to get the current time and converts it to microseconds.
 * - Otherwise, it uses the `clock_gettime` function with `CLOCK_MONOTONIC_COARSE`
 *   to get the current time and converts it to microseconds.
 *
 * @return uint64_t The current time in microseconds.
 */
static uint64_t get_time_internal() {
#if defined(CONFIG_TARGET_AM)
  uint64_t us = io_read(AM_TIMER_UPTIME).us;
#elif defined(CONFIG_TIMER_GETTIMEOFDAY)
  struct timeval now;
  gettimeofday(&now, NULL);
  uint64_t us = now.tv_sec * 1000000 + now.tv_usec;
#else
  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC_COARSE, &now);
  uint64_t us = now.tv_sec * 1000000 + now.tv_nsec / 1000;
#endif
  return us;
}

/**
 * Retrieves the current time relative to the system boot time.
 * 
 * This function calculates the elapsed time since the system boot by subtracting
 * the boot time from the current time. If the boot time has not been initialized,
 * it first calls `get_time_internal()` to obtain the boot time and stores it in
 * the `boot_time` variable. The function then retrieves the current time using
 * `get_time_internal()` again and returns the difference between the current time
 * and the boot time.
 * 
 * @return The elapsed time in some unit (e.g., microseconds, nanoseconds) since
 *         the system boot.
 */
uint64_t get_time() {
  if (boot_time == 0) boot_time = get_time_internal();
  uint64_t now = get_time_internal();
  return now - boot_time;
}

/**
 * Initializes the random number generator with a seed based on the current time.
 * This function retrieves the current time using the internal `get_time_internal`
 * function and uses it as the seed for the `srand` function. This ensures that
 * subsequent calls to `rand` will produce a different sequence of pseudo-random
 * numbers each time the program is run, provided the system time has changed.
 * This function should be called once at the start of the program to ensure
 * proper randomization.
 */
void init_rand() {
  srand(get_time_internal());
}
