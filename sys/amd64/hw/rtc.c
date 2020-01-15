#include "sys/amd64/hw/rtc.h"
#include "sys/amd64/hw/irq.h"
#include "sys/amd64/hw/io.h"
#include "sys/debug.h"
#include "sys/string.h"

#define RTC_CMOS_REG_PORT       0x70
#define RTC_CMOS_DATA_PORT      0x71

#define RTC_DISABLE_NMI         0x80
#define RTC_REGA_STATUS         0x0A
#define RTC_REGB_STATUS         0x0B
#define RTC_REGC_STATUS         0x0C

#define RTC_REG_SECOND          0x00
#define RTC_REG_MINUTE          0x02
#define RTC_REG_HOUR            0x04
#define RTC_REG_WEEKDAY         0x06
#define RTC_REG_DAY             0x07
#define RTC_REG_MONTH           0x08
#define RTC_REG_YEAR            0x09

#define RTC_REGB_BCD            0x04
#define RTC_REGB_AMPM           0x02

#define FROMBCD(bcd)            ((((bcd) / 16) * 10) + \
                                 ((bcd & 0xF)))

// [s]Please change this in Y3K[/s]I hope x86 dies by 3000
static uint8_t rtc_century = 20;

static inline void cmos_outb(uint8_t reg, uint8_t val) {
    outb(RTC_CMOS_REG_PORT, RTC_DISABLE_NMI | reg);
    for (size_t i = 0; i < 1000000; ++i);
    outb(RTC_CMOS_DATA_PORT, val);
}

static inline uint8_t cmos_inb(uint8_t reg) {
    outb(RTC_CMOS_REG_PORT, RTC_DISABLE_NMI | reg);
    for (size_t i = 0; i < 1000000; ++i);
    return inb(RTC_CMOS_DATA_PORT);
    // TODO; maybe reenable NMI?
}

static inline uint8_t cmos_read_time(uint8_t reg, uint8_t reg_b) {
    if (!(reg_b & RTC_REGB_BCD)) {
        return FROMBCD(cmos_inb(reg));
    } else {
        return cmos_inb(reg);
    }
}

void rtc_read(struct tm *time) {
    uint8_t reg_b = cmos_inb(RTC_REGB_STATUS);

    time->tm_sec = cmos_read_time(RTC_REG_SECOND, reg_b);
    time->tm_min = cmos_read_time(RTC_REG_MINUTE, reg_b);
    time->tm_hour = cmos_read_time(RTC_REG_HOUR, reg_b);
    //time->tm_wday = cmos_read_time(RTC_REG_WEEKDAY, reg_b);
    time->tm_mday = cmos_read_time(RTC_REG_DAY, reg_b);
    time->tm_mon = cmos_read_time(RTC_REG_MONTH, reg_b);
    time->tm_year = cmos_read_time(RTC_REG_YEAR, reg_b) + 100 * rtc_century;
}

static uint32_t rtc_irq(void *ctx) {
    uint8_t reg_c = cmos_inb(RTC_REGC_STATUS);
    if (reg_c == 0) {
        return IRQ_UNHANDLED;
    }
    // Do something here?
    return IRQ_HANDLED;
}

static void rtc_enable_irq(void) {
    uint8_t prev;

    // Gives IRQs approximately every 0.5s
    prev = cmos_inb(RTC_REGA_STATUS);
    prev &= 0xF0;
    prev |= 0xF;
    cmos_outb(RTC_REGA_STATUS, prev);

    // Enable IRQs
    prev = cmos_inb(RTC_REGB_STATUS);
    cmos_outb(RTC_REGB_STATUS, prev | 0x40);
}

void rtc_set_century(uint8_t century) {
    uint8_t reg_b = cmos_inb(RTC_REGB_STATUS);
    uint8_t century_val = cmos_read_time(century, reg_b);

    kdebug("Setting century: %d\n", century_val * 100);
    rtc_century = century_val;
}

void rtc_init(void) {
    cmos_inb(RTC_REGC_STATUS);
    irq_add_leg_handler(8, rtc_irq, NULL);
    rtc_enable_irq();
}
