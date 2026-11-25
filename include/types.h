#ifndef PRINTK
#define PRINTK
#include <builtin.h>
#include <cerrno>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <fcntl.h>
#include <format>
#include <string>
#include <string_view>
#include <unistd.h>
constexpr int five = 5;
constexpr int string_buffer_len = 128;
#include <lvector>
#include <vector>
enum class severity : int8_t {
    emerg = 0,
    alert = 1,
    crit = 2,
    error = 3,
    warning = 4,
    notice = 5,
    info = 6,
    debug = 7
};
class fsh {
  public:
    int uses;
};

namespace stdr {

class out {
  private:
    int fd;
    std::string buffer;

    static int open_path(const std::string &path) noexcept {
        if (path.find('\0') != std::string::npos) {
            errno = EINVAL;
            return -1;
        }
        return open(path.c_str(), O_WRONLY | O_APPEND);
    }

  public:
    out() { fd = open("/dev/kmsg", O_WRONLY | O_APPEND); }
    out(int a) : fd(a) {}
    out(const char *kmsg) : fd(open(kmsg, O_WRONLY | O_APPEND)) {}
    explicit out(const std::string &kmsg) : fd(open_path(kmsg)) {}
    [[nodiscard]] bool verify() const { return fd >= 0; }
    friend void closefd(stdr::out &obj);
    int vn(int8_t severity, const char *fmt, va_list args) const {
        std::string for_use;
        va_list len;
        va_copy(len, args);
        int lan = vsnprintf(nullptr, 0, fmt, len);
        va_end(len);
        if (lan < 0) {
            return -1;
        }
        auto length = static_cast<size_t>(lan);
        std::vector<char> buffer(length + 1);

        va_list argz;
        va_copy(argz, args);
        int rlen = vsnprintf(buffer.data(), buffer.size(), fmt, argz);
        va_end(argz);
        if (rlen < 0) {
            return -1;
        }
        buffer[length] = '\0';
        for_use = std::format("<{}>", severity);
        for_use.reserve(for_use.size() + length);
        for_use.append(buffer.data(), length);
        return static_cast<int>(
            builtin::write_all(fd, for_use.data(), for_use.size()));
    }
    out &operator<<(std::string_view value) {
        buffer.append(value);
        if (buffer.find('\n') != std::string::npos ||
            buffer.size() > string_buffer_len) {
            builtin::write_all(fd, buffer.data(), buffer.size());
            buffer.clear();
            buffer.reserve(string_buffer_len);
        }
        return *this;
    }
    out &operator<<(const std::string &value) {
        return *this << std::string_view(value);
    }
    out &operator<<(char c) {
        buffer.append(1, c);
        if (c == '\n' || buffer.size() > string_buffer_len) {
            builtin::write_all(fd, buffer.data(), buffer.size());
            buffer.clear();
            buffer.reserve(string_buffer_len);
        }
        return *this;
    }
    out &operator=(int other_fd) {
        fd = other_fd;
        return *this;
    }
    out &operator<<(fsh endl) {
        endl.uses++;
        builtin::write_all(fd, buffer.data(), buffer.size());
        buffer.clear();
        buffer.reserve(string_buffer_len);
        return *this;
    }
    friend int oflush(out &o);
    out &operator<<(int no) { return *this << std::format("{}", no); }
    out &operator<<(const char *str) { return *this << std::string_view(str); }
    out &operator<<(long no) { return *this << std::format("{}", no); }
    out &operator<<(unsigned long no) { return *this << std::format("{}", no); }
    out &operator<<(unsigned int no) { return *this << std::format("{}", no); }
    out &operator<<(double no) { return *this << std::format("{}", no); }
    out &operator<<(float no) { return *this << std::format("{}", no); }
    out &operator<<(bool no) { return *this << std::format("{}", no); }
    out &operator<<(long long no) { return *this << std::format("{}", no); }
    out &operator<<(unsigned long long no) {
        return *this << std::format("{}", no);
    }

    out &operator<<(short no) { return *this << std::format("{}", no); }
    out &operator<<(unsigned short no) {
        return *this << std::format("{}", no);
    }
    out &operator<<(char *str) { return *this << std::string_view(str); }
    out &operator<<(void *anyptr) {
        if (anyptr == nullptr) {
            return *this << std::string_view("(nil)");
        }
        return *this << std::format("{}", anyptr);
    }
    out &operator<<(const void *anyptr) {
        if (anyptr == nullptr) {
            return *this << std::string_view("(nil)");
        }
        return *this << std::format("{}", anyptr);
    }

    int operator()(int8_t severity, const char *fmt, ...) const {
        va_list args;
        va_start(args, fmt);
        int ret = vn(severity, fmt, args);
        va_end(args);
        return ret;
    }

    template <typename... Args>
    void operator()(const char *fmt, Args &&...args) {
        std::string formatted =
            std::vformat(std::string_view(fmt), std::make_format_args(args...));

        buffer.append(formatted);

        if (buffer.find('\n') != std::string::npos ||
            buffer.size() > string_buffer_len) {
            builtin::write_all(fd, buffer.data(), buffer.size());
            buffer.clear();
            buffer.reserve(string_buffer_len);
        }
    }

    ~out() {
        builtin::write_all(fd, buffer.data(), buffer.size());
        buffer.clear();
        close(fd);
    }
};
void closefd(stdr::out &obj) { close(obj.fd); }
int oflush(out &o) {
    builtin::write_all(o.fd, o.buffer.data(), o.buffer.size());
    o.buffer.clear();
    o.buffer.reserve(string_buffer_len);
    return 0;
}
class nil_t {
  private:
    void *null = nullptr;

  public:
    nil_t() = default;
    ~nil_t() { null = (void *)1; }
};

nil_t nil;
extern out kout;
extern out cout;
extern out cerr;
fsh endl;
template <typename Tp> bool is_in(std::vector<Tp> &vec, const Tp &tofind) {
    for (Tp item : vec) {
        if (item == tofind) {
            return true;
        }
    }
    return false;
}
} // namespace stdr
int printk(int8_t severity, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int ret = stdr::kout.vn(severity, fmt, args);
    va_end(args);
    return ret;
}
int pr_info(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int ret = stdr::kout.vn(static_cast<int8_t>(severity::info), fmt, args);
    va_end(args);
    return ret;
}
int pr_err(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int ret = stdr::kout.vn(static_cast<int8_t>(severity::error), fmt, args);
    va_end(args);
    return ret;
}
int pr_warn(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int ret = stdr::kout.vn(static_cast<int8_t>(severity::warning), fmt, args);
    va_end(args);
    return ret;
}
int pr_debug(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int ret = stdr::kout.vn(static_cast<int8_t>(severity::debug), fmt, args);
    va_end(args);
    return ret;
}
int pr_notice(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int ret = stdr::kout.vn(static_cast<int8_t>(severity::notice), fmt, args);
    va_end(args);
    return ret;
}
int pr_crit(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int ret = stdr::kout.vn(static_cast<int8_t>(severity::crit), fmt, args);
    va_end(args);
    return ret;
}
int pr_alert(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int ret = stdr::kout.vn(static_cast<int8_t>(severity::alert), fmt, args);
    va_end(args);
    return ret;
}
int pr_emerg(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int ret = stdr::kout.vn(static_cast<int8_t>(severity::emerg), fmt, args);
    va_end(args);
    return ret;
}

#endif
