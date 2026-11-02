#ifndef BUILTINNN
#define BUILTINNN
#include <cerrno>
#include <unistd.h>
namespace builtin {
ssize_t write_all(int fd, const void *buffer, size_t count) {
    const char *ptr = static_cast<const char *>(buffer);
    size_t total = 0;
    while (total < count) {
        ssize_t n = write(fd, ptr + total, count - total);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (n == 0) {
            break;
        }
        total += static_cast<size_t>(n);
    }
    return static_cast<ssize_t>(total);
}

ssize_t read_all(int fd, void *buffer, size_t count) {
    char *ptr = static_cast<char *>(buffer);
    size_t total = 0;
    while (total < count) {
        ssize_t n = read(fd, ptr + total, count - total);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (n == 0) {
            break;
        }
        total += static_cast<size_t>(n);
    }
    return static_cast<ssize_t>(total);
}
} // namespace builtin
#endif
