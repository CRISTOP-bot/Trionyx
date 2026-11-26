// SPDX-License-Identifier: GPL-2.0-only
// file by: 'Tr1zz'
// archivo hecho por: 'Tr1zz'


#include "string.h"
#include "stream.h"
namespace stdr {
string::string(const char *a) {
    if (a == nullptr) {
        return;
    }
    while (_size < sizeof(_data) - 1 && a[_size] != '\0') {
        _data[_size] = a[_size];
        ++_size;
    }
    _data[_size] = '\0';
}
const char *string::c_str() const { return _data; }
size_t string::size() const { return _size; }
bool string::empty() const { return _size == 0; }
char &string::operator[](size_t index) { return _data[index]; }
const char &string::operator[](size_t index) const { return _data[index]; }
bool string::operator==(const char *other) const {
    if (other == nullptr) {
        return false;
    }
    size_t i = 0;
    while (i < _size && other[i] != '\0') {
        if (_data[i] != other[i]) {
            return false;
        }
        ++i;
    }
    return i == _size && other[i] == '\0';
}
} // namespace stdr
