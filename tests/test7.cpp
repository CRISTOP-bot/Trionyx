// SPDX-License-Identifier: GPL-2.0-only
// file by: 'Tr1zz'
// archivo hecho por: 'Tr1zz'


#include <types.h>
#include <istdlib.h>
#include <c++>
int main()
{
    stdr::advanced_string hola = "Hola pinche mundo\n";
    stdr::out salida_estandar = 1;
    salida_estandar << hola.c_str();
    hola.append("Hola segunda linea\n");   
    salida_estandar << hola.c_str();
    salida_estandar << stdr::endl;
    return 0;
}
