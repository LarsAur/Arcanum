#pragma once

#include <types.hpp>
#include <board.hpp>
#include <eval.hpp>
#include <transpositionTable.hpp>
#include <moveSelector.hpp>
#include <memory>
#include <vector>
#include <unordered_map>

#define INF INT16_MAX
#define SEARCH_RECORD_STATS 1
#define SEARCH_MAX_PV_LENGTH 64

namespace Arcanum
{
    class HashFunction
    {
        public:
        size_t operator()(const hash_t& hash) const {
            return hash;
        }
    };

    typedef struct pvLine_t
    {
        uint8_t count;
        Move moves[SEARCH_MAX_PV_LENGTH];
    } pvline_t;

    // https://www.wbec-ridderkerk.nl/html/UCIProtocol.html

    typedef struct SearchStats
    {
        uint64_t evaluatedPositions; // Number of calls to board.evaulate()
        uint64_t exactTTValuesUsed; // Number of boards where all branches are pruned by getting the exact value from TT
        uint64_t lowerTTValuesUsed;
        uint64_t upperTTValuesUsed;
        uint64_t tbHits;
        uint8_t quietSearchDepth;
        uint64_t researchesRequired;
        uint64_t nullWindowSearches;
        uint64_t nullMoveCutoffs;
        uint64_t failedNullMoveCutoffs;
        uint64_t futilityPrunedMoves;
        uint64_t reverseFutilityCutoffs;
    } SearchStats;

    struct SearchParameters
    {
        int64_t msTime;
        uint64_t nodes;
        uint32_t depth;
        uint32_t mate; // TODO: This requires a search with only exact pruning
        bool infinite;
        uint32_t numSearchMoves;
        Move searchMoves[218];

        SearchParameters() : msTime(0), nodes(0), depth(0), mate(0), infinite(false), numSearchMoves(0) {};
    };

    class Searcher
    {
        private:
            std::unordered_map<hash_t, uint8_t, HashFunction> m_gameHistory;
            std::unique_ptr<TranspositionTable> m_tt;
            std::vector<hash_t> m_search_stack;
            std::vector<hash_t> m_knownEndgameMaterialDraws;
            Evaluator m_evaluator;
            KillerMoveManager m_killerMoveManager;
            RelativeHistory m_relativeHistory;
            uint8_t m_generation = 0; // Can only use the 6 upper bits of the generation
            uint8_t m_nonRevMovesRoot; // Number of non-reversable moves performed on the board in the root position.
            SearchStats m_stats;

            uint64_t m_numNodesSearched; // Number of nodes searched in a search call. Used to terminate search based on number of nodes.
            volatile bool m_stopSearch;

            eval_t m_alphaBeta(Board& board, pvLine_t* pvLine, eval_t alpha, eval_t beta, int depth, int plyFromRoot, bool isNullMoveSearch, uint8_t totalExtensions);
            eval_t m_alphaBetaQuiet(Board& board, eval_t alpha, eval_t beta, int plyFromRoot);
            bool m_isDraw(const Board& board) const;
        public:
            Searcher();
            ~Searcher();
            Move getBestMove(Board& board, int depth);
            Move getBestMoveInTime(Board& board, uint32_t ms);
            Move search(Board board, SearchParameters parameters);
            void stop();
            void resizeTT(uint32_t mbSize);
            void clearTT();
            SearchStats getStats();
            void logStats();
            std::unordered_map<hash_t, uint8_t, HashFunction>& getHistory();
            void addBoardToHistory(const Board& board);
            void clearHistory();
    };
}