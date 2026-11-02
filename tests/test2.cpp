#include <types.h>
int main() {
    stdr::cout << "Hola, numero " << 42 << " y, ademas, " << 3.14 << " y "
               << (void *)nullptr << " \n";
    pr_info("Hello, kernel!\n");
    return 0;
}
