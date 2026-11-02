#ifndef MEMORY_H
#define MEMORY_H
#include <stddef.h>
namespace stdr {
void free(void *ptr);
void *malloc(size_t size);
void mem_copy(void* dest, const void* src, size_t n);
} // namespace stdr
#endif
