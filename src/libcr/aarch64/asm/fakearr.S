.global _start
.section .data
    buffer:
        .word 10
.section .rodata
    saludo:
        .ascii "Hola.\n"
.section .text
    _start:
        mov w1, #0x10 
        ldr w2, =buffer
        str w1, [w2]
        mov x0, #0x01
        mov x1, [w2]
        mov x2, #0xFFFF
        svc #0



