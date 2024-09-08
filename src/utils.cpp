#include <utils.hpp>

#if defined(_WIN64)
#include <Libloaderapi.h>
#elif defined(__linux__)
#include <filesystem>
#endif

// Name of the log file used when PRINT_TO_FILE is defined
std::string _logFileName;

// Gets the path to the folder which the executable is in
std::string getWorkPath()
{
    // Get the path of the executable file
    #if defined(_WIN64)
        char execFullPath[2048];
        GetModuleFileNameA(NULL, execFullPath, 2048);
        // Move one folder up
        std::string path = std::string(execFullPath);
        size_t idx = path.find_last_of('\\');
        path = std::string(path).substr(0, idx + 1); // Keep the last '\'
        return path;
    #elif defined(__linux__)
        std::filesystem::path absPath = std::filesystem::canonical("/proc/self/exe");
        std::string path = std::string(absPath.parent_path().c_str());
        path.append("/");
        return path;
    #else
        #error Missing implementation of getWorkPath
    #endif

}

bool caseInsensitiveStrCmp(std::string& a, std::string& b)
{
    #if defined(_WIN64)
        return _stricmp(a.c_str(), b.c_str()) == 0;
    #elif defined(__linux__)
        return strcasecmp(a.c_str(), b.c_str()) == 0;
    #else
        #error Missing implementation of caseInsensitiveStrCmp
    #endif
}