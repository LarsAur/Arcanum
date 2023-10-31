#pragma once

#include <stddef.h>

namespace Memory
{
    void* pageAlignedMalloc(size_t bytes);
    void pageAlignedFree(void* ptr);
}