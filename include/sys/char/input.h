#pragma once
#include "sys/types.h"

struct chrdev;
extern struct chrdev *g_keyboard_tty;

#define INPUT_MOD_CONTROL   (1 << 0)
#define INPUT_MOD_SHIFT     (1 << 1)
#define INPUT_MOD_CAPS      (1 << 2)
#define INPUT_MOD_ALT       (1 << 3)

#define INPUT_KEY_LEFT      0x80
#define INPUT_KEY_RIGHT     0x81
#define INPUT_KEY_UP        0x82
#define INPUT_KEY_DOWN      0x83
#define INPUT_KEY_HOME      0x84
#define INPUT_KEY_END       0x85

void input_key(uint8_t byte, uint8_t mods, const char *map0, const char *map1);
void input_scan(uint8_t key);
