#pragma once
#include <board.hpp>
#include <pvtable.hpp>
#include <search.hpp>
#include <uci/option.hpp>
#include <thread>

#ifndef ARCANUM_VERSION
#define ARCANUM_VERSION dev_build
#endif

namespace Arcanum
{
    namespace Interface
    {
        struct SearchInfo
        {
            uint32_t depth;                      // Current depth in iterative deepening
            uint32_t seldepth;                   // Maximum plys from root in current depth interation
            uint64_t msTime;                     // Time searched
            uint64_t nsTime;                     // Time searched (nano-seconds)
            uint64_t nodes;                      // Number of nodes searched
            eval_t score;                        // Current best score in cp
            bool mate;                           // If mate is found
            int32_t mateDistance;                // Mate distance in moves (not plies.) Negative if engine is being mated.
            PvTable* pvTable;                    // PV-line
            uint32_t hashfull;                   // Permills of hashtable filled
            uint64_t tbHits;
            Board board;

            SearchInfo() :
                depth(0),
                seldepth(0),
                msTime(0),
                nodes(0),
                score(0),
                mate(0),
                mateDistance(0),
                pvTable(nullptr),
                hashfull(0),
                tbHits(0)
            {}
        };

        class UCI
        {
            private:
                static bool isSearching;
                static std::thread searchThread;
                static Board       board;
                static Searcher    searcher;

                static void newgame();
                static void listUCI();
                static void setoption(std::istringstream& is);
                static void go(std::istringstream& is);
                static void position(std::istringstream& is);
                static void isready();
                static void stop();
                static void eval();
                static void drawboard();
                static void fengen(std::istringstream& is);
                static void train(std::istringstream& is);
                static void help();
            public:
                // Options
                static SpinOption   optionHash;
                static ButtonOption optionClearHash;
                static StringOption optionSyzygyPath;
                static StringOption optionNNUEPath;
                static SpinOption   optionMoveOverhead;
                static CheckOption  optionNormalizeScore;
                static CheckOption  optionShowWDL;

                static void sendBestMove(const Move& move);
                static void sendInfo(const SearchInfo& info);
                static void loop();
        };
    }
}