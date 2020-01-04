#pragma once
#include "sys/types.h"

struct rtc_time {
    uint8_t second;
    uint8_t minute;
    uint8_t hour;
    uint8_t weekday;
    uint8_t day;
    uint8_t month;
    uint16_t year;
};

void rtc_read(struct rtc_time *time);
void rtc_set_century(uint8_t c);
void rtc_init(void);
