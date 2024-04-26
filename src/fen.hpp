#pragma once

#include <board.hpp>
#include <string>

namespace Arcanum
{
    class FEN
    {
        public:
        static constexpr const char* startpos = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
        static bool setFEN(Board& board, const std::string fen);
        static std::string getFEN(const Board& board);
        static std::string toString(const Board& board);
    };
};
