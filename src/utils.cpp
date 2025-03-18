#include <utils.hpp>
#include <algorithm>
#include <time.h>

#if defined(_WIN64)
#include <Libloaderapi.h>
#elif defined(__linux__)
#include <filesystem>
#endif

// Name of the log file used when LOG_FILE_NAME is defined
static std::string logFileName;

void logToFile(std::string str)
{
    // Check if an existing log file is created
    // If not, create a unique name using the current data and time
    if(logFileName.empty())
    {
        logFileName = getWorkPath()
            .append(std::string(TOSTRING(LOG_FILE_NAME)))
            .append("_")
            .append(getCurrentDateTime())
            .append(".log");
    }

    std::ofstream fileStream(logFileName, std::ofstream::out | std::ofstream::app);

    if(fileStream.is_open())
    {
        fileStream << str << std::endl;
        fileStream.close();
    }
    else { std::cerr << "Unable to open file " << logFileName << std::endl; }
}

// Get the current date/time. Format is YYYY-MM-DD_HH-mm-ss
const std::string getCurrentDateTime() {
    time_t now = time(0);
    struct tm tstruct;
    char buf[100];
    tstruct = *localtime(&now);
    strftime(buf, sizeof(buf), "%Y-%m-%d_%H-%M-%S", &tstruct);
    return buf;
}

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

// Check for case insensitive string equality
bool strEqCi(std::string a, std::string b)
{
    #if defined(_WIN64)
        return _stricmp(a.c_str(), b.c_str()) == 0;
    #elif defined(__linux__)
        return strcasecmp(a.c_str(), b.c_str()) == 0;
    #else
        #error Missing implementation of StrEqCi
    #endif
}

void toLowerCase(std::string& str)
{
    std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c){ return std::tolower(c); });
}