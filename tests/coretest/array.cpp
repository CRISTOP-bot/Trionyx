#include <types.h>
int main() {
    stdr::out kout;
    stdr::out cout = 1;
    stdr::out cerr, klog;
    int arr[] = {1, 2, 3, 4, 5};
    int *anyptr = arr;
    for (int i = 0;; anyptr++, i++) {
        stdr::cout << *anyptr << "\n";
        *anyptr = 0;
    }
}
