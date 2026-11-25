// SPDX-License-Identifier: GPL-2.0-only
// file by: 'Tr1zz'
// archivo hecho por: 'Tr1zz'


#include <types.h>
#include <util.h>
int main() {
    stdr::cout << "Hola, creando vector \n";
    stdr::vector<std::string> hola;
    hola.push_back("Hola perra\n");
    hola.push_back("Six seven\n");
    for (std::string &str : hola) {
        stdr::cout << str.data() << stdr::endl;
    }
}
