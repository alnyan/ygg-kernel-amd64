#include "arch/amd64/disasm/x86.h"

#include <sys/debug.h>
#include <sys/string.h>
#include <sys/assert.h>
#include <sys/panic.h>

// TODO: operand sizes
static const char *reg_name(int is_rex, uint8_t reg) {
    _assert(reg < 16);
    static const char *reg_names[] = {
        "rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi",
        "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",
        "eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi",
        "", "", "", "", "", "", "", "",
    };
    return reg_names[reg + 16 * !is_rex];
}

static const char *insn_name(enum opt opt) {
    switch (opt) {
    case OPT_ADD:
        return "add";
    case OPT_SUB:
        return "sub";
    case OPT_DEC:
        return "dec";
    case OPT_XOR:
        return "xor";
    case OPT_SHR:
        return "shr";

    case OPT_TEST:
        return "test";
    case OPT_CMP:
        return "cmp";

    case OPT_CALL:
        return "call";
    case OPT_JC:
        return "jc";
    case OPT_JZ:
        return "jz";
    case OPT_JMP:
        return "jmp";
    case OPT_RET:
        return "ret";

    case OPT_PUSH:
        return "push";
    case OPT_POP:
        return "pop";
    case OPT_MOV:
        return "mov";

    case OPT_SWAPGS:
        return "swapgs";
    case OPT_UD2:
        return "ud2";
    case OPT_IRET:
        return "iret";

    default:
        return "???";
    }
}

static void dump_op(uintptr_t off, size_t op_size, const struct op *op) {
    struct op_info *info = op->info;
    const struct rm_operand *arg;
    struct op_arg_info *ainf;

    debugf(DEBUG_DEFAULT, " %04x: ", off);
    if (!info) {
        debugs(DEBUG_DEFAULT, "BAD\n");
        return;
    }

    debugs(DEBUG_DEFAULT, insn_name(info->opt));

    // TODO: print operand sizes in certain cases
    //       (imm+mem, mem, imm)

    for (int i = 0; i < info->argc; ++i) {
        arg = &op->argv[i];
        if (i) {
            debugc(DEBUG_DEFAULT, ',');
        }
        debugc(DEBUG_DEFAULT, ' ');
        int rex = op->prefices & PREF_REX;

        switch (arg->type) {
        case RMOP_MSD:
            debugf(DEBUG_DEFAULT, "[%s+%s+%ld]", reg_name(rex, arg->reg), reg_name(rex, arg->reg2), arg->imm);
            break;
        case RMOP_MRD:
            debugf(DEBUG_DEFAULT, "[%s+%ld]", reg_name(rex, arg->reg), arg->imm);
            break;
        case RMOP_R:
            debugf(DEBUG_DEFAULT, "%s", reg_name(rex, arg->reg));
            break;
        case RMOP_XR:
            switch (arg->reg2) {
            case RMX_TYPE_CR:
                debugf(DEBUG_DEFAULT, "cr%u", arg->reg);
                break;
            default:
                debugf(DEBUG_DEFAULT, "??? (xr, reg2=%d)", arg->reg2);
                break;
            }
            break;
        case RMOP_MD:
            if (op->prefices & PREF_SEG_DS) {
                debugs(DEBUG_DEFAULT, "ds:");
            }
            if (op->prefices & PREF_SEG_ES) {
                debugs(DEBUG_DEFAULT, "es:");
            }
            if (op->prefices & PREF_SEG_FS) {
                debugs(DEBUG_DEFAULT, "es:");
            }
            if (op->prefices & PREF_SEG_GS) {
                debugs(DEBUG_DEFAULT, "gs:");
            }
            debugf(DEBUG_DEFAULT, "[%ld]", arg->imm);
            break;
        case RMOP_CD:
            debugf(DEBUG_DEFAULT, "<%%rip+%ld> (0x%08x)", arg->imm, off + op_size + arg->imm);
            break;
        case RMOP_I:
            debugf(DEBUG_DEFAULT, "#%ld", arg->imm);
            break;
        default:
            debugf(DEBUG_DEFAULT, "??? (%d)", arg->type);
            break;
        }
    }
    debugc(DEBUG_DEFAULT, '\n');
}

void dump_segment(const uint8_t *bytes, uintptr_t base, size_t size) {
    struct op op;
    ssize_t op_size;
    size_t off = 0;

    x86_set_mode(MODE_LONG);

    while (off < size) {
        op_size = read_op(bytes + off, base + off, &op);
        if (op_size <= 0) {
            panic("Failed to read op\n");
        }
        dump_op(base + off, op_size, &op);
        off += op_size;
    }
}
