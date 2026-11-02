#pragma once
#include <cstdint>
namespace alloc_checker {
void ResetCounters();
int64_t AllocCount();
int64_t DeallocCount();
}
