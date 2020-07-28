/** vim: set ft=cpp.doxygen :
 * @file sys/attr.h
 * @brief Storage/function attributes for clarification or
 *        compiler hints
 */
#pragma once

#define __weak __attribute__((weak))

// A bit of assembler "sorcery" here:
// By default (with only section name specified) gcc will spit out:
// .section .init,"aw",@progbits
// But .init section is "ax", so the result will be
// .section .init,"ax",@progbits //"aw",@progbits
// '//' will just comment out the stuff GCC produced
#define __init(name) \
    static void name(void); \
    const void *__init_##name __attribute__((section(".init,\"ax\",@progbits //"),used)) = name; \
    static void name(void)
