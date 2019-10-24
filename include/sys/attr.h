/** vim: set ft=cpp.doxygen :
 * @file sys/attr.h
 * @brief Storage/function attributes for clarification or
 *        compiler hints
 */
#pragma once

#define __weak __attribute__((weak))
#define __init __attribute__((constructor))
