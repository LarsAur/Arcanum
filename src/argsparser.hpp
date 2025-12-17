#pragma once

#include <string>
#include <utils.hpp>

namespace Arcanum
{
    class ArgsParser
    {
        private:
            template <typename T>
            static bool matchAndParseArg(const std::string& pattern, T& out, int argc, char* argv[], int& index)
            {
                std::string token = std::string(argv[index]);
                toLowerCase(token);
                if(pattern == token && index + 1 < argc)
                {
                    index++; // Only increment if the pattern matches
                    if constexpr (std::is_same_v<T, std::string>)
                    {
                        out = std::string(argv[index++]);
                    }
                    else if constexpr (std::is_same_v<T, uint32_t>)
                    {
                        out = std::stoul(std::string(argv[index++]));
                    }
                    else if constexpr (std::is_same_v<T, uint64_t>)
                    {
                        out = std::stoull(std::string(argv[index++]));
                    }
                    else if constexpr (std::is_same_v<T, float >)
                    {
                        out = std::stof(std::string(argv[index++]));
                    }
                    else
                    {
                        ERROR("Unsupported argument type in argument parser " << typeid(T).name())
                    }
                    return true;
                }
                return false;
            }

            static bool parseArgumentsAndRunFengen(int argc, char* argv[]);
            static bool parseArgumentsAndRunNnueTrainer(int argc, char* argv[]);
        public:
            // Parses command line arguments and runs UCI if the arguments are valid
            // Returns false if the arguments are not matching any commands
            static bool parseArgumentsAndRunCommand(int argc, char* argv[]);
    };
}