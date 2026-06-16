#include <tuning/postprocessing.hpp>
#include <tuning/dataloader.hpp>
#include <eval.hpp>
#include <thread>

using namespace Arcanum;

static bool isQuietPosition(
    Board& board,
    eval_t qMargin,
    eval_t margin,
    SearchParameters& searchParams,
    Searcher& searcher,
    eval_t* eval
){
    if(board.isChecked())
    {
        return false;
    }

    SearchResult result;
    SearchParameters qparams;

    eval_t staticEval = Evaluator::nnue.predictBoard(board);

    // Quiet search (search with low depth)
    qparams.depth = 1;
    qparams.useDepth = true;
    searcher.search(board, qparams, &result);
    if(std::abs(result.eval - staticEval) > qMargin)
    {
        return false;
    }

    // A deeper search
    searcher.search(board, searchParams, &result);
    if(std::abs(result.eval - staticEval) > margin)
    {
        return false;
    }

    *eval = result.eval;

    return true;
}

void PostProcessing::generateQuiets(const QuietGenParameters& params)
{
    DataLoader loader;
    DataStorer storer;
    std::mutex loaderMutex;
    std::mutex storerMutex;

    SearchParameters searchParams;
    searchParams.useDepth = params.depth > 0;
    searchParams.depth = params.depth;
    searchParams.useNodes = params.nodes > 0;
    searchParams.nodes = params.nodes;
    searchParams.useTime = params.movetime > 0;
    searchParams.msTime = params.movetime;

    struct Stats
    {
        uint64_t addedPositions;
        uint64_t skippedPositions;
        uint64_t offset;
    };

    Stats stats;
    stats.addedPositions = 0;
    stats.skippedPositions = 0;
    stats.offset = params.offset;

    if(!loader.open(params.inputPath))
    {
        ERROR("Failed to open input file: " << params.inputPath)
        return;
    }

    if(!storer.open(params.outputPath))
    {
        loader.close();
        ERROR("Failed to open output file: " << params.outputPath)
        return;
    }

    // Forward the loader to the offset position
    for(uint32_t i = 0; i < params.offset && !loader.eof(); i++)
    {
        loader.getNextBoard();
    }

    std::vector<std::thread> threads;
    for(uint32_t i = 0; i < params.numThreads; i++)
    {
        threads.emplace_back([&](){
            Searcher searcher;
            searcher.resizeTT(0);
            searcher.setVerbose(false);

            while(true)
            {
                loaderMutex.lock();
                if(loader.eof())
                {
                    loaderMutex.unlock();
                    break;
                }
                Board* board = loader.getNextBoard();
                Board boardCopy = Board(*board);
                stats.offset++;
                loaderMutex.unlock();

                Move* moves = boardCopy.getLegalMoves();
                boardCopy.generateCaptureInfo();
                uint8_t numMoves = boardCopy.getNumLegalMoves();

                for(uint8_t i = 0; i < numMoves; i++)
                {
                    eval_t eval;
                    Move move = moves[i];
                    Board newBoard = Board(boardCopy);
                    newBoard.performMove(move);
                    bool isQuiet = isQuietPosition(
                        newBoard,
                        params.qMargin,
                        params.margin,
                        searchParams,
                        searcher,
                        &eval
                    );

                    storerMutex.lock();
                    if (isQuiet)
                    {
                        storer.addPosition(newBoard, NULL_MOVE, eval, GameResult::DRAW);
                        stats.addedPositions++;
                    }
                    else
                    {
                        stats.skippedPositions++;
                    }

                    if((stats.addedPositions + stats.skippedPositions) % 10000 == 0)
                    {
                        INFO("Added positions: " << stats.addedPositions << ", Skipped positions: " << stats.skippedPositions << ", Offset: " << stats.offset);
                    }
                    storerMutex.unlock();
                }
            }
        });
    }

    for(uint32_t i = 0; i < params.numThreads; i++)
    {
        threads[i].join();
    }

    loader.close();
    storer.close();
}

void PostProcessing::reeval(const ReEvalParameters& params)
{
    DataLoader loader;
    DataStorer storer;
    std::mutex loaderMutex;
    std::mutex storerMutex;

    SearchParameters searchParams;
    searchParams.useDepth = params.depth > 0;
    searchParams.depth = params.depth;
    searchParams.useNodes = params.nodes > 0;
    searchParams.nodes = params.nodes;
    searchParams.useTime = params.movetime > 0;
    searchParams.msTime = params.movetime;

    uint32_t offset = params.offset;

    if(!loader.open(params.inputPath))
    {
        ERROR("Failed to open input file: " << params.inputPath)
        return;
    }

    if(!storer.open(params.outputPath))
    {
        loader.close();
        ERROR("Failed to open output file: " << params.outputPath)
        return;
    }

    // Forward the loader to the offset position
    for(uint32_t i = 0; i < params.offset && !loader.eof(); i++)
    {
        loader.getNextBoard();
    }

    std::vector<std::thread> threads;
    for(uint32_t i = 0; i < params.numThreads; i++)
    {
        threads.emplace_back([&](){
            Searcher searcher;
            searcher.resizeTT(0);
            searcher.setVerbose(false);

            while(true)
            {
                loaderMutex.lock();
                if(loader.eof())
                {
                    loaderMutex.unlock();
                    break;
                }
                Board* board = loader.getNextBoard();
                Board boardCopy = Board(*board);
                offset++;
                loaderMutex.unlock();

                SearchResult result;
                searcher.clear();
                Move move = searcher.search(boardCopy, searchParams, &result);

                storerMutex.lock();
                if(offset % 10000 == 0)
                {
                    INFO("Reevaluated positions (Offset): " << offset);
                }
                storer.addPosition(boardCopy, move, result.eval, GameResult::DRAW);
                storerMutex.unlock();
            }
        });
    }

    for(uint32_t i = 0; i < params.numThreads; i++)
    {
        threads[i].join();
    }

    loader.close();
    storer.close();
}