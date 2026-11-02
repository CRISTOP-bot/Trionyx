.global _start
.section .rodata
mensaje:
    .ascii "Hola\n"
hola:
    .ascii "Hola pinche mundo!\n"

.section .text
_start:
    mov x0, #1
    adr x1, mensaje
    mov x8, #64
    mov x2, #5
    svc #0
    adr x1, hola
    mov x2, #19
    svc #0
    mov x0, #0
    mov x8, #93
    svc #0

