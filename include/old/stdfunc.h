#ifndef STDFUNC_H
#define STDFUNC_H
auto eprintf(const char *fmt, ...) -> int;
auto writefd(int fd, const char *fmt, ...) -> int;
#endif
