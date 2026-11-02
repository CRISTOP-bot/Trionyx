#ifndef IO_H
#define IO_H
enum {
    O_RDONLY = 00000000,
    O_WRONLY = 00000001,
    O_RDWR = 00000002,
    O_CREAT = 00000100,
    O_EXCL = 00000200,
    O_NOCTTY = 00000400,
    O_TRUNC = 00001000,
    O_APPEND = 00002000,
    O_NONBLOCK = 00004000,
    O_DIRECTORY = 00200000
};
#define O_TMPFILE (02000000 | O_DIRECTORY)
#define AT_FDCWD (-100)
#ifdef __cplusplus
extern "C" {
#endif
#include <stdarg.h>
#include <stddef.h>
namespace stdr {
size_t write(int fd, const void *buffer, size_t size);
int open(const char *pathname, int flags, ...);
int close(int fd);
unsigned long long rprint(const char *string);
long read(int fd, void *buffer, size_t size);
int _libc_converge_strsize(const char *string);
void exit(int no);
int rprintf(const char *fmt, ...);
int vsnrprintf(char *buf, size_t size, const char *fmt, va_list args);
} // namespace stdr
#ifdef __cplusplus
}
#endif
#endif
