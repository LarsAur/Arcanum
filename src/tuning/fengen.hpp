#pragma once

#include <types.hpp>
#include <string>
#include <search.hpp>

namespace Arcanum
{
    class Fengen{
        private:

        enum Result
        {
            BLACK_WIN = -1,
            DRAW = 0,
            WHITE_WIN = 1,
        };

        std::vector<hash_t> m_materialDraws;
        void m_setupMaterialDraws();
        bool m_isFinished(Board& board, Searcher& searcher, Result& result);
        public:
        void start(std::string startPosPath, std::string outputPath, size_t numFens, uint8_t numThreads, uint32_t depth);
    };
}