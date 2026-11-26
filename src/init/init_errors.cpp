#include <init_errors.h>
#include <lvector>
#include <types.h>

using code_t = unsigned long long;
using handler_t = void (*)(code_t);
template <typename... Args>
void throw_error(handler_t handler, unsigned long long code, Args... args) {
    pr_err("Error: ");
    (pr_err(args), ...);
    pr_err("\n");
    pr_err("Error code: %llu \n", code);
    handler(code);
}

void generic_init_handler(code_t code);
