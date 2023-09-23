#pragma once
#include <board.hpp>

namespace Test
{
    void perft();
    void captureMoves();
    void zobrist();
    void draw();
    void symmetricEvaluation();
}

namespace Perf
{
    void search();
    void engineTest();
}
