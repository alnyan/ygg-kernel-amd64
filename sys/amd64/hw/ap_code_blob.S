.section .rodata
.global amd64_ap_code_start
.global amd64_ap_code_end

.align 8
amd64_ap_code_start:
    .incbin AP_CODE_BIN
amd64_ap_code_end:
