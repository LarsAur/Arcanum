#pragma once

#include <types.hpp>
#include <board.hpp>
#include <eval.hpp>
#include <transpositionTable.hpp>
#include <moveSelector.hpp>
#include <timer.hpp>
#include <vector>
#include <unordered_map>
#include <memory>

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

    typedef struct SearchStackElement
    {
        hash_t hash;
        eval_t staticEval;
    } SearchStackElement;

    // https://www.wbec-ridderkerk.nl/html/UCIProtocol.html

    typedef struct SearchStats
    {
        uint64_t nodes;       // Number of nodes visited
        uint64_t evaluations; // Number of calls to board.evaulate()
        uint64_t pvNodes;
        uint64_t nonPvNodes;
        uint64_t exactTTValuesUsed;
        uint64_t lowerTTValuesUsed;
        uint64_t upperTTValuesUsed;
        uint64_t tbHits;
        uint64_t researchesRequired;
        uint64_t nullWindowSearches;
        uint64_t nullMoveCutoffs;
        uint64_t failedNullMoveCutoffs;
        uint64_t futilityPrunedMoves;
        uint64_t razorCutoffs;
        uint64_t failedRazorCutoffs;
        uint64_t reverseFutilityCutoffs;

        SearchStats() :
            nodes(0),
            evaluations(0),
            pvNodes(0),
            nonPvNodes(0),
            exactTTValuesUsed(0),
            lowerTTValuesUsed(0),
            upperTTValuesUsed(0),
            tbHits(0),
            researchesRequired(0),
            nullWindowSearches(0),
            nullMoveCutoffs(0),
            failedNullMoveCutoffs(0),
            futilityPrunedMoves(0),
            razorCutoffs(0),
            failedRazorCutoffs(0),
            reverseFutilityCutoffs(0)
        {};

    } SearchStats;

    struct SearchParameters
    {
        bool useTime;
        bool useNodes;
        bool useDepth;

        int64_t msTime;
        uint64_t nodes;
        uint32_t depth;
        uint32_t mate; // TODO: This requires a search with only exact pruning
        bool infinite;
        uint32_t numSearchMoves;
        Move searchMoves[MAX_MOVE_COUNT];

        SearchParameters() :
            useTime(false),
            useNodes(false),
            useDepth(false),
            msTime(0),
            nodes(0),
            depth(0),
            mate(0),
            infinite(false),
            numSearchMoves(0)
        {};
    };

    struct SearchResult
    {
        eval_t eval;
    };

    class Searcher
    {
        private:
            std::unordered_map<hash_t, uint8_t, HashFunction> m_gameHistory;
            std::unique_ptr<TranspositionTable> m_tt;
            std::vector<SearchStackElement> m_searchStack;
            std::vector<hash_t> m_knownEndgameMaterialDraws;
            uint8_t m_lmrReductions[SEARCH_MAX_PV_LENGTH][MAX_MOVE_COUNT];
            Timer m_timer;
            Evaluator m_evaluator;
            KillerMoveManager m_killerMoveManager;
            RelativeHistory m_relativeHistory;
            SearchParameters m_searchParameters;
            uint8_t m_generation;
            uint8_t m_numPiecesRoot; // Number of pieces in the root of the search
            SearchStats m_stats;
            uint64_t m_numNodesSearched; // Number of nodes searched in a search call. Used to terminate search based on number of nodes.
            uint8_t m_seldepth;
            bool m_verbose; // Print use output and stats while searching
            volatile bool m_stopSearch;

            bool m_isDraw(const Board& board) const;
            bool m_shouldStop();

            template <bool isPv>
            eval_t m_alphaBeta(Board& board, pvLine_t* pvLine, eval_t alpha, eval_t beta, int depth, int plyFromRoot, bool isNullMoveSearch, uint8_t totalExtensions);
            template <bool isPv>
            eval_t m_alphaBetaQuiet(Board& board, eval_t alpha, eval_t beta, int plyFromRoot);
        public:
            Searcher();
            ~Searcher();
            Move getBestMove(Board& board, int depth, SearchResult* searchResult = nullptr);
            Move getBestMoveInTime(Board& board, uint32_t ms, SearchResult* searchResult = nullptr);
            Move search(Board board, SearchParameters parameters, SearchResult* searchResult = nullptr);
            void stop();
            void resizeTT(uint32_t mbSize);
            void clear();
            void setVerbose(bool enable);
            SearchStats getStats();
            void logStats();
            std::unordered_map<hash_t, uint8_t, HashFunction>& getHistory();
            void addBoardToHistory(const Board& board);
            void clearHistory();
    };
}