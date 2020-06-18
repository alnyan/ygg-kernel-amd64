#include "sys/char/input.h"
#include "sys/char/tty.h"
#include "sys/console.h"
#include "sys/debug.h"
#include "sys/ctype.h"

struct console *g_keyboard_console = NULL;

void input_scan(uint8_t c) {
    //switch (c) {
    //case INPUT_KEY_UP:
    //    tty_data_write(g_keyboard_tty, '\033');
    //    tty_data_write(g_keyboard_tty, '[');
    //    tty_data_write(g_keyboard_tty, 'A');
    //    break;
    //case INPUT_KEY_DOWN:
    //    tty_data_write(g_keyboard_tty, '\033');
    //    tty_data_write(g_keyboard_tty, '[');
    //    tty_data_write(g_keyboard_tty, 'B');
    //    break;
    //case INPUT_KEY_RIGHT:
    //    tty_data_write(g_keyboard_tty, '\033');
    //    tty_data_write(g_keyboard_tty, '[');
    //    tty_data_write(g_keyboard_tty, 'C');
    //    break;
    //case INPUT_KEY_LEFT:
    //    tty_data_write(g_keyboard_tty, '\033');
    //    tty_data_write(g_keyboard_tty, '[');
    //    tty_data_write(g_keyboard_tty, 'D');
    //    break;
    //case INPUT_KEY_HOME:
    //    tty_data_write(g_keyboard_tty, '\033');
    //    tty_data_write(g_keyboard_tty, '[');
    //    tty_data_write(g_keyboard_tty, '[');
    //    tty_data_write(g_keyboard_tty, 'H');
    //    break;
    //case INPUT_KEY_END:
    //    tty_data_write(g_keyboard_tty, '\033');
    //    tty_data_write(g_keyboard_tty, '[');
    //    tty_data_write(g_keyboard_tty, '[');
    //    tty_data_write(g_keyboard_tty, 'F');
    //    break;
    //default:
    //    kdebug("Ignoring unhandled key: %02x\n", c);
    //    break;
    //}
}

void input_key(uint8_t key, uint8_t mods, const char *map0, const char *map1) {
    if (!g_keyboard_console) {
        return;
    }

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
            console_type(g_keyboard_console, c);
        } else {
            // Special keys
            input_scan(c);
        }
    } else {
        //char c = map0[key];

        //switch (c) {
        //case 'c':
        //case '.':
        //case 'd':
        //    tty_control_write(g_keyboard_tty, c);
        //    break;
        //}
    }
}
