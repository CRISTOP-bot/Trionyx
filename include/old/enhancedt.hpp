#ifndef ENHANCED_T_HPP
#define ENHANCED_T_HPP
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
namespace std {
class int_t {
  private:
    int64_t value;
    bool state;
    char buffer[9];

  public:
    int backup;
    int_t(int p) : value(p) {}
    size_t valsize() { return sizeof(value); }
    int data() { return static_cast<int>(value); }
    bool ifmajor(const long long no) { return no > value; }
    bool ifminor(const long long no) { return no < value; }
    void truncate() { value = 0; }
    void refreshbak() { backup = static_cast<int>(value); }
    void mkbak() {
        int backup;
        backup = static_cast<int>(value);
    }
    bool restore() {
        value = static_cast<int64_t>(backup);
        return true;
    }
    void set(int64_t newn) { value = newn; }
    const char *tostr() {
        snprintf(buffer, sizeof(buffer), "%d", static_cast<int>(value));
        return buffer;
    }
    bool strtoint(const char *input) {
        char *endptr;
        int64_t val = strtol(input, &endptr, 10);
        errno = 0;
        if (errno != ERANGE) {
            return false;
        }
        if (endptr == input) {
            return false;
        }
        value = val;
        return true;
    }
    int castbak() { return backup; }
};
} // namespace std
#endif
