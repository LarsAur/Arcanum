#pragma once

#include <board.hpp>
#include <transpositionTable.hpp>
#include <memory>

#define INF INT16_MAX

namespace ChessEngine2
{
    class Searcher
    {
        private:
            // TODO: Hashtables for repeat in search.
            std::unique_ptr<TranspositionTable> m_tt;
            eval_t m_alphaBeta(Board board, eval_t alpha, eval_t beta, int depth, int quietDepth);
            eval_t m_alphaBetaQuiet(Board board, eval_t alpha, eval_t beta, int depth);
            uint8_t m_generation = 0; // Can only use the 6 upper bits of the generation
        public:
            Searcher();
            ~Searcher();
            Move getBestMove(Board board, int depth, int quietDepth);
            Move getBestMoveInTime(Board board, int ms, int quietDepth);
    };
}