#pragma once

#include <tuning/dataloader.hpp>
#include <types.hpp>
#include <string>
#include <search.hpp>

namespace Arcanum
{
    class Fengen{
        private:
        bool m_isFinished(Board& board, Searcher& searcher, GameResult& result);
        public:
        void start(std::string startPosPath, std::string outputPath, size_t numFens, uint8_t numThreads, uint32_t depth, uint32_t movetime, uint32_t nodes);
    };
}