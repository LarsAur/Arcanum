#include <history/quiethistory.hpp>

using namespace Arcanum;

QuietHistory::QuietHistory()
{
    m_historyScore = new int32_t[TableSize];
    ASSERT_OR_EXIT(m_historyScore != nullptr, "Failed to allocate memory for quiet history table")
    clear();
}

QuietHistory::~QuietHistory()
{
    delete[] m_historyScore;
}

inline uint32_t QuietHistory::m_getIndex(Color turn, square_t from, square_t to)
{
    return turn + 2 * (from + 64 * to);
}

inline int32_t QuietHistory::m_getBonus(uint8_t depth)
{
    return std::min(2000, 16 * depth * depth);
}

void QuietHistory::m_addBonus(const Move& move, Color turn, int32_t bonus)
{
    uint32_t index = m_getIndex(turn, move.from, move.to);
    m_historyScore[index] += bonus - (m_historyScore[index] * std::abs(bonus) / 16384);
}

void QuietHistory::update(const Move& bestMove, const Move* quiets, uint8_t numQuiets, uint8_t depth, Color turn)
{
    int32_t bonus = m_getBonus(depth);

    m_addBonus(bestMove, turn, bonus);

    for(uint8_t i = 0; i < numQuiets; i++)
    {
        m_addBonus(quiets[i], turn, -bonus);
    }
}

int32_t QuietHistory::get(const Move& move, Color turn)
{
    uint32_t index = m_getIndex(turn, move.from, move.to);
    return m_historyScore[index];
}

void QuietHistory::clear()
{
    memset(m_historyScore, 0, TableSize*sizeof(int32_t));
}