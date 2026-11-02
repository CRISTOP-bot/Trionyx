#include "init.hpp"
#include <iostream>
#include <string>
auto main(int argc, char **argv) -> int {
    if (argc != 2)
        return 1;
    activity(argv[1]);
    std::cout << "espero haya salido bem \n";
    return 0;
}
