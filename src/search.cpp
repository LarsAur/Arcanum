#include <search.hpp>

using namespace ChessEngine2;

Searcher::Searcher()
{

}

Searcher::~Searcher()
{
    
}

int64_t Searcher::m_alphaBetaQuiet(Board board, int64_t alpha, int64_t beta, int depth, Color evalFor)
{
    if(depth == 0)
    {
        return evalFor == WHITE ? board.evaluate() : -board.evaluate(); // TODO: quiesce( alpha, beta );
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
        board.performMove(moves[i]);
        int64_t score = -m_alphaBetaQuiet(new_board, -beta, -alpha, depth - 1, Color(evalFor ^ 1));
        if( score >= beta )
            return beta;   //  fail hard beta-cutoff
        if( score > bestScore )
        {
            bestScore = score;
            if( score > alpha)
            {
                alpha = score; // alpha acts like max in MiniMax
            }
        } 
    }

    return bestScore;
}

int64_t Searcher::m_alphaBeta(Board board, int64_t alpha, int64_t beta, int depth, Color evalFor)
{
    if(depth == 0)
    {
        return m_alphaBetaQuiet(board, alpha, beta, 4, evalFor); // TODO: quiesce( alpha, beta );
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
        board.performMove(moves[i]);
        int64_t score = -m_alphaBeta(new_board, -beta, -alpha, depth - 1, Color(evalFor ^ 1));
        if( score >= beta )
            return beta;   //  fail hard beta-cutoff
        if( score > bestScore )
        {
            bestScore = score;
            if( score > alpha)
            {
                alpha = score; // alpha acts like max in MiniMax
            }
        } 
    }

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

    std::cout << "Alpha: " << alpha << std::endl; 
    
    return bestMove;
}