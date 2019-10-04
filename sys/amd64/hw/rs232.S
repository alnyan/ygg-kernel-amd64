.section .text
.global rs232_send
rs232_send:
    // %rdi - port
    // %rsi - sym

    movw %si, %ax
    movw %di, %dx
    outb %al, %dx

    ret
