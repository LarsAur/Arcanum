#include <memory.hpp>
#include <utils.hpp>
#include <stdlib.h>
#include <transpositionTable.hpp>
#include <inttypes.h>
#include <list>

#if defined(_WIN64)
    #include <sysinfoapi.h>
#elif defined(__linux__)
    #include <unistd.h>
#else
    LOG("Else")
#endif

#define ASSUMED_PAGE_SIZE 1024

struct UnalignedPointerInfo
{
    void *unalignedPtr;
    void *alignedPtr;
};

std::list<UnalignedPointerInfo> unalignedPointerInfos;

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
        // Create an aligned pointer from the potential unaligned malloc
        // Add it to a list of artificial aligned pointers
        // This is required to free them properly later
        UnalignedPointerInfo uapi;
        uapi.unalignedPtr = malloc(bytes + ASSUMED_PAGE_SIZE);
        // Align the pointer by rounding up to the closest page
        uapi.alignedPtr =  reinterpret_cast<void*>((reinterpret_cast<uint64_t>(uapi.unalignedPtr) + ASSUMED_PAGE_SIZE) & ~(ASSUMED_PAGE_SIZE - 1));
        unalignedPointerInfos.push_back(uapi);

        DEBUG(uapi.unalignedPtr)
        DEBUG(uapi.alignedPtr)

        ptr = uapi.alignedPtr;
        WARNING("Page size not found, assuming page size of " << ASSUMED_PAGE_SIZE << " bytes")
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
    // Search in the list of unaliged pointers and see if the aligned version matches
    // If a match is found, the unaligned version is freed and removed from the list
    // If it is not found, the pointer is assumed to be aligned_allocated and freed using _aligned_free
    for(auto it = unalignedPointerInfos.begin(); it != unalignedPointerInfos.end(); it++)
    {
        if(it->alignedPtr == ptr)
        {
            DEBUG(it->unalignedPtr)
            DEBUG(it->alignedPtr)
            free(it->unalignedPtr);
            unalignedPointerInfos.erase(it);
            return;
        }
    }

    _aligned_free(ptr);
}