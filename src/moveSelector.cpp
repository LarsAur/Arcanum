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

inline int32_t MoveSelector::m_getMoveScore(const Move& move)
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

    if(score == 0)
    {
        score = m_relativeHistory->get(move, m_board->getTurn());
    }

    return score;
}

inline void MoveSelector::m_scoreMoves()
{
    m_numHighScoreMoves = 0;
    m_numLowScoreMoves = 0;
    for(uint8_t i = 0; i < m_numMoves; i++)
    {
        int32_t score = m_getMoveScore(m_moves[i]);
        if(score > MILLION)
            m_highScoreIdxPairs[m_numHighScoreMoves++] = {.score = score, .index = i};
        else
            m_lowScoreIdxPairs[m_numLowScoreMoves++] = {.score = score, .index = i};
    }
}

MoveSelector::MoveSelector(const Move *moves, const uint8_t numMoves, int plyFromRoot, KillerMoveManager* killerMoveManager, RelativeHistory* relativeHistory, Board *board, Move ttMove)
{
    m_numMoves = numMoves;
    m_moves = moves;
    if(m_numMoves == 1)
    {
        m_numHighScoreMoves = 1;
        m_highScoreIdxPairs[0].index = 0;
        return;
    }

    m_ttMove = ttMove;
    m_board = board;
    m_killerMoveManager = killerMoveManager;
    m_relativeHistory = relativeHistory;
    m_plyFromRoot = plyFromRoot;

    m_bbOpponentPawnAttacks = m_board->getOpponentPawnAttacks();
    m_bbOpponentAttacks = m_board->getOpponentAttacks();

    m_scoreMoves();

    std::sort(m_highScoreIdxPairs, m_highScoreIdxPairs + m_numHighScoreMoves, [](const ScoreIndex& o1, const ScoreIndex& o2){ return o1.score < o2.score; });
}

const Move* MoveSelector::getNextMove()
{
    if(m_numHighScoreMoves > 0 && m_numHighScoreMoves != 0xff)
    {
        return m_moves + m_highScoreIdxPairs[--m_numHighScoreMoves].index;
    }

    if(m_numHighScoreMoves == 0)
    {
        m_numHighScoreMoves = 0xff; // Set the number of moves to an 'illegal' number
        std::sort(m_lowScoreIdxPairs, m_lowScoreIdxPairs + m_numLowScoreMoves, [](const ScoreIndex& o1, const ScoreIndex& o2){ return o1.score < o2.score; });
    }

    return m_moves + m_lowScoreIdxPairs[--m_numLowScoreMoves].index;
}

KillerMoveManager::KillerMoveManager()
{
    clear();
}

// Have to check if the move is not a capture move before adding it to the killer move list
// This can be checked in the add function, but it is faster to do it before the function call,
// because if can avoid overhead from calling the function
void KillerMoveManager::add(Move move, uint8_t plyFromRoot)
{
    if(plyFromRoot >= KILLER_MOVES_MAX_PLY)
    {
        WARNING("Killer moves ply from root is too large: " << unsigned(plyFromRoot))
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
        WARNING("Cannot check for killer move at " << unsigned(plyFromRoot) << " plyFromRoot")
        return false;
    }

    if(move == m_killerMoves[plyFromRoot][0] || move == m_killerMoves[plyFromRoot][1])
    {
        return true;
    }

    return false;
}

void KillerMoveManager::clear()
{
    for(int i = 0; i < KILLER_MOVES_MAX_PLY; i++)
    {
        for(int j = 0; j < 2; j++)
        {
            m_killerMoves[i][j] = Move(0,0);
        }
    }
}

RelativeHistory::RelativeHistory()
{
    clear();
}

// Moves should only be added to the history if at an appropriate depth
// Too low depth will instill much noise.
// Moves should only be added at beta cutoffs
// Add history score when a quiet move causes a beta-cutoff
void RelativeHistory::addHistory(const Move& move, uint8_t depth, Color turn)
{
    m_hhScores[turn][move.from][move.to] += depth * depth;
}

// Add butterfly score when a quiet move does not cause a beta-cutoff
void RelativeHistory::addButterfly(const Move& move, uint8_t depth, Color turn)
{
    m_bfScores[turn][move.from][move.to] += depth * depth;
}

uint32_t RelativeHistory::get(const Move& move, Color turn)
{
    // Bitshift the history score, to avoid rounding down
    return (m_hhScores[turn][move.from][move.to] << 16) / m_bfScores[turn][move.from][move.to];
}

void RelativeHistory::clear()
{
    for(int i = 0; i < 64; i++)
    {
        for(int j = 0; j < 64; j++)
        {
            m_hhScores[Color::WHITE][i][j] = 0;
            m_hhScores[Color::BLACK][i][j] = 0;
            m_bfScores[Color::WHITE][i][j] = 1;
            m_bfScores[Color::BLACK][i][j] = 1;
        }
    }
}
