#include "arch/amd64/disasm/x86.h"
#include "sys/assert.h"
#include "sys/debug.h"
//#include <assert.h>
//#include <stdlib.h>
//#include <stdio.h>

//#define PRINT_OPCODES

// insn REG/MEM, REG, IMM
#define OP_ARG3_RRMI(sz) \
    { \
        { 1, ARG_SRC_MODRM, .rm = 1 }, \
        { 0, ARG_SRC_MODRM, .rm = 0 }, \
        { 0, ARG_SRC_IMM0, .imm_size = sz } \
    }

// insn REG/MEM, REG
#define OP_ARG2_RRM_DEF \
    { \
        { 1, ARG_SRC_MODRM, .rm = 1 }, \
        { 0, ARG_SRC_MODRM, .rm = 0 } \
    }

// insn REG/MEM, REG
#define OP_ARG2_RMC_DEF(b) \
    { \
        { 1, ARG_SRC_MODRM, .rm = 1 }, \
        { 0, ARG_SRC_OPREG, .reg = b } \
    }

// insn REG/MEM, IMM
#define OP_ARG2_RMI_DEF(sz) \
    { \
        { 0, ARG_SRC_MODRM, .rm = 1 }, \
        { 1, ARG_SRC_IMM0, .imm_size = sz } \
    }

// insn REG, IMM
#define OP_ARG2_CI_DEF(sz, b) \
    { \
        { 1, ARG_SRC_OPREG, .reg = b }, \
        { 0, ARG_SRC_IMM0, .imm_size = sz } \
    }

// insn MOFFS, REG
#define OP_ARG2_TC_DEF(sz, b) \
    { \
        { 1, ARG_SRC_MOFFS, .imm_size = sz }, \
        { 0, ARG_SRC_OPREG, .reg = b }, \
    }

// insn REG, MOFFS
#define OP_ARG2_CT_DEF(sz, b) \
    { \
        { 1, ARG_SRC_OPREG, .reg = b }, \
        { 0, ARG_SRC_MOFFS, .imm_size = sz }, \
    }

// insn REG, REG/MEM
#define OP_ARG2_RMR_DEF \
    { \
        { 1, ARG_SRC_MODRM, .rm = 0 }, \
        { 0, ARG_SRC_MODRM, .rm = 1 } \
    }

// insn SREG, REG/MEM
#define OP_ARG2_RMS_DEF \
    { \
        { 1, ARG_SRC_SREG }, \
        { 0, ARG_SRC_MODRM, .rm = 1 } \
    }

// insn IMM, IMM
#define OP_ARG2_II_DEF(sz0, sz1) \
    { \
        { 0, ARG_SRC_IMM0, .imm_size = sz0 }, \
        { 0, ARG_SRC_IMM1, .imm_size = sz1 } \
    }

// insn REG
#define OP_ARG1_C_DEF(b) \
    { \
        { 0, ARG_SRC_OPREG, .reg = b } \
    }

// insn REG/MEM
#define OP_ARG1_RM_DEF \
    { \
        { 0, ARG_SRC_MODRM, .rm = 1 } \
    }

// insn IMM
#define OP_ARG1_I_DEF(sz) \
    { \
        { 0, ARG_SRC_IMM0, .imm_size = sz } \
    }

// insn xREG
#define OP_ARG1_EXT_C_DEF(t, r) \
    { \
        { 0, ARG_SRC_OPXREG, .ext = t, .reg = r } \
    }

// insn REL
#define OP_ARG1_REL_DEF(sz) \
    { \
        { 0, ARG_SRC_IMMREL, .imm_size = sz } \
    }


// SSE
#define OP_ARG2_SSE_RMR_DEF \
    { \
        { 1, ARG_SRC_SSE, .rm = 0 }, \
        { 0, ARG_SRC_SSE, .rm = 1 } \
    }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-braces"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
static struct op_info ops_1[0x800] = {
//    // 8-bit instructions
//    // 00 /r                    ADD r/m8, r8
    [0x000] = { OPT_ADD, OP_PRESENT | OP_DEF8, 2, OP_ARG2_RRM_DEF },
//    // 02 /r                    ADD r8, r/m8
//    [0x002] = { "add", OP_PRESENT | OP_DEF8, 2, OP_ARG2_RMR_DEF },
//    // 04 /r                    ADD AL, imm8
//    [0x004] = { "add", OP_PRESENT | OP_DEF8, 2, OP_ARG2_CI_DEF(8, 0) },
//    // 08 /r                    OR r/m8, r8
//    [0x008] = { "or", OP_PRESENT | OP_DEF8, 2, OP_ARG2_RRM_DEF },
//    // 0C ib                    OR AL, imm8
//    [0x00C] = { "or", OP_PRESENT | OP_DEF8, 2, OP_ARG2_CI_DEF(8, 0) },
//    // 0A /r                    OR r8, r/m8
//    [0x00A] = { "or", OP_PRESENT | OP_DEF8, 2, OP_ARG2_RMR_DEF },
//    // 10 /r                    ADC r/m8, r8
//    [0x010] = { "adc", OP_PRESENT | OP_DEF8, 2, OP_ARG2_RRM_DEF },
//    // 18 /r                    SBB r/m8, r8
//    [0x018] = { "sbb", OP_PRESENT | OP_DEF8, 2, OP_ARG2_RRM_DEF },
//    // 1A /r                    SBB r8, r/m8
//    [0x01A] = { "sbb", OP_PRESENT | OP_DEF8, 2, OP_ARG2_RMR_DEF },
//    // 1C /r                    SBB AL, imm8
//    [0x01C] = { "sbb", OP_PRESENT | OP_DEF8, 2, OP_ARG2_CI_DEF(8, 0) },
//    // 20 /r                    AND r/m8, r8
//    [0x020] = { "and", OP_PRESENT | OP_DEF8, 2, OP_ARG2_RRM_DEF },
//    // 2C ib                    SUB AL, imm8
//    [0x02C] = { "sub", OP_PRESENT | OP_DEF8, 2, OP_ARG2_CI_DEF(8, 0) },
//    // 28 /r                    SUB r/m8, r8
//    [0x028] = { "sub", OP_PRESENT | OP_DEF8, 2, OP_ARG2_RRM_DEF },
//    // 30 /r                    XOR r/m8, r8
//    [0x030] = { "xor", OP_PRESENT | OP_DEF8, 2, OP_ARG2_RRM_DEF },
//    // 32 /r                    XOR r8, r/m8
//    [0x032] = { "xor", OP_PRESENT | OP_DEF8, 2, OP_ARG2_RMR_DEF },
//    // 34 ib                    XOR AL, imm8
//    [0x034] = { "xor", OP_PRESENT | OP_DEF8, 2, OP_ARG2_CI_DEF(8, 0) },
//    // 38 /r                    CMP r/m8, r8
//    [0x038] = { "cmp", OP_PRESENT | OP_DEF8, 2, OP_ARG2_RRM_DEF },
//    // 3A /r                    CMP r8, r/m8
//    [0x03A] = { "cmp", OP_PRESENT | OP_DEF8, 2, OP_ARG2_RMR_DEF },
//    // 3C ib                    CMP AL, imm8
//    [0x03C] = { "cmp", OP_PRESENT | OP_DEF8, 2, OP_ARG2_CI_DEF(8, 0) },
//
//    // 80 /0 ib                 ADD r/m8, imm8
//    [0x080] = { "add", OP_PRESENT | OP_DEF8 | OP_RMEXT, 2, OP_ARG2_RMI_DEF(8) },
//    // 80 /1 ib                 OR r/m8, imm8
//    [0x180] = { "or", OP_PRESENT | OP_DEF8 | OP_RMEXT, 2, OP_ARG2_RMI_DEF(8) },
//    // 80 /4 ib                 AND r/m8, imm8
//    [0x480] = { "and", OP_PRESENT | OP_DEF8 | OP_RMEXT, 2, OP_ARG2_RMI_DEF(8) },
//    // 80 /7 ib                 CMP r/m8, imm8
//    [0x780] = { "cmp", OP_PRESENT | OP_DEF8 | OP_RMEXT, 2, OP_ARG2_RMI_DEF(8) },
//
//    // F6 /0 ib                 TEST r/m8, imm8
//    [0x0F6] = { "test", OP_PRESENT | OP_DEF8 | OP_RMEXT, 2, OP_ARG2_RMI_DEF(8) },
//
//    // 86 /r                    XCHG r8, r/m8
//    [0x086] = { "xchg", OP_PRESENT | OP_DEF8, 2, OP_ARG2_RMR_DEF },
//
//    // C6 /0 ib                 MOV r/m8, imm8
//    [0x0C6] = { "mov", OP_PRESENT | OP_DEF8 | OP_RMEXT, 2, OP_ARG2_RMI_DEF(8) },
//
    // A0                       MOV AL, moffs8
    [0x0A0] = { OPT_MOV, OP_PRESENT | OP_DEF8, 2, OP_ARG2_CT_DEF(64, 0) },
    // A2                       MOV moffs8, AL
    [0x0A2] = { OPT_MOV, OP_PRESENT | OP_DEF8, 2, OP_ARG2_TC_DEF(64, 0) },

//    // 84 /r                    TEST r/m8, r8
//    [0x084] = { "test", OP_PRESENT | OP_DEF8, 2, OP_ARG2_RRM_DEF },
    // 88 /r                    MOV r/m8, r8
    [0x088] = { OPT_MOV, OP_PRESENT | OP_DEF8, 2, OP_ARG2_RRM_DEF },
    // 8A /r                    MOV r8, r/m8
    [0x08A] = { OPT_MOV, OP_PRESENT | OP_DEF8, 2, OP_ARG2_RMR_DEF },
//    // A4
//    [0x0A4] = { "movs", OP_PRESENT | OP_DEF8, 0 },
//    // AA                       STOS m8
    [0x0AA] = { OPT_STOSB, OP_PRESENT | OP_DEF8, 0 },
//    // AC                       LODS m8
//    [0x0AC] = { "lods", OP_PRESENT | OP_DEF8, 0 },
//
//    // FE /0                    INC r/m8
//    [0x0FE] = { "inc", OP_PRESENT | OP_DEF8 | OP_RMEXT, 1, OP_ARG1_RM_DEF },
//    // FE /1                    DEC r/m8
//    [0x1FE] = { "dec", OP_PRESENT | OP_DEF8 | OP_RMEXT, 1, OP_ARG1_RM_DEF },
//
//    // A8 ib                    TEST AL, imm8
//    [0x0A8] = { "test", OP_PRESENT | OP_DEF8, 2, OP_ARG2_CI_DEF(8, 0) },
//
//    // C0 /0 ib                 ROL r/m8, imm8
//    [0x0C0] = { "rol", OP_PRESENT | OP_DEF8 | OP_RMEXT, 2, OP_ARG2_RMI_DEF(8) },
//    // C0 /5 ib                 SHR r/m8, imm8
//    [0x5C0] = { "shr", OP_PRESENT | OP_DEF8 | OP_RMEXT, 2, OP_ARG2_RMI_DEF(8) },
//
//    // D0 /0                    ROL r/m8
//    [0x0D0] = { "rol", OP_PRESENT | OP_DEF8 | OP_RMEXT, 1, OP_ARG1_RM_DEF },
//    // D0 /5                    SHR r/m8
//    [0x5D0] = { "shr", OP_PRESENT | OP_DEF8 | OP_RMEXT, 1, OP_ARG1_RM_DEF },
//
//    // EC                       IN AL, DX
//    // AL is implied
//    [0x0EC] = { "in", OP_PRESENT | OP_DEF8, 1, OP_ARG1_C_DEF(2) },
//    // EE                       OUT DX, AL
//    // AL is implied
//    [0x0EE] = { "out", OP_PRESENT | OP_DEF8, 1, OP_ARG1_C_DEF(2) },
//
//    // 6C                       INS m8, DX
//    // DX is implied
//    // ES:(E)DI is implied
//    [0x06C] = { "ins", OP_PRESENT | OP_DEF8, 0 },
//    // 6E                       OUTS DX, m8
//    // DX is implied
//    // DS:(E)SI is implied
//    [0x06E] = { "outs", OP_PRESENT | OP_DEF8, 0 },
//
//
//    // "Simple" instructions
//    // 01 /r                    ADD r/m16, r16
//    // 01 /r                    ADD r/m32, r32
//    [0x001] = { "add", OP_PRESENT, 2, OP_ARG2_RRM_DEF },
//    // 03 /r                    ADD r16, r/m16
//    // 03 /r                    ADD r32, r/m32
//    [0x003] = { "add", OP_PRESENT, 2, OP_ARG2_RMR_DEF },
//    // 05 /r                    ADD AX, imm16
//    // 05 /r                    ADD EAX, imm32
//    [0x005] = { "add", OP_PRESENT, 2, OP_ARG2_CI_DEF(32, 0) },
//    // 09 /r                    OR r/m16, r16
//    // 09 /r                    OR r/m32, r32
//    [0x009] = { "or", OP_PRESENT, 2, OP_ARG2_RRM_DEF },
    // 29 /r                    SUB r/m16, r16
    // 29 /r                    SUB r/m32, r32
    [0x029] = { OPT_SUB, OP_PRESENT, 2, OP_ARG2_RRM_DEF },
    // 31 /r                    XOR r/m16, r16
    // 31 /r                    XOR r/m32, r32
    [0x031] = { OPT_XOR, OP_PRESENT, 2, OP_ARG2_RRM_DEF },
    // 39 /r                    CMP r/m16, r16
    // 39 /r                    CMP r/m32, r32
    [0x039] = { OPT_CMP, OP_PRESENT, 2, OP_ARG2_RRM_DEF },
    // 3B /r                    CMP r16, r/m16
    // 3B /r                    CMP r32, r/m32
    [0x03B] = { OPT_CMP, OP_PRESENT, 2, OP_ARG2_RMR_DEF },
//
//    // C8 iw ib                 ENTER imm16, imm8
//    [0x0C8] = { "enter", OP_PRESENT, 2, OP_ARG2_II_DEF(16, 8) },
//
//    // 90                       NOP
//    [0x090] = { "nop", OP_PRESENT, 0 },
//    // 61                       POPA
//    // 61                       POPAD
//    [0x061] = { "popa", OP_PRESENT, 0 },
//    // 62 /r                    BOUND r16, m16&16
//    // 62 /r                    BOUND r32, m32&32
//    [0x062] = { "bound", OP_PRESENT, 2, OP_ARG2_RMR_DEF },
//    // 63 /r                    ARPL r/m16, r16
//    [0x063] = { "arpl", OP_PRESENT, 2, OP_ARG2_RRM_DEF },
//
//    // 06                       PUSH ES
//    [0x006] = { "push", OP_PRESENT, 1, OP_ARG1_EXT_C_DEF(RMX_TYPE_SEG, RMX_ES) },
//    // 07                       POP ES
//    [0x007] = { "pop", OP_PRESENT, 1, OP_ARG1_EXT_C_DEF(RMX_TYPE_SEG, RMX_ES) },
//    // 0E                       PUSH CS
//    [0x00E] = { "push", OP_PRESENT, 1, OP_ARG1_EXT_C_DEF(RMX_TYPE_SEG, RMX_CS) },
//    // 1E                       PUSH DS
//    [0x01E] = { "push", OP_PRESENT, 1, OP_ARG1_EXT_C_DEF(RMX_TYPE_SEG, RMX_DS) },
//    // 1F                       POP DS
//    [0x01F] = { "pop", OP_PRESENT, 1, OP_ARG1_EXT_C_DEF(RMX_TYPE_SEG, RMX_DS) },
//
//    // 0D iw                    OR AX, imm16
//    // 0D id                    OR EAX, imm32
//    [0x00D] = { "or", OP_PRESENT, 2, OP_ARG2_CI_DEF(32, 0) },
//    // 25 iw                    AND AX, imm16
//    // 25 id                    AND EAX, imm32
//    [0x025] = { "and", OP_PRESENT, 2, OP_ARG2_CI_DEF(32, 0) },
//
    // 70 cb                    JO rel8
    [0x070] = { OPT_JO, OP_PRESENT, 1, OP_ARG1_REL_DEF(8) },
    // 71 cb                    JNO rel8
    [0x071] = { OPT_JNO, OP_PRESENT, 1, OP_ARG1_REL_DEF(8) },
    // 72 cb                    JC rel8
    [0x072] = { OPT_JC, OP_PRESENT, 1, OP_ARG1_REL_DEF(8) },
    // 73 cb                    JNC rel8
    [0x073] = { OPT_JNC, OP_PRESENT, 1, OP_ARG1_REL_DEF(8) },
    // 74 cb                    JZ rel8
    [0x074] = { OPT_JZ, OP_PRESENT, 1, OP_ARG1_REL_DEF(8) },
    // 75 cb                    JNZ rel8
    [0x075] = { OPT_JNZ, OP_PRESENT, 1, OP_ARG1_REL_DEF(8) },
    // 76 cb                    JNA rel8
    [0x076] = { OPT_JNA, OP_PRESENT, 1, OP_ARG1_REL_DEF(8) },
    // 77 cb                    JA rel8
    [0x077] = { OPT_JA, OP_PRESENT, 1, OP_ARG1_REL_DEF(8) },
    // 78 cb                    JS rel8
    [0x078] = { OPT_JS, OP_PRESENT, 1, OP_ARG1_REL_DEF(8) },
    // 79 cb                    JNS rel8
    [0x079] = { OPT_JNS, OP_PRESENT, 1, OP_ARG1_REL_DEF(8) },
    // 7A cb                    JP rel8
    [0x07A] = { OPT_JP, OP_PRESENT, 1, OP_ARG1_REL_DEF(8) },
    // 7C cb                    JL rel8
    [0x07C] = { OPT_JL, OP_PRESENT, 1, OP_ARG1_REL_DEF(8) },
    // 7D cb                    JNL rel8
    [0x07D] = { OPT_JNL, OP_PRESENT, 1, OP_ARG1_REL_DEF(8) },
    // 7E cb                    JNG rel8
    [0x07E] = { OPT_JNG, OP_PRESENT, 1, OP_ARG1_REL_DEF(8) },
    // 7F cb                    JG rel8
    [0x07F] = { OPT_JG, OP_PRESENT, 1, OP_ARG1_REL_DEF(8) },
    // E2 cb                    LOOP rel8
    [0x0E2] = { OPT_LOOP, OP_PRESENT, 1, OP_ARG1_REL_DEF(8) },
    // E1 cb                    LOOPZ rel8
    [0x0E1] = { OPT_LOOPZ, OP_PRESENT, 1, OP_ARG1_REL_DEF(8) },
    // E0 cb                    LOOPNZ rel8
    [0x0E0] = { OPT_LOOPNZ, OP_PRESENT, 1, OP_ARG1_REL_DEF(8) },
    // E3 cb                    JCXZ rel8
    [0x0E3] = { OPT_JCXZ, OP_PRESENT, 1, OP_ARG1_REL_DEF(8) },
//
//    // 3D iw                    CMP AX, imm16
//    // 3D id                    CMP EAX, imm32
//    [0x03D] = { "cmp", OP_PRESENT, 2, OP_ARG2_CI_DEF(32, 0) },
//
//    // AB                       STOS m16
//    // AB                       STOS m32
//    [0x0AB] = { "stos", OP_PRESENT, 0 },
//    // AF                       SCAS m16
//    // AF                       SCAS m32
//    [0x0AF] = { "scas", OP_PRESENT, 0 },
//
//    // CB                       RETF
//    [0x0CB] = { "retf", OP_PRESENT, 0 },
    // CF                       IRET
    [0x0CF] = { OPT_IRET, OP_PRESENT, 0 },
//
//    // 11 /r                    ADC r/m16, r16
//    // 11 /r                    ADC r/m32, r32
//    [0x011] = { "adc", OP_PRESENT, 2, OP_ARG2_RRM_DEF },
//    // 13 /r                    ADC r16, r/m16
//    // 13 /r                    ADC r32, r/m32
//    [0x013] = { "adc", OP_PRESENT, 2, OP_ARG2_RMR_DEF },
//    // E9 cw                    JMP rel16
//    // E9 cd                    JMP rel32
    [0x0E9] = { OPT_JMP, OP_PRESENT, 1, OP_ARG1_REL_DEF(32) },
//    // EB cb                    JMP rel8
    [0x0EB] = { OPT_JMP, OP_PRESENT, 1, OP_ARG1_REL_DEF(8) },
    // 85 /r                    TEST r/m16, r16
    // 85 /r                    TEST r/m32, r32
    [0x085] = { OPT_TEST, OP_PRESENT, 2, OP_ARG2_RRM_DEF },
    // 89 /r                    MOV r/m16, r16
    // 89 /r                    MOV r/m32, r32
    [0x089] = { OPT_MOV, OP_PRESENT, 2, OP_ARG2_RRM_DEF },
    // 8B /r                    MOV r16, r/m16
    // 8B /r                    MOV r32, r/m32
    [0x08B] = { OPT_MOV, OP_PRESENT, 2, OP_ARG2_RMR_DEF },
    // 8E /r                    MOV Sreg, r/m16
    [0x08E] = { OPT_MOV, OP_PRESENT, 2, OP_ARG2_RMS_DEF },
    // 8C /r                    MOV r/m16, Sreg
    [0x08C] = { OPT_MOV, OP_PRESENT, 2, OP_ARG2_RRM_DEF },
    // 9C                       PUSHF
//    [0x09C] = { "pushf", OP_PRESENT, 0 },
//    // 9D                       POPF
//    [0x09D] = { "popf", OP_PRESENT, 0 },
//    // C2 iw                    RET imm16
//    [0x0C2] = { "ret", OP_PRESENT, 1, OP_ARG1_I_DEF(16) },
//    // F9                       STC
//    [0x0F9] = { "stc", OP_PRESENT, 0 },
//    // FA                       CLI
//    [0x0FA] = { "cli", OP_PRESENT, 0 },
//    // F8                       NOP
//    [0x0F8] = { "clc", OP_PRESENT, 0 },
//    // FC
//    [0x0FC] = { "cld", OP_PRESENT, 0 },
//    // CD ib                    INT imm8
//    [0x0CD] = { "int", OP_PRESENT, 1, OP_ARG1_I_DEF(8) },
//    // 8D /r                    LEA r16, m
//    // 8D /r                    LEA r32, m
//    [0x08D] = { "lea", OP_PRESENT, 2, OP_ARG2_RMR_DEF },
//
//    // 69 /r iw                 IMUL r16, r/m16, imm16
//    // 69 /r id                 IMUL r32, r/m32, imm32
//    [0x069] = { "imul", OP_PRESENT, 3, OP_ARG3_RRMI(32) },
//    // 6B /r ib                 IMUL r16, r/m16, imm8
//    // 6B /r ib                 IMUL r32, r/m32, imm8
//    [0x06B] = { "imul", OP_PRESENT, 3, OP_ARG3_RRMI(8) },
//
//    // 0B /r                    OR r16, r/m16
//    // 0B /r                    OR r32, r/m32
//    [0x00B] = { "or", OP_PRESENT, 2, OP_ARG2_RMR_DEF },
//    // 33 /r                    XOR r16, r/m16
//    // 33 /r                    XOR r32, r/m32
//    [0x033] = { "xor", OP_PRESENT, 2, OP_ARG2_RMR_DEF },
//
//    // 19 /r                    SBB r/m16, r16
//    // 19 /r                    SBB r/m32, r32
//    [0x019] = { "sbb", OP_PRESENT, 2, OP_ARG2_RRM_DEF },
//
    // C3                       RET
    [0x0C3] = { OPT_RET, OP_PRESENT, 0 },
//    // 99                       CWD
//    // 99                       CDQ
//    // REX.W + 99               CQO
//    [0x099] = { "cdq/cwd/cqo", OP_PRESENT, 0 },
//
//    // E8 cw                    CALL rel16
//    // E8 cd                    CALL rel32
//    [0x0E8] = { "call", OP_PRESENT, 1, OP_ARG1_REL_DEF(32) },
//
//    // 21 /r                    AND r/m16, r16
//    // 21 /r                    AND r/m32, r32
//    [0x021] = { "and", OP_PRESENT, 2, OP_ARG2_RRM_DEF },
//    // 23 /r                    AND r16, r/m16
//    // 23 /r                    AND r32, r/m32
//    [0x023] = { "and", OP_PRESENT, 2, OP_ARG2_RMR_DEF },
//
//    // ED                       IN AX, DX
//    // ED                       IN EAX, DX
//    // (E)AX is implied
//    [0x0ED] = { "in", OP_PRESENT, 1, OP_ARG1_C_DEF(2) },
//    // EF                       OUT DX, AX
//    // EF                       OUT DX, EAX
//    // AL/AX/EAX implied, so only 1 argument
//    [0x0EF] = { "out", OP_PRESENT, 1, OP_ARG1_C_DEF(2) },
//    // 6F                       OUTS DX, m16
//    // 6F                       OUTS DX, m32
//    // DX is implied
//    // DS:(E)SI is implied
//    [0x06F] = { "outs", OP_PRESENT, 0 },
//    // 6D                       INS m16, DX
//    // 6D                       INS m32, DX
//    // DX is implied
//    // ES:(E)DI is implied
//    [0x06D] = { "ins", OP_PRESENT, 0 },
//
//    // A1                       MOV AX, moffs16
//    // A1                       MOV EAX, moffs32
    [0x0A1] = { OPT_MOV, OP_PRESENT, 2, OP_ARG2_CT_DEF(64, 0) },
//    // A3                       MOV moffs16, AX
//    // A3                       MOV moffs32, EAX
    [0x0A3] = { OPT_MOV, OP_PRESENT, 2, OP_ARG2_TC_DEF(64, 0) },
//
//    // 6A ib                    PUSH imm8
    [0x06A] = { OPT_PUSH, OP_PRESENT, 1, OP_ARG1_I_DEF(8) },
//    // 68 iw                    PUSH imm16
//    // 68 id                    PUSH imm32
    [0x068] = { OPT_PUSH, OP_PRESENT, 1, OP_ARG1_I_DEF(32) },
//
//    // Instructions with in-opcode register
//    // 50 + rw                  PUSH r16
//    // 50 + rd                  PUSH r32
//    // REX.W + 50 + rd          PUSH r64
    [0x050] = { OPT_PUSH, OP_PRESENT | OP_DEF64, 1, OP_ARG1_C_DEF(0) },
    [0x051] = { OPT_PUSH, OP_PRESENT | OP_DEF64, 1, OP_ARG1_C_DEF(1) },
    [0x052] = { OPT_PUSH, OP_PRESENT | OP_DEF64, 1, OP_ARG1_C_DEF(2) },
    [0x053] = { OPT_PUSH, OP_PRESENT | OP_DEF64, 1, OP_ARG1_C_DEF(3) },
    [0x054] = { OPT_PUSH, OP_PRESENT | OP_DEF64, 1, OP_ARG1_C_DEF(4) },
    [0x055] = { OPT_PUSH, OP_PRESENT | OP_DEF64, 1, OP_ARG1_C_DEF(5) },
    [0x056] = { OPT_PUSH, OP_PRESENT | OP_DEF64, 1, OP_ARG1_C_DEF(6) },
    [0x057] = { OPT_PUSH, OP_PRESENT | OP_DEF64, 1, OP_ARG1_C_DEF(7) },
    // 58 + rw                  POP r16
    // 58 + rd                  POP r32
    // REX.W + 58 + rd          POP r64
    [0x058] = { OPT_POP, OP_PRESENT | OP_DEF64, 1, OP_ARG1_C_DEF(0) },
    [0x059] = { OPT_POP, OP_PRESENT | OP_DEF64, 1, OP_ARG1_C_DEF(1) },
    [0x05A] = { OPT_POP, OP_PRESENT | OP_DEF64, 1, OP_ARG1_C_DEF(2) },
    [0x05B] = { OPT_POP, OP_PRESENT | OP_DEF64, 1, OP_ARG1_C_DEF(3) },
    [0x05C] = { OPT_POP, OP_PRESENT | OP_DEF64, 1, OP_ARG1_C_DEF(4) },
    [0x05D] = { OPT_POP, OP_PRESENT | OP_DEF64, 1, OP_ARG1_C_DEF(5) },
    [0x05E] = { OPT_POP, OP_PRESENT | OP_DEF64, 1, OP_ARG1_C_DEF(6) },
    [0x05F] = { OPT_POP, OP_PRESENT | OP_DEF64, 1, OP_ARG1_C_DEF(7) },
    // B0 + rb ib               MOV r8, imm8
    [0x0B0] = { OPT_MOV, OP_PRESENT | OP_DEF8, 2, OP_ARG2_CI_DEF(8, 0) },
    [0x0B1] = { OPT_MOV, OP_PRESENT | OP_DEF8, 2, OP_ARG2_CI_DEF(8, 1) },
    [0x0B2] = { OPT_MOV, OP_PRESENT | OP_DEF8, 2, OP_ARG2_CI_DEF(8, 2) },
    [0x0B3] = { OPT_MOV, OP_PRESENT | OP_DEF8, 2, OP_ARG2_CI_DEF(8, 3) },
    [0x0B4] = { OPT_MOV, OP_PRESENT | OP_DEF8, 2, OP_ARG2_CI_DEF(8, 4) },
    [0x0B5] = { OPT_MOV, OP_PRESENT | OP_DEF8, 2, OP_ARG2_CI_DEF(8, 5) },
    [0x0B6] = { OPT_MOV, OP_PRESENT | OP_DEF8, 2, OP_ARG2_CI_DEF(8, 6) },
    [0x0B7] = { OPT_MOV, OP_PRESENT | OP_DEF8, 2, OP_ARG2_CI_DEF(8, 7) },
    // B8 + rw iw               MOV r16, imm16
    // B8 + rd id               MOV r32, imm32
    // REX.W + B8 + rd io       MOV r64, imm64
    [0x0B8] = { OPT_MOV, OP_PRESENT, 2, OP_ARG2_CI_DEF(64, 0) },
    [0x0B9] = { OPT_MOV, OP_PRESENT, 2, OP_ARG2_CI_DEF(64, 1) },
    [0x0BA] = { OPT_MOV, OP_PRESENT, 2, OP_ARG2_CI_DEF(64, 2) },
    [0x0BB] = { OPT_MOV, OP_PRESENT, 2, OP_ARG2_CI_DEF(64, 3) },
    [0x0BC] = { OPT_MOV, OP_PRESENT, 2, OP_ARG2_CI_DEF(64, 4) },
    [0x0BD] = { OPT_MOV, OP_PRESENT, 2, OP_ARG2_CI_DEF(64, 5) },
    [0x0BE] = { OPT_MOV, OP_PRESENT, 2, OP_ARG2_CI_DEF(64, 6) },
    [0x0BF] = { OPT_MOV, OP_PRESENT, 2, OP_ARG2_CI_DEF(64, 7) },
//
//
//    // Instructions with ModR/M extension
//    // DC /0    FADD m64fp
//    [0x0DC] = { "fadd", OP_PRESENT | OP_RMEXT | OP_DEF64, 1, OP_ARG1_RM_DEF },
//    // DC /5    FSUBR m64fp
//    [0x5DC] = { "fsubr", OP_PRESENT | OP_RMEXT | OP_DEF64, 1, OP_ARG1_RM_DEF },
//
    // FF /0    INC r/m16
    // FF /0    INC r/m32
    [0x0FF] = { OPT_INC, OP_PRESENT | OP_RMEXT, 1, OP_ARG1_RM_DEF },
    // FF /2    CALL r/m16
    // FF /2    CALL r/m32
    [0x2FF] = { OPT_CALL, OP_PRESENT | OP_RMEXT, 1, OP_ARG1_RM_DEF },
//    // FF /1    DEC r/m16
//    // FF /1    DEC r/m32
//    [0x1FF] = { "dec", OP_PRESENT | OP_RMEXT, 1, OP_ARG1_RM_DEF },
    // FF /3    CALL m16:16
    // FF /3    CALL m16:32
    [0x3FF] = { OPT_CALL, OP_PRESENT | OP_RMEXT, 1, OP_ARG1_RM_DEF },
//    // FF /4    JMP r/m16
//    // FF /4    JMP r/m32
//    [0x4FF] = { "jmp", OP_PRESENT | OP_RMEXT, 1, OP_ARG1_RM_DEF },
//    // FF /6    PUSH r/m16
//    // FF /6    PUSH r/m32
//    [0x6FF] = { "push", OP_PRESENT | OP_RMEXT, 1, OP_ARG1_RM_DEF },
//
//    // 8F /0                    POP r/m16
//    // 8F /0                    POP r/m32
//    [0x08F] = { "pop", OP_PRESENT | OP_RMEXT, 1, OP_ARG1_RM_DEF },
//
    // C1 /0 ib                 ROL r/m16, imm8
    // C1 /0 ib                 ROL r/m32, imm8
    [0x0C1] = { OPT_ROL, OP_PRESENT | OP_RMEXT, 2, OP_ARG2_RMI_DEF(8) },
    // C1 /4 ib                 SAL r/m16, imm8
    // C1 /4 ib                 SAL r/m32, imm8
    [0x4C1] = { OPT_SAL, OP_PRESENT | OP_RMEXT, 2, OP_ARG2_RMI_DEF(8) },
    // C1 /5 ib                 SHR r/m16, imm8
    // C1 /5 ib                 SHR r/m32, imm8
    [0x5C1] = { OPT_SHR, OP_PRESENT | OP_RMEXT, 2, OP_ARG2_RMI_DEF(8) },
    // C1 /7 ib                 SAR r/m16, imm8
    // C1 /7 ib                 SAR r/m32, imm8
    [0x7C1] = { OPT_SAR, OP_PRESENT | OP_RMEXT, 2, OP_ARG2_RMI_DEF(8) },

    // C7 /0 iw                 MOV r/m16, imm16
    // C7 /0 id                 MOV r/m32, imm32
    // REX.W + C7 /0 id         MOV r/m64, imm32
    [0x0C7] = { OPT_MOV, OP_PRESENT | OP_RMEXT, 2, OP_ARG2_RMI_DEF(32) },
//
//    // TODO: OP_ARG2_RM1_DEF
//    // D1 /0                    ROL r/m16, 1
//    // D1 /0                    ROL r/m32, 1
//    [0x0D1] = { "rol", OP_PRESENT | OP_RMEXT, 1, OP_ARG1_RM_DEF },
//    // D1 /4                    SHL r/m16, 1
//    // D1 /4                    SHL r/m32, 1
//    [0x4D1] = { "shl", OP_PRESENT | OP_RMEXT, 1, OP_ARG1_RM_DEF },
//    // D1 /5                    SHR r/m16, 1
//    // D1 /5                    SHR r/m32, 1
//    [0x5D1] = { "shr", OP_PRESENT | OP_RMEXT, 1, OP_ARG1_RM_DEF },
//    // D1 /7                    SAR r/m16, 1
//    // D1 /7                    SAR r/m32, 1
//    [0x7D1] = { "sar", OP_PRESENT | OP_RMEXT, 1, OP_ARG1_RM_DEF },
//
//    // D3 /0                    ROL r/m16, CL
//    // D3 /0                    ROL r/m32, CL
//    [0x0D3] = { "rol", OP_PRESENT | OP_RMEXT, 2, OP_ARG2_RMC_DEF(1), },
//    // D3 /4                    SHL r/m16, CL
//    // D3 /4                    SHL r/m32, CL
//    [0x4D3] = { "shl", OP_PRESENT | OP_RMEXT, 2, OP_ARG2_RMC_DEF(1), },
//    // D3 /5                    SHR r/m16, CL
//    // D3 /5                    SHR r/m32, CL
//    [0x5D3] = { "shr", OP_PRESENT | OP_RMEXT, 2, OP_ARG2_RMC_DEF(1), },
//    // D3 /7                    SAR r/m16, CL
//    // D3 /7                    SAR r/m32, CL
//    [0x7D3] = { "sar", OP_PRESENT | OP_RMEXT, 2, OP_ARG2_RMC_DEF(1), },

    // 81 /0 iw                 ADD r/m16, imm16
    // 81 /0 id                 ADD r/m32, imm32
    [0x081] = { OPT_ADD, OP_PRESENT | OP_RMEXT, 2, OP_ARG2_RMI_DEF(32) },
    // 81 /1 iw                 OR r/m16, imm16
    // 81 /1 id                 OR r/m32, imm32
    [0x181] = { OPT_OR, OP_PRESENT | OP_RMEXT, 2, OP_ARG2_RMI_DEF(32) },
    // 81 /4 iw                 AND r/m16, imm16
    // 81 /4 id                 AND r/m32, imm32
    [0x481] = { OPT_AND, OP_PRESENT | OP_RMEXT, 2, OP_ARG2_RMI_DEF(32) },
    // 81 /5 iw                 SUB r/m16, imm16
    // 81 /5 id                 SUB r/m32, imm32
    [0x581] = { OPT_SUB, OP_PRESENT | OP_RMEXT, 2, OP_ARG2_RMI_DEF(32) },
    // 81 /7 iw                 CMP r/m16, imm16
    // 81 /7 id                 CMP r/m32, imm32
    [0x781] = { OPT_CMP, OP_PRESENT | OP_RMEXT, 2, OP_ARG2_RMI_DEF(32) },

    // 83 /0 iw                 ADD r/m16, imm8
    // 83 /0 id                 ADD r/m32, imm8
    [0x083] = { OPT_ADD, OP_PRESENT | OP_RMEXT, 2, OP_ARG2_RMI_DEF(8) },
//    // 83 /1 iw                 OR r/m16, imm8
//    // 83 /1 id                 OR r/m32, imm8
//    [0x183] = { "or", OP_PRESENT | OP_RMEXT, 2, OP_ARG2_RMI_DEF(8) },
//    // 83 /4 iw                 AND r/m16, imm8
//    // 83 /4 id                 AND r/m32, imm8
//    [0x483] = { "and", OP_PRESENT | OP_RMEXT, 2, OP_ARG2_RMI_DEF(8) },
    // 83 /5 iw                 SUB r/m16, imm8
    // 83 /5 id                 SUB r/m32, imm8
    [0x583] = { OPT_SUB, OP_PRESENT | OP_RMEXT, 2, OP_ARG2_RMI_DEF(8) },
//    // 83 /7 iw                 CMP r/m16, imm8
//    // 83 /7 id                 CMP r/m32, imm8
//    [0x783] = { "cmp", OP_PRESENT | OP_RMEXT, 2, OP_ARG2_RMI_DEF(8) },
//
//    // F7 /0 iw                 TEST r/m16, imm16
//    // F7 /0 id                 TEST r/m32, imm32
//    [0x0F7] = { "test", OP_PRESENT | OP_RMEXT, 2, OP_ARG2_RMI_DEF(32) },
//    // F7 /2                    NOT r/m16
//    // F7 /2                    NOT r/m32
//    [0x2F7] = { "not", OP_PRESENT | OP_RMEXT, 1, OP_ARG1_RM_DEF },
//    // F7 /3                    NEG r/m16
//    // F7 /3                    NEG r/m32
//    [0x3F7] = { "neg", OP_PRESENT | OP_RMEXT, 1, OP_ARG1_RM_DEF },
//    // F7 /6                    DIV r/m16
//    // F7 /6                    DIV r/m32
//    [0x6F7] = { "div", OP_PRESENT | OP_RMEXT, 1, OP_ARG1_RM_DEF },
//    // F7 /7                    IDIV r/m16
//    // F7 /7                    IDIV r/m32
//    [0x7F7] = { "idiv", OP_PRESENT | OP_RMEXT, 1, OP_ARG1_RM_DEF },
//
//
    // 16-bit only opcodes (overlap REX prefix)
    // 40 + rw  INC r16
    [0x040] = { OPT_INC, OP_PRESENT, 1, OP_ARG1_C_DEF(0) },
    [0x041] = { OPT_INC, OP_PRESENT, 1, OP_ARG1_C_DEF(1) },
    [0x042] = { OPT_INC, OP_PRESENT, 1, OP_ARG1_C_DEF(2) },
    [0x043] = { OPT_INC, OP_PRESENT, 1, OP_ARG1_C_DEF(3) },
    [0x044] = { OPT_INC, OP_PRESENT, 1, OP_ARG1_C_DEF(4) },
    [0x045] = { OPT_INC, OP_PRESENT, 1, OP_ARG1_C_DEF(5) },
    [0x046] = { OPT_INC, OP_PRESENT, 1, OP_ARG1_C_DEF(6) },
    [0x047] = { OPT_INC, OP_PRESENT, 1, OP_ARG1_C_DEF(7) },

    // 48 + rw  DEC r16
    [0x048] = { OPT_DEC, OP_PRESENT, 1, OP_ARG1_C_DEF(0) },
    [0x049] = { OPT_DEC, OP_PRESENT, 1, OP_ARG1_C_DEF(1) },
    [0x04A] = { OPT_DEC, OP_PRESENT, 1, OP_ARG1_C_DEF(2) },
    [0x04B] = { OPT_DEC, OP_PRESENT, 1, OP_ARG1_C_DEF(3) },
    [0x04C] = { OPT_DEC, OP_PRESENT, 1, OP_ARG1_C_DEF(4) },
    [0x04D] = { OPT_DEC, OP_PRESENT, 1, OP_ARG1_C_DEF(5) },
    [0x04E] = { OPT_DEC, OP_PRESENT, 1, OP_ARG1_C_DEF(6) },
    [0x04F] = { OPT_DEC, OP_PRESENT, 1, OP_ARG1_C_DEF(7) },
};

static struct op_info ops_0f[0x800] = {
//    // 8-bit operand
//    // 0F 94 cw     SETZ r/m8
//    [0x094] = { "setz", OP_PRESENT | OP_DEF8, 1, OP_ARG1_RM_DEF },
//    // 0F 95 cw     SETNZ r/m8
//    [0x095] = { "setnz", OP_PRESENT | OP_DEF8, 1, OP_ARG1_RM_DEF },
//    // 0F 9C cw     SETL r/m8
//    [0x09C] = { "setl", OP_PRESENT | OP_DEF8, 1, OP_ARG1_RM_DEF },
//
//    // 0F B6 /r                 MOVZX r16, r/m8
//    // 0F B6 /r                 MOVZX r32, r/m8
//    [0x0B6] = { "movzx", OP_PRESENT | OP_DEF8, 2, OP_ARG2_RRM_DEF },
//
//    // 0F BE /r                 MOVSX r16, r/m8
//    // 0F BE /r                 MOVSX r32, r/m8
//    [0x0BE] = { "movsx", OP_PRESENT | OP_DEF8, 2, OP_ARG2_RRM_DEF },
//
//    // Everything else
//    // 0F 00 /0                 SLDT r/m16
//    [0x000] = { "sldt", OP_PRESENT | OP_RMEXT, 1, OP_ARG1_RM_DEF },
//    // 0F 00 /3                 LTR r/m16
//    [0x300] = { "ltr", OP_PRESENT | OP_RMEXT, 1, OP_ARG1_RM_DEF },
//
    // 0F 0B                    UD2
    [0x00B] = { OPT_UD2, OP_PRESENT, 0 },
//    // 0F 82 cw     JC rel16
//    // 0F 82 cd     JC rel32
//    [0x082] = { "jc", OP_PRESENT, 1, OP_ARG1_REL_DEF(32) },
//    // 0F 83 cw     JNC rel16
//    // 0F 83 cd     JNC rel32
//    [0x083] = { "jnc", OP_PRESENT, 1, OP_ARG1_REL_DEF(32) },
//    // 0F 84 cw     JZ rel16
//    // 0F 84 cd     JZ rel32
//    [0x084] = { "jz", OP_PRESENT, 1, OP_ARG1_REL_DEF(32) },
//    // 0F 85 cw     JNZ rel16
//    // 0F 85 cd     JNZ rel32
//    [0x085] = { "jnz", OP_PRESENT, 1, OP_ARG1_REL_DEF(32) },
//    // 0F 86 cw     JNA rel16
//    // 0F 86 cd     JNA rel32
//    [0x086] = { "jna", OP_PRESENT, 1, OP_ARG1_REL_DEF(32) },
//    // 0F 87 cw     JA rel16
//    // 0F 87 cd     JA rel32
//    [0x087] = { "ja", OP_PRESENT, 1, OP_ARG1_REL_DEF(32) },
//    // 0F 88 cw     JS rel16
//    // 0F 88 cd     JS rel32
//    [0x088] = { "js", OP_PRESENT, 1, OP_ARG1_REL_DEF(32) },
//    // 0F 8D cw     JNL rel16
//    // 0F 8D cd     JNL rel32
//    [0x08D] = { "jnl", OP_PRESENT, 1, OP_ARG1_REL_DEF(32) },
//    // 0F 8E cw     JNG rel16
//    // 0F 8E cd     JNG rel32
//    [0x08E] = { "jng", OP_PRESENT, 1, OP_ARG1_REL_DEF(32) },
//
    // TODO: this instruction's operand size depends on CPU operating mode
    // 0F 20 /r                 MOV r32, CR0-CR7
    // 0F 20 /r                 MOV r64, CR0-CR7
    [0x020] = { OPT_MOV, OP_PRESENT, 2, {
        { 1, ARG_SRC_MODRM, .rm = 1 },
        { 0, ARG_SRC_MODRM_CR, .rm = 0 }
    }},
    // 0F 22 /r                 MOV CR0-CR7, r32
    // 0F 22 /r                 MOV CR0-CR7, r64
    [0x022] = { OPT_MOV, OP_PRESENT, 2, {
        { 1, ARG_SRC_MODRM_CR, .rm = 0 },
        { 0, ARG_SRC_MODRM, .rm = 1 }
    }},
//
//    // 0F 41 /r                 CMOVNO r16, r/m16
//    [0x041] = { "cmovno", OP_PRESENT, 2, OP_ARG2_RMR_DEF },
//
//    // 0F A4 /r ib  SHLD r/m16, r16, imm8
//    // 0F A4 /r ib  SHLD r/m32, r32, imm8
//    [0x0A4] = { "shld", OP_PRESENT, 3, OP_ARG3_RRMI(8) },
//
//    // 0F B7 /r                 MOVZX r32, r/m16
//    // REX.W + 0F B7 /r         MOVZX r64, r/m32
//    [0x0B7] = { "movzx", OP_PRESENT, 2, OP_ARG2_RRM_DEF },
//
//    // 0F 08                    INVD
//    [0x008] = { "invd", OP_PRESENT, 0 },
//
//    // 0F AF /r                 IMUL r16, r/m16
//    // 0F AF /r                 IMUL r32, r/m32
//    // REX.W + 0F AF /r         IMUL r64, r/m64
//    [0x0AF] = { "imul", OP_PRESENT, 2, OP_ARG2_RRM_DEF },
//
//    // 0F FF /r                 UD0 r32, r/m32
//    [0x0FF] = { "ud0", OP_PRESENT, 2, OP_ARG2_RMR_DEF },
//
//    // 0F 05                    SYSCALL
//    [0x005] = { "syscall", OP_PRESENT, 0 },
//
//    // SSE shit that appeared in BIOS I've tried to disassemble
//    // 0F 10 /r                 MOVUPS xmm1, xmm2/m128
//    [0x010] = { "movups", OP_PRESENT | OP_SSE, 2, OP_ARG2_SSE_RMR_DEF },
};

static struct op_info ops_0f01_swapgs = {
    OPT_SWAPGS, OP_PRESENT, 0
};
#pragma GCC diagnostic pop

ssize_t x86_match_op(const uint8_t *code, struct op_info **info, uint8_t *modrm) {
    size_t pos = 0;

    if (code[0] == 0x0F) {
        if (code[1] == 0x38) {
            return -1;
        } else if (code[1] == 0x3A) {
            return -1;
        } else {
            // Omit 0x0F
            ++pos;
#ifdef PRINT_OPCODES
            printf("0F ");
            printf("%02hhX ", code[pos]);
#endif
            uint8_t l2_op = code[pos++];

            if (l2_op == 0x01) {
                // Special?
                uint8_t l3_op = code[pos++];

                switch (l3_op) {
                case 0xF8:
                    *info = &ops_0f01_swapgs;
                    *modrm = 0;
                    return 3;
                default:
                    return -1;
                }

                return -1;
            }

            if (ops_0f[l2_op].flags & OP_PRESENT) {
                // Check if opcode has a ModR/M extension
                *info = &ops_0f[l2_op];

                if ((*info)->flags & OP_RMEXT) {
                    uint8_t r;
                    *modrm = code[pos++];

                    r = ((*modrm) >> 3) & 0x7;
                    *info = &ops_0f[(((uint16_t) r) << 8) | l2_op];

                    if (!((*info)->flags & OP_PRESENT)) {
#ifdef PRINT_OPCODES
                        printf("%02hhX ", *modrm);
                        printf(" (R = %u) ", r);
#endif
                        return -1;
                    }
                }

                return pos;
            }

            return -1;
        }
    }

#ifdef PRINT_OPCODES
    printf("%02hhX ", code[0]);
#endif

    uint8_t l1_op = code[pos++];

    if (ops_1[l1_op].flags & OP_PRESENT) {
        // Check if opcode has a ModR/M extension
        *info = &ops_1[l1_op];

        if ((*info)->flags & OP_RMEXT) {
            uint8_t r;
            *modrm = code[pos++];

            r = ((*modrm) >> 3) & 0x7;
            *info = &ops_1[(((uint16_t) r) << 8) | l1_op];
            if (!((*info)->flags & OP_PRESENT)) {
#ifdef PRINT_OPCODES
                printf("%02hhX ", *modrm);
                printf(" (R = %u) ", r);
#endif
                return -1;
            }
        }

        return pos;
    }

    return -1;
}
