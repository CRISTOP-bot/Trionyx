#include "memory.h"
#include "call.h"
#include "stddef.h"
#include "stream.h"
#include "string.h"
namespace stdr {
static int int_to_str(char *buf, size_t size, unsigned long long val, int base,
                      bool uppercase) {
    char tmp[65];
    int i = 0;
    const char *digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
    if (val == 0) {
        tmp[i++] = '0';
    } else {
        while (val > 0) {
            tmp[i++] = digits[val % base];
            val /= base;
        }
    }
    int len = i;
    int written = 0;
    while (i > 0) {
        if (written + 1 < (int)size) {
            buf[written] = tmp[i - 1];
        }
        written++;
        i--;
    }
    if (written < (int)size) {
        buf[written] = '\0';
    } else if (size > 0) {
        buf[size - 1] = '\0';
    }
    return len;
}
void *brk(void *addr) {
    return reinterpret_cast<void *>(
        rsyscall(SYS_brk, reinterpret_cast<long>(addr)));
}

int vsnrprintf(char *buf, size_t size, const char *fmt, va_list args) {
    size_t pos = 0;
    for (size_t i = 0; fmt[i] != '\0'; i++) {
        if (fmt[i] != '%') {
            if (pos + 1 < size) {
                buf[pos] = fmt[i];
            }
            pos++;
            continue;
        }
        i++;
        if (fmt[i] == '\0') {
            break;
        }

        switch (fmt[i]) {
        case 's': {
            const char *s = va_arg(args, const char *);
            if (s == nullptr) {
                s = "(null)";
            }
            for (int j = 0; s[j] != '\0'; j++) {
                if (pos + 1 < size) {
                    buf[pos] = s[j];
                }
                pos++;
            }
            break;
        }
        case 'd':
        case 'i': {
            long long val = va_arg(args, int);
            char num_buf[64];
            bool negative = false;
            if (val < 0) {
                negative = true;
                val = -val;
            }
            if (negative) {
                if (pos + 1 < size) {
                    buf[pos] = '-';
                }
                pos++;
            }
            int len = int_to_str(num_buf, sizeof(num_buf),
                                 (unsigned long long)val, 10, false);
            for (int j = 0; j < len; j++) {
                if (pos + 1 < size) {
                    buf[pos] = num_buf[j];
                }
                pos++;
            }
            break;
        }
        case 'u': {
            unsigned long long val = va_arg(args, unsigned int);
            char num_buf[64];
            int len = int_to_str(num_buf, sizeof(num_buf), val, 10, false);
            for (int j = 0; j < len; j++) {
                if (pos + 1 < size) {
                    buf[pos] = num_buf[j];
                }
                pos++;
            }
            break;
        }
        case 'x':
        case 'X': {
            unsigned long long val = va_arg(args, unsigned int);
            char num_buf[64];
            int len =
                int_to_str(num_buf, sizeof(num_buf), val, 16, fmt[i] == 'X');
            for (int j = 0; j < len; j++) {
                if (pos + 1 < size) {
                    buf[pos] = num_buf[j];
                }
                pos++;
            }
            break;
        }
        case 'p': {
            void *ptr = va_arg(args, void *);
            auto val = (unsigned long long)ptr;
            char num_buf[64];
            if (pos + 2 < size) {
                buf[pos] = '0';
                buf[pos + 1] = 'x';
            }
            pos += 2;
            int len = int_to_str(num_buf, sizeof(num_buf), val, 16, false);
            for (int j = 0; j < len; j++) {
                if (pos + 1 < size) {
                    buf[pos] = num_buf[j];
                }
                pos++;
            }
            break;
        }
        case 'c': {
            char c = (char)va_arg(args, int);
            if (pos + 1 < size) {
                buf[pos] = c;
            }
            pos++;
            break;
        }
        case '%': {
            if (pos + 1 < size) {
                buf[pos] = '%';
            }
            pos++;
            break;
        }
        default: {
            if (pos + 1 < size) {
                buf[pos] = '%';
            }
            pos++;
            if (pos + 1 < size) {
                buf[pos] = fmt[i];
            }
            pos++;
            break;
        }
        }
    }
    if (size > 0) {
        if (pos < size) {
            buf[pos] = '\0';
        } else {
            buf[size - 1] = '\0';
        }
    }
    return static_cast<int>(pos);
}
long read(int fd, void *buffer, size_t size) {
    return rsyscall(SYS_read, static_cast<long>(fd), buffer, size);
}
size_t write(int fd, const void *buffer, size_t size) {
    return rsyscall(SYS_write, fd, buffer, size);
}
void exit(int no) { rsyscall(SYS_exit, no); }
int open(const char *pathname, int flags, ...) {
    size_t mode = 0;
    if (((flags & O_CREAT) != 0) || ((flags & O_TMPFILE) == O_TMPFILE)) {
        __builtin_va_list args;
        __builtin_va_start(args, flags);
        mode = __builtin_va_arg(args, unsigned int);
        __builtin_va_end(args);
    }

    return static_cast<int>(
        rsyscall(SYS_openat, AT_FDCWD, pathname, flags, mode));
}
int close(int fd) { return static_cast<int>(rsyscall(SYS_close, fd)); }
int _libc_converge_strsize(const char *string) {
    if (string == nullptr) {
        return 0;
    }
    int md = 0;
    while (string[md] != '\0') {
        md++;
    }
    return md;
}
unsigned long long rprint(const char *string) {
    size_t len = 0;
    if (string != nullptr) {
        while (string[len] != '\0') {
            len++;
        }
    }
    size_t ret = write(1, string, len);
    return ret;
}
int rprintf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char b[1];
    int buffer_size = vsnrprintf(b, sizeof(b), fmt, args);
    va_end(args);
    if (buffer_size < 0) {
        return buffer_size;
    }
    size_t capacity = static_cast<size_t>(buffer_size) + 1;
    char *mem = static_cast<char *>(malloc(capacity));
    if (mem == nullptr) {
        return -1;
    }
    va_start(args, fmt);
    int len = vsnrprintf(mem, capacity, fmt, args);
    va_end(args);
    if (len > 0) {
        rprint(mem);
    }
    free(mem);
    return len;
}
int rscan(char *buffer, size_t bufferz) {
    int readed = read(0, buffer, bufferz);
    if (readed == 0) {
        return 0;
    }
    if (readed < 0) {
        return -1;
    }
    for (int i = 0; i < bufferz; i++) {
        if (buffer[i] == '\n') {
            buffer[i] = '\0';
        }
    }

    return readed;
}
void FILE::_chfd(int newfd) { _fd = newfd; }

int FILE::_fopen_activity_setfd(int fd) {
    if (_mod_count) {
        return -1;
    }
    if (_init) {
        return -1;
    }
    if (err) {
        return -1;
    }
    if (fd >= 0) {
        _fd = fd;
    } else {
        return -1;
    }
    _init = true;
    _mod_count = true;
    return 0;
}
int FILE::fswrite(int slot) {
    int a = -1;
    if (slot == 1) {
        if (_is_pw_pending) {
            _last_write = write(_fd, _pre_write, _pw_sz);
            a++;
            free(_pre_write);
            _is_pw_pending = false;
            return a + _last_write;
        } else {
            return -1;
        }
    } else if (slot == 2) {
        if (_is_pw2_pending) {
            _last_write = write(_fd, _pre_write_2, _pw2_sz);
            a++;
            free(_pre_write_2);
            _is_pw2_pending = false;
            return a + _last_write;
        } else {
            return -1;
        }
    } else {
        return -1;
    }
}
template <typename T> int FILE::fsave(T *msg, size_t size) {
    if (_is_pw_pending) {
        if (!_is_pw2_pending) {
            static_cast<T>(_pre_write_2) = malloc(sizeof(T) * size);
            if (_pre_write_2 == nullptr) {
                return -1;
            }
            mem_copy(_pre_write_2, msg, sizeof(T) * size);
            _is_pw2_pending = true;
            _pw2_sz = sizeof(T) * size;
            return 0;
        } else {
            return -1;
        }
    } else {
        static_cast<T>(_pre_write) = malloc(sizeof(T) * size);
        if (_pre_write == nullptr) {
            return -1;
        }
        mem_copy(_pre_write, msg, sizeof(T) * size);
        _is_pw_pending = true;
        _pw_sz = sizeof(T) * size;
        return 0;
    }
}
template <typename T> int FILE::writeft(T *nonpointer, size_t size) {
    if (!check()) {
        return -1;
    }
    if (nonpointer == nullptr) {
        return -1;
    }
    if (size > 12000) {
        return -1;
    }

    if (!writed) {
        writed = true;
    }
    if (_was_write_err) {
        _was_write_err = false;
    }
    _last_write = write(_fd, nonpointer, size);
    return _last_write;
}
fileerr_t FILE::_check_sane() {
    fileerr_t ret;
    if (!check()) {
        ret.if_error = err;
        ret.invalid_structure = true;
        return ret;
    }
    if (writed) {
        ret.last_write = _last_write;
    }
    if (readed) {
        ret.last_read = _last_read;
    }
    ret.fd = _fd;
    return ret;
}
[[nodiscard]] bool FILE::check() const {
    return _init && !err && _mod_count && _fd >= 0;
}
void FILE::_method_internal_fclosef() {
    if (_fd >= 0) {
        fswrite(1);
        fswrite(2);
    }
    fswrite(1);
    fswrite(2);
    close(_fd);
}

void fclose(FILE *f) {
    f->_method_internal_fclosef();
    free(f);
}
FILE *fopen(const char *path, const char *mod) {
    string mode = mod;
    FILE *_int = static_cast<FILE *>(malloc(sizeof(FILE)));
    if (_int == nullptr) {
        return nullptr;
    }
    if (mode == "r") {
        int cfd = open(path, O_RDONLY);
        int mskfd = _int->_fopen_activity_setfd(cfd);
        if (mskfd != 0) {
            if (cfd >= 0) {
                close(cfd);
            }
            free(_int);
            return nullptr;
        }
    } else if (mode == "w") {
        int cfd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0655);
        int mskfd = _int->_fopen_activity_setfd(cfd);
        if (mskfd != 0) {
            if (cfd >= 0) {
                close(cfd);
            }
            free(_int);
            return nullptr;
        }
    } else if (mode == "a") {
        int cfd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0655);
        int mskfd = _int->_fopen_activity_setfd(cfd);
        if (mskfd != 0) {
            if (cfd >= 0) {
                close(cfd);
            }
            free(_int);
            return nullptr;
        }
    } else if (mode == "r+") {
        int cfd = open(path, O_RDWR);
        int mskfd = _int->_fopen_activity_setfd(cfd);
        if (mskfd != 0) {
            if (cfd >= 0) {
                close(cfd);
            }
            free(_int);
            return nullptr;
        }
    } else if (mode == "w+") {
        int cfd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0655);
        int mskfd = _int->_fopen_activity_setfd(cfd);
        if (mskfd != 0) {
            if (cfd >= 0) {
                close(cfd);
            }
            free(_int);
            return nullptr;
        }
    } else if (mode == "a+") {
        int cfd = open(path, O_RDWR | O_CREAT | O_APPEND, 0655);
        int mskfd = _int->_fopen_activity_setfd(cfd);
        if (mskfd != 0) {
            if (cfd >= 0) {
                close(cfd);
            }
            free(_int);
            return nullptr;
        }
    } else {
        free(_int);
        return nullptr;
    }
    return _int;
}
int fwritef(FILE *f, void *a, size_t b) {
    if (f == nullptr) {
        return 1;
    }
    return f->writeft(a, b);
}
int frprintf(FILE *f, const char *fmt, ...) {
    if (f == nullptr) {
        return -1;
    }
    char b[1];
    va_list fakearg;
    va_start(fakearg, fmt);
    int sz = vsnrprintf(b, sizeof(b), fmt, fakearg);
    va_end(fakearg);
    if (sz < 0) {
        return -1;
    }
    sz++;
    va_list args;
    va_start(args, fmt);
    char *buffer = static_cast<char *>(malloc(static_cast<size_t>(sz)));
    if (buffer == nullptr) {
        va_end(args);
        return -1;
    }
    int len = vsnrprintf(buffer, sz, fmt, args);
    if (len < 0) {
        va_end(args);
        free(buffer);
        return -1;
    }
    f->writeft(buffer, len);
    va_end(args);
    free(buffer);
    return len;
}

// SYSCALLS
int getpid() { return static_cast<int>(rsyscall(SYS_getpid)); }
long io_setup(unsigned nr_events, unsigned long *ctx) {
    return rsyscall(SYS_io_setup, nr_events, ctx);
}
long io_destroy(unsigned long ctx) { return rsyscall(SYS_io_destroy, ctx); }
void exit_group(int status) { rsyscall(SYS_exit_group, status); }
void *mmap(void *addr, size_t length, int prot, int flags, int fd,
           long offset) {
    long ret = rsyscall(SYS_mmap, addr, length, prot, flags, fd, offset);
    if (ret < 0 && ret >= -4095) {
        return reinterpret_cast<void *>(-1L);
    }
    return reinterpret_cast<void *>(ret);
}
int gettid() { return static_cast<int>(rsyscall(SYS_gettid)); }
long munmap(void *addr, unsigned long length) {
    return rsyscall(SYS_munmap, addr, length);
}
int fstat(int fd, void *statbuf) {
    return static_cast<int>(rsyscall(SYS_fstat, fd, statbuf));
}
int newfstatat(int dirfd, const char *pathname, void *statbuf, int flags) {
    return static_cast<int>(
        rsyscall(SYS_newfstatat, dirfd, pathname, statbuf, flags));
}
// additional kit
bool streq(const char *str1, const char *str2) {
    int i = 0;
    while (str1[i] != '\0' && str2[i] != '\0') {
        if (str1[i] != str2[i]) {
            return false;
        }
        i++;
    }
    return str1[i] == str2[i];
}
char *char_malloc(size_t size) {
    void *mem = malloc(size);
    return static_cast<char *>(mem);
}
int fdrprintf(int fd, const char *fmt, ...) {}
} // namespace stdr
