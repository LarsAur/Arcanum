#include <tuning/fengen.hpp>
#include <tuning/gamerunner.hpp>
#include <fstream>
#include <thread>
#include <mutex>
#include <vector>
#include <search.hpp>
#include <syzygy.hpp>
#include <fen.hpp>
#include <timer.hpp>

using namespace Arcanum;

void Fengen::start(FengenParameters params)
{
    std::vector<std::thread> threads;
    std::mutex readLock;
    std::mutex writeLock;
    size_t fenCount = 0LL;
    size_t gameCount = 0LL;
    Timer msTimer = Timer();

    // Set search parameters
    SearchParameters searchParams = SearchParameters();

    searchParams.useTime    = params.movetime > 0;
    searchParams.msTime     = params.movetime;
    searchParams.useDepth   = params.depth > 0;
    searchParams.depth      = params.depth;
    searchParams.useNodes   = params.nodes > 0;
    searchParams.nodes      = params.nodes;

    msTimer.start();

    std::ifstream posStream = std::ifstream(params.startposPath);

    if(!posStream.is_open())
    {
        ERROR("Unable to open " << params.startposPath)
        return;
    }

    // Forward to the startposition given by offset
    LOG("Forwarding to startposition " << params.offset)
    for(size_t i = 0; i < params.offset; i++)
    {
        std::string unusedFen;
        std::getline(posStream, unusedFen);
    }

    DataStorer encoder = DataStorer();
    if(!encoder.open(params.outputPath))
    {
        ERROR("Unable to open " << params.outputPath)
        posStream.close();
        return;
    }

    auto fn = [&]()
    {
        std::string startfen;
        std::vector<Move> moves;
        std::vector<eval_t> scores;
        GameResult result;
        GameRunner runner;

        readLock.lock();
        moves.reserve(300);
        scores.reserve(300);
        Searcher searchers[2] = {Searcher(false), Searcher(false)};
        searchers[0].resizeTT(8);
        searchers[1].resizeTT(8);
        readLock.unlock();

        runner.setSearchers(&searchers[0], &searchers[1]);
        runner.setDrawAdjudication(true, 10, 10, 40);
        runner.setResignAdjudication(false);
        runner.setMoveLimit(300);
        runner.setSearchParameters(searchParams);

        while (true)
        {
            readLock.lock();

            posStream.peek();
            if(posStream.eof() || fenCount > params.numFens)
            {
                readLock.unlock();
                break;
            }

            std::getline(posStream, startfen);
            readLock.unlock();

            // Parse the board in relaxed mode and get the fen from the board
            // this is in case the edp does not provide move-clocks
            Board board = Board(startfen, false);
            startfen = board.fen();
            // Play the game and record the moves, scores and result of the game
            runner.play(board, &moves, &scores, &result);

            writeLock.lock();
            gameCount++;

            // Store the game using the selected encoding
            encoder.addGame(startfen, moves, scores, result);

            fenCount += moves.size() + 1; // Num moves + startfen
            if((fenCount % 1000) < ((fenCount - moves.size() - 1) % 1000)  )
            {
                LOG(
                    fenCount << " fens " <<
                    1000000.0f / msTimer.getMs() << " fens/sec " <<
                    100 * fenCount / params.numFens << "% " <<
                    gameCount << " games (offset: " << gameCount + params.offset << ")"
                )
                msTimer.start();
            }
            writeLock.unlock();

            moves.clear();
            scores.clear();
            searchers[0].clearHistory();
            searchers[1].clearHistory();
            searchers[0].clear();
            searchers[1].clear();
        }
    };

    for(uint32_t i = 0; i < params.numThreads; i++)
    {
        threads.push_back(std::thread(fn));
    }

    for(uint32_t i = 0; i < params.numThreads; i++)
    {
        threads.at(i).join();
    }

    encoder.close();
    LOG("Finished generating FENs")
}