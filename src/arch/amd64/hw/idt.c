#include "arch/amd64/hw/ints.h"
#include "arch/amd64/hw/pic8259.h"
#include "sys/mem.h"
#include <stdint.h>
#include <stddef.h>

#define ignore(x) if (0) {x = x;}

#define IDT_NENTR   256

#define IDT_FLG_TASK32      0x5
#define IDT_FLG_INT16       0x6
#define IDT_FLG_TRAP16      0x7
#define IDT_FLG_INT32       0xE
#define IDT_FLG_TRAP32      0xF
#define IDT_FLG_SS          (1 << 4)
#define IDT_FLG_R0          (0 << 5)
#define IDT_FLG_R1          (1 << 5)
#define IDT_FLG_R2          (2 << 5)
#define IDT_FLG_R3          (3 << 5)
#define IDT_FLG_P           (1 << 7)

extern void amd64_isr_0();
extern void amd64_isr_1();
extern void amd64_isr_2();
extern void amd64_isr_3();
extern void amd64_isr_4();
extern void amd64_isr_5();
extern void amd64_isr_6();
extern void amd64_isr_7();
extern void amd64_isr_8();
extern void amd64_isr_9();
extern void amd64_isr_10();
extern void amd64_isr_11();
extern void amd64_isr_12();
extern void amd64_isr_13();
extern void amd64_isr_14();
extern void amd64_isr_15();
extern void amd64_isr_16();
extern void amd64_isr_17();
extern void amd64_isr_18();
extern void amd64_isr_19();
extern void amd64_isr_20();
extern void amd64_isr_21();
extern void amd64_isr_22();
extern void amd64_isr_23();
extern void amd64_isr_24();
extern void amd64_isr_25();
extern void amd64_isr_26();
extern void amd64_isr_27();
extern void amd64_isr_28();
extern void amd64_isr_29();
extern void amd64_isr_30();
extern void amd64_isr_31();

// 8259 PIC IRQs
extern void amd64_irq_0();  // Timer
extern void amd64_irq_1();  // TODO
extern void amd64_irq_2();  // TODO
extern void amd64_irq_3();  // TODO
extern void amd64_irq_4();  // TODO
extern void amd64_irq_5();  // TODO
extern void amd64_irq_6();  // TODO
extern void amd64_irq_7();  // TODO
extern void amd64_irq_8();  // TODO
extern void amd64_irq_9();  // TODO
extern void amd64_irq_10(); // TODO
extern void amd64_irq_11(); // TODO
extern void amd64_irq_12(); // TODO
extern void amd64_irq_13(); // TODO
extern void amd64_irq_14(); // TODO
extern void amd64_irq_15(); // TODO

typedef struct {
    uint16_t base_lo;
    uint16_t selector;
    uint8_t zero;
    uint8_t flags;
    uint16_t base_hi;
    uint32_t base_ex;
    uint32_t zero1;
} amd64_idt_entry_t;

typedef struct {
    uint16_t size;
    uintptr_t offset;
} __attribute__((packed)) amd64_idt_ptr_t;

static amd64_idt_entry_t idt[IDT_NENTR];
static amd64_idt_ptr_t idtr;

void amd64_idt_set(int idx, uintptr_t base, uint16_t selector, uint8_t flags) {
    idt[idx].base_lo = base & 0xFFFF;
    idt[idx].base_hi = (base >> 16) & 0xFFFF;
    idt[idx].base_ex = (base >> 32) & 0xFFFFFFFF;
    idt[idx].selector = selector;
    idt[idx].flags = flags;
    idt[idx].zero = 0;
}

void amd64_idt_init(void) {
    idtr.offset = (uintptr_t) idt;
    idtr.size = sizeof(idt) - 1;

    memset(idt, 0, sizeof(idt));

    // Exceptions
    amd64_idt_set(0 , (uintptr_t) amd64_isr_0 , 0x08, IDT_FLG_P | IDT_FLG_R0 | IDT_FLG_INT32);
    amd64_idt_set(1 , (uintptr_t) amd64_isr_1 , 0x08, IDT_FLG_P | IDT_FLG_R0 | IDT_FLG_INT32);
    amd64_idt_set(2 , (uintptr_t) amd64_isr_2 , 0x08, IDT_FLG_P | IDT_FLG_R0 | IDT_FLG_INT32);
    amd64_idt_set(3 , (uintptr_t) amd64_isr_3 , 0x08, IDT_FLG_P | IDT_FLG_R0 | IDT_FLG_INT32);
    amd64_idt_set(4 , (uintptr_t) amd64_isr_4 , 0x08, IDT_FLG_P | IDT_FLG_R0 | IDT_FLG_INT32);
    amd64_idt_set(5 , (uintptr_t) amd64_isr_5 , 0x08, IDT_FLG_P | IDT_FLG_R0 | IDT_FLG_INT32);
    amd64_idt_set(6 , (uintptr_t) amd64_isr_6 , 0x08, IDT_FLG_P | IDT_FLG_R0 | IDT_FLG_INT32);
    amd64_idt_set(7 , (uintptr_t) amd64_isr_7 , 0x08, IDT_FLG_P | IDT_FLG_R0 | IDT_FLG_INT32);
    amd64_idt_set(8 , (uintptr_t) amd64_isr_8 , 0x08, IDT_FLG_P | IDT_FLG_R0 | IDT_FLG_INT32);
    amd64_idt_set(9 , (uintptr_t) amd64_isr_9 , 0x08, IDT_FLG_P | IDT_FLG_R0 | IDT_FLG_INT32);
    amd64_idt_set(10, (uintptr_t) amd64_isr_10, 0x08, IDT_FLG_P | IDT_FLG_R0 | IDT_FLG_INT32);
    amd64_idt_set(11, (uintptr_t) amd64_isr_11, 0x08, IDT_FLG_P | IDT_FLG_R0 | IDT_FLG_INT32);
    amd64_idt_set(12, (uintptr_t) amd64_isr_12, 0x08, IDT_FLG_P | IDT_FLG_R0 | IDT_FLG_INT32);
    amd64_idt_set(13, (uintptr_t) amd64_isr_13, 0x08, IDT_FLG_P | IDT_FLG_R0 | IDT_FLG_INT32);
    amd64_idt_set(14, (uintptr_t) amd64_isr_14, 0x08, IDT_FLG_P | IDT_FLG_R0 | IDT_FLG_INT32);
    amd64_idt_set(15, (uintptr_t) amd64_isr_15, 0x08, IDT_FLG_P | IDT_FLG_R0 | IDT_FLG_INT32);
    amd64_idt_set(16, (uintptr_t) amd64_isr_16, 0x08, IDT_FLG_P | IDT_FLG_R0 | IDT_FLG_INT32);
    amd64_idt_set(17, (uintptr_t) amd64_isr_17, 0x08, IDT_FLG_P | IDT_FLG_R0 | IDT_FLG_INT32);
    amd64_idt_set(18, (uintptr_t) amd64_isr_18, 0x08, IDT_FLG_P | IDT_FLG_R0 | IDT_FLG_INT32);
    amd64_idt_set(19, (uintptr_t) amd64_isr_19, 0x08, IDT_FLG_P | IDT_FLG_R0 | IDT_FLG_INT32);
    amd64_idt_set(20, (uintptr_t) amd64_isr_20, 0x08, IDT_FLG_P | IDT_FLG_R0 | IDT_FLG_INT32);
    amd64_idt_set(21, (uintptr_t) amd64_isr_21, 0x08, IDT_FLG_P | IDT_FLG_R0 | IDT_FLG_INT32);
    amd64_idt_set(22, (uintptr_t) amd64_isr_22, 0x08, IDT_FLG_P | IDT_FLG_R0 | IDT_FLG_INT32);
    amd64_idt_set(23, (uintptr_t) amd64_isr_23, 0x08, IDT_FLG_P | IDT_FLG_R0 | IDT_FLG_INT32);
    amd64_idt_set(24, (uintptr_t) amd64_isr_24, 0x08, IDT_FLG_P | IDT_FLG_R0 | IDT_FLG_INT32);
    amd64_idt_set(25, (uintptr_t) amd64_isr_25, 0x08, IDT_FLG_P | IDT_FLG_R0 | IDT_FLG_INT32);
    amd64_idt_set(26, (uintptr_t) amd64_isr_26, 0x08, IDT_FLG_P | IDT_FLG_R0 | IDT_FLG_INT32);
    amd64_idt_set(27, (uintptr_t) amd64_isr_27, 0x08, IDT_FLG_P | IDT_FLG_R0 | IDT_FLG_INT32);
    amd64_idt_set(28, (uintptr_t) amd64_isr_28, 0x08, IDT_FLG_P | IDT_FLG_R0 | IDT_FLG_INT32);
    amd64_idt_set(29, (uintptr_t) amd64_isr_29, 0x08, IDT_FLG_P | IDT_FLG_R0 | IDT_FLG_INT32);
    amd64_idt_set(30, (uintptr_t) amd64_isr_30, 0x08, IDT_FLG_P | IDT_FLG_R0 | IDT_FLG_INT32);
    amd64_idt_set(31, (uintptr_t) amd64_isr_31, 0x08, IDT_FLG_P | IDT_FLG_R0 | IDT_FLG_INT32);

    amd64_idt_set(IRQ_BASE + 0 , (uintptr_t) amd64_irq_0 , 0x08, IDT_FLG_P | IDT_FLG_R0 | IDT_FLG_INT32);
    amd64_idt_set(IRQ_BASE + 1 , (uintptr_t) amd64_irq_1 , 0x08, IDT_FLG_P | IDT_FLG_R0 | IDT_FLG_INT32);
    amd64_idt_set(IRQ_BASE + 2 , (uintptr_t) amd64_irq_2 , 0x08, IDT_FLG_P | IDT_FLG_R0 | IDT_FLG_INT32);
    amd64_idt_set(IRQ_BASE + 3 , (uintptr_t) amd64_irq_3 , 0x08, IDT_FLG_P | IDT_FLG_R0 | IDT_FLG_INT32);
    amd64_idt_set(IRQ_BASE + 4 , (uintptr_t) amd64_irq_4 , 0x08, IDT_FLG_P | IDT_FLG_R0 | IDT_FLG_INT32);
    amd64_idt_set(IRQ_BASE + 5 , (uintptr_t) amd64_irq_5 , 0x08, IDT_FLG_P | IDT_FLG_R0 | IDT_FLG_INT32);
    amd64_idt_set(IRQ_BASE + 6 , (uintptr_t) amd64_irq_6 , 0x08, IDT_FLG_P | IDT_FLG_R0 | IDT_FLG_INT32);
    amd64_idt_set(IRQ_BASE + 7 , (uintptr_t) amd64_irq_7 , 0x08, IDT_FLG_P | IDT_FLG_R0 | IDT_FLG_INT32);
    amd64_idt_set(IRQ_BASE + 8 , (uintptr_t) amd64_irq_8 , 0x08, IDT_FLG_P | IDT_FLG_R0 | IDT_FLG_INT32);
    amd64_idt_set(IRQ_BASE + 9 , (uintptr_t) amd64_irq_9 , 0x08, IDT_FLG_P | IDT_FLG_R0 | IDT_FLG_INT32);
    amd64_idt_set(IRQ_BASE + 10, (uintptr_t) amd64_irq_10, 0x08, IDT_FLG_P | IDT_FLG_R0 | IDT_FLG_INT32);
    amd64_idt_set(IRQ_BASE + 11, (uintptr_t) amd64_irq_11, 0x08, IDT_FLG_P | IDT_FLG_R0 | IDT_FLG_INT32);
    amd64_idt_set(IRQ_BASE + 12, (uintptr_t) amd64_irq_12, 0x08, IDT_FLG_P | IDT_FLG_R0 | IDT_FLG_INT32);
    amd64_idt_set(IRQ_BASE + 13, (uintptr_t) amd64_irq_13, 0x08, IDT_FLG_P | IDT_FLG_R0 | IDT_FLG_INT32);
    amd64_idt_set(IRQ_BASE + 14, (uintptr_t) amd64_irq_14, 0x08, IDT_FLG_P | IDT_FLG_R0 | IDT_FLG_INT32);
    amd64_idt_set(IRQ_BASE + 15, (uintptr_t) amd64_irq_15, 0x08, IDT_FLG_P | IDT_FLG_R0 | IDT_FLG_INT32);

    asm volatile ("lea idtr(%%rip), %%rax; lidt (%%rax)":::"memory");
}
