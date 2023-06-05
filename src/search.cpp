#include <search.hpp>
#include <algorithm>
#include <utils.hpp>

using namespace ChessEngine2;

Searcher::Searcher()
{
    m_tt = std::unique_ptr<TranspositionTable>(new TranspositionTable(26));
}

Searcher::~Searcher()
{
    
}

int64_t Searcher::m_alphaBetaQuiet(Board board, int64_t alpha, int64_t beta, int depth, Color evalFor)
{
    if(depth == 0)
    {
        return evalFor == WHITE ? board.evaluate() : -board.evaluate();
    }

    std::unordered_map<hash_t, uint8_t>* boardHistory = Board::getBoardHistory();
    auto it = boardHistory->find(board.getHash());
    if(it != boardHistory->end())
    {
        if(it->second == 2)
        {
            return 0LL;
        }
    }

    Move* moves = board.getLegalCaptureAndCheckMoves();
    uint8_t numMoves = board.getNumLegalMoves();

    if(numMoves == 0)
    {
        return evalFor == WHITE ? board.evaluate() : -board.evaluate();
    }

    int64_t bestScore = -INF;
    for (int i = 0; i < numMoves; i++)  {
        Board new_board = Board(board);
        new_board.performMove(moves[i]);
        bestScore = std::max(bestScore, -m_alphaBetaQuiet(new_board, -beta, -alpha, depth - 1, Color(evalFor ^ 1)));
        alpha = std::max(alpha, bestScore);
        if( alpha >= beta)
        {
            break;
        } 
    }

    return bestScore;
}

int64_t Searcher::m_alphaBeta(Board board, int64_t alpha, int64_t beta, int depth, Color evalFor)
{
    int64_t originalAlpha = alpha;
    ttEntry_t entry = m_tt->getEntry(board.getHash());
    if((entry.flags & TT_FLAG_VALID) && (entry.hash == board.getHash()) && entry.depth >= depth)
    {
        if(entry.flags & TT_FLAG_EXACT)
        {
            return entry.value;
        }
        else if(entry.flags & TT_FLAG_LOWERBOUND)
        {
            alpha = std::max(alpha, entry.value);
        }
        else if(entry.flags & TT_FLAG_UPPERBOUND)
        {
            beta = std::min(alpha, entry.value);
        }

        if(alpha >= beta)
        {
            return entry.value;
        }
    }

    if(depth == 0)
    {
        return m_alphaBetaQuiet(board, alpha, beta, 4, evalFor); // TODO: quiesce( alpha, beta );
    }

    // Check for repeated possition
    std::unordered_map<hash_t, uint8_t>* boardHistory = Board::getBoardHistory();
    auto it = boardHistory->find(board.getHash());
    if(it != boardHistory->end())
    {
        if(it->second == 2)
        {
            return 0LL;
        }
    }

    Move* moves = board.getLegalMoves();
    uint8_t numMoves = board.getNumLegalMoves();

    if(numMoves == 0)
    {
        return evalFor == WHITE ? board.evaluate() : -board.evaluate();
    }

    int64_t bestScore = -INF;
    for (int i = 0; i < numMoves; i++)  {
        Board new_board = Board(board);
        new_board.performMove(moves[i]);
        bestScore = std::max(bestScore, -m_alphaBeta(new_board, -beta, -alpha, depth - 1, Color(evalFor ^ 1)));
        alpha = std::max(alpha, bestScore);
        if(alpha >= beta)
        {
            break;
        } 
    }

    ttEntry_t newEntry;
    newEntry.value = bestScore;
    if(bestScore <= originalAlpha)
    {
        newEntry.flags = TT_FLAG_UPPERBOUND | TT_FLAG_VALID;
    }
    else if(bestScore >= beta)
    {
        newEntry.flags = TT_FLAG_LOWERBOUND | TT_FLAG_VALID;
    }
    else
    {
        newEntry.flags = TT_FLAG_EXACT | TT_FLAG_VALID;
    }

    newEntry.depth = depth;
    newEntry.hash = board.getHash();
    m_tt->addEntry(newEntry);

    return bestScore;
}

Move Searcher::getBestMove(Board board, int depth)
{
    Move* moves = board.getLegalMoves();
    uint8_t numMoves = board.getNumLegalMoves();

    Move bestMove;

    int64_t alpha = -INF;
    int64_t beta = INF;
    int64_t bestScore = -INF;

    for (int i = 0; i < numMoves; i++)  {
        Board new_board = Board(board);
        new_board.performMove(moves[i]);

        int64_t score = -m_alphaBeta(new_board, -beta, -alpha, depth - 1, Color(1^board.getTurn()));
        
        if(score > bestScore)
        {
            bestScore = score;
            bestMove = moves[i];
            if(score > alpha)
            {   
                alpha = score;
            }
        }
    }

    ttStats_t stats = m_tt->getStats();
    CHESS_ENGINE2_LOG("Entries Added: " << stats.entriesAdded)
    CHESS_ENGINE2_LOG("Replacements: " << stats.replacements)
    CHESS_ENGINE2_LOG("Entries in table: " << stats.entriesAdded - stats.replacements)
    CHESS_ENGINE2_LOG("Lookups: " << stats.lookups)
    CHESS_ENGINE2_LOG("Lookup misses: " << stats.lookupMisses)
    CHESS_ENGINE2_LOG("Lookup hits: " << stats.lookups - stats.lookupMisses)
    
    return bestMove;
}