#pragma once

#include <syzygy/tbprobe.hpp>
#include <board.hpp>

namespace Arcanum
{
    class Syzygy
    {
        public:
        enum class WDLResult : int8_t
        {
            LOSS = 0,
            DRAW = 1,
            WIN  = 2,
            FAILED = 3
        };

        static WDLResult TBProbeDTZ(Board& board, Move* moves, uint8_t& numMoves);
        static WDLResult TBProbeWDL(const Board& board);
        static bool TBInit(std::string path);
        static void TBFree();
    };
}