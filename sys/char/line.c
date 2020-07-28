#include "arch/amd64/cpu.h"
#include "sys/char/ring.h"
#include "sys/char/line.h"
#include "sys/char/chr.h"
#include "sys/char/tty.h"
#include "sys/assert.h"
#include "sys/ctype.h"
#include "sys/debug.h"

ssize_t line_read(struct chrdev *chr, void *buf, size_t pos, size_t lim) {
    struct thread *thr = get_cpu()->thread;
    _assert(chr->type == CHRDEV_TTY);
    struct ring *r = &chr->buffer;

    size_t rd = 0;
    size_t rem = lim;
    char *wr = buf;
    char c;

    // Only needed for stuff like select(), which is called before read()
    chr->buffer.flags &= ~RING_SIGNAL_RET;

    if (r->flags & RING_SIGNAL_EOF) {
        return 0;
    }

    // NO ICANON YET
    // TODO: pass this through for non-canonical mode
    while (rem) {
        if (ring_getc(thr, r, &c, 0) < 0) {
            // TODO: make this optional via termios
            if (r->flags & RING_SIGNAL_EOF) {
                // ^D
                return rd;
            } else {
                // ^C
                // Reset the state
                rem = lim;
                wr = buf;
                rd = 0;
                continue;
            }
        }

        if (c == 0x17 && chr->tc.c_lflag & ICANON) {
            // Erase until the beginning of the word
            // TODO: erase preceding space groups
            while (rd) {
                if (rd > 1 && isspace(*(wr - 1))) {
                    break;
                }
                if (chr->tc.c_lflag & ECHOE) {
                    tty_puts(chr, "\033[D \033[D");
                }

                --wr;
                ++rem;
                --rd;
            }

            continue;
        }
        if (c == 0x7F && chr->tc.c_lflag & ICANON) {
            if (rd) {
                if (chr->tc.c_lflag & ECHOE) {
                    tty_puts(chr, "\033[D \033[D");
                }

                --wr;
                ++rem;
                --rd;
            }

            continue;
        }

        *wr++ = c;
        ++rd;
        --rem;

        if (c == '\n') {
            break;
        }
    }

    chr->buffer.flags &= ~RING_SIGNAL_RET;
    return rd;
}

ssize_t simple_line_read(struct chrdev *chr, void *buf, size_t pos, size_t lim) {
    struct thread *thr = get_cpu()->thread;
    struct ring *r = &chr->buffer;

    size_t rd = 0;
    size_t rem = lim;
    char *wr = buf;

    // Non-TTYs don't support breaks/signals
    do {
        if (!ring_readable(r)) {
            asm volatile ("sti; hlt; cli");
        } else {
            break;
        }
    } while (1);

    // TODO: properly handle interrupts
    while (rem && ring_readable(r)) {
        if (ring_getc(thr, r, wr, 1) < 0) {
            break;
        }
        --rem;
        ++wr;
        ++rd;
    }

    return rd;
}
