#include <am.h>
#include <nemu.h>

/**
 * Initializes the timer hardware or software timer mechanism.
 * This method sets up the necessary configurations for the timer,
 * such as setting the timer frequency, enabling timer interrupts,
 * or initializing any required data structures. It ensures that
 * the timer is ready to be used for scheduling or timekeeping purposes.
 * This function should be called once during system initialization
 * before any timer-related operations are performed.
 */
void __am_timer_init() {
}

/**
 * @brief Initializes the uptime structure by setting the microseconds field to zero.
 *
 * This function is responsible for initializing an AM_TIMER_UPTIME_T structure
 * by setting its `us` (microseconds) field to zero. This can be used to reset
 * or initialize the uptime tracking mechanism before starting a new measurement.
 *
 * @param uptime Pointer to the AM_TIMER_UPTIME_T structure to be initialized.
 */
void __am_timer_uptime(AM_TIMER_UPTIME_T *uptime) {
  uptime->us = 0;
}

/**
 * @brief Resets the fields of an AM_TIMER_RTC_T structure to default values.
 *
 * This function initializes the fields of the provided AM_TIMER_RTC_T structure
 * to their default values. Specifically, it sets the `second`, `minute`, `hour`,
 * `day`, and `month` fields to 0, and the `year` field to 1900. This is typically
 * used to reset or initialize a real-time clock (RTC) structure before further
 * processing or configuration.
 *
 * @param rtc Pointer to the AM_TIMER_RTC_T structure to be reset.
 */
void __am_timer_rtc(AM_TIMER_RTC_T *rtc) {
  rtc->second = 0;
  rtc->minute = 0;
  rtc->hour   = 0;
  rtc->day    = 0;
  rtc->month  = 0;
  rtc->year   = 1900;
}
