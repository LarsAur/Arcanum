#pragma once
#include <board.hpp>

namespace Test
{
    void perft();
    void captureMoves();
    void zobrist();
    void draw();
    void see();
}

namespace Perf
{
    void search();
    void engineTest();
}
