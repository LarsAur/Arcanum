#include <tuning/postprocessing.hpp>
#include <tuning/dataloader.hpp>
#include <eval.hpp>
#include <zobrist.hpp>
#include <thread>
#include <set>

using namespace Arcanum;

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

void PostProcessing::quiesce(const QuiesceParameters& params)
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

void PostProcessing::deduplicate(const DeduplicateParameters& params)
{
    DataLoader loader;
    DataStorer storer;

    uint32_t duplicateCount = 0;
    uint32_t uniqueCount = 0;

    std::set<hash_t> bucketHashes;

    // Bucket the positions based on board hash, then de-duplicate each bucket separately
    for(uint32_t i = params.startBucket; i < params.buckets; i++)
    {
        INFO("Processing bucket " << i << " / " << params.buckets)
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

        while(!loader.eof())
        {
            Board* board = loader.getNextBoard();
            GameResult result = loader.getResult();
            Move move = loader.getMove();
            eval_t score = loader.getScore();

            // Calculate the hash as it is not pre-calulated by the DataLoader
            hash_t hash, pawnHash, materialHash;
            Zobrist::getHashes(*board, hash, pawnHash, materialHash);

            // Check if the position does not belongs to the current bucket
            uint64_t bucket = hash % params.buckets;
            if(bucket != i)
            {
                continue;
            }

            // Check if the position is already in the bucket
            if(bucketHashes.find(hash) != bucketHashes.end())
            {
                duplicateCount++;
                continue;
            }

            bucketHashes.insert(hash);
            storer.addPosition(*board, move, score, result);
            uniqueCount++;
        }

        INFO("Bucket " << i << " complete. Unique positions: " << uniqueCount << ", Duplicate positions: " << duplicateCount)

        bucketHashes.clear();
        loader.close();
        // Close to flush the data to disk after each bucket
        storer.close();
    }

    INFO("Deduplication complete. Unique positions: " << uniqueCount << ", Duplicate positions: " << duplicateCount)
}

void PostProcessing::filter(const FilterParameters& params)
{
    DataLoader loader;
    DataStorer storer;

    uint32_t offset = params.offset;
    uint32_t removed = 0;
    uint32_t removedCaptures = 0;
    uint32_t removedChecks = 0;
    uint32_t removedMaxHalfMoves = 0;
    uint32_t removedMaxEval = 0;
    uint32_t removedMinPieces = 0;
    uint32_t removedSingleMove = 0;
    uint32_t removedStaticMargin = 0;

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

    while(!loader.eof())
    {
        Board* board = loader.getNextBoard();
        eval_t eval = loader.getScore();
        GameResult result = loader.getResult();
        Move move = loader.getMove();

        offset++;

        if(offset % 1000000 == 0)
        {
            INFO("Filtered positions (Offset): " << offset << ", Removed: " << removed)
            INFO("Removed captures: " << removedCaptures)
            INFO("Removed checks: " << removedChecks)
            INFO("Removed max half moves: " << removedMaxHalfMoves)
            INFO("Removed max eval: " << removedMaxEval)
            INFO("Removed min pieces: " << removedMinPieces)
            INFO("Removed single move: " << removedSingleMove)
            INFO("Removed static margin: " << removedStaticMargin)
        }

        if(params.filterCaptures && !move.isNull() && move.isCapture())
        {
            removed++;
            removedCaptures++;
            continue;
        }

        if(params.filterChecks && board->isChecked())
        {
            removed++;
            removedChecks++;
            continue;
        }

        if(params.filterMaxHalfMoves && (board->getHalfMoves() > params.maxHalfMoves))
        {
            removed++;
            removedMaxHalfMoves++;
            continue;
        }

        if(params.filterMaxEval && (std::abs(eval) > params.maxEval))
        {
            removed++;
            removedMaxEval++;
            continue;
        }

        if(params.filterMinPieces && (board->getNumPieces() < params.minPieces))
        {
            removed++;
            removedMinPieces++;
            continue;
        }

        if(params.filterSingleMove)
        {
            board->getLegalMoves();
            if(board->getNumLegalMoves() <= 1)
            {
                removed++;
                removedSingleMove++;
                continue;
            }
        }

        if(params.filterStaticMargin)
        {
            eval_t staticEval = Evaluator::nnue.predictBoard(*board);
            if(std::abs(staticEval - eval) > params.staticMargin)
            {
                removed++;
                removedStaticMargin++;
                continue;
            }
        }

        storer.addPosition(*board, move, eval, result);
    }

    loader.close();
    storer.close();

    INFO("Finished filtering positions (Offset): " << offset << ", Removed: " << removed)
}