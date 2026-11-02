#ifndef BUILTINNN
#define BUILTINNN
#include <unistd.h>
namespace builtin {
ssize_t write_all(int fd, const void *buffer, size_t count);
ssize_t read_all(int fd, void *buffer, size_t count);
} // namespace builtin
#endif
