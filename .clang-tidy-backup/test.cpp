#include "bootchart.h"
#include <iostream>
#include <string>
int main(int argc, char **argv) {
    if (argc != 2)
        return 1;
    activity(argv[1]);
    std::cout << "espero haya salido bem \n";
    return 0;
}
