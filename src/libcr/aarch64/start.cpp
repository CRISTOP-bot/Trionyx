extern "C" int main(int, char **, char **);
extern "C"
__attribute__((naked, noreturn, used))
void _start()
{
    asm volatile(
        "ldr x0, [sp]\n\t"
        "add x1, sp, #8\n\t"
        "add x2, sp, #16\n\t"
        "add x2, x2, x0, lsl #3\n\t"
        "mov x9, sp\n\t"
        "bic x9, x9, #15\n\t"
        "mov sp, x9\n\t"
        "bl main\n\t"
        "mov x8, #94\n\t"
        "svc #0\n\t"
        "1:\n\t"
        "hlt #0\n\t"
        "b 1b\n\t"
    );
}
