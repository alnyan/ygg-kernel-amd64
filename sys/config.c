#include "sys/config.h"
#include "sys/string.h"
#include "sys/ctype.h"
#include "sys/debug.h"

#define VALUE_BOOLEAN           0
#define VALUE_NUMBER            1
#define VALUE_STRING            2

// Cmdline parameter list
static const struct __kernel_cmdline_pair {
    const char *key;
    int cfg_index;
    int value_type;
} kernel_cmdline_pairs[] = {
    { "root",       CFG_ROOT,       VALUE_STRING },
    { "init",       CFG_INIT,       VALUE_STRING },
    { "rdinit",     CFG_RDINIT,     VALUE_STRING },
    { "console",    CFG_CONSOLE,    VALUE_STRING },
    { "debug",      CFG_DEBUG,      VALUE_NUMBER },
};

// Default config
uintptr_t kernel_config[__CFG_SIZE] = {
    0,
    [CFG_ROOT] = (uintptr_t) "ram0",
    [CFG_INIT] = (uintptr_t) "/init",
    [CFG_RDINIT] = (uintptr_t) "/init",
    [CFG_CONSOLE] = (uintptr_t) "tty0",
    [CFG_DEBUG] = DEBUG_ALL_SERIAL |
                  DEBUG_DISP(DEBUG_INFO) |
                  DEBUG_DISP(DEBUG_WARN) |
                  DEBUG_DISP(DEBUG_ERROR) |
                  DEBUG_DISP(DEBUG_FATAL),
};

static int parse_number(const char *s, intptr_t *res) {
    *res = 0;

    while (isdigit(*s)) {
        *res *= 10;
        *res += (*s) - '0';
        ++s;
    }

    if (*s) {
        // Not a digit
        return -1;
    }

    return 0;
}

static void kernel_set_opt(const struct __kernel_cmdline_pair *pair, const char *key, const char *value) {
    uintptr_t new_value;
    if ((pair->value_type != VALUE_BOOLEAN) && !value) {
        kwarn("Cmdline option requires a value: \"%s\"\n", key);
        return;
    }

    switch (pair->value_type) {
    case VALUE_BOOLEAN:
        if (value) {
            // Check if it's "false", "no", "off" or "true", "yes", "on"
            if (!strcmp(value, "false") || !strcmp(value, "no") || !strcmp(value, "off")) {
                new_value = 0;
            } else if (!strcmp(value, "true") || !strcmp(value, "yes") || !strcmp(value, "on")) {
                new_value = 1;
            } else {
                kwarn("Cmdline flag has invalid value: \"%s=%s\", expected boolean\n", key, value);
                return;
            }
        } else {
            // Just "opt" without value - assume it's true
            new_value = 1;
        }
        break;
    case VALUE_STRING:
        // TODO: use a separate array for strings
        new_value = (uintptr_t) value;
        kdebug("opt \"%s=%s\" (string)\n", key, value);
        break;
    case VALUE_NUMBER:
        if (parse_number(value, (intptr_t *) &new_value) != 0) {
            kerror("Cmdline option is not a number: \"%s=%s\"\n", key, value);
            return;
        }
        break;
    default:
        kerror("Invalid value type, kernel is fucked up\n");
        return;
    }

    kernel_config[pair->cfg_index] = new_value;
}

static void kernel_cmdline_opt(char *opt) {
    char *key = opt;
    char *value = NULL;

    if ((value = strchr(opt, '='))) {
        *value = 0;
        ++value;
    }

    for (size_t i = 0; i < sizeof(kernel_cmdline_pairs) / sizeof(kernel_cmdline_pairs[0]); ++i) {
        if (!strcmp(kernel_cmdline_pairs[i].key, key)) {
            kernel_set_opt(&kernel_cmdline_pairs[i], key, value);
            return;
        }
    }

    kwarn("Unknown cmdline option: \"%s\"\n", key);
}

// TODO: support stuff with spaces
void kernel_set_cmdline(char *cmdline) {
    char *p = cmdline;

    while (1) {
        while (isspace(*p)) {
            ++p;
        }
        char *e = strchr(p, ' ');
        if (e) {
            *e = 0;
        }

        kernel_cmdline_opt(p);

        if (!e) {
            break;
        }
        p = e + 1;
    }
}
