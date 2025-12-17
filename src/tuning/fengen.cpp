#include <tuning/fengen.hpp>
#include <tuning/gamerunner.hpp>
#include <fstream>
#include <thread>
#include <mutex>
#include <vector>
#include <random>
#include <search.hpp>
#include <syzygy.hpp>
#include <fen.hpp>
#include <timer.hpp>
#include <syzygy.hpp>

using namespace Arcanum;

void Fengen::start(FengenParameters params)
{
    std::vector<std::thread> threads;
    std::mutex readLock;
    std::mutex writeLock;
    size_t fenCount = 0LL;
    size_t gameCount = 0LL;
    Timer msTimer = Timer();
    bool readInputPositions = !params.startposPath.empty();
    uint64_t results[3] = {0, 0, 0};

    // Initialize syzygy
    if(!params.syzygyPath.empty())
    {
        TBInit(params.syzygyPath);
    }

    // Set search parameters
    SearchParameters searchParams = SearchParameters();

    searchParams.useTime    = params.movetime > 0;
    searchParams.msTime     = params.movetime;
    searchParams.useDepth   = params.depth > 0;
    searchParams.depth      = params.depth;
    searchParams.useNodes   = params.nodes > 0;
    searchParams.nodes      = params.nodes;

    msTimer.start();

    std::ifstream posStream;

    if(readInputPositions)
    {
        posStream.open(params.startposPath);

        if(!posStream.is_open())
        {
            ERROR("Unable to open " << params.startposPath)
            return;
        }

        // Forward to the startposition given by offset
        INFO("Forwarding to startposition " << params.offset)
        for(size_t i = 0; i < params.offset; i++)
        {
            std::string unusedFen;
            std::getline(posStream, unusedFen);
        }
    }

    DataStorer encoder = DataStorer();
    if(!encoder.open(params.outputPath))
    {
        ERROR("Unable to open " << params.outputPath)
        posStream.close();
        return;
    }

    auto fn = [&](uint32_t id)
    {
        std::string startfen;
        GameRunner runner;

        readLock.lock();
        readLock.unlock();

        runner.setDrawAdjudication(true, 10, 6, 40);
        runner.setResignAdjudication(false);
        runner.setMoveLimit(300);
        runner.setSearchParameters(searchParams);
        runner.setRandomSeed(time(nullptr) + id * 1000);
        runner.setTTSize(0); // Disable TT
        runner.setDatagenMode(true);

        while (true)
        {
            Board board;

            // Read starting positions from input file
            if(readInputPositions)
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
                board = Board(startfen, false);
            }
            else
            {
                board = Board(FEN::startpos);
            }

            // If enabled, randomize the position.
            if(params.numRandomMoves > 0)
            {
                runner.randomizeInitialPosition(params.numRandomMoves, board, 400); // TODO: Set an eval limit
            }

            // Play the game
            runner.play();

            writeLock.lock();
            gameCount++;
            results[runner.getResult() + 1]++;

            // Store the game using the selected encoding
            encoder.addGame(runner.getInitialPosition(), runner.getMoves(), runner.getEvals(), runner.getResult());

            fenCount += runner.getMoves().size() + 1; // Num moves + startfen
            if((fenCount % 1000) < ((fenCount - runner.getMoves().size() - 1) % 1000)  )
            {
                INFO(
                    fenCount << " fens " <<
                    std::fixed << std::setprecision(2) << 1000000.0f / msTimer.getMs() << " fens/sec " <<
                    100 * fenCount / params.numFens << "% " <<
                    gameCount << " games (offset: " << gameCount + params.offset << ") " <<
                    "Results: W: " << results[2] << " B: " << results[0] << " D: " << results[1]
                )
                msTimer.start();
            }
            writeLock.unlock();
        }
    };

    for(uint32_t i = 0; i < params.numThreads; i++)
    {
        threads.push_back(std::thread(fn, i));
    }

    for(uint32_t i = 0; i < params.numThreads; i++)
    {
        threads.at(i).join();
    }

    TBFree();
    encoder.close();
    INFO("Finished generating FENs")
}