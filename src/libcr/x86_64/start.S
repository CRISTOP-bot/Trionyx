.section .text
.global _start
.type _start, @function

_start:
xor %ebp, %ebp

mov (%rsp), %rdi
lea 8(%rsp), %rsi
lea 16(%rsp,%rdi,8), %rdx

and $-16, %rsp
call main

mov %eax, %edi
mov $231, %eax
syscall

hlt

.size _start, .-_start

.section .note.GNU-stack,"",@progbits
