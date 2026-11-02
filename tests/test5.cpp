#include <iostream>
#include <util.h>
int main() {
    std::vector<std::string> test = stdr::tok("H$o$l$a$,$ $mun$do!\n", '$');
    for (std::string s : test) {
        std::cout << s << "\n";
    }
}
