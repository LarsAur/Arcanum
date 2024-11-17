#pragma once

#include <iostream>
#include <string>
#include <fstream>
#include <cstring>

#define COLOR_BLACK "\e[0;30m"
#define COLOR_RED "\e[0;31m"
#define COLOR_GREEN "\e[0;32m"
#define COLOR_YELLOW "\e[0;33m"
#define COLOR_BLUE "\e[0;34m"
#define COLOR_PURPLE "\e[0;35m"
#define COLOR_CYAN "\e[0;36m"
#define COLOR_WHITE "\e[0;37m"

#define DEBUG_COLOR COLOR_CYAN
#define LOG_COLOR COLOR_WHITE
#define WARNING_COLOR COLOR_YELLOW
#define ERROR_COLOR COLOR_RED
#define SUCCESS_COLOR COLOR_GREEN
#define FAIL_COLOR COLOR_RED
// Default
#define DEFAULT_COLOR COLOR_WHITE

extern std::string _logFileName;
#ifdef PRINT_TO_FILE
#define CREATE_LOG_FILE(_name) _logFileName = std::string(_name).append(".log"); \
{ \
    std::ofstream fileStream(_logFileName, std::ofstream::out | std::ofstream::trunc); \
    if(fileStream.is_open()) \
    { \
        fileStream << "Created log file " << _logFileName << std::endl; \
        fileStream.close(); \
    } \
    else { std::cerr << "Unable to create file " << _logFileName << std::endl; } \
}
#else
    #define CREATE_LOG_FILE(_name) ;
#endif

#define _FILE_PRINT(_str) { \
    std::ofstream fileStream(_logFileName, std::ofstream::out | std::ofstream::app); \
    if(fileStream.is_open()) \
    { \
        fileStream << _str << std::endl; \
        fileStream.close(); \
    } \
    else { std::cerr << "Unable to open file " << _logFileName << std::endl; } \
}

// Get the file name from the full file path
#define __FILENAME__ (strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__)

#ifndef DISABLE_DEBUG
    #ifndef PRINT_TO_FILE
        #define DEBUG(_str) std::cout << DEBUG_COLOR << "[DEBUG]   " << DEFAULT_COLOR << "[" << __FILENAME__ << ":" <<  __LINE__ << "] " << _str << std::endl;
    #else
        #define DEBUG(_str) _FILE_PRINT("[DEBUG]   [" << __FILENAME__ << ":" <<  __LINE__ << "] " << _str)
    #endif
#else
    #define DEBUG(_str) ;
#endif

#ifndef DISABLE_LOG
    #ifndef PRINT_TO_FILE
        #define LOG(_str) std::cout << LOG_COLOR << "[LOG]   " << DEFAULT_COLOR << "[" << __FILENAME__ << ":" <<  __LINE__ << "] " << _str << std::endl;
    #else
        #define LOG(_str) _FILE_PRINT("[LOG]   [" << __FILENAME__ << ":" <<  __LINE__ << "] " << _str)
    #endif
#else
    #define LOG(_str) ;
#endif

#ifndef DISABLE_WARNING
    #ifndef PRINT_TO_FILE
        #define WARNING(_str) std::cout << WARNING_COLOR << "[WARNING]   " << DEFAULT_COLOR << "[" << __FILENAME__ << ":" <<  __LINE__ << "] " << _str << std::endl;
    #else
        #define WARNING(_str) _FILE_PRINT("[WARNING]   [" << __FILENAME__ << ":" <<  __LINE__ << "] " << _str)
    #endif
#else
    #define WARNING(_str) ;
#endif

#ifndef DISABLE_ERROR
    #ifndef PRINT_TO_FILE
        #define ERROR(_str) std::cout << ERROR_COLOR << "[ERROR]   " << DEFAULT_COLOR << "[" << __FILENAME__ << ":" <<  __LINE__ << "] " << _str << std::endl;
    #else
        #define ERROR(_str) _FILE_PRINT("[ERROR]   [" << __FILENAME__ << ":" <<  __LINE__ << "] " << _str)
    #endif
#else
    #define ERROR(_str) ;
#endif

#ifndef DISABLE_SUCCESS
    #define SUCCESS(_str) std::cout << SUCCESS_COLOR << "[SUCCESS]   " << DEFAULT_COLOR << "[" << __FILENAME__ << ":" <<  __LINE__ << "] " << _str << std::endl;
#else
    #define SUCCESS(_str) ;
#endif

#ifndef DISABLE_FAIL
    #define FAIL(_str) std::cout << FAIL_COLOR << "[FAIL]      " << DEFAULT_COLOR << "[" << __FILENAME__ << ":" <<  __LINE__ << "] " << _str << std::endl;
#else
    #define FAIL(_str) ;
#endif


#define UCI_OUT(_str) std::cout << _str << std::endl;

// Gets the path to the folder which the executable is in
std::string getWorkPath();

// Check for case insensitive string equality
bool strEqCi(std::string a, std::string b);

// Convert string to lowercase
void toLowerCase(std::string& str);