#include "user/termios.h"
#include "user/signum.h"
#include "sys/char/input.h"
#include "user/errno.h"
#include "sys/char/line.h"
#include "sys/char/ring.h"
#include "sys/char/tty.h"
#include "sys/char/chr.h"
#include "sys/console.h"
#include "sys/string.h"
#include "sys/thread.h"
#include "sys/assert.h"
#include "sys/ctype.h"
#include "sys/debug.h"
#include "sys/sched.h"
#include "sys/panic.h"
#include "sys/heap.h"
#include "sys/dev.h"
#include "sys/mm.h"

#define TTY_BUF_SIZE        128

static ssize_t tty_write(struct chrdev *tty, const void *buf, size_t pos, size_t lim);
static int tty_ioctl(struct chrdev *tty, unsigned int cmd, void *arg);

static const struct termios default_termios = TERMIOS_DEFAULT;
static char last_tty_n = '0';
static char last_ttys_n = '0';

void tty_data_write(struct chrdev *tty, char c) {
    _assert(tty && tty->type == CHRDEV_TTY);

    if (c == '\r' && (tty->tc.c_iflag & ICRNL)) {
        c = '\n';
    }

    if ((tty->tc.c_lflag & ICANON) && (tty->tc.c_cc[VEOF] == c)) {
        // EOF
        ring_signal(&tty->buffer, RING_SIGNAL_EOF);
        return;
    }

    if (tty->tc.c_cc[VSUSP] == c) {
        if (tty->tc.c_lflag & ISIG) {
            process_signal_pgid(((struct tty_data *) tty->dev_data)->fg_pgid, SIGSTOP);
            return;
        } else {
            tty_putc(tty, '^');
            tty_putc(tty, 'Z');
        }
    }
    if (tty->tc.c_cc[VINTR] == c) {
        if (tty->tc.c_lflag & ISIG) {
            process_signal_pgid(((struct tty_data *) tty->dev_data)->fg_pgid, SIGINT);
            return;
        } else {
            // Just display ^C
            tty_putc(tty, '^');
            tty_putc(tty, 'C');
        }
    }

    ring_putc(NULL, &tty->buffer, c, 0);

    switch (c) {
    case '\n':
        if (tty->tc.c_lflag & ECHONL) {
            tty_putc(tty, c);
        }
        if (tty->tc.c_lflag & ICANON) {
            // Trigger line flush
            ring_signal(&tty->buffer, RING_SIGNAL_RET);
        }
        break;
    case 0x17:
    case 0x7F:
        ring_signal(&tty->buffer, 0);
        break;
    case 0x0C:
        if (tty->tc.c_lflag & ECHO) {
            tty_putc(tty, '^');
            tty_putc(tty, 'L');
        }
        break;
    case '\033':
        if ((tty->tc.c_lflag & ECHO) && (tty->tc.c_lflag & ICANON)) {
            tty_puts(tty, "^[");
        }
        break;
    default:
        // This ignores ICANON
        if (tty->tc.c_lflag & ECHO) {
            tty_putc(tty, c);
        }
    }
}

void tty_puts(struct chrdev *tty, const char *s) {
    for (; *s; ++s) {
        tty_putc(tty, *s);
    }
}

int tty_create(struct console *master) {
    struct chrdev *tty = kmalloc(sizeof(struct chrdev));
    _assert(tty);

    struct tty_data *data = kmalloc(sizeof(struct tty_data));
    _assert(data);

    data->fg_pgid = 1;
    data->master = NULL;
    data->buffer = NULL;
    list_head_init(&data->list);

    tty->type = CHRDEV_TTY;
    memcpy(&tty->tc, &default_termios, sizeof(struct termios));
    tty->write = tty_write;
    tty->ioctl = tty_ioctl;
    tty->read = line_read;
    tty->dev_data = data;
    data->has_console = 1;

    ring_init(&tty->buffer, TTY_BUF_SIZE);

    console_attach(master, tty);

    char name[5] = "ttyX";
    name[3] = last_tty_n++;
    dev_add(DEV_CLASS_CHAR, DEV_CHAR_TTY, tty, name);

    return 0;
}

int tty_create_serial(struct tty_serial *ser) {
    struct chrdev *tty = kmalloc(sizeof(struct chrdev));
    _assert(tty);

    struct tty_data *data = kmalloc(sizeof(struct tty_data));
    _assert(data);

    data->fg_pgid = 1;
    list_head_init(&data->list);

    tty->type = CHRDEV_TTY;
    memcpy(&tty->tc, &default_termios, sizeof(struct termios));
    tty->tc.c_iflag |= ICRNL;
    tty->write = tty_write;
    tty->ioctl = tty_ioctl;
    tty->read = line_read;
    tty->dev_data = data;
    data->has_console = 0;
    data->serial = ser;
    data->serial->tty = tty;

    ring_init(&tty->buffer, TTY_BUF_SIZE);

    char name[6] = "ttySX";
    name[4] = last_ttys_n++;
    dev_add(DEV_CLASS_CHAR, DEV_CHAR_TTY, tty, name);

    return 0;
}

void tty_putc(struct chrdev *tty, int c) {
    struct tty_data *data = tty->dev_data;
    _assert(data);

    if (data->has_console) {
        _assert(data->master);
        console_putc(data->master, tty, c);
    } else {
        _assert(data->serial);
        data->serial->putc(data->serial->ctx, c);
    }
}

static struct vnode *tty_link_getter(struct thread *ctx, struct vnode *link, char *buf, size_t len) {
    struct process *proc;

    if (!(proc = ctx->proc)) {
        return NULL;
    }

    if (buf) {
        if (proc->ctty) {
            if (strlen(proc->ctty->name) < len) {
                strcpy(buf, proc->ctty->name);
            } else {
                return NULL;
            }
        } else {
            buf[0] = 0;
        }
    }

    return proc->ctty;
}

void tty_init(void) {
    dev_add_live_link("tty", tty_link_getter);
}

static ssize_t tty_write(struct chrdev *tty, const void *buf, size_t pos, size_t lim) {
    struct tty_data *data = tty->dev_data;
    _assert(data);

    for (size_t i = 0; i < lim; ++i) {
        if (data->has_console) {
            _assert(data->master);
            console_putc(data->master, tty, ((const unsigned char *) buf)[i]);
        } else {
            _assert(data->serial);
            data->serial->putc(data->serial->ctx, ((const unsigned char *) buf)[i]);
        }
    }

    return lim;
}

static int tty_ioctl(struct chrdev *tty, unsigned int cmd, void *arg) {
    struct tty_data *data = tty->dev_data;
    _assert(data);

    switch (cmd) {
    case TIOCGWINSZ:
        {
            _assert(arg);
            struct winsize *winsz = arg;
            if (data->has_console) {
                _assert(data->master);

                winsz->ws_col = data->master->width_chars;
                winsz->ws_row = data->master->height_chars;
            } else {
                winsz->ws_col = 80;
                winsz->ws_row = 25;
            }
        }
        return 0;
    case TCGETS:
        memcpy(arg, &tty->tc, sizeof(struct termios));
        return 0;
    case TCSETS:
        memcpy(&tty->tc, arg, sizeof(struct termios));
        if (tty->tc.c_lflag & ICANON) {
            tty->buffer.flags &= ~RING_RAW;
        } else {
            tty->buffer.flags |= RING_RAW;
        }
        return 0;
    case TIOCSPGRP:
        // Clear interrupts and stuff
        tty->buffer.flags = 0;
        data->fg_pgid = *(pid_t *) arg;
        return 0;
    }
    return -EINVAL;
}
