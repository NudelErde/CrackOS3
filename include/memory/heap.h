#pragma once
#include "features/optional.h"
#include "memory/mem.h"
#include "memory/paging.h"
#include "test/test.h"

namespace kheap {
void init();
void* malloc(size_t size);
void free(void* ptr);
optional<size_t> get_size(void* ptr);
Test::Result test();
}// namespace kheap

inline void* kmalloc(size_t size) { return kheap::malloc(size); }
inline void kfree(void* ptr) { kheap::free(ptr); }
inline optional<size_t> ksize(void* ptr) { return kheap::get_size(ptr); }

void* operator new(size_t size);
void* operator new[](size_t size);
void operator delete(void* ptr) noexcept;
void operator delete[](void* ptr) noexcept;
void operator delete(void* ptr, size_t) noexcept;
void operator delete[](void* ptr, size_t) noexcept;