#include <tests/test.hpp>
#include <search.hpp>
#include <fen.hpp>
#include <timer.hpp>

using namespace Arcanum;

bool Test::runSelfplayTest()
{
    constexpr uint32_t SearchDepth = 15;
    constexpr uint32_t TurnsToPlay = 20;

    Timer timer;
    Searcher whiteSearcher = Searcher();
    Searcher blackSearcher = Searcher();

    whiteSearcher.resizeTT(32);
    blackSearcher.resizeTT(32);

    Board board = Board(FEN::startpos);
    whiteSearcher.addBoardToHistory(board);
    blackSearcher.addBoardToHistory(board);

    SearchParameters params;
    params.depth = SearchDepth;
    params.useDepth = true;

    timer.start();

    for(uint32_t i = 0; i < TurnsToPlay; i++)
    {
        DEBUG("Turn: " << i << "/" << TurnsToPlay)
        Move whiteMove = whiteSearcher.search(board, params);
        board.performMove(whiteMove);
        whiteSearcher.addBoardToHistory(board);
        blackSearcher.addBoardToHistory(board);
        Move blackMove = blackSearcher.search(board, params);
        board.performMove(blackMove);
        whiteSearcher.addBoardToHistory(board);
        blackSearcher.addBoardToHistory(board);
    }

    SUCCESS("Completed SelfPlayTest in " << timer.getMs() << "ms")

    return true;
}