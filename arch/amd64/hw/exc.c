#if defined(AMD64_SMP)
#include "arch/amd64/smp/ipi.h"
#include "arch/amd64/smp/smp.h"
#endif
#include "arch/amd64/disasm/front.h"
#include "arch/amd64/cpu.h"
#include "sys/mem/phys.h"
#include "sys/thread.h"
#include "sys/string.h"
#include "sys/types.h"
#include "sys/sched.h"
#include "sys/debug.h"
#include "sys/panic.h"
#include "sys/syms.h"
#include "sys/mm.h"

#define X86_EXCEPTION_DE        0
#define X86_EXCEPTION_UD        6
#define X86_EXCEPTION_GP        13
#define X86_EXCEPTION_PF        14
#define X86_EXCEPTION_XF        19

#define X86_PF_EXEC             (1 << 4)
#define X86_PF_RESVD            (1 << 3)
#define X86_PF_USER             (1 << 2)
#define X86_PF_WRITE            (1 << 1)
#define X86_PF_PRESENT          (1 << 0)

#define X86_FLAGS_CF            (1 << 0)
#define X86_FLAGS_PF            (1 << 2)
#define X86_FLAGS_AF            (1 << 4)
#define X86_FLAGS_ZF            (1 << 6)
#define X86_FLAGS_SF            (1 << 7)
#define X86_FLAGS_TF            (1 << 8)
#define X86_FLAGS_IF            (1 << 9)
#define X86_FLAGS_DF            (1 << 10)
#define X86_FLAGS_OF            (1 << 11)

struct amd64_exception_frame {
    uint64_t stack_canary;

    uint64_t r15, r14, r13, r12;
    uint64_t r11, r10, r9, r8;

    uint64_t rdi, rsi, rbp;
    uint64_t rbx, rdx, rcx, rax;

    uint64_t exc_no, exc_code;

    uint64_t rip, cs, rflags, rsp, ss;
};

int do_pfault(struct amd64_exception_frame *frame, uintptr_t cr2, uintptr_t cr3) {
    mm_space_t space = (mm_space_t) MM_VIRTUALIZE(cr3);

    if (!cr2) {
        // Don't even try to resolve NULL references
        return -1;
    }

    if (cr2 < KERNEL_VIRT_BASE) {
        // TODO: was that user CS check necessary?

        // Userspace fault
        uint64_t flags;
        uintptr_t phys = mm_map_get(space, cr2 & MM_PAGE_MASK, &flags);
        struct thread *thr = thread_self;
        _assert(thr);
        struct process *proc = thr->proc;
        _assert(proc);

        if (phys != MM_NADDR) {
            // If the exception was caused by write operation
            if ((frame->exc_no & X86_PF_WRITE) &&               // Error was caused by write
                (flags & MM_PAGE_USER) &&                       // Page is user-accessible
                (!(flags & MM_PAGE_WRITE))) {                   // Page is not writable
                struct page *page = PHYS2PAGE(phys);
                _assert(page);
                _assert(page->refcount);

                if (page->usage != PU_PRIVATE) {
                    panic("Write to non-CoW page triggered a page fault\n");
                }

                if (page->refcount == 2) {
                    //kdebug("[%d] Cloning page @ %p\n", proc->pid, cr2 & MM_PAGE_MASK);
                    uintptr_t new_phys = mm_phys_alloc_page(PU_PRIVATE);
                    _assert(new_phys != MM_NADDR);
                    memcpy((void *) MM_VIRTUALIZE(new_phys), (const void *) MM_VIRTUALIZE(phys), MM_PAGE_SIZE);
                    _assert(mm_umap_single(space, cr2 & MM_PAGE_MASK, 1) == phys);
                    _assert(mm_map_single(space, cr2 & MM_PAGE_MASK, new_phys, MM_PAGE_USER | MM_PAGE_WRITE) == 0);
                } else if (page->refcount == 1) {
                    //kdebug("[%d] Only one referring to %p now, claiming ownership\n", proc->pid, cr2 & MM_PAGE_MASK);
                    _assert(mm_umap_single(space, cr2 & MM_PAGE_MASK, 1) == phys);
                    _assert(mm_map_single(space, cr2 & MM_PAGE_MASK, phys, MM_PAGE_USER | MM_PAGE_WRITE) == 0);
                } else {
                    //kdebug("Page refcount == %d\n", page->refcount);
                    panic("???\n");
                }

                return 0;
            }
        }

        return -1;
    } else {
        // Kernel page faults are not resolvable
        return -1;
    }
}

static int exc_safe_read_byte(mm_space_t space, uintptr_t addr, uint8_t *byte) {
    uintptr_t page;
    page = mm_map_get(space, addr & ~0xFFF, NULL);
    if (page == MM_NADDR) {
        return -1;
    }
    *byte = *(uint8_t *) MM_VIRTUALIZE(page + (addr & 0xFFF));
    return 0;
}

static void exc_dump_code(int level, uintptr_t rip) {
#define DISASM_BYTES    72
    uintptr_t cr3;
    mm_space_t space;
    uint8_t bytes[DISASM_BYTES];
    size_t count;

    asm volatile ("movq %%cr3, %0":"=r"(cr3));
    space = (mm_space_t) MM_VIRTUALIZE(cr3);

    count = 0;
    for (size_t i = 0; i < sizeof(bytes); ++i) {
        // TODO: overflow check here
        if (exc_safe_read_byte(space, rip + i, &bytes[i]) != 0) {
            break;
        }
        ++count;
    }

    dump_segment(bytes, rip, count);
    debug_dump(level, bytes, count);
#undef DISASM_BYTES
}

static void exc_dump(int level, struct amd64_exception_frame *frame) {
    uintptr_t cr2, cr3;

    debugs(level, "\033[41m");
    if (sched_ready) {
        thread_dump(level, thread_self);
    }
    if (frame->exc_no == X86_EXCEPTION_PF) {
        asm volatile ("movq %%cr2, %0":"=r"(cr2));
        asm volatile ("movq %%cr3, %0":"=r"(cr3));
        debugf(level, "Page fault without resolution:\n");
        debugf(level, "%%cr3 = %p\n", cr3);
        if (MM_VIRTUALIZE(cr3) == (uintptr_t) mm_kernel) {
            debugf(level, "(Kernel)\n");
        }
        debugf(level, "Fault address: %p\n", cr2);

        uintptr_t phys = mm_map_get((mm_space_t) MM_VIRTUALIZE(cr3), cr2 & MM_PAGE_MASK, NULL);
        if (phys == MM_NADDR) {
            debugf(level, "Has no physical address (non-present?)\n");
        } else {
            debugf(level, "Refers to phys %p\n", phys);

            struct page *page = PHYS2PAGE(phys);
            kdebug("  Refcount: %u\n", page->refcount);
            kdebug("  Usage: %u\n", page->usage);
        }

        if (frame->exc_code & X86_PF_RESVD) {
            debugf(level, " - Page structure has reserved bit set\n");
        }
        if (frame->exc_code & X86_PF_EXEC) {
            debugf(level, " - Instruction fetch\n");
        } else {
            if (frame->exc_code & X86_PF_WRITE) {
                debugf(level, " - Write operation\n");
            } else {
                debugf(level, " - Read operation\n");
            }
        }
        if (!(frame->exc_code & X86_PF_PRESENT)) {
            debugf(level, " - Refers to non-present page\n");
        }
        if (frame->exc_code & X86_PF_USER) {
            debugf(level, " - Userspace\n");
        }
    }

    // Dump frame
    debugf(level, "CPU raised exception #%u\n", frame->exc_no);

    debugf(level, "%%rax = %p, %%rcx = %p\n", frame->rax, frame->rcx);
    debugf(level, "%%rdx = %p, %%rbx = %p\n", frame->rdx, frame->rbx);
    debugf(level, "%%rbp = %p\n", frame->rbp);
    debugf(level, "%%rsi = %p, %%rdi = %p\n", frame->rsi, frame->rdi);

    debugf(level, "%%r8  = %p, %%r9  = %p\n",  frame->r8,  frame->r9);
    debugf(level, "%%r10 = %p, %%r11 = %p\n", frame->r10, frame->r11);
    debugf(level, "%%r12 = %p, %%r13 = %p\n", frame->r12, frame->r13);
    debugf(level, "%%r14 = %p, %%r15 = %p\n", frame->r14, frame->r15);

    debugf(level, "Execution context:\n");
    debugf(level, "%%cs:%%rip = %02x:%p\n", frame->cs, frame->rip);
    debugf(level, "%%ss:%%rsp = %02x:%p\n", frame->ss, frame->rsp);
    debugf(level, "%%rflags = %08x (IOPL=%u %c%c%c%c%c%c%c %c%c)\n",
            frame->rflags,
            (frame->rflags >> 12) & 0x3,
            (frame->rflags & X86_FLAGS_CF) ? 'C' : '-',
            (frame->rflags & X86_FLAGS_PF) ? 'P' : '-',
            (frame->rflags & X86_FLAGS_AF) ? 'A' : '-',
            (frame->rflags & X86_FLAGS_ZF) ? 'Z' : '-',
            (frame->rflags & X86_FLAGS_SF) ? 'S' : '-',
            (frame->rflags & X86_FLAGS_DF) ? 'D' : '-',
            (frame->rflags & X86_FLAGS_OF) ? 'O' : '-',
            (frame->rflags & X86_FLAGS_IF) ? 'I' : '-',
            (frame->rflags & X86_FLAGS_TF) ? 'T' : '-');
    uintptr_t sym_base;
    const char *sym_name;
    if (ksym_find_location(frame->rip, &sym_name, &sym_base) == 0) {
        debugf(level, "   Backtrace:\n");
        debugf(level, "0: %p <%s + %04x>\n", frame->rip, sym_name, frame->rip - sym_base);
        debug_backtrace(DEBUG_FATAL, frame->rbp, 1, 10);
    } else {
        debugf(level, "%rip is in unknown location\n");
    }

    exc_dump_code(level, frame->rip);

    debugs(level, "\033[0m");
}

void amd64_exception(struct amd64_exception_frame *frame) {
    if (frame->exc_no == X86_EXCEPTION_PF) {
        uintptr_t cr2, cr3;
        asm volatile ("movq %%cr2, %0":"=r"(cr2));
        asm volatile ("movq %%cr3, %0":"=r"(cr3));

        if (do_pfault(frame, cr2, cr3) == 0) {
            // Exception is resolved
            return;
        }
    }

    uintptr_t cr3;
    asm volatile ("movq %%cr3, %0":"=r"(cr3));

    // Check if the exception can be resolved by signaling the thread
    if (frame->cs == 0x23) {
        _assert(thread_self);

        switch (frame->exc_no) {
        case X86_EXCEPTION_UD:
            kerror("SIGILL in %d\n", thread_self->proc->pid);
            exc_dump(DEBUG_DEFAULT, frame);
            thread_signal(thread_self, SIGILL);
            return;
        case X86_EXCEPTION_DE:
        case X86_EXCEPTION_XF:
            kerror("SIGFPE in %d\n", thread_self->proc->pid);
            exc_dump(DEBUG_DEFAULT, frame);
            thread_signal(thread_self, SIGFPE);
            return;
        case X86_EXCEPTION_GP:
        case X86_EXCEPTION_PF:
            kerror("SIGSEGV in %d\n", thread_self->proc->pid);
            exc_dump(DEBUG_DEFAULT, frame);
            while (1);
            thread_signal(thread_self, SIGSEGV);
            return;
        }
    }

    exc_dump(DEBUG_FATAL, frame);

#if defined(AMD64_SMP)
    // Send PANIC IPIs to all other CPUs
    size_t cpu = get_cpu()->processor_id;
    kfatal("cpu%u initiates panic sequence\n", cpu);
    for (size_t i = 0; i < smp_ncpus; ++i) {
        if (i != cpu) {
            amd64_ipi_send(i, IPI_VECTOR_PANIC);
        }
    }
#endif

    while (1) {
        asm volatile ("cli; hlt");
    }
}
