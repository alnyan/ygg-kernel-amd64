; vi: ft=asm :
org 0x7000
bits 16

;   Application processor bootstrapping code
; --------------------------------------------
;   Parameters passed to us from BSP:
;   0x7FC0      Kernel PML4 physical address
;   0x7FC8      GDTR physical address
;   0x7FD0      IDTR
;   0x7FD8      Kernel AP core entrypoint
;   0x7FE0      Kernel AP stack
; --------------------------------------------

ap_startup:
    cli

    mov ax, 0
    mov ds, ax

    ; Disable NMI
    in al, 0x70
    or al, 0x80
    out 0x70, al

    ; Load protected-mode GDT
    lgdt [ap_prot_gdtr]
    mov eax, cr0
    or al, 1
    mov cr0, eax

    jmp 0x08:ap_startup_prot

bits 32
ap_startup_prot:
    cli

    mov eax, 0x10
    mov ds, eax

    ; Enable PAE, PSE
    mov eax, cr4
    or eax, (1 << 5) | (1 << 4)
    mov cr4, eax

    ; Load cr3 = PML4
    mov eax, dword [0x7FC0]
    mov cr3, eax

    ; Enable EFER.LME
    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8
    wrmsr

    ; Enable paging
    mov eax, cr0
    or eax, 1 << 31
    mov cr0, eax

    ; Load long-mode GDT (will be reloaded with upper address)
    mov eax, [0x7FC8]
    lgdt [eax]

    jmp 0x08:ap_startup_long

bits 64
align 8
ap_startup_long:
    mov rax, 0x10
    mov ds, rax

    ; Reload GDT from upper memory pointer
    mov rax, [0x7FC8]
    mov rcx, 0xFFFFFF0000000000
    add rax, rcx

    lgdt [rax]
    mov ax, 0x28
    ltr ax

    mov rax, 0x10
    mov ds, rax
    mov ss, rax
    mov es, rax
    mov fs, rax
    mov gs, rax

    ; Load IDT
    mov rax, qword [0x7FD0]
    lidt [rax]

    ; Load stack
    mov rsp, qword [0x7FE0]
    mov rbp, rsp

    ; Jump to kernel entry
    mov rax, qword [0x7FD8]
    jmp rax

; Protected-mode GDT
align 4
ap_prot_gdt:
    ; 0x00 - Null
    dq 0
    ; 0x08 - Code: 32-bit, ex, pr, sys
    dq 0xCF98000000FFFF
    ; 0x10 - Data: 32-bit, rw, pr, sys
    dq 0xCF92000000FFFF
ap_prot_gdt_end:

ap_prot_gdtr:
    dw ap_prot_gdt_end - ap_prot_gdt - 1
    dd ap_prot_gdt
