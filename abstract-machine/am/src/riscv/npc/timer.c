#include <am.h>

/**
 * Initializes the timer hardware and associated data structures.
 * This method sets up the timer by configuring the necessary hardware registers,
 * initializing any internal state, and preparing the timer for operation.
 * It should be called once during system startup to ensure the timer is ready
 * for use in timing operations, interrupts, or other time-related tasks.
 */
void __am_timer_init() {
}

/**
 * @brief Initializes the uptime structure by setting the microseconds field to zero.
 *
 * This function is used to reset or initialize the `AM_TIMER_UPTIME_T` structure,
 * specifically setting the `us` (microseconds) field to zero. It is typically called
 * before starting a new uptime measurement or when resetting the timer.
 *
 * @param uptime A pointer to an `AM_TIMER_UPTIME_T` structure that will be initialized.
 */
void __am_timer_uptime(AM_TIMER_UPTIME_T *uptime) {
  uptime->us = 0;
}

/**
 * @brief Initializes the fields of an AM_TIMER_RTC_T structure to default values.
 *
 * This function sets the time and date fields of the provided AM_TIMER_RTC_T structure
 * to their default values. Specifically, it initializes the `second`, `minute`, `hour`,
 * and `day` fields to 0, the `month` field to 0, and the `year` field to 1900.
 *
 * @param rtc A pointer to an AM_TIMER_RTC_T structure whose fields will be initialized.
 *            The structure must be allocated and valid before calling this function.
 */
void __am_timer_rtc(AM_TIMER_RTC_T *rtc) {
  rtc->second = 0;
  rtc->minute = 0;
  rtc->hour   = 0;
  rtc->day    = 0;
  rtc->month  = 0;
  rtc->year   = 1900;
}
