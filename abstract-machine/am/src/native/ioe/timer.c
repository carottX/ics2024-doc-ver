#include <am.h>
#include <sys/time.h>
#include <time.h>

static struct timeval boot_time = {};

/**
 * Configures the timer module by setting its presence and RTC (Real-Time Clock) capabilities.
 * This function initializes the provided `AM_TIMER_CONFIG_T` structure by setting both the
 * `present` and `has_rtc` fields to `true`, indicating that the timer module is available
 * and supports RTC functionality.
 *
 * @param cfg Pointer to an `AM_TIMER_CONFIG_T` structure that will be configured.
 */
void __am_timer_config(AM_TIMER_CONFIG_T *cfg) {
  cfg->present = cfg->has_rtc = true;
}

/**
 * Populates the provided AM_TIMER_RTC_T structure with the current date and time.
 * This function retrieves the current time using the `time` function, converts it to
 * a local time representation using `localtime`, and then assigns the individual
 * components (seconds, minutes, hours, day, month, and year) to the corresponding
 * fields in the `rtc` structure.
 *
 * @param rtc Pointer to an AM_TIMER_RTC_T structure where the current date and time
 *            will be stored. The structure must be pre-allocated by the caller.
 *            The fields in the structure will be populated as follows:
 *            - `second`: Current second (0-59)
 *            - `minute`: Current minute (0-59)
 *            - `hour`: Current hour (0-23)
 *            - `day`: Current day of the month (1-31)
 *            - `month`: Current month (1-12)
 *            - `year`: Current year (e.g., 2023)
 */
void __am_timer_rtc(AM_TIMER_RTC_T *rtc) {
  time_t t = time(NULL);
  struct tm *tm = localtime(&t);
  rtc->second = tm->tm_sec;
  rtc->minute = tm->tm_min;
  rtc->hour   = tm->tm_hour;
  rtc->day    = tm->tm_mday;
  rtc->month  = tm->tm_mon + 1;
  rtc->year   = tm->tm_year + 1900;
}

/**
 * Calculates the system uptime in microseconds and stores it in the provided
 * `AM_TIMER_UPTIME_T` structure. The uptime is computed as the difference between
 * the current time (obtained using `gettimeofday`) and the `boot_time` (a global
 * variable representing the system's boot time). The result is rounded to the
 * nearest microsecond by adding 500 microseconds before storing it in the
 * `uptime->us` field.
 *
 * @param uptime Pointer to an `AM_TIMER_UPTIME_T` structure where the calculated
 *               uptime in microseconds will be stored.
 */
void __am_timer_uptime(AM_TIMER_UPTIME_T *uptime) {
  struct timeval now;
  gettimeofday(&now, NULL);
  long seconds = now.tv_sec - boot_time.tv_sec;
  long useconds = now.tv_usec - boot_time.tv_usec;
  uptime->us = seconds * 1000000 + (useconds + 500);
}

/**
 * Initializes the timer by capturing the current time of day and storing it in
 * the `boot_time` variable. This method is typically used to record the system's
 * boot time or the start time of a process, which can later be used to calculate
 * elapsed time or other time-based metrics.
 *
 * The `gettimeofday` function is used to retrieve the current time, which includes
 * seconds and microseconds since the Epoch (00:00:00 UTC, January 1, 1970). The
 * result is stored in the `boot_time` structure for future reference.
 */
void __am_timer_init() {
  gettimeofday(&boot_time, NULL);
}
