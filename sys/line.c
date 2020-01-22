#include "sys/ring.h"
#include "sys/debug.h"
#include "sys/assert.h"
#include "sys/chr.h"
#include "sys/tty.h"
#include "sys/amd64/cpu.h"
#include "sys/line.h"

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

    if (!(chr->tc.c_iflag & ICANON)) {
        // Just spit out data as soon as it's available
        return simple_line_read(chr, buf, pos, lim);
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

        // TODO: ICANON
        if (c == '\033') {
            if (rem >= 2) {
                *wr++ = '^';
                *wr++ = '[';
            }
            rem -= 2;
            rd += 2;
            continue;
        }

        if (c == '\b' && (chr->tc.c_lflag & ECHOE)) {
            if (rd) {
                tty_puts(chr, "\033[D \033[D");

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
    while (ring_readable(r)) {
        if (ring_getc(thr, r, wr, 1) < 0) {
            break;
        }
        --rem;
        ++wr;
        ++rd;
    }

    if (rd == 0) {
        return -1;
    }

    return rd;
}
