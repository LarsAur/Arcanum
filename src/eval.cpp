#include <eval.hpp>
#include <algorithm>
#include <memory.hpp>

using namespace Arcanum;

Evaluator::Evaluator()
{
    m_accumulatorStackPointer = 0;
    m_nnue.load("../nnue/test768_180");

}

Evaluator::~Evaluator()
{
    for(auto accPtr : m_accumulatorStack)
    {
        delete accPtr;
    }
}


void Evaluator::initAccumulatorStack(const Board& board)
{
    if(m_accumulatorStack.empty())
        m_accumulatorStack.push_back(new NN::Accumulator);

    m_accumulatorStackPointer = 0;
    m_nnue.initAccumulator(m_accumulatorStack[0], board);
}

void Evaluator::pushMoveToAccumulator(const Board& board, const Move& move)
{
    if(m_accumulatorStack.size() == m_accumulatorStackPointer + 1)
    {
        m_accumulatorStack.push_back(new NN::Accumulator);
    }

    m_nnue.incAccumulator(
        m_accumulatorStack[m_accumulatorStackPointer],
        m_accumulatorStack[m_accumulatorStackPointer+1],
        board,
        move
    );

    m_accumulatorStackPointer++;

    #ifdef VERIFY_NNUE_INCR
    eval_t e1 = m_nnue.evaluate(m_accumulatorStack[m_accumulatorStackPointer], board.m_turn);
    eval_t e2 = m_nnue.evaluateBoard(board);
    // Check if the difference is larger than one,
    // This is because the net using floating point may accumulate some error
    // because of this, an error of up to 1 is acceptable
    if(std::abs(e1 - e2) > 1)
    {
        DEBUG(e1 << "  " << e2)
        LOG(unsigned(move.from) << " " << unsigned(move.to) << " Type: " << MOVED_PIECE(move.moveInfo) << " Capture: " << CAPTURED_PIECE(move.moveInfo) << " Castle: " << CASTLE_SIDE(move.moveInfo) << " Enpassant " << (move.moveInfo & MoveInfoBit::ENPASSANT))
        DEBUG(board.getBoardString())
        exit(-1);
    }
    #endif
}

void Evaluator::popMoveFromAccumulator()
{
    m_accumulatorStackPointer--;
}

bool Evaluator::isCheckMateScore(eval_t eval)
{
    return std::abs(eval) > MATE_SCORE - MAX_MATE_DISTANCE;
}

bool Evaluator::isTbCheckMateScore(eval_t eval)
{
    return std::abs(eval) > (TB_MATE_SCORE - TB_MAX_MATE_DISTANCE) && !isCheckMateScore(eval);
}

// Evaluates positive value for WHITE
eval_t Evaluator::evaluate(Board& board, uint8_t plyFromRoot, bool noMoves)
{
    eval_t eval = 0;

    // If it is known from search that the position has no moves
    // Checking for legal moves can be skipped
    if(!noMoves) noMoves = !board.hasLegalMove();

    // Check for stalemate and checkmate
    if(noMoves)
    {
        if(board.isChecked(board.m_turn))
        {
            eval = board.m_turn == WHITE ? -MATE_SCORE + plyFromRoot : MATE_SCORE - plyFromRoot;
            return eval;
        }

        return eval;
    };

    // eval_t score = m_nnue.evaluateBoard(board);

    eval_t score = m_nnue.evaluate(
        m_accumulatorStack[m_accumulatorStackPointer],
        board.getTurn()
    );

    return board.m_turn == WHITE ? score : -score;
}