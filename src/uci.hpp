#pragma once
#include <board.hpp>
#include <eval.hpp>

namespace UCI
{
    struct SearchInfo
    {
        uint32_t depth;                      // Current depth in iterative deepening
        uint64_t msTime;                     // Time searched
        uint64_t nodes;                      // Number of nodes searched
        Arcanum::eval_t score;               // Current best score in cp
        bool mate;                           // If mate is found
        int32_t mateDistance;                // Mate distance in moves (not plies.) Negative if engine is being mated.
        Arcanum::Move bestMove;              // Current best move
        std::vector<Arcanum::Move> pvLine;   // PV-line
        uint32_t hashfull;                   // Permills of hashtable filled

        SearchInfo() : depth(0), msTime(0), nodes(0), score(0), mateDistance(0), bestMove(Arcanum::Move(0,0)), hashfull(0) {}
    };

    void loop();
    void sendUciInfo(const SearchInfo& info);
    void sendUciBestMove(const Arcanum::Move& move);
}