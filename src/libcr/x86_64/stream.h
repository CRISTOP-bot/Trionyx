#ifndef STDIO_H
#define STDIO_H
#include "call.h"
#include "iofile.h"
#include <stddef.h>
namespace stdr {
void *brk(void *addr);
struct fileerr_t {
    int last_read;
    int last_write;
    int fd;
    bool if_error;
    bool invalid_structure;
};

class FILE {
  private:
    friend FILE *fopen(const char *path, const char *mod);

    int _fd;
    bool err;

    int _last_write;
    int _last_read;
    int _get_counter;
    [[nodiscard]] bool check() const;
    bool _mod_count;
    bool _init;
    bool _was_write_err;
    bool writed;
    bool readed;
    void *_pre_write;
    bool _is_pw_pending;
    void *_pre_write_2;
    bool _is_pw2_pending;
    size_t _pw_sz;
    size_t _pw2_sz;
    void _chfd(int newfd);
  public:
    FILE(const FILE &) = delete;
    FILE &operator=(const FILE &) = delete;

    int _fopen_activity_setfd(int fd);
    int fswrite(int slot);

    template <typename T> int fsave(T *msg, size_t size);

    template <typename T> int writeft(T *data, size_t size);

    fileerr_t _check_sane();
    void _method_internal_fclosef();
};

FILE *fopen(const char *path, const char *mod);
void fclose(FILE *f);
int frprintf(FILE *f, const char *fmt, ...);
int rscan(char *buffer, size_t bufferz);
int getpid();
long io_setup(unsigned nr_events, unsigned long *ctx);
long io_destroy(unsigned long ctx);
void exit_group(int status);
void *mmap(void *addr, size_t length, int prot, int flags, int fd,
           long offset);
int gettid();
long munmap(void *addr, unsigned long length);
int fstat(int fd, void *statbuf);
int newfstatat(int dirfd, const char *pathname, void *statbuf, int flags);
// additional kit
bool streq(const char *str1, const char *str2);
char *char_malloc(size_t size);
} // namespace stdr
#endif
