#include <search.hpp>
#include <moveSelector.hpp>
#include <algorithm>
#include <utils.hpp>

using namespace ChessEngine2;

Searcher::Searcher()
{
    m_tt = std::unique_ptr<TranspositionTable>(new TranspositionTable(20));
}

Searcher::~Searcher()
{
    
}

eval_t Searcher::m_alphaBetaQuiet(Board board, eval_t alpha, eval_t beta, int depth, Color evalFor)
{
    std::unordered_map<hash_t, uint8_t>* boardHistory = Board::getBoardHistory();
    auto it = boardHistory->find(board.getHash());
    if(it != boardHistory->end())
    {
        if(it->second == 2)
        {
            return 0LL;
        }
    }

    if(depth == 0)
    {
        return evalFor == WHITE ? board.evaluate() : -board.evaluate();
    }

    Move* moves = board.getLegalCaptureAndCheckMoves();
    uint8_t numMoves = board.getNumLegalMoves();
    if(numMoves == 0)
    {
        return evalFor == WHITE ? board.evaluate() : -board.evaluate();
    }
    board.generateCaptureInfo();
    MoveSelector moveSelector = MoveSelector(moves, numMoves);

    eval_t bestScore = -INF;
    for (int i = 0; i < numMoves; i++)  {
        Board new_board = Board(board);
        const Move *move = moveSelector.getNextMove();
        new_board.performMove(*move);
        eval_t score = -m_alphaBetaQuiet(new_board, -beta, -alpha, depth - 1, Color(evalFor ^ 1));
        bestScore = std::max(bestScore, score);
        alpha = std::max(alpha, bestScore);
        if( alpha >= beta)
        {
            break;
        } 
    }

    return bestScore;
}

eval_t Searcher::m_alphaBeta(Board board, eval_t alpha, eval_t beta, int depth, int quietDepth, Color evalFor)
{
    // Check for repeated positions from previous searches
    std::unordered_map<hash_t, uint8_t>* boardHistory = Board::getBoardHistory();
    auto globalSearchIt = boardHistory->find(board.getHash());
    if(globalSearchIt != boardHistory->end())
    {
        if(globalSearchIt->second == 2)
        {
            return 0LL;
        }
    }

    int64_t originalAlpha = alpha;
    bool ttHit;
    ttEntry_t* entry = m_tt->getEntry(board.getHash(), &ttHit);
    if(ttHit && (entry->depth >= depth))
    {
        if((entry->flags & TT_FLAG_MASK) == TT_FLAG_EXACT)
        {
            return entry->value;
        }
        else if((entry->flags & TT_FLAG_MASK) == TT_FLAG_LOWERBOUND)
        {
            alpha = std::max(alpha, entry->value);
        }
        else if((entry->flags & TT_FLAG_MASK) == TT_FLAG_UPPERBOUND)
        {
            beta = std::min(alpha, entry->value);
        }

        if(alpha >= beta)
        {
            return entry->value;
        }
    }

    if(depth == 0)
    {
        return m_alphaBetaQuiet(board, alpha, beta, quietDepth, evalFor);
    }

    // First search the move found to be best previously
    eval_t bestScore = -INF;
    Move bestMove = Move(0, 0);
    Move* moves = nullptr;
    uint8_t numMoves = 0;
    if(ttHit)
    {
        Board new_board = Board(board);
        bestMove = entry->bestMove;
        new_board.performMove(bestMove);
        bestScore = -m_alphaBeta(new_board, -beta, -alpha, depth - 1, quietDepth, Color(evalFor ^ 1));
        alpha = std::max(alpha, bestScore);
        if(alpha >= beta)
        {
            goto l_complete_search;
        } 
    }

    moves = board.getLegalMoves();
    numMoves = board.getNumLegalMoves();
    if(numMoves == 0)
    {
        return evalFor == WHITE ? board.evaluate() : -board.evaluate();
    }
    board.generateCaptureInfo();

    { // Separate scope for moveSelector to get around goto l_complete_search
    MoveSelector moveSelector = MoveSelector(moves, numMoves);
    for (int i = 0; i < numMoves; i++)  {
        const Move* move = moveSelector.getNextMove();
        // Skip searching the best move found in the transposition table
        if(bestMove == *move)
        {
            continue;
        }

        // Generate new board and make the move
        Board new_board = Board(board);
        new_board.performMove(*move);
        eval_t score = -m_alphaBeta(new_board, -beta, -alpha, depth - 1, quietDepth, Color(evalFor ^ 1));
        if(score > bestScore)
        {
            bestScore = score;
            bestMove = *move;
        }
        alpha = std::max(alpha, bestScore);
        if(alpha >= beta)
        {
            break;
        } 
    }
    }

    l_complete_search:

    ttEntry_t newEntry;
    newEntry.value = bestScore;
    newEntry.bestMove = bestMove;
    if(bestScore <= originalAlpha)
    {
        newEntry.flags = (m_generation & ~TT_FLAG_MASK) | TT_FLAG_UPPERBOUND;
    }
    else if(bestScore >= beta)
    {
        newEntry.flags = (m_generation & ~TT_FLAG_MASK) | TT_FLAG_LOWERBOUND;
    }
    else
    {
        newEntry.flags = (m_generation & ~TT_FLAG_MASK) | TT_FLAG_EXACT;
    }

    newEntry.depth = depth;
    m_tt->addEntry(newEntry, board.getHash());

    return bestScore;
}

Move Searcher::getBestMove(Board board, int depth, int quietDepth)
{
    Move* moves = board.getLegalMoves();
    uint8_t numMoves = board.getNumLegalMoves();
    board.generateCaptureInfo();
    MoveSelector moveSelector = MoveSelector(moves, numMoves);

    Move bestMove;
    eval_t alpha = -INF;
    eval_t beta = INF;
    eval_t bestScore = -INF;

    for (int i = 0; i < numMoves; i++)  {
        Board new_board = Board(board);
        const Move *move = moveSelector.getNextMove();
        new_board.performMove(*move);

        eval_t score = -m_alphaBeta(new_board, -beta, -alpha, depth - 1, quietDepth, Color(1^board.getTurn()));
        
        if(score > bestScore)
        {
            bestScore = score;
            bestMove = *move;
            if(score > alpha)
            {   
                alpha = score;
            }
        }
    }

    #if TT_RECORD_STATS == 1
    ttStats_t stats = m_tt->getStats();
    CHESS_ENGINE2_LOG("Entries Added: " << stats.entriesAdded)
    CHESS_ENGINE2_LOG("Replacements: " << stats.replacements)
    CHESS_ENGINE2_LOG("Entries in table: " << stats.entriesAdded - stats.replacements - stats.blockedReplacements << " (" << 100 * (stats.entriesAdded - stats.replacements - stats.blockedReplacements) / float(m_tt->getEntryCount()) << "%)")
    CHESS_ENGINE2_LOG("Lookups: " << stats.lookups)
    CHESS_ENGINE2_LOG("Lookup misses: " << stats.lookupMisses)
    CHESS_ENGINE2_LOG("Lookup hits: " << stats.lookups - stats.lookupMisses)
    CHESS_ENGINE2_LOG("Blocked replacements " << stats.blockedReplacements);
    #endif

    m_generation += 1; // Generation will update every 4th search

    return bestMove;
}