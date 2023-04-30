#pragma once

#include <board.hpp>

#define INF 100000000LL

namespace ChessEngine2
{
    class Searcher
    {
        private:
        // TODO: Hashtables etc.

            int64_t m_alphaBeta(Board board, int64_t alpha, int64_t beta, int depth, Color evalFor);
            int64_t m_alphaBetaQuiet(Board board, int64_t alpha, int64_t beta, int depth, Color evalFor);
            
        public:
            Searcher();
            ~Searcher();
            Move getBestMove(Board board, int depth);
    };
}