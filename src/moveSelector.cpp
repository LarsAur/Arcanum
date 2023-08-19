#include <moveSelector.hpp>
#include <algorithm>
#include <utils.hpp>

using namespace Arcanum;

#define MILLION 1000000
#define WINNING_CAPTURE_BIAS 8 * MILLION
#define PROMOTE_BIAS 6 * MILLION
#define KILLER_BIAS 4 * MILLION
#define LOSING_CAPTURE_BIAS 2 * MILLION

static const uint16_t s_pieceValues[6]
{
    100, 500, 300, 300, 900, 1000
};

inline int32_t MoveSelector::m_getMoveScore(Move move)
{
    // Always prioritize PV moves
    if(move == m_ttMove)
    {
        return INT32_MAX;
    }

    Piece movePiece = Piece(LS1B(move.moveInfo & MOVE_INFO_MOVE_MASK));
    bitboard_t bbTo = (1LL << move.to);
    int32_t score = 0;

    // If it is a capture
    if(move.moveInfo & MOVE_INFO_CAPTURE_MASK)
    {
        Piece capturePiece = Piece(LS1B(move.moveInfo & MOVE_INFO_CAPTURE_MASK) - 16);
        int32_t materialDelta = s_pieceValues[capturePiece] - s_pieceValues[movePiece];
        
        // Check if recapture is available
        if(m_bbOpponentAttacks & bbTo)
        {
            score += (materialDelta >= 0 ? WINNING_CAPTURE_BIAS : LOSING_CAPTURE_BIAS) + materialDelta;
        }
        else
        {
            score += WINNING_CAPTURE_BIAS + materialDelta;
        }
    }
    else // Is not a capture move
    {
        if(m_killerMoveManager->contains(move, m_plyFromRoot))
        {
            score += KILLER_BIAS;
        }
    }

    if(move.moveInfo & MOVE_INFO_PROMOTE_MASK)
    {
        Piece promotePiece = Piece(LS1B(move.moveInfo & MOVE_INFO_PROMOTE_MASK) - 11); // Not -12 because rook is at index 1
        score += PROMOTE_BIAS + s_pieceValues[promotePiece];
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

MoveSelector::MoveSelector(const Move *moves, const uint8_t numMoves, int plyFromRoot, KillerMoveManager* killerMoveManager, Board *board, Move ttMove)
{
    m_numMoves = numMoves;
    m_moves = moves;
    m_ttMove = ttMove;
    m_board = board;
    m_killerMoveManager = killerMoveManager;
    m_plyFromRoot = plyFromRoot;

    m_bbOpponentPawnAttacks = m_board->getopponentPawnAttacks();
    m_bbOpponentAttacks = m_board->getopponentAttacks();

    m_scoreMoves();
    
    std::make_heap(m_scoreIdxPairs, m_scoreIdxPairs + m_numMoves, compare);
}

const Move* MoveSelector::getNextMove()
{
    std::pop_heap(m_scoreIdxPairs, m_scoreIdxPairs + m_numMoves, compare);
    return m_moves + m_scoreIdxPairs[--m_numMoves].second;
}

KillerMoveManager::KillerMoveManager()
{
    // Initialize the killer move table to not contain any moves
    for(int i = 0; i < KILLER_MOVES_MAX_PLY; i++)
    {
        for(int j = 0; j < 2; j++)
        {
            m_killerMoves[i][j] = Move(0,0);
        }
    }
}

// Have to check if the move is not a capture move before adding it to the killer move list
// This can be checked in the add function, but it is faster to do it before the function call, 
// because if can avoid overhead from calling the function 
void KillerMoveManager::add(Move move, uint8_t plyFromRoot)
{
    if(plyFromRoot >= KILLER_MOVES_MAX_PLY)
    {
        WARNING("Killer moves ply from root is too large: " << plyFromRoot)
        return;
    }

    // The move do not need to be added if it already exists in the table
    if(move == m_killerMoves[plyFromRoot][0] || move == m_killerMoves[plyFromRoot][1])
    {
        return;
    }

    // Implementation of a queue with only 2 elements
    m_killerMoves[plyFromRoot][1] = m_killerMoves[plyFromRoot][0];
    m_killerMoves[plyFromRoot][0] = move;
}

bool KillerMoveManager::contains(Move move, uint8_t plyFromRoot) const
{
    if(move == Move(0,0))
    {
        WARNING("Cannot check for killer move Move(0,0)")
        return false;
    }

    if(plyFromRoot >= KILLER_MOVES_MAX_PLY)
    {
        WARNING("Cannot check for killer move at " << plyFromRoot << " plyFromRoot")
        return false;
    }

    if(move == m_killerMoves[plyFromRoot][0] || move == m_killerMoves[plyFromRoot][1])
    {
        return true;
    }

    return false;
}