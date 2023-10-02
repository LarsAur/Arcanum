#pragma once

#include <stddef.h>

namespace Arcanum
{
    void* pageAlignedMalloc(size_t bytes);
    void pageAlignedFree(void* ptr);
}