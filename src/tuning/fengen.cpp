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

static bool parseStringArg(const std::string& pattern, std::string& out, int argc, char* argv[], int& index)
{
    std::string token = std::string(argv[index]);
    toLowerCase(token);
    if(pattern == token && index + 1 < argc)
    {
        index++; // Only increment if the pattern matches
        out = std::string(argv[index++]);
        return true;
    }

    return false;
}

static bool parseUInt32Arg(const std::string& pattern, uint32_t& out, int argc, char* argv[], int& index)
{
    std::string token = std::string(argv[index]);
    toLowerCase(token);
    if(pattern == token && index + 1 < argc)
    {
        index++; // Only increment if the pattern matches
        out = std::stoul(std::string(argv[index++]));
        return true;
    }

    return false;
}

bool Fengen::parseArgumentsAndRunFengen(int argc, char* argv[])
{
    // Check if the command is "fengen"

    if(argc < 2)
    {
        return false;
    }

    std::string command = std::string(argv[1]);
    toLowerCase(command);
    if(command != "fengen")
    {
        return false;
    }

    // Parse arguments
    FengenParameters params;

    DEBUG("Parsing fengen arguments")
    int index = 2;
    while(index < argc)
    {
        if(parseStringArg("--positions",      params.startposPath,   argc, argv, index)) { continue; }
        if(parseStringArg("--output",         params.outputPath,     argc, argv, index)) { continue; }
        if(parseUInt32Arg("--numrandommoves", params.numRandomMoves, argc, argv, index)) { continue; }
        if(parseUInt32Arg("--numfens",        params.numFens,        argc, argv, index)) { continue; }
        if(parseUInt32Arg("--numthreads",     params.numThreads,     argc, argv, index)) { continue; }
        if(parseUInt32Arg("--depth",          params.depth,          argc, argv, index)) { continue; }
        if(parseUInt32Arg("--movetime",       params.movetime,       argc, argv, index)) { continue; }
        if(parseUInt32Arg("--nodes",          params.nodes,          argc, argv, index)) { continue; }
        if(parseUInt32Arg("--offset",         params.offset,         argc, argv, index)) { continue; }

        ERROR("Unknown argument: " << argv[index])
        return true; // Return true since this is a fengen command, even if the arguments are invalid
    }

    // Validate input
    bool valid = true;

    if(params.numFens <= 0)
    { valid = false; ERROR("Number of fens cannot be 0 or less") }

    if(params.numThreads <= 0)
    { valid = false; ERROR("Number of threads cannot be 0 or less") }

    if(params.startposPath == "" && params.numRandomMoves == 0)
    { valid = false; ERROR("numrandommoves cannot be 0 when there is no path to edp file with starting positions") }

    if(params.outputPath == "")
    { valid = false; ERROR("Output path cannot be empty")            }

    if(params.depth == 0 && params.movetime == 0 && params.nodes == 0)
    { valid = false; ERROR("Search depth, movetime and nodes cannot be 0 at the same time") }

    if(valid)
    {
        LOG("Starting fengen with parameters:")
        LOG("\tStartpos path:     " << params.startposPath)
        LOG("\tNum random moves:  " << params.numRandomMoves)
        LOG("\tOutput path:       " << params.outputPath)
        LOG("\tOffset:            " << params.offset)
        LOG("\tNum fens:          " << params.numFens)
        LOG("\tNum threads:       " << params.numThreads)
        LOG("\tDepth:             " << params.depth)
        LOG("\tMovetime (ms):     " << params.movetime)
        LOG("\tNodes:             " << params.nodes)
        Fengen::start(params);
    }

    return true; // Return true since this is a fengen command, even if the arguments are invalid
}

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
                LOG(
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

    encoder.close();
    LOG("Finished generating FENs")
}