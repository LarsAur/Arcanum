#pragma once

#include <stddef.h>

namespace Memory
{
    void* alignedMalloc(const size_t bytes, const size_t alignment);
    void* pageAlignedMalloc(const size_t bytes);
    void alignedFree(void* ptr);
}