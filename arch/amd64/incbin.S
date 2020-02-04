.section .rodata
.global INCBIN_START
.global INCBIN_END

.align 8
INCBIN_START:
    .incbin INCBIN_FILE
INCBIN_END:
