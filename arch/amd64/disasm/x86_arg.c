#include "arch/amd64/disasm/x86.h"
#include "arch/amd64/disasm/util.h"
#include "sys/assert.h"
#include "sys/string.h"
#include "sys/debug.h"
//#include <sys/param.h>
//#include <_assert.h>
//#include <stdlib.h>
//#include <stdio.h>

struct operand_bytes {
    int has_imm0;
    int has_imm1;
    int has_sib;
    int has_disp;
    int has_modrm;

    uint8_t sib;
    int64_t disp;
    int64_t imm0;
    int64_t imm1;
    uint8_t modrm;
};

static void x86_read_imm(const uint8_t *data, size_t imm_size, int64_t *v) {
    switch (imm_size) {
    case 8:
        *v = qmovsx8(data[0]);
        break;
    case 16:
        *v = qmovsx16(
            ((uint16_t) data[1] << 8) |
            data[0]
        );
        break;
    case 32:
        *v = qmovsx32(
            ((uint32_t) data[3] << 24) |
            ((uint32_t) data[2] << 16) |
            ((uint32_t) data[1] << 8) |
            data[0]
        );
        break;
    default:
        panic("Immediate size not implemented: %zu\n", imm_size);
        break;
    }
}

static ssize_t x86_read_operand_bytes(const uint8_t *data, const struct op *op, struct operand_bytes *bytes) {
    // Operand bytes:
    // ... | ModR/M | SIB | Disp | Imm
    size_t pos = 0;
    const struct op_info *info = op->info;

    bytes->has_sib = 0;
    bytes->has_disp = 0;
    bytes->has_imm0 = 0;
    bytes->has_imm1 = 0;

    // 1. ModR/M
    if (!bytes->has_modrm) {
        for (int i = 0; i < info->argc; ++i) {
            if (info->argv[i].source == ARG_SRC_MODRM
             || info->argv[i].source == ARG_SRC_MODRM_CR
             || info->argv[i].source == ARG_SRC_SREG
             || info->argv[i].source == ARG_SRC_SSE) {
                bytes->has_modrm = 1;
                bytes->modrm = data[pos++];
                break;
            }
        }
    }
    // 2. SIB/Disp
    if (bytes->has_modrm) {
        uint8_t mod = bytes->modrm >> 6;
        uint8_t rm = bytes->modrm & 7;

        if (op->address_size > 16) {
            if (op->rex & REX_B) {
                rm |= 1 << 3;
            }

            if (mod != 3 && (rm == 4 || rm == 12)) {
                bytes->has_sib = 1;
                bytes->sib = data[pos];
                ++pos;
            }
            if (mod == 1) {
                bytes->has_disp = 1;
                bytes->disp = qmovsx8(data[pos]);
                ++pos;
            } else if (mod == 2) {
                bytes->has_disp = 1;
                bytes->disp = qmovsx32(
                    ((uint32_t) data[pos + 3] << 24) |
                    ((uint32_t) data[pos + 2] << 16) |
                    ((uint32_t) data[pos + 1] << 8) |
                    data[pos]
                );
                pos += 4;
            } else if (mod == 0 && (rm == 5 || rm == 13)) {
                bytes->has_disp = 1;
                bytes->disp = qmovsx32(
                    ((uint32_t) data[pos + 3] << 24) |
                    ((uint32_t) data[pos + 2] << 16) |
                    ((uint32_t) data[pos + 1] << 8) |
                    data[pos]
                );
                pos += 4;
            } else if (bytes->has_sib) {
                uint8_t base = bytes->sib & 0x7;

                if (mod == 0 && base == 5) {
                    bytes->has_disp = 1;
                    bytes->disp = qmovsx32(
                        ((uint32_t) data[pos + 3] << 24) |
                        ((uint32_t) data[pos + 2] << 16) |
                        ((uint32_t) data[pos + 1] << 8) |
                        data[pos]
                    );
                    pos += 4;
                } else {
                    // TODO
                    panic("???\n");
                }
            }
        } else {
            if (mod == 1) {
                bytes->has_disp = 1;
                bytes->disp = data[pos];
                ++pos;
            } else if (mod == 2) {
                bytes->has_disp = 1;
                bytes->disp = qmovsx16(((uint16_t) data[pos + 1] << 8) | data[pos]);
                pos += 2;
            } else if (mod == 0 && rm == 6) {
                bytes->has_disp = 1;
                bytes->disp = ((uint16_t) data[pos + 1] << 8) | data[pos];
                pos += 2;
            }
        }
    }
    // 3. Imm0
    for (int i = 0; i < info->argc; ++i) {
        if (info->argv[i].source == ARG_SRC_IMM0 ||
            info->argv[i].source == ARG_SRC_IMMREL ||
            info->argv[i].source == ARG_SRC_MOFFS) {
            size_t imm_size = (info->argv[i].source == ARG_SRC_IMM0) ? op->operand_size : op->address_size;
            imm_size = MIN(imm_size, info->argv[i].imm_size);
            bytes->has_imm0 = 1;
            x86_read_imm(data + pos, imm_size, &bytes->imm0);
            pos += imm_size >> 3;
            break;
        }
    }
    // 4. Imm1
    for (int i = 0; i < info->argc; ++i) {
        if (info->argv[i].source == ARG_SRC_IMM1) {
            size_t imm_size = op->operand_size;
            imm_size = MIN(imm_size, info->argv[i].imm_size);
            bytes->has_imm1 = 1;
            x86_read_imm(data + pos, imm_size, &bytes->imm1);
            pos += imm_size >> 3;
            break;
        }
    }

    return pos;
}

ssize_t x86_read_operands(const uint8_t *data, struct op *op, const uint8_t *modrm) {
    struct op_arg_info *info;
    struct operand_bytes bytes;
    uint8_t mod, rm, r;
    bytes.has_modrm = !!modrm;
    if (modrm) {
        bytes.modrm = *modrm;
    }

    ssize_t res = x86_read_operand_bytes(data, op, &bytes);

    // Process operand bytes
    for (int i = 0; i < op->info->argc; ++i) {
        info = &op->info->argv[i];

        switch (info->source) {
        case ARG_SRC_MODRM:
            _assert(bytes.has_modrm);
            mod = bytes.modrm >> 6;
            rm = bytes.modrm & 7;
            r = (bytes.modrm >> 3) & 7;

            if (op->rex & REX_B) {
                rm |= 1 << 3;
            }
            if (op->rex & REX_R) {
                r |= 1 << 3;
            }

            if (info->rm == 0) {
                // Just reg
                op->argv[i].type = RMOP_R;
                op->argv[i].reg = r;
                break;
            }

            op->argv[i].imm = 0;
            if (op->address_size > 16) {
                if (mod == 3) {
                    // Just reg
                    op->argv[i].type = RMOP_R;
                    op->argv[i].reg = rm;
                } else {
                    if (rm == 4 || rm == 12) {
                        uint8_t base;
                        uint8_t index;
                        // [SIB + disp0/8/32]
                        _assert(bytes.has_sib);
                        op->argv[i].type = RMOP_MSD;

                        base = bytes.sib & 0x7;
                        index = (bytes.sib >> 3) & 7;

                        if (mod == 0) {
                            if (base == 5) {
                                _assert(bytes.has_disp);
                                op->argv[i].imm = bytes.disp;
                            } else {
                                op->argv[i].imm = 0;
                            }

                            switch (index) {
                            case 0:
                            case 1:
                            case 2:
                            case 3:
                                op->argv[i].type = RMOP_MSD;
                                op->argv[i].reg = base | ((op->rex & REX_B) << 3);
                                op->argv[i].reg2 = index | (!!(op->rex & REX_X) << 3);
                                break;
                            case 4:
                                _assert(bytes.has_disp);
                                op->argv[i].type = RMOP_MD;
                                break;
                            default:
                                panic("TODO: handle sib, mod=0, index=%d\n", index);
                            }
                        } else if (mod == 1) {
                            if (index < 4) {
                                panic("TODO: handle sib, mod=%d, index=%d\n", mod, index);
                            } else if (index == 4) {
                                _assert(bytes.has_disp);
                                op->argv[i].type = RMOP_MRD;
                                op->argv[i].imm = bytes.disp;
                                op->argv[i].reg = base | ((op->rex & REX_B) << 3);
                                break;
                            } else {
                                panic("TODO: handle sib, mod=%d, index=%d\n", mod, index);
                            }
                        } else {
                            // TODO
                            panic("TODO: handle sib, mod=%d, index=%d\n", mod, index);
                            //_assert(bytes.has_disp);
                            //op->argv[i].imm = bytes.disp;
                        }

                        op->argv[i].reg = index;
                        if (op->rex & REX_X) {
                            op->argv[i].reg |= 1 << 3;
                        }

                        op->argv[i].reg2 = base;
                        if (op->rex & REX_B) {
                            op->argv[i].reg2 |= 1 << 3;
                        }
                    } else if (mod == 0 && (rm == 5 || rm == 13)) {
                        // [RIP + disp32]
                        _assert(bytes.has_disp);
                        op->argv[i].type = RMOP_MCD;
                        op->argv[i].imm = bytes.disp;
                    } else {
                        op->argv[i].type = RMOP_MRD;
                        if (mod == 0) {
                            // [REG]
                            op->argv[i].imm = 0;
                        } else {
                            // [REG + disp]
                            _assert(bytes.has_disp);
                            op->argv[i].imm = bytes.disp;
                        }

                        op->argv[i].reg = rm;
                    }
                }
            } else {
                if (mod == 3) {
                    op->argv[i].type = RMOP_R;
                    op->argv[i].reg = rm;
                } else {
                    if (mod == 0 && rm == 6) {
                        // [disp]
                        op->argv[i].type = RMOP_MD;
                    } else if (rm < 4) {
                        // [REG + REG2 + disp]
                        op->argv[i].type = RMOP_MRRD;
                    } else {
                        // [REG + disp]
                        op->argv[i].type = RMOP_MRD;
                    }

                    if (mod > 0 || rm == 6) {
                        _assert(bytes.has_disp);
                        op->argv[i].imm = bytes.disp;
                    } else {
                        op->argv[i].imm = 0;
                    }

                    switch (rm) {
                    case 0:
                        // [BX + SI + disp0/8/16]
                        op->argv[i].reg = RM_BX;
                        op->argv[i].reg2 = RM_SI;
                        break;
                    case 1:
                        // [BX + DI + disp0/8/16]
                        op->argv[i].reg = RM_BX;
                        op->argv[i].reg2 = RM_DI;
                        break;
                    case 2:
                        // [BP + SI + disp0/8/16]
                        op->argv[i].reg = RM_BP;
                        op->argv[i].reg2 = RM_SI;
                        break;
                    case 3:
                        // [BP + DI + disp0/8/16]
                        op->argv[i].reg = RM_BP;
                        op->argv[i].reg2 = RM_DI;
                        break;
                    case 4:
                        // [SI + disp0/8/16]
                        op->argv[i].reg = RM_SI;
                        break;
                    case 5:
                        // [DI + disp0/8/16]
                        op->argv[i].reg = RM_DI;
                        break;
                    case 6:
                        // [BP + disp] or [disp]
                        if (mod) {
                            op->argv[i].reg = RM_BP;
                        }
                        break;
                    case 7:
                        // [BX + disp0/8/16]
                        op->argv[i].reg = RM_BX;
                        break;
                    default:
                        panic("Unsupported addressing mode: Real, MOD = %u, RM = %u\n", mod, rm);
                    }
                }
            }
            break;
        case ARG_SRC_OPXREG:
            op->argv[i].type = RMOP_XR;
            op->argv[i].reg = info->reg;
            op->argv[i].reg2 = info->ext;
            break;
        case ARG_SRC_SSE:
            op->argv[i].type = RMOP_XR;
            op->argv[i].reg2 = RMX_TYPE_XMM;
            _assert(bytes.has_modrm);
            if (info->rm) {
                op->argv[i].reg = bytes.modrm & 7;
            } else {
                op->argv[i].reg = (bytes.modrm >> 3) & 7;
            }
            break;
        case ARG_SRC_OPREG:
            op->argv[i].type = RMOP_R;
            op->argv[i].reg = info->reg;
            break;
        case ARG_SRC_IMM0:
            _assert(bytes.has_imm0);
            op->argv[i].type = RMOP_I;
            op->argv[i].imm = bytes.imm0;
            break;
        case ARG_SRC_IMM1:
            _assert(bytes.has_imm1);
            op->argv[i].type = RMOP_I;
            op->argv[i].imm = bytes.imm1;
            break;
        case ARG_SRC_IMMREL:
            _assert(bytes.has_imm0);
            op->argv[i].type = RMOP_CD;
            op->argv[i].imm = bytes.imm0;
            break;
        case ARG_SRC_SREG:
            op->argv[i].type = RMOP_XR;
            op->argv[i].reg = (bytes.modrm >> 3) & 7;
            if (op->rex & REX_R) {
                op->argv[i].reg |= 1 << 3;
            }
            op->argv[i].reg2 = 0;
            break;
        case ARG_SRC_MOFFS:
            _assert(bytes.has_imm0);
            op->argv[i].type = RMOP_MD;
            op->argv[i].imm = bytes.imm0;
            break;
        case ARG_SRC_MODRM_CR:
            _assert(bytes.has_modrm);
            op->argv[i].type = RMOP_XR;
            op->argv[i].reg2 = RMX_TYPE_CR;
            if (info->rm) {
                op->argv[i].reg = bytes.modrm & 7;
            } else {
                op->argv[i].reg = (bytes.modrm >> 3) & 7;
            }
            break;
        default:
            panic("Unhandled operand: %d\n", info->source);
        }
    }

    return res;
}
