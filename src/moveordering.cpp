#include <moveordering.hpp>
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

    int32_t score = 0;

    // If it is a capture
    if(move.isCapture())
    {
        int32_t materialDelta = s_pieceValues[move.capturedPiece()] - s_pieceValues[move.movedPiece()];

        // Use SEE to check for winning or losing capture
        score += (m_board->see(move) ? WINNING_CAPTURE_BIAS : LOSING_CAPTURE_BIAS) + materialDelta;
    }
    else // Is not a capture move
    {
        if(m_killerMoveManager->contains(move, m_plyFromRoot) || m_counterMoveManager->contains(move, m_prevMove, m_board->getTurn()))
        {
            score += KILLER_BIAS;
        }
    }

    if(move.isPromotion())
    {
        score += PROMOTE_BIAS + s_pieceValues[move.promotedPiece()];
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
        if(score > 0)
            m_highScoreIdxPairs[m_numHighScoreMoves++] = {.score = score, .index = i};
        else
        {
            score = m_history->get(m_moves[i], m_board->getTurn());
            m_lowScoreIdxPairs[m_numLowScoreMoves++] = {.score = score, .index = i};
        }

    }
}

MoveSelector::MoveSelector(
    const Move *moves,
    const uint8_t numMoves,
    int plyFromRoot,
    KillerMoveManager* killerMoveManager,
    History* relativeHistory,
    CounterMoveManager* counterMoveManager,
    Board *board,
    const Move ttMove = NULL_MOVE,
    const Move prevMove = NULL_MOVE
)
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
    m_prevMove = prevMove;
    m_board = board;
    m_counterMoveManager = counterMoveManager;
    m_killerMoveManager = killerMoveManager;
    m_history = relativeHistory;
    m_plyFromRoot = plyFromRoot;

    m_bbOpponentPawnAttacks = m_board->getOpponentPawnAttacks();
    m_bbOpponentAttacks = m_board->getOpponentAttacks();

    m_scoreMoves();

    std::stable_sort(m_highScoreIdxPairs, m_highScoreIdxPairs + m_numHighScoreMoves, [](const ScoreIndex& o1, const ScoreIndex& o2){ return o1.score < o2.score; });
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
        std::stable_sort(m_lowScoreIdxPairs, m_lowScoreIdxPairs + m_numLowScoreMoves, [](const ScoreIndex& o1, const ScoreIndex& o2){ return o1.score < o2.score; });
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
            m_killerMoves[i][j] = NULL_MOVE;
        }
    }
}

History::History()
{
    clear();
}

inline int32_t History::m_getBonus(uint8_t depth)
{
    return std::min(2000, 16 * depth * depth);
}

void History::updateHistory(const Move& bestMove, const std::array<Move, MAX_MOVE_COUNT>& quiets, uint8_t numQuiets, uint8_t depth, Color turn)
{
    int32_t bonus = m_getBonus(depth);

    m_addBonus(bestMove, turn, bonus);

    for(uint8_t i = 0; i < numQuiets; i++)
    {
        m_addBonus(quiets[i], turn, -bonus);
    }
}

void History::m_addBonus(const Move& move, Color turn, int32_t bonus)
{
    m_historyScore[turn][move.from][move.to] += bonus - (m_historyScore[turn][move.from][move.to] * std::abs(bonus) / 16384);
}

int32_t History::get(const Move& move, Color turn)
{
    return m_historyScore[turn][move.from][move.to];
}

void History::clear()
{
    for(int i = 0; i < 64; i++)
    {
        for(int j = 0; j < 64; j++)
        {
            m_historyScore[Color::WHITE][i][j] = 0;
            m_historyScore[Color::BLACK][i][j] = 0;
        }
    }
}


CounterMoveManager::CounterMoveManager()
{
    clear();
}

void CounterMoveManager::setCounter(const Move& counterMove, const Move& prevMove, Color turn)
{
    m_counterMoves[turn][prevMove.from][prevMove.to] = counterMove;
}

bool CounterMoveManager::contains(const Move& move, const Move& prevMove, Color turn)
{
    return m_counterMoves[turn][prevMove.from][prevMove.to] == move;
}

void CounterMoveManager::clear()
{
    for(int i = 0; i < 64; i++)
    {
        for(int j = 0; j < 64; j++)
        {
            m_counterMoves[Color::WHITE][i][j] = NULL_MOVE;
            m_counterMoves[Color::BLACK][i][j] = NULL_MOVE;
        }
    }
}
