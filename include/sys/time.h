/** vim: set ft=cpp.doxygen :
 * @file sys/time.h
 * @brief Kernel timer values
 */
#pragma once
#include <stdint.h>

/**
 * @brief Desired resolution of systick timer in Hz
 */
#define SYSTICK_RES     1000

/**
 * @brief Kernel timer tick counter
 */
extern volatile uint64_t systick;
