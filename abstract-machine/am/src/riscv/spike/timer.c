#include <am.h>

static uint64_t boot_time = 0;

#define CLINT_MMIO 0x2000000ul
#define TIME_BASE 0xbff8

/**
 * Reads the current time from the Core Local Interruptor (CLINT) timer.
 * 
 * The CLINT timer is accessed via memory-mapped I/O (MMIO) at the base address
 * defined by `CLINT_MMIO`. The timer value is stored in two 32-bit registers:
 * one for the lower 32 bits (LO) and one for the higher 32 bits (HI). This method
 * combines these two 32-bit values into a single 64-bit value representing the
 * current time in timer ticks.
 * 
 * The returned time value is divided by 10 to convert it into a more usable unit,
 * such as microseconds or another scaled time unit, depending on the timer's
 * frequency.
 * 
 * @return The current time in a scaled unit (e.g., microseconds) as a 64-bit value.
 */
static uint64_t read_time() {
  uint32_t lo = *(volatile uint32_t *)(CLINT_MMIO + TIME_BASE + 0);
  uint32_t hi = *(volatile uint32_t *)(CLINT_MMIO + TIME_BASE + 4);
  uint64_t time = ((uint64_t)hi << 32) | lo;
  return time / 10;
}

/**
 * @brief Updates the uptime value in the provided AM_TIMER_UPTIME_T structure.
 *
 * This function calculates the current uptime by subtracting the boot time
 * from the current time, as returned by the `read_time()` function. The result
 * is stored in the `us` field of the `uptime` structure, representing the
 * elapsed time in microseconds since the system booted.
 *
 * @param uptime Pointer to an AM_TIMER_UPTIME_T structure where the uptime
 *               value will be stored.
 */
void __am_timer_uptime(AM_TIMER_UPTIME_T *uptime) {
  uptime->us = read_time() - boot_time;
}

/**
 * Initializes the timer by recording the current boot time.
 * This function reads the current time using the `read_time()` function
 * and stores it in the `boot_time` variable. The `boot_time` is typically
 * used as a reference point for measuring elapsed time since system startup.
 */
void __am_timer_init() {
  boot_time = read_time();
}

/**
 * @brief Resets the fields of an AM_TIMER_RTC_T structure to default values.
 *
 * This function initializes the real-time clock (RTC) structure by setting all 
 * time-related fields to zero, except for the year, which is set to 1900. 
 * Specifically, the second, minute, hour, day, and month fields are reset to 0, 
 * while the year field is initialized to 1900.
 *
 * @param rtc Pointer to an AM_TIMER_RTC_T structure whose fields will be reset.
 */
void __am_timer_rtc(AM_TIMER_RTC_T *rtc) {
  rtc->second = 0;
  rtc->minute = 0;
  rtc->hour   = 0;
  rtc->day    = 0;
  rtc->month  = 0;
  rtc->year   = 1900;
}
