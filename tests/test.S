.section .bss
.align 4
buffer:
    .skip 256
.section .rodata
msg:
    .ascii "\n"
.global _start
.section .text
_start:
    mov x0, #0
    adr x1, buffer
    mov x2, #256
    mov x8, #63
    svc #0
    mov x5, x0
    mov x0, #1
    adr x1, buffer
    mov x2, x5
    mov x8, #64
    svc #0

    adr x1, msg
    mov x2, #1
    mov x8, #64
    svc #0

    mov x8, #93
    mov x0, #0
    svc #0
