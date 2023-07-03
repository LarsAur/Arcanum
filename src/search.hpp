#pragma once

#include <board.hpp>
#include <transpositionTable.hpp>
#include <memory>

#define INF INT16_MAX
#define SEARCH_RECORD_STATS 1

namespace ChessEngine2
{

    typedef struct searchStats_t
    {
        uint64_t evaluatedPositions; // Number of calls to board.evaulate()
        uint64_t exactTTValuesUsed; // Number of boards where all branches are pruned by getting the exact value from TT
        uint64_t lowerTTValuesUsed;
        uint64_t upperTTValuesUsed;
    } searchStats_t;

    class Searcher
    {
        private:
            // TODO: Hashtables for repeat in search.
            std::unique_ptr<TranspositionTable> m_tt;
            eval_t m_alphaBeta(Board& board, eval_t alpha, eval_t beta, int depth, int quietDepth);
            eval_t m_alphaBetaQuiet(Board& board, eval_t alpha, eval_t beta, int depth);
            uint8_t m_generation = 0; // Can only use the 6 upper bits of the generation
            searchStats_t m_stats; 
        public:
            Searcher();
            ~Searcher();
            Move getBestMove(Board& board, int depth, int quietDepth);
            Move getBestMoveInTime(Board& board, int ms, int quietDepth);
            searchStats_t getStats();
    };
}