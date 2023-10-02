#include <memory.hpp>
#include <stddef.h>
#include <inttypes.h>
#include <utils.hpp>
#include <stdlib.h>

#if defined(_WIN64)
    #include <sysinfoapi.h>
#elif defined(__linux__)
    #include <unistd.h>
#else
    LOG("Else")
#endif

void* Arcanum::pageAlignedMalloc(size_t bytes)
{
    int32_t pageSize = 0;
    #if defined(_WIN64)
        SYSTEM_INFO info;
        GetSystemInfo(&info);
        pageSize = info.dwPageSize;
    #elif defined(__linux__)
        pageSize = sysconf(_SC_PAGE_SIZE);
    #endif

    void* ptr = nullptr;
    if(pageSize == 0)
    {
        ptr = malloc(bytes);
        uint64_t ptrWithInfo = (uint64_t) ptr | 0x1;
        ptr = (void*) ptrWithInfo; // Add info to the pointer to indicate if it is aligned or not
        DEBUG(ptr)
        WARNING("Page size not found, might be unable to allocate page aligned memory")
    }
    else
    {
        // Rounding up to the pagesize
        size_t allocSize = ((bytes + pageSize - 1) / pageSize) * pageSize;
        ptr = _aligned_malloc(allocSize, pageSize);
    }

    if(ptr == nullptr)
    {
        WARNING("Unable to allocate page aligned memory")
    }

    return ptr;
};

void Arcanum::pageAlignedFree(void* ptr)
{
    if((uint64_t) ptr & 0x1)
    {
        uint64_t ptrWithoutInfo = (uint64_t) ptr & ~0x1;
        ptr = (void*) ptrWithoutInfo; // Remove the additional info in the two lowest bits
        free(ptr);
    }
    else
        _aligned_free(ptr);
}