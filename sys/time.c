#include "user/time.h"
#include "sys/types.h"

#define leap_days_to_1970           477

uint64_t system_time = 0;
time_t system_boot_time = 0;

static const int days_to_month365[] = {
    0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365
};

static const int days_to_month366[] = {
    0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366
};

static inline int is_leap(int year) {
    return (year % 4 == 0 && ((year % 100 != 0) || (year % 400 == 0)));
}

static int tm_check(const struct tm *tm) {
    return tm->tm_mday < 0 || tm->tm_mday > 31 ||
           tm->tm_mon < 0 || tm->tm_mon > 12 ||
           tm->tm_year < 1970 || tm->tm_year > 2500 ||
           tm->tm_hour < 0 || tm->tm_hour > 23 ||
           tm->tm_min < 0 || tm->tm_min > 59 ||
           tm->tm_sec < 0 || tm->tm_sec > 59;
}

time_t time(void) {
    return system_boot_time + (time_t) (system_time / 1000000000ULL);
}

time_t mktime(struct tm *tm) {
    if (tm_check(tm) != 0) {
        return 0;
    }

    // Calculate year day when month starts
    int month_startd = (is_leap(tm->tm_year) ? days_to_month366 : days_to_month365)[tm->tm_mon - 1];

    // Calculate number of days from epoch to year start
    int prev_year = tm->tm_year;
    int days_before = ((prev_year - 1970) * 365) + (prev_year) / 4 - (prev_year) / 100 + (prev_year / 400) - leap_days_to_1970;

    // Add number of seconds in epoch days
    time_t result = days_before + month_startd + tm->tm_mday - 2;
    result *= 86400;

    result += tm->tm_hour * 3600 + tm->tm_min * 60 + tm->tm_sec;

    return result;
}
