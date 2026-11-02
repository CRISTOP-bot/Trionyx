using syscall_t = long;

extern "C" syscall_t rsyscall(syscall_t nr, syscall_t a0, syscall_t a1,
                              syscall_t a2, syscall_t a3, syscall_t a4,
                              syscall_t a5) {
    register syscall_t x0 asm("x0") = a0;
    register syscall_t x1 asm("x1") = a1;
    register syscall_t x2 asm("x2") = a2;
    register syscall_t x3 asm("x3") = a3;
    register syscall_t x4 asm("x4") = a4;
    register syscall_t x5 asm("x5") = a5;
    register syscall_t x8 asm("x8") = nr;

    asm volatile("svc #0"
                 : "+r"(x0)
                 : "r"(x1), "r"(x2), "r"(x3), "r"(x4), "r"(x5), "r"(x8)
                 : "memory", "cc");

    return x0;
}
