#include "sys/amd64/hw/ints.h"
#include "sys/amd64/regs.h"
#include "sys/debug.h"

int amd64_err_num;

void amd64_exc_handler(amd64_ctx_regs_t *regs) {
    kfatal("Unhandled exception #%d\n", amd64_err_num);

    amd64_ctx_dump(DEBUG_FATAL, regs);

    while (1) {
        asm volatile ("cli; hlt");
    }
}
