#include <memory.hpp>
#include <utils.hpp>
#include <stdlib.h>
#include <list>

#if defined(_WIN64)
    #include <sysinfoapi.h>
#elif defined(__linux__)
    #include <unistd.h>
#else
    LOG("Else")
#endif

#define ASSUMED_PAGE_SIZE 4096

#if !defined(_WIN64) && !defined(__linux__)
    struct UnalignedPointerInfo
    {
        void *unalignedPtr;
        void *alignedPtr;
    };

    std::list<UnalignedPointerInfo> unalignedPointerInfos;
#endif

void* Memory::alignedMalloc(const size_t bytes, const size_t alignment)
{
    void* ptr = nullptr;
    // Rounding up to the alignment
    size_t allocSize = ((bytes + alignment - 1) / alignment) * alignment;
    #if defined(_WIN64)
    ptr = _aligned_malloc(allocSize, alignment);
    #elif defined(__linux__)
    ptr = aligned_alloc(pageSize, allocSize);
    #else

    // Create an aligned pointer from the potential unaligned malloc
    // Add it to a list of artificial aligned pointers
    // This is required to free them properly later
    UnalignedPointerInfo uapi;
    uapi.unalignedPtr = malloc(bytes + ASSUMED_PAGE_SIZE);
    // Align the pointer by rounding up to the closest page
    uapi.alignedPtr =  reinterpret_cast<void*>((reinterpret_cast<uint64_t>(uapi.unalignedPtr) + ASSUMED_PAGE_SIZE) & ~(ASSUMED_PAGE_SIZE - 1));
    unalignedPointerInfos.push_back(uapi);

    ptr = uapi.alignedPtr;
    #endif

    if(ptr == nullptr)
    {
        WARNING("Unable to allocate page aligned memory")
    }

    return ptr;
}

void* Memory::pageAlignedMalloc(const size_t bytes)
{
    int32_t pageSize = 0;
    #if defined(_WIN64)
        SYSTEM_INFO info;
        GetSystemInfo(&info);
        pageSize = info.dwPageSize;
    #elif defined(__linux__)
        pageSize = sysconf(_SC_PAGE_SIZE);
    #endif

    if(pageSize == 0)
    {
        pageSize = ASSUMED_PAGE_SIZE;
        WARNING("Page size not found, assuming page size of " << ASSUMED_PAGE_SIZE << " bytes")
    }

    return alignedMalloc(bytes, pageSize);
};

void Memory::alignedFree(void* ptr)
{
    #if defined(_WIN64)
    _aligned_free(ptr);
    #elif defined(__linux__)
    free(ptr);
    #else
    // Search in the list of unaliged pointers and see if the aligned version matches
    // If a match is found, the unaligned version is freed and removed from the list
    // If it is not found, the pointer is assumed to be aligned_allocated and freed using _aligned_free
    for(auto it = unalignedPointerInfos.begin(); it != unalignedPointerInfos.end(); it++)
    {
        if(it->alignedPtr == ptr)
        {
            free(it->unalignedPtr);
            unalignedPointerInfos.erase(it);
            return;
        }
    }
    #endif
}