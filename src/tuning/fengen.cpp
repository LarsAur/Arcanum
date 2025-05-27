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
        LOG("Forwarding to startposition " << params.offset)
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
        std::vector<Move> moves;
        std::vector<eval_t> scores;
        GameResult result;
        GameRunner runner;
        std::uniform_int_distribution<uint8_t> randDist(0, 255);
        std::mt19937 randGen(time(nullptr) + id);

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

            // Apply random moves to the starting position
            // If the position at some point contains a checkmate, the position is re-randomized
            while(true)
            {
                Board randomBoard = Board(board);
                bool randomized = true;
                for(uint32_t i = 0; i < params.numRandomMoves; i++)
                {
                    Move* moves = randomBoard.getLegalMoves();
                    uint8_t numMoves = randomBoard.getNumLegalMoves();
                    randomBoard.generateCaptureInfo();
                    uint8_t randIndex = randDist(randGen) % numMoves;
                    randomBoard.performMove(moves[randIndex]);
                    if(!randomBoard.hasLegalMove())
                    {
                        randomized = false;
                        break;
                    }
                }

                if(randomized)
                {
                    board = Board(randomBoard);
                    break;
                }
            }

            startfen = board.fen();

            // Play the game and record the moves, scores and result of the game
            runner.play(board, &moves, &scores, &result);

            writeLock.lock();
            gameCount++;
            results[result + 1]++;

            // Store the game using the selected encoding
            encoder.addGame(startfen, moves, scores, result);

            fenCount += moves.size() + 1; // Num moves + startfen
            if((fenCount % 1000) < ((fenCount - moves.size() - 1) % 1000)  )
            {
                LOG(
                    fenCount << " fens " <<
                    1000000.0f / msTimer.getMs() << " fens/sec " <<
                    100 * fenCount / params.numFens << "% " <<
                    gameCount << " games (offset: " << gameCount + params.offset << ") " <<
                    "Results: W: " << results[2] << " B: " << results[0] << " D: " << results[1]
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
        threads.push_back(std::thread(fn, i));
    }

    for(uint32_t i = 0; i < params.numThreads; i++)
    {
        threads.at(i).join();
    }

    encoder.close();
    LOG("Finished generating FENs")
}