// Простая локальная замена распределённого отдельно allocations_checker.
// Подсчитывает вызовы обычных operator new/delete (без специальных aligned new).
#include "allocations_checker.h"
#include <atomic>
#include <cstdlib>
#include <new>
static std::atomic<int64_t> g_allocations{0};
static std::atomic<int64_t> g_deallocations{0};
namespace alloc_checker {
void ResetCounters() { g_allocations.store(0); g_deallocations.store(0); }
int64_t AllocCount() { return g_allocations.load(); }
int64_t DeallocCount() { return g_deallocations.load(); }
}
void* operator new(std::size_t n) {
    if (void* p = std::malloc(n ? n : 1)) { ++g_allocations; return p; }
    throw std::bad_alloc();
}
void* operator new[](std::size_t n) {
    if (void* p = std::malloc(n ? n : 1)) { ++g_allocations; return p; }
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept { if (p) { ++g_deallocations; std::free(p); } }
void operator delete[](void* p) noexcept { if (p) { ++g_deallocations; std::free(p); } }
void operator delete(void* p, std::size_t) noexcept { if (p) { ++g_deallocations; std::free(p); } }
void operator delete[](void* p, std::size_t) noexcept { if (p) { ++g_deallocations; std::free(p); } }
