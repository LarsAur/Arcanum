#pragma once

#include <board.hpp>
#include <string>

namespace Test
{
    void findNumMovesAtDepth(Arcanum::Board& board, uint32_t depth, uint64_t *count);
    void perft(std::string fen, uint32_t depth);
    void perft(Arcanum:: Board& board, uint32_t depth);
}