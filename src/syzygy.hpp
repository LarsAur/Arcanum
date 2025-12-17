#pragma once

#include <syzygy/tbprobe.hpp>
#include <board.hpp>

namespace Arcanum
{
    class Syzygy
    {
        public:
        static bool TBProbeDTZ(Board& board, Move* moves, uint8_t& numMoves, uint8_t& wdl);
        static uint32_t TBProbeWDL(const Board& board);
        static bool TBInit(std::string path);
        static void TBFree();
    };
}