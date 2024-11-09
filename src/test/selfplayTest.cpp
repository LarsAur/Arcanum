#include <test/selfplayTest.hpp>
#include <search.hpp>
#include <fen.hpp>
#include <timer.hpp>

using namespace Arcanum;

void Benchmark::SelfplayTest::runSelfplayTest()
{
    constexpr uint32_t SearchDepth = 15;
    constexpr uint32_t TurnsToPlay = 20;

    Timer timer;
    Searcher whiteSearcher = Searcher();
    Searcher blackSearcher = Searcher();
    Board board = Board(FEN::startpos);
    whiteSearcher.addBoardToHistory(board);
    blackSearcher.addBoardToHistory(board);

    timer.start();

    for(uint32_t i = 0; i < TurnsToPlay; i++)
    {
        DEBUG("Turn: " << i << "/" << TurnsToPlay)
        Move whiteMove = whiteSearcher.getBestMove(board, SearchDepth);
        board.performMove(whiteMove);
        whiteSearcher.addBoardToHistory(board);
        blackSearcher.addBoardToHistory(board);
        Move blackMove = blackSearcher.getBestMove(board, SearchDepth);
        board.performMove(blackMove);
        whiteSearcher.addBoardToHistory(board);
        blackSearcher.addBoardToHistory(board);
    }

    SUCCESS("Completed SelfPlayTest in " << timer.getMs() << "ms")
}