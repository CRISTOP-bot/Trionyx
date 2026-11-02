.global _start
.section .rodata
    msg:
        .ascii "Hola mundo de mierda! \n"
.section .text
_start:
    mov x0, #1
    adr x1, msg
    mov x2, #23
    mov x8, #64
    svc #0
    mov x0, #0
    mov x8, #93
    svc #0
