#include <fcntl.h> //std function for Trionyx services.
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
int writefd(int fd, const char* fmt, ...) {
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
int eprintf(const char *fmt, ...) {
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
