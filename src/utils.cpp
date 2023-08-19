#include <utils.hpp>

/// aligned_large_pages_alloc() will return suitably aligned memory, if possible using large pages.
// This is copied from https://github.dev/official-stockfish/Stockfish/blob/master/src/tt.cpp

#ifdef _WIN32
#if _WIN32_WINNT < 0x0601
#undef  _WIN32_WINNT
#define _WIN32_WINNT 0x0601 // Force to include needed API prototypes
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
// The needed Windows API for processor groups could be missed from old Windows
// versions, so instead of calling them directly (forcing the linker to resolve
// the calls at compile time), try to load them at runtime. To do this we need
// first to define the corresponding function pointers.
extern "C" {
using fun1_t = bool(*)(LOGICAL_PROCESSOR_RELATIONSHIP,
                       PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX, PDWORD);
using fun2_t = bool(*)(USHORT, PGROUP_AFFINITY);
using fun3_t = bool(*)(HANDLE, CONST GROUP_AFFINITY*, PGROUP_AFFINITY);
using fun4_t = bool(*)(USHORT, PGROUP_AFFINITY, USHORT, PUSHORT);
using fun5_t = WORD(*)();
using fun6_t = bool(*)(HANDLE, DWORD, PHANDLE);
using fun7_t = bool(*)(LPCSTR, LPCSTR, PLUID);
using fun8_t = bool(*)(HANDLE, BOOL, PTOKEN_PRIVILEGES, DWORD, PTOKEN_PRIVILEGES, PDWORD);
}
#endif

#if defined(__linux__) && !defined(__ANDROID__)
#include <stdlib.h>
#include <sys/mman.h>
#endif

#if defined(_WIN32)

static void* aligned_large_pages_alloc_windows([[maybe_unused]] size_t allocSize) {

  #if !defined(_WIN64)
    return nullptr;
  #else

  HANDLE hProcessToken { };
  LUID luid { };
  void* mem = nullptr;

  const size_t largePageSize = GetLargePageMinimum();
  if (!largePageSize)
      return nullptr;

  // Dynamically link OpenProcessToken, LookupPrivilegeValue and AdjustTokenPrivileges

  HMODULE hAdvapi32 = GetModuleHandle(TEXT("advapi32.dll"));

  if (!hAdvapi32)
      hAdvapi32 = LoadLibrary(TEXT("advapi32.dll"));

  auto fun6 = (fun6_t)(void(*)())GetProcAddress(hAdvapi32, "OpenProcessToken");
  if (!fun6)
      return nullptr;
  auto fun7 = (fun7_t)(void(*)())GetProcAddress(hAdvapi32, "LookupPrivilegeValueA");
  if (!fun7)
      return nullptr;
  auto fun8 = (fun8_t)(void(*)())GetProcAddress(hAdvapi32, "AdjustTokenPrivileges");
  if (!fun8)
      return nullptr;

  // We need SeLockMemoryPrivilege, so try to enable it for the process
  if (!fun6( // OpenProcessToken()
      GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hProcessToken))
          return nullptr;

  if (fun7( // LookupPrivilegeValue(nullptr, SE_LOCK_MEMORY_NAME, &luid)
      nullptr, "SeLockMemoryPrivilege", &luid))
  {
      TOKEN_PRIVILEGES tp { };
      TOKEN_PRIVILEGES prevTp { };
      DWORD prevTpLen = 0;

      tp.PrivilegeCount = 1;
      tp.Privileges[0].Luid = luid;
      tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

      // Try to enable SeLockMemoryPrivilege. Note that even if AdjustTokenPrivileges() succeeds,
      // we still need to query GetLastError() to ensure that the privileges were actually obtained.
      if (fun8( // AdjustTokenPrivileges()
              hProcessToken, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), &prevTp, &prevTpLen) &&
          GetLastError() == ERROR_SUCCESS)
      {
          // Round up size to full pages and allocate
          allocSize = (allocSize + largePageSize - 1) & ~size_t(largePageSize - 1);
          mem = VirtualAlloc(
              nullptr, allocSize, MEM_RESERVE | MEM_COMMIT | MEM_LARGE_PAGES, PAGE_READWRITE);

          // Privilege no longer needed, restore previous state
          fun8( // AdjustTokenPrivileges ()
              hProcessToken, FALSE, &prevTp, 0, nullptr, nullptr);
      }
  }

  CloseHandle(hProcessToken);

  return mem;

  #endif
}

void* aligned_large_pages_alloc(size_t allocSize) {

  // Try to allocate large pages
  void* mem = aligned_large_pages_alloc_windows(allocSize);

  // Fall back to regular, page aligned, allocation if necessary
  if (!mem)
      mem = VirtualAlloc(nullptr, allocSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

  return mem;
}

#else

void* aligned_large_pages_alloc(size_t allocSize) {

#if defined(__linux__)
  constexpr size_t alignment = 2 * 1024 * 1024; // assumed 2MB page size
#else
  constexpr size_t alignment = 4096; // assumed small page size
#endif

  // round up to multiples of alignment
  size_t size = ((allocSize + alignment - 1) / alignment) * alignment;
#if defined(__linux__)
  void *mem = aligned_alloc(alignment, size);
#else
  void *mem = std_aligned_alloc(alignment, size);
#endif
#if defined(MADV_HUGEPAGE)
  madvise(mem, size, MADV_HUGEPAGE);
#endif
  return mem;
}

#endif


/// aligned_large_pages_free() will free the previously allocated ttmem

#if defined(_WIN32)

void aligned_large_pages_free(void* mem) {
  if (mem && !VirtualFree(mem, 0, MEM_RELEASE))
  {
      DWORD err = GetLastError();
      std::cerr << "Failed to free large page memory. Error code: 0x"
                << std::hex << err
                << std::dec << std::endl;
      exit(EXIT_FAILURE);
  }
}
#elif defined(_WIN64)
void aligned_large_pages_free(void *mem) {
  std_aligned_free(mem);
}
#else
void aligned_large_pages_free(void *mem) {
  free(mem);
}
#endif