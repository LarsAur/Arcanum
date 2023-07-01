#include <moveSelector.hpp>
#include <algorithm>
#include <utils.hpp>

using namespace ChessEngine2;

static uint8_t s_pieceValues[6]
{
    10, 50, 30, 30, 90, 100
};

inline int32_t MoveSelector::m_getMoveScore(Move move)
{
    // Always prioritize PV moves
    if(move == m_ttMove)
    {
        return INT16_MAX;
    }

    Piece movePiece = Piece(LS1B(move.moveInfo & MOVE_INFO_MOVE_MASK));
    int32_t score = 0;

    // Punish moves to positions attacked by opponent pawns
    bitboard_t bbTo = (1LL << move.to);
    if(m_bbOpponentPawnAttacks & bbTo)
    {
        score -= (s_pieceValues[movePiece] << 1);
    }

    if(m_bbOpponentAttacks & bbTo)
    {
        score -= s_pieceValues[movePiece];
    }

    if(move.moveInfo & MOVE_INFO_CAPTURE_MASK)
    {
        Piece capturePiece = Piece(LS1B(move.moveInfo & MOVE_INFO_CAPTURE_MASK) - 16);
        score += (s_pieceValues[capturePiece] << 3) - s_pieceValues[movePiece];
    }

    if(move.moveInfo & MOVE_INFO_PROMOTE_MASK)
    {
        Piece promotePiece = Piece(LS1B(move.moveInfo & MOVE_INFO_PROMOTE_MASK) - 11); // Not -12 because rook is at index 1
        score += s_pieceValues[promotePiece] << 4;
    }

    return score;
}

inline void MoveSelector::m_scoreMoves()
{
    for(uint8_t i = 0; i < m_numMoves; i++)
    {
        m_scoreIdxPairs[i] = std::pair<int32_t, uint8_t>(m_getMoveScore(m_moves[i]), i);
    }
}

static bool compare(std::pair<int32_t, uint8_t> o1, std::pair<int32_t, uint8_t> o2)
{
    return o1.first < o2.first;
}

MoveSelector::MoveSelector(const Move *moves, const uint8_t numMoves, Board *board, Move ttMove)
{
    m_numMoves = numMoves;
    m_moves = moves;
    m_ttMove = ttMove;
    m_board = board;

    m_bbOpponentPawnAttacks = m_board->getOponentPawnAttacks();
    m_bbOpponentAttacks = m_board->getOponentAttacks();

    m_scoreMoves();
    
    std::make_heap(m_scoreIdxPairs, m_scoreIdxPairs + m_numMoves, compare);
}

const Move* MoveSelector::getNextMove()
{
    std::pop_heap(m_scoreIdxPairs, m_scoreIdxPairs + m_numMoves, compare);
    return m_moves + m_scoreIdxPairs[--m_numMoves].second;
}