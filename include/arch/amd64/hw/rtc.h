#pragma once
#include "sys/types.h"

void rtc_read(struct tm *time);
void rtc_set_century(uint8_t c);
void rtc_init(void);
