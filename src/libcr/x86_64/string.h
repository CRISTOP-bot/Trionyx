#ifndef STRING_CR_H
#define STRING_CR_H
#include "stream.h"
namespace stdr {
class string {
  private:
    size_t _size{0};
    char _data[512];

  public:
    string(const char *a);
    [[nodiscard]] const char *c_str() const;
    [[nodiscard]] size_t size() const;
    [[nodiscard]] bool empty() const;
    char &operator[](size_t index);
    const char &operator[](size_t index) const;
    bool operator==(const char *other) const;
};
} // namespace stdr
#endif
