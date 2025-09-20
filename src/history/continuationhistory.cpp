#include "history/continuationhistory.hpp"

using namespace Arcanum;

ContinuationHistory::ContinuationHistory()
{
    m_scores = new int32_t[TableSize];
    ASSERT_OR_EXIT(m_scores != nullptr, "Failed to allocate memory for continuation history table")
    clear();
}

ContinuationHistory::~ContinuationHistory()
{
    delete[] m_scores;
}

inline uint32_t ContinuationHistory::m_getIndex(Color turn, Piece prevPiece, square_t prevTo, Piece movePiece, square_t moveTo)
{
    return turn + 2 * (prevPiece + 6 * (prevTo + 64 * (movePiece + 6 * moveTo)));
}

void ContinuationHistory::m_addBonus(const Move& move, const Move& prevMove, Color turn, int32_t bonus)
{
    uint32_t index = m_getIndex(turn, prevMove.movedPiece(), prevMove.to, move.movedPiece(), move.to);
    m_scores[index] += bonus - (m_scores[index] * std::abs(bonus) / 16384);
}

int32_t ContinuationHistory::m_getScore(const Move& move, const Move& prevMove, Color turn)
{
    uint32_t index = m_getIndex(turn, prevMove.movedPiece(), prevMove.to, move.movedPiece(), move.to);
    return m_scores[index];
}

int32_t ContinuationHistory::m_getBonus(uint8_t depth)
{
    return std::min(2000, 16 * depth * depth);
}

void ContinuationHistory::update(const Move* moveStack, uint8_t plyFromRoot, const Move& move, const Move* quiets, uint8_t numQuiets, Color turn, uint8_t depth)
{
    int32_t bonus = m_getBonus(depth);

    for(uint8_t i = 0; i < numQuiets; i++)
    {
        switch(plyFromRoot)
        {
            default: m_addBonus(quiets[i], moveStack[plyFromRoot - 3], turn, -bonus);
                [[fallthrough]];
            case 2: m_addBonus(quiets[i], moveStack[plyFromRoot - 2], turn, -bonus);
                [[fallthrough]];
            case 1: m_addBonus(quiets[i], moveStack[plyFromRoot - 1], turn, -bonus);
                [[fallthrough]];
            case 0: break;
        }
    }

    switch(plyFromRoot)
    {
        default: m_addBonus(move, moveStack[plyFromRoot - 3], turn, bonus);
            [[fallthrough]];
        case 2: m_addBonus(move, moveStack[plyFromRoot - 2], turn, bonus);
            [[fallthrough]];
        case 1: m_addBonus(move, moveStack[plyFromRoot - 1], turn, bonus);
            [[fallthrough]];
        case 0: break;
    }
}

int32_t ContinuationHistory::get(const Move* moveStack, uint8_t plyFromRoot, const Move& move, Color turn)
{
    int32_t score = 0;

    switch(plyFromRoot)
    {
        default: score += m_getScore(move, moveStack[plyFromRoot - 3], turn);
            [[fallthrough]];
        case 2: score += m_getScore(move, moveStack[plyFromRoot - 2], turn);
            [[fallthrough]];
        case 1: score += m_getScore(move, moveStack[plyFromRoot - 1], turn);
            [[fallthrough]];
        case 0: break;
    }

    return score;
}

void ContinuationHistory::clear()
{
    memset(m_scores, 0, TableSize*sizeof(int32_t));
}