#pragma once

#include <iostream>
#include <string.h>

// Enable / Disable logging features
#define _CE2_DEBUG_ENABLE
#define _CE2_LOG_ENABLE
#define _CE2_WARNING_ENABLE
#define _CE2_ERROR_ENABLE
#define _CE2_SUCCESS_ENABLE

#define CE2_COLOR_BLACK "\e[0;30m"
#define CE2_COLOR_RED "\e[0;31m"
#define CE2_COLOR_GREEN "\e[0;32m"
#define CE2_COLOR_YELLOW "\e[0;33m"
#define CE2_COLOR_BLUE "\e[0;34m"
#define CE2_COLOR_PURPLE "\e[0;35m"
#define CE2_COLOR_CYAN "\e[0;36m"
#define CE2_COLOR_WHITE "\e[0;37m"

#define CE2_DEBUG_COLOR CE2_COLOR_CYAN
#define CE2_LOG_COLOR CE2_COLOR_WHITE
#define CE2_WARNING_COLOR CE2_COLOR_YELLOW
#define CE2_ERROR_COLOR CE2_COLOR_RED
#define CE2_SUCCESS_COLOR CE2_COLOR_GREEN
// Default
#define CE2_DEFAULT_COLOR CE2_COLOR_WHITE

// Get the file name from the full file path
#define __FILENAME__ (strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__)

#ifdef _CE2_DEBUG_ENABLE
#define CE2_DEBUG(_str) std::clog << CE2_DEBUG_COLOR << "[DEBUG] " << CE2_DEFAULT_COLOR << "\t[" << __FILENAME__ << ":" <<  __LINE__ << "] " << _str << std::endl;
#else
#define CE2_DEBUG(_str)
#endif

#ifdef _CE2_LOG_ENABLE
#define CE2_LOG(_str) std::clog << CE2_LOG_COLOR << "[LOG] " << CE2_DEFAULT_COLOR << "\t[" << __FILENAME__ << ":" <<  __LINE__ << "] " << _str << std::endl;
#else
#define CE2_LOG(_str)
#endif

#ifdef _CE2_WARNING_ENABLE
#define CE2_WARNING(_str) std::clog << CE2_WARNING_COLOR << "[WARNING] " << CE2_DEFAULT_COLOR << "[" << __FILENAME__ << ":" <<  __LINE__ << "] " << _str << std::endl;
#else
#define CE2_WARNING(_str)
#endif

#ifdef _CE2_ERROR_ENABLE
#define CE2_ERROR(_str) std::clog << CE2_ERROR_COLOR << "[ERROR] " << CE2_DEFAULT_COLOR << "\t[" << __FILENAME__ << ":" <<  __LINE__ << "] " << _str << std::endl;
#else
#define CE2_ERROR(_str)
#endif

#ifdef _CE2_SUCCESS_ENABLE
#define CE2_SUCCESS(_str) std::clog << CE2_SUCCESS_COLOR << "[SUCCESS] " << CE2_DEFAULT_COLOR << "\t[" << __FILENAME__ << ":" <<  __LINE__ << "] " << _str << std::endl;
#else
#define CE2_SUCCESS(_str)
#endif

void* aligned_large_pages_alloc(size_t allocSize);
void aligned_large_pages_free(void* mem);
