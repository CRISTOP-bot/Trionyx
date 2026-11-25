// SPDX-License-Identifier: GPL-2.0-only
// file by: 'Tr1zz'
// archivo hecho por: 'Tr1zz'


#include <iostream>
#include <util.h>
int main() {
    std::vector<std::string> test = stdr::tok("H$o$l$a$,$ $mun$do!\n", '$');
    for (std::string s : test) {
        std::cout << s << "\n";
    }
}
