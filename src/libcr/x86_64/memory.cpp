#include "stream.h"
#include <stddef.h>
namespace stdr {
struct Header {
    size_t size;
    bool is_free;
    Header *next;
};
#define ALIGN8(size) (((size) + 7) & ~7)
#define HEADER_SIZE sizeof(Header)
static Header *heap_head = nullptr;
static Header *heap_tail = nullptr;
static Header *req(Header *last, size_t size) {
    void *current_brk = stdr::brk(nullptr);
    size_t total_size = ALIGN8(size + HEADER_SIZE);
    void *new_brk = stdr::brk(static_cast<char *>(current_brk) + total_size);
    if (new_brk == current_brk) {
        return nullptr;
    }
    auto *header = static_cast<Header *>(current_brk);
    header->size = size;
    header->is_free = false;
    header->next = nullptr;
    if (last != nullptr) {
        last->next = header;
    }
    return header;
}
void *malloc(size_t size) {
    if (size == 0) {
        return nullptr;
    }
    size_t aligned_size = ALIGN8(size);
    Header *current = heap_head;
    while (current != nullptr) {
        if (current->is_free && current->size >= aligned_size) {
            current->is_free = false;
            return static_cast<void *>(current + 1);
        }
        current = current->next;
    }
    Header *new_block = nullptr;
    if (heap_head == nullptr) {
        new_block = req(nullptr, aligned_size);
        if (new_block == nullptr) {
            return nullptr;
        }
        heap_head = new_block;
    } else {
        new_block = req(heap_tail, aligned_size);
        if (new_block == nullptr) {
            return nullptr;
        }
    }
    heap_tail = new_block;
    return static_cast<void *>(new_block + 1);
}
void free(void *ptr) {
    if (ptr == nullptr) {
        return;
    }
    Header *header = static_cast<Header *>(ptr) - 1;
    header->is_free = true;
}
void mem_copy(void* dest, const void* src, size_t n) {
    char* d = static_cast<char*>(dest);
    const char* s = static_cast<const char*>(src);
    for (size_t i = 0; i < n; ++i) {
        d[i] = s[i];
    }
}
} // namespace stdr
