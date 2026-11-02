#ifndef PROCESS_H
#define PROCESS_H
namespace stdr {
int execve(const char *path, char *const *argv, char *const *envp);
}
#endif
