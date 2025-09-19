#include <history/capturehistory.hpp>

using namespace Arcanum;

CaptureHistory::CaptureHistory()
{
    m_historyScore = new int32_t[TableSize];
    ASSERT_OR_EXIT(m_historyScore != nullptr, "Failed to allocate memory for capture history table")
    clear();
}

CaptureHistory::~CaptureHistory()
{
    delete[] m_historyScore;
}

inline uint32_t CaptureHistory::m_getIndex(Color turn, square_t to, Piece movedPiece, Piece capturedPiece)
{
    return turn + 2 * (to + 64 * (movedPiece + 6 * capturedPiece));
}

inline int32_t CaptureHistory::m_getBonus(uint8_t depth)
{
    return std::min(2000, 16 * depth * depth);
}

void CaptureHistory::m_addBonus(const Move& move, Color turn, int32_t bonus)
{
    uint32_t index = m_getIndex(turn, move.from, move.movedPiece(), move.capturedPiece());
    m_historyScore[index] += bonus - (m_historyScore[index] * std::abs(bonus) / 16384);
}

void CaptureHistory::updateHistory(const Move& bestMove, const Move* captures, uint8_t numCaptures, uint8_t depth, Color turn)
{
    int32_t bonus = m_getBonus(depth);

    m_addBonus(bestMove, turn, bonus);

    for(uint8_t i = 0; i < numCaptures; i++)
    {
        m_addBonus(captures[i], turn, -bonus);
    }
}

int32_t CaptureHistory::get(const Move& move, Color turn)
{
    return m_historyScore[m_getIndex(turn, move.from, move.movedPiece(), move.capturedPiece())];
}

void CaptureHistory::clear()
{
    memset(m_historyScore, 0, TableSize*sizeof(int32_t));
}