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
    searcher.clear();
    searcher.search(board, qparams, &result);
    if(std::abs(result.eval - staticEval) > qMargin)
    {
        return false;
    }

    // A deeper search
    searcher.clear();
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
            searcher.resizeTT(params.ttSize);
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
                GameResult gameResult = loader.getResult();
                Board boardCopy = Board(*board);
                offset++;
                if(offset % 10000 == 0)
                {
                    INFO("Reevaluated positions (Offset): " << offset);
                }
                loaderMutex.unlock();

                SearchResult result;
                searcher.clear();
                Move move = searcher.search(boardCopy, searchParams, &result);

                if(Evaluator::isMateScore(result.eval))
                {
                    continue;
                }

                storerMutex.lock();
                storer.addPosition(boardCopy, move, result.eval, gameResult);
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

static eval_t quiesceBoardHelper(Board& board, eval_t alpha, eval_t beta, uint32_t depth, Board* boards, uint32_t maxDepth)
{
    Move* moves = board.getLegalCaptureMoves();
    uint8_t numMoves = board.getNumLegalMoves();
    board.generateCaptureInfo();

    bool checked = board.isChecked();

    if(depth >= maxDepth)
    {
        return -Evaluator::MateScore;
    }

    if(!checked)
    {
        eval_t staticEval = Evaluator::nnue.predictBoard(board);

        if(staticEval >= beta)
        {
            return staticEval;
        }

        if(staticEval > alpha)
        {
            boards[depth] = Board(board);
            alpha = staticEval;
        }
    }

    if(checked && numMoves == 0)
    {
        return -Evaluator::MateScore;
    }

    for(uint8_t i = 0; i < numMoves; i++)
    {
        Move move = moves[i];
        Board newBoard = Board(board);
        newBoard.performMove(move);

        eval_t score = -quiesceBoardHelper(newBoard, -beta, -alpha, depth + 1, boards, maxDepth);

        if(score >= beta)
        {
            return score;
        }

        if(score > alpha)
        {
            boards[depth] = boards[depth + 1];
            alpha = score;
        }
    }

    return alpha;
}

static Board quiesceBoard(Board& board, eval_t* score)
{
    constexpr uint32_t MaxDepth = 8;
    Board boards[MaxDepth];

    *score = quiesceBoardHelper(board, -Evaluator::MateScore, Evaluator::MateScore, 0, boards, MaxDepth);

    return boards[0];
}

void PostProcessing::filter(const FilterParameters& params)
{
    DataLoader loader;
    DataStorer storer;
    std::mutex loaderMutex;
    std::mutex storerMutex;

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
            while(true)
            {
                loaderMutex.lock();
                if(loader.eof())
                {
                    loaderMutex.unlock();
                    break;
                }
                Board* pboard = loader.getNextBoard();
                Board board = Board(*pboard);
                eval_t eval = loader.getScore();
                GameResult result = loader.getResult();
                Move move = loader.getMove();
                offset++;
                if(offset % 10000 == 0)
                {
                    INFO("Filtered positions (Offset): " << offset);
                }

                loaderMutex.unlock();

                if(Evaluator::isMateScore(eval))
                {
                    continue;
                }

                if(board.getNumPieces() <= 6)
                {
                    continue;
                }

                eval_t staticEval;

                Board qBoard = quiesceBoard(board, &staticEval);

                if(Evaluator::isMateScore(staticEval))
                {
                    WARNING("Quiesce board is a mate score, skipping: " << board.fen())
                    continue;
                }

                if(board.getTurn() != qBoard.getTurn())
                {
                    eval = -eval;
                }

                {
                    storerMutex.lock();
                    storer.addPosition(qBoard, move, eval, result);
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