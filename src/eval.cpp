#include <eval.hpp>
#include <memory.hpp>
#include <fen.hpp>
#include <algorithm>
#include <syzygy.hpp>

using namespace Arcanum;

NNUE Evaluator::nnue = NNUE();

Evaluator::Evaluator()
{
    m_accumulatorStackIndex = 0;
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
    {
        m_accumulatorStack.push_back(new NNUE::Accumulator);
        m_accumulatorUpdates.push_back({});
    }

    m_accumulatorStackIndex = 0;
    nnue.initializeAccumulator(m_accumulatorStack[0], board);
    m_accumulatorUpdates[0].updated[Color::WHITE] = true;
    m_accumulatorUpdates[0].updated[Color::BLACK] = true;
}

void Evaluator::pushMoveToAccumulator(const Board& board, const Move& move)
{
    if(m_accumulatorStack.size() == m_accumulatorStackIndex + 1)
    {
        m_accumulatorStack.push_back(new NNUE::Accumulator);
        m_accumulatorUpdates.push_back({});
    }

    // Calculate the NNUE deltas
    m_accumulatorUpdates[m_accumulatorStackIndex + 1].updated[Color::WHITE] = false;
    m_accumulatorUpdates[m_accumulatorStackIndex + 1].updated[Color::BLACK] = false;
    NNUE::findDeltaFeatures(board, move, m_accumulatorUpdates[m_accumulatorStackIndex + 1].deltaFeatures);

    m_accumulatorStackIndex++;
}

void Evaluator::m_propagateAccumulatorUpdates(Color perspective)
{
    // Find a root where the accumulator is updated by walking the stack
    uint32_t rootIndex = m_accumulatorStackIndex;
    while(!m_accumulatorUpdates[rootIndex].updated[perspective])
    {
        rootIndex--;
    }

    // Propagate the updates to each accumulator down from the root
    while(rootIndex < m_accumulatorStackIndex)
    {
        nnue.incrementAccumulatorPerspective(
            m_accumulatorStack[rootIndex],
            m_accumulatorStack[rootIndex + 1],
            m_accumulatorUpdates[rootIndex + 1].deltaFeatures,
            perspective
        );

        m_accumulatorUpdates[rootIndex + 1].updated[perspective] = true;
        rootIndex++;
    }
}

void Evaluator::popMoveFromAccumulator()
{
    m_accumulatorStackIndex--;
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

    m_propagateAccumulatorUpdates(board.getTurn());
    return nnue.predict(m_accumulatorStack[m_accumulatorStackIndex], board);
}