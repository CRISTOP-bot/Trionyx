#ifndef INIT_ERRORS_H
#define INIT_ERRORS_H

namespace stdr {
class errorsv {
    public:
        enum class init : unsigned int {
            test = 0,
            fatal = 1,
            out_of_range = 2,
            unrecoverable = 0xdeadbeef,
            no_mem = 0xbed,
            mount_error = 0xafdef,
            knotificable_error = 0xabcdef,
            onotificable_error = 0xfedcab,
            fatal_internal = 0xfabaef,
            internal_not = 0xfeabaf
        };
        /*
         * 0 = 0000, 0 | 0 = 0000, F & 0 = 0000
         * 1 = 0001, 0 | 
         * 2 = 0010
         * 3 = 0011
         * 4 = 0100
         * 5 = 0101
         * 6 = 0110
         * 7 = 0111
         * 8 = 1000
         * 9 = 1001
         * a = 1010
         * b = 1011
         * c = 1100
         * d = 1101
         * e = 1110
         * f = 1111
        enum class services : unsigned long long {
            not_ready = 0x4faf3fd3,
            already_running = 0xffff'ffff'ffff'f,
            dont_exist = 0xffff'ffff'ffff'ffff,
            exists = 0x0000'0000'0000'0000,
            not_running = 0x0000'0000'0000'0,
            ready = 0x60
        };
};
}
#endif
