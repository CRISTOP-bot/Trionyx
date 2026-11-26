// SPDX-License-Identifier: GPL-2.0-only
// file by: 'Tr1zz'
// archivo hecho por: 'Tr1zz'
#ifndef BUILTINNN
#define BUILTINNN
#include <unistd.h>
namespace builtin {
ssize_t write_all(int fd, const void *buffer, size_t count);
ssize_t read_all(int fd, void *buffer, size_t count);
} // namespace builtin
#endif
