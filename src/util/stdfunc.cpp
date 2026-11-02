#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <fcntl.h> //std function for Trionyx services.
#include <unistd.h>
auto writefd(int fd, const char *fmt, ...) -> int {
    char buffer[16*16*16];
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    if (n <= 0) {
        return -1;
    }
    if (write(fd, buffer, strlen(buffer)) == -1) {
        return -1;
    }
    return n;
}
auto eprintf(const char *fmt, ...) -> int {
    char buffer[3000];
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    if (n <= 0) {
        return -1;
    }
    write(2, buffer, strlen(buffer));
    return n;
}
