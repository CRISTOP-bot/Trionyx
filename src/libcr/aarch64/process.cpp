#include "process.h"
#include "stream.h"
int execve(const char *path, char *const *argv, char *const *envp) {
    return rsyscall(SYS_execve, path, argv, envp);
}
