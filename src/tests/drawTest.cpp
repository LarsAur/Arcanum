#include <tests/test.hpp>
#include <tuning/gamerunner.hpp>
#include <search.hpp>
#include <board.hpp>
#include <utils.hpp>

using namespace Arcanum;

// Test if search will find its way around 3-fold repetition checkmate
static bool testCheckmateWithoutRepeat()
{
    Searcher wsearcher(false);
    Searcher bsearcher(false);
    GameRunner runner;
    std::vector<Move> moves;
    std::vector<eval_t> scores;
    GameResult result;

    SearchParameters params;
    params.useTime = true;
    params.msTime = 200;

    runner.setSearchers(&wsearcher, &bsearcher);
    runner.setSearchParameters(SearchParameters());
    runner.setMoveLimit(10);

    // Setup the board and history such that the shortest checkmate would be a 3-fold repetition
    const Board repeat = Board("k7/1p1p1p2/pPpPpPp1/P1P1P1P1/7R/8/8/K7 b - - 0 1");
    wsearcher.addBoardToHistory(repeat);
    wsearcher.addBoardToHistory(repeat);
    bsearcher.addBoardToHistory(repeat);
    bsearcher.addBoardToHistory(repeat);
    const Board initialBoard = Board("k7/1p1p1p2/pPpPpPp1/P1P1P1P1/R7/8/8/K7 w - - 0 1");
    wsearcher.addBoardToHistory(initialBoard);
    bsearcher.addBoardToHistory(initialBoard);

    // Play the game out
    runner.play(initialBoard, &moves, &scores, &result);

    // Check that white won
    if(result != GameResult::WHITE_WIN)
    {
        DEBUG("Result: " << result)
        ERROR("Did not checkmate when possible")
        return false;
    }

    // Check that the position was not repeated
    Board replayBoard = Board(initialBoard);
    for(const auto& move : moves)
    {
        replayBoard.performMove(move);
        if(replayBoard.getHash() == repeat.getHash())
        {
            ERROR("Repeated position: " << repeat.fen())
            return false;
        }
    }

    // Check that the shortest mate without repetition was found
    if(moves.size() != 5)
    {
        ERROR("Did not find shortest mate without repetition, found mate in " << moves.size() << " instead of 5")
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