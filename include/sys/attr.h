/** vim: set ft=cpp.doxygen :
 * @file sys/attr.h
 * @brief Storage/function attributes for clarification or
 *        compiler hints
 */
#pragma once

#define __weak __attribute__((weak))
#define __init(name) \
    static void name(void); \
    const void *__init_##name __attribute__((section(".init"),used)) = name; \
    static void name(void)
