#include <history/correctionhistory.hpp>
#include <algorithm>

using namespace Arcanum;

CorrectionHistory::CorrectionHistory()
{
    m_pawnCorrections = new int16_t[PawnTableSize];

    ASSERT_OR_EXIT(m_pawnCorrections != nullptr, "Failed to allocate memory for CorrectionHistory pawn table")

    clear();
}

CorrectionHistory::~CorrectionHistory()
{
    delete[] m_pawnCorrections;
}

uint32_t CorrectionHistory::m_getPawnIndex(hash_t pawnHash, Color turn) const
{
    constexpr hash_t HashMask = ((PawnTableSize / 2) - 1) << 1;
    return static_cast<uint32_t>((pawnHash & HashMask) + turn);
}


void CorrectionHistory::update(const Board& board, eval_t bestScore, eval_t staticEval, uint8_t depth)
{
    // Pawn correction
    uint32_t index = m_getPawnIndex(board.getPawnHash(), board.getTurn());
    eval_t correction = (bestScore - staticEval);
    eval_t bonus = std::clamp(correction * depth / 8, -CorrectionLimit, CorrectionLimit);
    m_pawnCorrections[index] += bonus - m_pawnCorrections[index] * abs(bonus) / CorrectionLimit;
}

eval_t CorrectionHistory::get(const Board& board) const
{
    eval_t correction = 0;
    // Pawn correction
    uint32_t index = m_getPawnIndex(board.getPawnHash(), board.getTurn());
    correction += static_cast<eval_t>(m_pawnCorrections[index] / 256);

    return correction;
}

void CorrectionHistory::clear()
{
    for(size_t i = 0; i < PawnTableSize; i++)
    {
        m_pawnCorrections[i] = 0;
    }
}