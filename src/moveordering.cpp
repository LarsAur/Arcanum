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

inline int32_t MoveSelector::m_getMoveScore(const Move& move, Phase& phase)
{
    // Always prioritize PV moves
    if(move == m_ttMove)
    {
        phase = TT_PHASE;
        // Copy potential changes in the moveinfo in case it is a false ttHit
        m_ttMove = move;
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

    if(score > 0)
    {
        phase = HIGH_SCORING_PHASE;
        return score;
    }

    score = m_history->get(move, m_board->getTurn());

    phase = score >= 0 ? LOW_SCORING_PHASE : NEGATIVE_SCORING_PHASE;

    return score;
}

inline void MoveSelector::m_scoreMoves()
{
    for(int8_t i = 0; i < Phase::NUM_PHASES; i++)
    {
        m_numMovesInPhase[i] = 0;
    }

    Phase phase;
    m_phase = NEGATIVE_SCORING_PHASE;
    for(uint8_t i = 0; i < m_numMoves; i++)
    {
        // Get the score and corresponding phase of the move
        int32_t score = m_getMoveScore(m_moves[i], phase);

        // Update the current phase if an earlier phase is found
        m_phase = std::min(phase, m_phase);

        // Add the move to the corresponding phase list except if it is the TT move
        if(phase != TT_PHASE)
        {
            m_scoreIndexPairs[phase][m_numMovesInPhase[phase]++] = { .score = score, .index = i };
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

    // If there is only a single move, set it as the TT move to avoid scoring and sorting it
    if(m_numMoves == 1)
    {
        m_phase = TT_PHASE;
        m_ttMove = m_moves[0];
        return;
    }

    m_sortRequired = true;
    m_ttMove = ttMove;
    m_prevMove = prevMove;
    m_board = board;
    m_counterMoveManager = counterMoveManager;
    m_killerMoveManager = killerMoveManager;
    m_history = relativeHistory;
    m_plyFromRoot = plyFromRoot;

    m_scoreMoves();
}

const Move* MoveSelector::getNextMove()
{
    if(m_phase == TT_PHASE)
    {
        m_phase = Phase(m_phase + 1);
        return &m_ttMove;
    }

    // Move to the next phase if there are no more moves in the current phase
    // Note that this can in theory loop infinitly or read out of bounds,
    // if getNextMove() is called when there are no moves left
    while (m_numMovesInPhase[m_phase] == 0)
    {
        m_phase = Phase(m_phase + 1);
        m_sortRequired = true;
    }

    // Sort the moves when starting a new phase
    if(m_sortRequired)
    {
        uint8_t numMoves = m_numMovesInPhase[m_phase];
        ScoreIndex* start = m_scoreIndexPairs[m_phase];
        ScoreIndex* end = start + numMoves;
        std::stable_sort(start, end, [](const ScoreIndex& o1, const ScoreIndex& o2){ return o1.score < o2.score; });
        m_sortRequired = false;
    }

    // Get the index of the next move
    uint8_t index = m_scoreIndexPairs[m_phase][--m_numMovesInPhase[m_phase]].index;

    return m_moves + index;
}

MoveSelector::Phase MoveSelector::getPhase() const
{
    return m_phase;
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
