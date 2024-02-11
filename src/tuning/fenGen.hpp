#pragma once

#include <board.hpp>
#include <string>
#include <vector>

namespace Tuning
{
    class FenGen
    {
        private:
        std::string m_outputFilePath;
        volatile uint64_t m_fenCount;
        bool m_isQuiet(Arcanum::Board& board);
        void m_runIteration(std::string pgnFile);
        public:
        void setOutputFile(std::string path);
        void start(uint8_t threadCount, std::string directory);
    };
}
