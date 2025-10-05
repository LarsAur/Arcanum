#include <tests/test.hpp>
#include <perft.hpp>
#include <timer.hpp>
#include <utils.hpp>

using namespace Arcanum;

static bool perftPosition(std::string fen, uint8_t ply, uint64_t expected, uint64_t* total)
{
    Board board = Board(fen);
    uint64_t count = findNumMovesAtDepth(board, ply);
    bool passed = count == expected;

    if(passed)
    {
        SUCCESS("Success perft with " << fen << " at " << unsigned(ply) << " depth")
    }
    else
    {
        FAIL("Failed perft with " << fen << " at " << unsigned(ply) << " depth. Expected: " << expected << " Got: " << count)
    }

    *total += count;
    return passed;
}

bool Test::runPerftTest()
{
    uint64_t total = 0;
    bool passed = true;
    Timer timer;
    timer.start();

    passed &= perftPosition("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 7, 3195901860LL, &total);
    passed &= perftPosition("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 6, 8031647685LL, &total);
    passed &= perftPosition("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 7, 178633661LL, &total);
    passed &= perftPosition("r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", 6, 706045033LL, &total);
    passed &= perftPosition("r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ - 0 1", 6, 706045033LL, &total);
    passed &= perftPosition("rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", 5, 89941194LL, &total);
    passed &= perftPosition("r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10", 6, 6923051137LL, &total);

    int64_t timeMs = timer.getMs();
    int64_t nps = (total * 1000) / std::max(int64_t(1), timeMs);

    if(passed)
    {
        SUCCESS("Completed perft test in " << timeMs << " ms, " << total << " Nodes, " << nps << " Nodes / Sec");
    }
    else
    {
        FAIL("Completed perft test in " << timeMs << " ms, " << total << " Nodes, " << nps << " Nodes / Sec");
    }

    return passed;
}