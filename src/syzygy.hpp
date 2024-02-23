#pragma once

#include <syzygy/tbprobe.hpp>
#include <board.hpp>

namespace Arcanum
{
    bool TBProbeDTZ(Board& board, Move* moves, uint8_t& numMoves, uint8_t& wdl);
    uint32_t TBProbeWDL(const Board& board);
    bool TBInit(std::string path);
    void TBFree();
}