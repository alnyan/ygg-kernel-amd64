#pragma once
#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>
#include "x86_op.h"

// Present bit
#define OP_PRESENT  (1 << 0)
// The operand size for the instruction defaults to 64 bits in long mode
#define OP_DEF64    (1 << 2)
// Instruction uses 8 bit operands
#define OP_DEF8     (1 << 3)
// Instruction uses R/M as an extension of the opcode
#define OP_RMEXT    (1 << 4)
// SSE
#define OP_SSE      (1 << 5)

#define MODE_REAL   0
#define MODE_LEGACY 1
#define MODE_LONG   2

#define ARG_SRC_MODRM       0
#define ARG_SRC_IMM0        1
#define ARG_SRC_OPREG       2
#define ARG_SRC_IMMREL      3
#define ARG_SRC_SREG        4
#define ARG_SRC_MOFFS       5
#define ARG_SRC_SSE         6
#define ARG_SRC_IMM1        7
#define ARG_SRC_OPXREG      8
#define ARG_SRC_MODRM_CR    9

// reg
#define RMOP_R              0
// [reg + disp]
#define RMOP_MRD            1
// [reg + reg + disp]
#define RMOP_MRRD           2
// [sib + disp]
#define RMOP_MSD            3
// [disp]
#define RMOP_MD             4
// reg2:
// 0 - Sreg
#define RMOP_XR             5
// [ip + disp]
#define RMOP_MCD            6
// imm
#define RMOP_I              7
// ip + disp
#define RMOP_CD             8

// 16-bit and higher
#define RM_AX               0
#define RM_CX               1
#define RM_DX               2
#define RM_BX               3
#define RM_SP               4
#define RM_BP               5
#define RM_SI               6
#define RM_DI               7

// 8-bit operations
#define RM8_AL              0
#define RM8_CL              1
#define RM8_DL              2
#define RM8_BL              3
#define RM8_AH              4
#define RM8_CH              5
#define RM8_DH              6
#define RM8_BH              7

// Segment registers
#define RMX_ES              0
#define RMX_CS              1
#define RMX_SS              2
#define RMX_DS              3
#define RMX_FS              4
#define RMX_GS              5

#define RMX_TYPE_SEG        0
#define RMX_TYPE_FPU        1
#define RMX_TYPE_MMX        2
#define RMX_TYPE_XMM        3
#define RMX_TYPE_YMM        4
#define RMX_TYPE_CR         5
#define RMX_TYPE_DEBUG      6

#define PREF_OP_SIZE    (1 << 0)
#define PREF_AD_SIZE    (1 << 1)
#define PREF_LOCK       (1 << 31)
#define PREF_REX        (1 << 30)
#define PREF_REP        (1 << 2)
#define PREF_SEG_DS     (1 << 3)
#define PREF_SEG_ES     (1 << 4)
#define PREF_SEG_FS     (1 << 5)
#define PREF_SEG_GS     (1 << 6)
#define PREF_SEG_SS     (1 << 7)
#define PREF_SEG_CS     (1 << 8)

#define REX_W           (1 << 3)
#define REX_R           (1 << 2)
#define REX_X           (1 << 1)
#define REX_B           (1 << 0)

struct rm_operand {
    uint8_t type;

    uint8_t mod;
    uint8_t reg;
    uint8_t reg2;
    int64_t imm;
    uint8_t seg;
};

struct op_arg_info {
    // Direction:
    // 0 - Source operand
    // 1 - Destination operand
    uint8_t dir;
    uint8_t source;
    union {
        // Size of the immediate operand in bits (maximum size)
        uint8_t imm_size;
        // 0 - the argument is R part of ModR/M
        // 1 - the argument is RM part of ModR/M
        uint8_t rm;
        // Register number
        uint8_t reg;
    };
    uint8_t ext;
};

struct op_info {
    enum opt opt;
    uint8_t flags;
    int argc;
    struct op_arg_info argv[8];
};

struct op {
    struct op_info *info;
    uint64_t addr;
    uintptr_t pos;
    size_t len;

    uint32_t prefices;
    uint8_t rex;
    size_t operand_size;
    size_t address_size;
    size_t argc;
    struct rm_operand argv[8];
};

void x86_set_mode(int mode);
ssize_t x86_read_operands(const uint8_t *data, struct op *op, const uint8_t *modrm);
ssize_t x86_match_op(const uint8_t *data, struct op_info **info, uint8_t *modrm);
ssize_t read_op(const uint8_t *data, uint64_t base_addr, struct op *op);
