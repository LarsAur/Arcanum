#pragma once

#include <iostream>
#include <string>
#include <fstream>
#include <cstring>
#include <sstream>

#define STRINGIFY(s) #s
#define TOSTRING(x) STRINGIFY(x)

#define COLOR_BLACK "\x1B[0;30m"
#define COLOR_RED "\x1B[0;31m"
#define COLOR_GREEN "\x1B[0;32m"
#define COLOR_YELLOW "\x1B[0;33m"
#define COLOR_BLUE "\x1B[0;34m"
#define COLOR_PURPLE "\x1B[0;35m"
#define COLOR_CYAN "\x1B[0;36m"
#define COLOR_WHITE "\x1B[0;37m"

#define DEBUG_COLOR COLOR_CYAN
#define LOG_COLOR COLOR_WHITE
#define WARNING_COLOR COLOR_YELLOW
#define ERROR_COLOR COLOR_RED
#define SUCCESS_COLOR COLOR_GREEN
#define FAIL_COLOR COLOR_RED
#define TEST_INFO_COLOR COLOR_PURPLE
// Default
#define DEFAULT_COLOR COLOR_WHITE

// Get the current date/time. Format is YYYY-MM-DD_HH-mm-ss
const std::string getCurrentDateTime();

// Gets the path to the folder which the executable is in
std::string getWorkPath();

// Check for case insensitive string equality
bool strEqCi(std::string a, std::string b);

// Convert string to lowercase
void toLowerCase(std::string& str);

// Logs the
void logToFile(std::string str);

#define _FILE_PRINT(_str) { \
    std::stringstream ss; \
    ss << _str; \
    logToFile(ss.str()); \
}

// Get the file name from the full file path
#define __FILENAME__ (strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__)

#ifndef DISABLE_DEBUG
    #ifndef LOG_FILE_NAME
        #define DEBUG(_str) std::cout << DEBUG_COLOR << "[DEBUG]   " << DEFAULT_COLOR << "[" << __FILENAME__ << ":" <<  __LINE__ << "] " << _str << std::endl;
    #else
        #define DEBUG(_str) _FILE_PRINT("[DEBUG]   [" << __FILENAME__ << ":" <<  __LINE__ << "] " << _str)
    #endif
#else
    #define DEBUG(_str) ;
#endif

#ifndef DISABLE_LOG
    #ifndef LOG_FILE_NAME
        #define LOG(_str) std::cout << LOG_COLOR << "[LOG]   " << DEFAULT_COLOR << "[" << __FILENAME__ << ":" <<  __LINE__ << "] " << _str << std::endl;
    #else
        #define LOG(_str) _FILE_PRINT("[LOG]   [" << __FILENAME__ << ":" <<  __LINE__ << "] " << _str)
    #endif
#else
    #define LOG(_str) ;
#endif

#ifndef DISABLE_WARNING
    #ifndef LOG_FILE_NAME
        #define WARNING(_str) std::cout << WARNING_COLOR << "[WARNING]   " << DEFAULT_COLOR << "[" << __FILENAME__ << ":" <<  __LINE__ << "] " << _str << std::endl;
    #else
        #define WARNING(_str) _FILE_PRINT("[WARNING]   [" << __FILENAME__ << ":" <<  __LINE__ << "] " << _str)
    #endif
#else
    #define WARNING(_str) ;
#endif

#ifndef DISABLE_ERROR
    #ifndef LOG_FILE_NAME
        #define ERROR(_str) std::cout << ERROR_COLOR << "[ERROR]   " << DEFAULT_COLOR << "[" << __FILENAME__ << ":" <<  __LINE__ << "] " << _str << std::endl;
    #else
        #define ERROR(_str) _FILE_PRINT("[ERROR]   [" << __FILENAME__ << ":" <<  __LINE__ << "] " << _str)
    #endif
#else
    #define ERROR(_str) ;
#endif

// SUCCESS, FAIL and INFO are only used in tests, so they always print to console
#define SUCCESS(_str) std::cout << SUCCESS_COLOR   << "[SUCCESS] " << DEFAULT_COLOR << "[" << __FILENAME__ << ":" <<  __LINE__ << "] " << _str << std::endl;
#define FAIL(_str)    std::cout << FAIL_COLOR      << "[FAIL]    " << DEFAULT_COLOR << "[" << __FILENAME__ << ":" <<  __LINE__ << "] " << _str << std::endl;
#define TESTINFO(_str)std::cout << TEST_INFO_COLOR << "[INFO]    " << DEFAULT_COLOR << "[" << __FILENAME__ << ":" <<  __LINE__ << "] " << _str << std::endl;

// UCI is used for communication with the GUI, so it always prints to console
// This macro mainly exists for semantic purposes
#define UCI_OUT(_str) std::cout << _str << std::endl;

#define ASSERT_OR_EXIT(_cond, _msg) { \
    if(!(_cond)) { \
        ERROR((_msg)) \
        exit(EXIT_FAILURE); \
    } \
}