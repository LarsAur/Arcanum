#pragma once

#include <board.hpp>
#include <string>

namespace Arcanum
{
    uint64_t findNumMovesAtDepth(Board& board, uint32_t depth);
    void perft(Board& board, uint32_t depth);
}