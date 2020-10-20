#include "arch/amd64/disasm/x86.h"
#include "arch/amd64/disasm/util.h"

#include "sys/assert.h"
#include "sys/debug.h"
//#include <sys/param.h>
//#include <stdlib.h>
//#include <assert.h>
//#include <string.h>
//#include <stdio.h>

static uint32_t processor_mode = MODE_REAL;

static ssize_t x86_read_prefices(const uint8_t *data, uint32_t *prefices) {
    uint8_t byte;
    uint32_t p = 0;
    size_t pos = 0;

    while (1) {
        int is_prefix = 1;
        byte = data[pos];
        switch (byte) {
        // Grp1
        case 0xF0:
            p |= PREF_LOCK;
            break;
        case 0xF2:
            break;
        case 0xF3:
            p |= PREF_REP;
            break;

        // Grp2
        case 0x26:
            p |= PREF_SEG_ES;
            break;
        case 0x2E:
            p |= PREF_SEG_CS;
            break;
        case 0x3E:
            p |= PREF_SEG_DS;
            break;
        case 0x64:
            p |= PREF_SEG_FS;
            break;
        case 0x65:
            p |= PREF_SEG_GS;
            break;

        // Grp3
        case 0x66:
            p |= PREF_OP_SIZE;
            break;

        // Grp4
        case 0x67:
            p |= PREF_AD_SIZE;
            break;

        default:
            is_prefix = 0;
            break;
        }

        if (is_prefix) {
            ++pos;
        } else {
            break;
        }
    }

    *prefices = p;
    return pos;
}

static void x86_sizes(uint32_t prefices, uint8_t rex, uint32_t insn_flags, size_t *operand_size, size_t *address_size) {
    switch (processor_mode) {
    case MODE_REAL:
        if (prefices & PREF_AD_SIZE) {
            *address_size = 32;
        } else {
            *address_size = 16;
        }
        if (prefices & PREF_OP_SIZE) {
            *operand_size = 32;
        } else {
            *operand_size = 16;
        }
        break;
    case MODE_LONG:
        if (prefices & PREF_AD_SIZE) {
            *address_size = 32;
        } else {
            *address_size = 64;
        }
        if ((rex & REX_W) || (insn_flags & OP_DEF64)) {
            _assert(!(prefices & PREF_OP_SIZE));
            *operand_size = 64;
        } else {
            *operand_size = 32;

            if (prefices & PREF_OP_SIZE) {
                *operand_size = 16;
            }
        }
        break;
    default:
        panic("Unhandled CPU mode\n");
    }

    if (insn_flags & OP_DEF8) {
        *operand_size = 8;
    }
}

ssize_t read_op(const uint8_t *data, uint64_t base_addr, struct op *op) {
    // Read prefixes
    size_t pos = 0;
    uint8_t byte;
    ssize_t op_len;
    uint32_t prefices = 0;
    uint8_t rex = 0;
    int has_modrm = 0;
    uint8_t modrm;
    struct op_info *info = NULL;

    // Read prefices
    if ((op_len = x86_read_prefices(data, &prefices)) < 0) {
        return -1;
    }

    pos += op_len;
    byte = data[pos];
    op->rex = 0;

    if (processor_mode != MODE_REAL) {
        // Check if it's REX
        if ((byte & 0xF0) == 0x40) {
            rex = byte;
            prefices |= PREF_REX;

            ++pos;

            if ((processor_mode != MODE_LONG) && (rex & REX_W)) {
                kerror("REX.W in legacy modes\n");
                return -1;
            }
        }
    }

    if ((op_len = x86_match_op(data + pos, &info, &modrm)) > 0) {
        op->rex = rex;
        op->info = info;
        op->prefices = prefices;
        pos += op_len;

        x86_sizes(prefices, rex, info->flags, &op->operand_size, &op->address_size);

        // This means x86_match_op has already read ModR/M for us
        if (info->flags & OP_RMEXT) {
            has_modrm = 1;
        }

        if ((op_len = x86_read_operands(data + pos, op, has_modrm ? &modrm : NULL)) < 0) {
            return -1;
        }
        pos += op_len;

        return pos;
    } else {
        op->info = NULL;
        return pos + 1;
    }
}

void x86_set_mode(int mode) {
    processor_mode = mode;
}
