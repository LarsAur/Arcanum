#include <utils.hpp>

// Gets the path to the folder which the executable is in
std::string getWorkPath()
{
    // Get the path of the executable file
    char execFullPath[2048];
    #if defined(_WIN64)
        GetModuleFileNameA(NULL, execFullPath, 2048);
    #elif defined(__linux__)
        ERROR("Missing implementation")
    #else
        ERROR("Missing implementation")
    #endif

    // Move one folder up
    std::string path = std::string(execFullPath);
    size_t idx = path.find_last_of('\\');
    path = std::string(path).substr(0, idx + 1); // Keep the last '\'

    return path;
}