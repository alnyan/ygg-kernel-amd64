#include "sys/char/input.h"
#include "sys/char/tty.h"
#include "sys/console.h"
#include "sys/debug.h"
#include "sys/ctype.h"

struct console *g_keyboard_console = NULL;

void input_scan(uint8_t c) {
    switch (c) {
    case INPUT_KEY_UP:
        console_type(g_keyboard_console, '\033');
        console_type(g_keyboard_console, '[');
        console_type(g_keyboard_console, 'A');
        break;
    case INPUT_KEY_DOWN:
        console_type(g_keyboard_console, '\033');
        console_type(g_keyboard_console, '[');
        console_type(g_keyboard_console, 'B');
        break;
    case INPUT_KEY_RIGHT:
        console_type(g_keyboard_console, '\033');
        console_type(g_keyboard_console, '[');
        console_type(g_keyboard_console, 'C');
        break;
    case INPUT_KEY_LEFT:
        console_type(g_keyboard_console, '\033');
        console_type(g_keyboard_console, '[');
        console_type(g_keyboard_console, 'D');
        break;
    case INPUT_KEY_HOME:
        console_type(g_keyboard_console, '\033');
        console_type(g_keyboard_console, '[');
        console_type(g_keyboard_console, '[');
        console_type(g_keyboard_console, 'H');
        break;
    case INPUT_KEY_END:
        console_type(g_keyboard_console, '\033');
        console_type(g_keyboard_console, '[');
        console_type(g_keyboard_console, '[');
        console_type(g_keyboard_console, 'F');
        break;
    default:
        kdebug("Ignoring unhandled key: %02x\n", c);
        break;
    }
}

void input_key(uint8_t key, uint8_t mods, const char *map0, const char *map1) {
    if (!g_keyboard_console) {
        return;
    }

    if (!(mods & INPUT_MOD_CONTROL)) {
        const char *map = (mods & INPUT_MOD_SHIFT) ? map1 : map0;
        uint8_t c = map[key];

        if (c && c <= 0x7F) {
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
        char c = map0[key];

        switch (c) {
        case 'd':
            console_type(g_keyboard_console, 4);    // EOT
            break;
        case 'c':
            console_type(g_keyboard_console, 3);    // ETX
            break;
        case 'w':
            console_type(g_keyboard_console, 23);   // ETB
            break;
        case 'z':
            console_type(g_keyboard_console, 26);   // SUB
            break;
        default:
            break;
        }
    }
}
