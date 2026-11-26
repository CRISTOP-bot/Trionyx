// SPDX-License-Identifier: GPL-2.0-only
// file by: 'Tr1zz'
// archivo hecho por: 'Tr1zz'


#include "process.h"
#include "stream.h"
int execve(const char *path, char *const *argv, char *const *envp) {
    return rsyscall(SYS_execve, path, argv, envp);
}
