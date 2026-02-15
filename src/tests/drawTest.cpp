#include <tests/test.hpp>
#include <tuning/gamerunner.hpp>
#include <search.hpp>
#include <board.hpp>
#include <utils.hpp>

using namespace Arcanum;

// Test if search will find its way around 3-fold repetition checkmate
static bool testCheckmateWithoutRepeat()
{
    GameRunner runner;

    SearchParameters params;
    params.useTime = true;
    params.msTime = 200;

    runner.setTTSize(32);
    runner.setSearchParameters(params);
    runner.setMoveLimit(10);

    // Setup the board and history such that the shortest checkmate would be a 3-fold repetition
    const Board repeat = Board("k7/1p1p1p2/pPpPpPp1/P1P1P1P1/7R/8/8/K7 b - - 0 1");
    runner.getSearcher(Color::WHITE).addBoardToHistory(repeat);
    runner.getSearcher(Color::WHITE).addBoardToHistory(repeat);
    runner.getSearcher(Color::BLACK).addBoardToHistory(repeat);
    runner.getSearcher(Color::BLACK).addBoardToHistory(repeat);

    // Play the game out
    const Board initialBoard = Board("k7/1p1p1p2/pPpPpPp1/P1P1P1P1/R7/8/8/K7 w - - 0 1");
    runner.setInitialPosition(initialBoard);
    runner.play(false);

    // Check that white won
    if(runner.getResult() != GameResult::WHITE_WIN)
    {
        FAIL("Did not checkmate when possible")
        return false;
    }

    // Check that the position was not repeated
    Board replayBoard = Board(initialBoard);
    for(const auto& move : runner.getMoves())
    {
        replayBoard.performMove(move);
        if(replayBoard.getHash() == repeat.getHash())
        {
            FAIL("Repeated position: " << repeat.fen())
            return false;
        }
    }

    // Check that the shortest mate without repetition was found
    if(runner.getMoves().size() != 5)
    {
        FAIL("Did not find shortest mate without repetition, found mate in " << runner.getMoves().size() << " instead of 5")
        return false;
    }

    SUCCESS("Found checkmate without repetition")
    return true;
}

bool Test::runDrawTest()
{
    bool passed = true;

    passed &= testCheckmateWithoutRepeat();

    if(passed)
    {
        SUCCESS("All draw tests passed")
    }
    else
    {
        FAIL("Some draw tests failed")
    }

    return passed;
}