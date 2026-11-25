// SPDX-License-Identifier: GPL-2.0-only
// file by: 'Tr1zz'
// archivo hecho por: 'Tr1zz'


#ifndef INIT_ERRORS_H
#define INIT_ERRORS_H

namespace stdr {
class error {
  public:
    enum class init : unsigned short {
        test = 1,
        fatal = 4,
        out_of_range = 4 << 1,
        unrecoverable = 4 << 2,
        no_mem = 4 << 3,
        mount_error = 4 << 4,
        knotificable_error = 4 << 5,
        onotificable_error = 4 << 6,
        fatal_internal = 4 << 7,
        internal_not = 4 << 8
    };
    /*
     * 0 = 0000, 0 | 0 = 0000, F & 0 = 0000
     * 1 = 0001, 0 | 1 = 0001, F & 1 = 0001
     * 2 = 0010, 1 << 1 = 2,
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
     */
    enum class services : unsigned long long {
        not_ready = 2ULL << 1,
        already_running = 2ULL << 2,
        dont_exist = 2ULL << 4,
        exists = 2ULL << 8,
        not_running = 2ULL << 16,
        ts_ready = 2ULL << 32,
        unknown = 2ULL << 60
    };
    enum class runtime : unsigned long long {
        dont_exists = 16ULL << 1,
        exists = 32ULL << 2,
        handler_does_not_exist = 64ULL << 4,
        kmsg_unavail = 128ULL << 8,
        stdin_unavail = 256ULL << 16,
        stdout_unavail = 512ULL << 32,
        stderr_unavail = 1024ULL << 48
    };
};
} // namespace stdr
#endif
