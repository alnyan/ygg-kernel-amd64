#include "sys/input.h"
#include "sys/debug.h"
#include "sys/ctype.h"
#include "sys/tty.h"

struct chrdev *g_keyboard_tty = 0;

void input_scan(uint8_t c) {
    switch (c) {
    case INPUT_KEY_UP:
        tty_data_write(g_keyboard_tty, '\033');
        tty_data_write(g_keyboard_tty, '[');
        tty_data_write(g_keyboard_tty, 'A');
        break;
    case INPUT_KEY_DOWN:
        tty_data_write(g_keyboard_tty, '\033');
        tty_data_write(g_keyboard_tty, '[');
        tty_data_write(g_keyboard_tty, 'B');
        break;
    case INPUT_KEY_RIGHT:
        tty_data_write(g_keyboard_tty, '\033');
        tty_data_write(g_keyboard_tty, '[');
        tty_data_write(g_keyboard_tty, 'C');
        break;
    case INPUT_KEY_LEFT:
        tty_data_write(g_keyboard_tty, '\033');
        tty_data_write(g_keyboard_tty, '[');
        tty_data_write(g_keyboard_tty, 'D');
        break;
    case INPUT_KEY_HOME:
        tty_data_write(g_keyboard_tty, '\033');
        tty_data_write(g_keyboard_tty, '[');
        tty_data_write(g_keyboard_tty, '[');
        tty_data_write(g_keyboard_tty, 'H');
        break;
    case INPUT_KEY_END:
        tty_data_write(g_keyboard_tty, '\033');
        tty_data_write(g_keyboard_tty, '[');
        tty_data_write(g_keyboard_tty, '[');
        tty_data_write(g_keyboard_tty, 'F');
        break;
    default:
        kdebug("Ignoring unhandled key: %02x\n", c);
        break;
    }
}

void input_key(uint8_t key, uint8_t mods, const char *map0, const char *map1) {
    if (!(mods & INPUT_MOD_CONTROL)) {
        const char *map = (mods & INPUT_MOD_SHIFT) ? map1 : map0;
        uint8_t c = map[key];

        if (c && c < 0x7F) {
            if (mods & INPUT_MOD_CAPS) {
                if (islower(c)) {
                    c = toupper(c);
                } else if (isupper(c)) {
                    c = tolower(c);
                }
            }

            // Normal key
            tty_data_write(g_keyboard_tty, c);
        } else {
            // Special keys
            input_scan(c);
        }
    } else {
        char c = map0[key];

        switch (c) {
        case 'c':
        case 'd':
            tty_control_write(g_keyboard_tty, c);
            break;
        }
    }
}
