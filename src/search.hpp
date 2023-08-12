#pragma once

#include <board.hpp>
#include <transpositionTable.hpp>
#include <moveSelector.hpp>
#include <memory>
#include <vector>

#define INF INT16_MAX
#define SEARCH_RECORD_STATS 1
#define SEARCH_MAX_PV_LENGTH 64

namespace ChessEngine2
{

    typedef struct pvLine_t
    {
        uint8_t count;
        Move moves[SEARCH_MAX_PV_LENGTH];
    } pvline_t;

    typedef struct searchStats_t
    {
        uint64_t evaluatedPositions; // Number of calls to board.evaulate()
        uint64_t exactTTValuesUsed; // Number of boards where all branches are pruned by getting the exact value from TT
        uint64_t lowerTTValuesUsed;
        uint64_t upperTTValuesUsed;
        uint8_t quietSearchDepth;
    } searchStats_t;

    // https://www.wbec-ridderkerk.nl/html/UCIProtocol.html
    typedef struct uciInfo_t
    {
        uint8_t depth;
        uint8_t seldepth;
        uint32_t time;
        uint64_t nodes;
        eval_t score;
        pvLine_t pv;
    } uciInfo_t;

    class Searcher
    {
        private:
            // TODO: Hashtables for repeat in search.
            std::unique_ptr<TranspositionTable> m_tt;
            std::unique_ptr<Eval> m_eval;
            std::vector<hash_t> m_search_stack;
            KillerMoveManager m_killerMoveManager;
            EvalTrace m_alphaBeta(Board& board, pvLine_t* pvLine, EvalTrace alpha, EvalTrace beta, int depth, int plyFromRoot, int quietDepth);
            EvalTrace m_alphaBetaQuiet(Board& board, EvalTrace alpha, EvalTrace beta, int depth, int plyFromRoot);
            uint8_t m_generation = 0; // Can only use the 6 upper bits of the generation
            searchStats_t m_stats;
            uciInfo_t m_uciInfo;            
            bool m_stopSearch;

            void m_clearUCIInfo();
        public:
            Searcher();
            ~Searcher();
            Move getBestMove(Board& board, int depth, int quietDepth);
            Move getBestMoveInTime(Board& board, int ms, int quietDepth);
            searchStats_t getStats();
            uciInfo_t getUCIInfo();
    };
}