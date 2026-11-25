// SPDX-License-Identifier: GPL-2.0-only
// file by: 'Tr1zz'
// archivo hecho por: 'Tr1zz'


#ifndef ISTDLIB_H
#define ISTDLIB_H
#include <lvector>
#include <types.h>

namespace stdr {
class advanced_string {
  private:
    char *data = nullptr;
    size_t data_size = 0;
    constexpr static const int sixteen = 16;
    constexpr static int MAX_C_STR_LEN = 1 << sixteen;
    constexpr static int DEFAULT_SIZE = 32;

    void privalloc(size_t size) {
        data = new char[size]{};
        data_size = size;
    }

    size_t cstrlen(const char *c_str) const {
        size_t size = 0;
        while (size < MAX_C_STR_LEN && c_str[size] != '\0') {
            ++size;
        }
        return size;
    }

    void realloc(size_t newsize) {
        char *tmp = new char[newsize]{};
        delete[] data;
        data = tmp;
        data_size = newsize;
    }

    void extract_and_dump(const char *addr) {
        size_t len = cstrlen(addr);
        char *tmp = new char[len + 1]{};

        for (size_t i = 0; i < len; ++i) {
            tmp[i] = addr[i];
        }

        delete[] data;
        data = tmp;
        data_size = len + 1;
    }

  public:
    advanced_string() { privalloc(DEFAULT_SIZE); }

    advanced_string(const char *c_str) { extract_and_dump(c_str); }

    advanced_string(const std::string &another) {
        extract_and_dump(another.c_str());
    }

    advanced_string(const advanced_string &another) {
        extract_and_dump(another.data);
    }

    advanced_string(advanced_string &&another) noexcept
        : data(another.data), data_size(another.data_size) {
        another.data = nullptr;
        another.data_size = 0;
    }

    advanced_string &operator=(const advanced_string &another) {
        if (this != &another) {
            extract_and_dump(another.data);
        }
        return *this;
    }

    advanced_string &operator=(advanced_string &&another) noexcept {
        if (this != &another) {
            delete[] data;
            data = another.data;
            data_size = another.data_size;

            another.data = nullptr;
            another.data_size = 0;
        }
        return *this;
    }

    ~advanced_string() { delete[] data; }

    char *begin() { return data; }

    char *end() { return data ? data + data_size - 1 : nullptr; }

    const char *cbegin() const { return data; }

    const char *cend() const { return data ? data + data_size - 1 : nullptr; }

    [[nodiscard]] size_t size() const { return data_size ? data_size - 1 : 0; }

    bool starts_with(const char *with) const {
        size_t len = cstrlen(with);

        if (len > size())
            return false;

        for (size_t i = 0; i < len; ++i) {
            if (data[i] != with[i])
                return false;
        }

        return true;
    }

    int trim(char c) { char *tmpob }
};
} // namespace stdr

#endif
