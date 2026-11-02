.global __builtin_hello_world
.section .rodata
    msg:
        .byte 0x48, 0x65, 0x6C, 0x6C, 0x6F, 0x20, 0x77, 0x6F, 0x72, 0x6C, 0x64, 0x21, 0x0A
.section .text
__builtin_hello_world:
    mov x0, #1
    adr x1, msg
    mov x2, #13
    mov x8, #64
    svc #0
    ret

