#pragma once

#include <iostream>
#include <string.h>

// Enable / Disable logging features
#define _CHESS_ENGINE2_DEBUG_ENABLE
#define _CHESS_ENGINE2_LOG_ENABLE
#define _CHESS_ENGINE2_WARN_ENABLE
#define _CHESS_ENGINE2_ERR_ENABLE
#define _CHESS_ENGINE2_SUCCESS_ENABLE

#define CHESS_ENGINE2_COLOR_BLACK "\e[0;30m"
#define CHESS_ENGINE2_COLOR_RED "\e[0;31m"
#define CHESS_ENGINE2_COLOR_GREEN "\e[0;32m"
#define CHESS_ENGINE2_COLOR_YELLOW "\e[0;33m"
#define CHESS_ENGINE2_COLOR_BLUE "\e[0;34m"
#define CHESS_ENGINE2_COLOR_PURPLE "\e[0;35m"
#define CHESS_ENGINE2_COLOR_CYAN "\e[0;36m"
#define CHESS_ENGINE2_COLOR_WHITE "\e[0;37m"

#define CHESS_ENGINE2_DEBUG_COLOR CHESS_ENGINE2_COLOR_CYAN
#define CHESS_ENGINE2_LOG_COLOR CHESS_ENGINE2_COLOR_WHITE
#define CHESS_ENGINE2_WARN_COLOR CHESS_ENGINE2_COLOR_YELLOW
#define CHESS_ENGINE2_ERR_COLOR CHESS_ENGINE2_COLOR_RED
#define CHESS_ENGINE2_SUCCESS_COLOR CHESS_ENGINE2_COLOR_GREEN
// Default
#define CHESS_ENGINE2_DEFAULT_COLOR CHESS_ENGINE2_COLOR_WHITE

// Get the file name from the full file path
#define __FILENAME__ (strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__)

#ifdef _CHESS_ENGINE2_DEBUG_ENABLE
#define CHESS_ENGINE2_DEBUG(_str) std::cout << CHESS_ENGINE2_DEBUG_COLOR << "[DEBUG] " << CHESS_ENGINE2_DEFAULT_COLOR << "\t[" << __FILENAME__ << ":" <<  __LINE__ << "] " << _str << std::endl;
#else
#define CHESS_ENGINE2_DEBUG(_str)
#endif

#ifdef _CHESS_ENGINE2_LOG_ENABLE
#define CHESS_ENGINE2_LOG(_str) std::cout << CHESS_ENGINE2_LOG_COLOR << "[LOG] " << CHESS_ENGINE2_DEFAULT_COLOR << "\t[" << __FILENAME__ << ":" <<  __LINE__ << "] " << _str << std::endl;
#else
#define CHESS_ENGINE2_LOG(_str)
#endif

#ifdef _CHESS_ENGINE2_WARN_ENABLE
#define CHESS_ENGINE2_WARN(_str) std::cout << CHESS_ENGINE2_WARN_COLOR << "[WARNING] " << CHESS_ENGINE2_DEFAULT_COLOR << "[" << __FILENAME__ << ":" <<  __LINE__ << "] " << _str << std::endl;
#else
#define CHESS_ENGINE2_WARN(_str)
#endif

#ifdef _CHESS_ENGINE2_ERR_ENABLE
#define CHESS_ENGINE2_ERR(_str) std::cout << CHESS_ENGINE2_ERR_COLOR << "[ERROR] " << CHESS_ENGINE2_DEFAULT_COLOR << "\t[" << __FILENAME__ << ":" <<  __LINE__ << "] " << _str << std::endl;
#else
#define CHESS_ENGINE2_ERR(_str)
#endif

#ifdef _CHESS_ENGINE2_SUCCESS_ENABLE
#define CHESS_ENGINE2_SUCCESS(_str) std::cout << CHESS_ENGINE2_SUCCESS_COLOR << "[SUCCESS] " << CHESS_ENGINE2_DEFAULT_COLOR << "\t[" << __FILENAME__ << ":" <<  __LINE__ << "] " << _str << std::endl;
#else
#define CHESS_ENGINE2_SUCCESS(_str)
#endif