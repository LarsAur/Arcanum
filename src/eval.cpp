#include <eval.hpp>
#include <memory.hpp>
#include <fen.hpp>
#include <algorithm>
#include <syzygy.hpp>

using namespace Arcanum;

NNUE Evaluator::nnue = NNUE();

Evaluator::Evaluator()
{
    m_accumulatorStackPointer = 0;
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
        m_accumulatorStack.push_back(new NNUE::Accumulator);

    m_accumulatorStackPointer = 0;
    nnue.initializeAccumulator(m_accumulatorStack[0], board);
}

void Evaluator::pushMoveToAccumulator(const Board& board, const Move& move)
{
    if(m_accumulatorStack.size() == m_accumulatorStackPointer + 1)
    {
        m_accumulatorStack.push_back(new NNUE::Accumulator);
    }

    nnue.incrementAccumulator(
        m_accumulatorStack[m_accumulatorStackPointer],
        m_accumulatorStack[m_accumulatorStackPointer+1],
        board,
        move
    );

    m_accumulatorStackPointer++;

    #ifdef VERIFY_NNUE_INCR
    Board newBoard = Board(board);
    newBoard.performMove(move);
    eval_t e1 = nnue.predict(m_accumulatorStack[m_accumulatorStackPointer], newBoard.m_turn);
    eval_t e2 = nnue.predictBoard(newBoard);
    // Check if the difference is larger than one,
    // This is because the net using floating point may accumulate some error
    // because of this, an error of up to 1 is acceptable
    if(std::abs(e1 - e2) > 1)
    {
        DEBUG(e1 << "  " << e2)
        LOG(unsigned(move.from) << " " << unsigned(move.to) << " Type: " << MOVED_PIECE(move.moveInfo) << " Capture: " << CAPTURED_PIECE(move.moveInfo) << " Castle: " << CASTLE_SIDE(move.moveInfo) << " Enpassant " << (move.moveInfo & MoveInfoBit::ENPASSANT))
        DEBUG(FEN::toString(newBoard))
        exit(-1);
    }
    #endif
}

void Evaluator::popMoveFromAccumulator()
{
    m_accumulatorStackPointer--;
}

bool Evaluator::isRealMateScore(eval_t eval)
{
    return std::abs(eval) >= MATE_SCORE - MAX_MATE_DISTANCE;
}

bool Evaluator::isTbMateScore(eval_t eval)
{
    return std::abs(eval) >= (TB_MATE_SCORE - TB_MAX_MATE_DISTANCE) && !isRealMateScore(eval);
}

bool Evaluator::isMateScore(eval_t eval)
{
    return std::abs(eval) >= TB_MATE_SCORE - TB_MAX_MATE_DISTANCE;
}

bool Evaluator::isCloseToMate(Board& board, eval_t eval)
{
    return (std::abs(eval) > 900) || (board.getNumPieces() <= 5);
}

// Positive values represents advantage for current player
eval_t Evaluator::evaluate(Board& board, uint8_t plyFromRoot)
{
    // Check for stalemate and checkmate
    if(!board.hasLegalMove())
    {
        // Checkmate
        if(board.isChecked())
            return -MATE_SCORE + plyFromRoot;

        // Stalemate
        return 0;
    };

    return nnue.predict(m_accumulatorStack[m_accumulatorStackPointer], board);
}