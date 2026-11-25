// SPDX-License-Identifier: GPL-2.0-only
// file by: 'Tr1zz'
// archivo hecho por: 'Tr1zz'


#include <types.h>
#include <util.h>
int main() {
    stdr::vector<char> casa;
    casa.push_back("Hola?\n", 7);
    stdr::cout << casa.data();
    for (int i = 0; i < casa.size(); i++) {
        stdr::cout << casa[i];
    }
    stdr::vector<int> espacio;
    espacio.push_back(1);
    int arreglo_importable[] = {6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
    espacio.push_back(arreglo_importable, 10);
    for (int i = 0; i < espacio.size(); i++) {
        stdr::cout << "Numero: " << espacio[i] << " \n";
    }
}
