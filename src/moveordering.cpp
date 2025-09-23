#include <moveordering.hpp>
#include <algorithm>
#include <utils.hpp>

using namespace Arcanum;

static const uint16_t s_pieceValues[6]
{
    100, 500, 300, 300, 900, 1000
};

inline void MoveSelector::m_scoreMoves()
{
    Color turn = m_board->getTurn();

    Move prevMove = m_plyFromRoot == 0 ? NULL_MOVE : m_moveStack[m_plyFromRoot - 1];

    for(uint8_t i = 0; i < m_numMoves; i++)
    {
        const Move& move = m_moves[i];

        // Check if the move is the TT move
        if(m_moveFromTT == move)
        {
            m_ttMove = &m_moves[i];
            continue;
        }

        if(move.isCapture() || move.isPromotion())
        {
            int32_t score = 0;

            if(move.isCapture())
            {
                score += (s_pieceValues[move.capturedPiece()] - s_pieceValues[move.movedPiece()]) * 16;
                score += m_heuristics->captureHistory.get(move, turn);
            }

            if(move.isPromotion())
            {
                score += s_pieceValues[move.promotedPiece()] * 16000;
            }

            m_movesAndScores[m_numCaptures].score = score;
            m_movesAndScores[m_numCaptures].index = i;
            m_numCaptures++;
            continue;
        }

        // Quiet moves

        if(m_heuristics->killerManager.contains(move, m_plyFromRoot))
        {
            m_killers[m_numKillers++] = &m_moves[i];
            continue;
        }

        if(m_heuristics->counterManager.contains(move, prevMove, turn))
        {
            m_counter = &m_moves[i];
            continue;
        }

        int32_t quietScore = m_heuristics->quietHistory.get(move, turn);
        quietScore += m_heuristics->continuationHistory.get(m_moveStack, m_plyFromRoot, move, turn);
        m_numQuiets++;
        m_movesAndScores[MAX_MOVE_COUNT - m_numQuiets].score = quietScore;
        m_movesAndScores[MAX_MOVE_COUNT - m_numQuiets].index = i;
    }

    m_captureMovesAndScores = &m_movesAndScores[0];
    m_quietMovesAndScores   = &m_movesAndScores[MAX_MOVE_COUNT - m_numQuiets];
    m_badCaptureMovesAndScores = &m_movesAndScores[m_numCaptures];
}

MoveSelector::MoveSelector(
    const Move *moves,
    const uint8_t numMoves,
    int plyFromRoot,
    MoveOrderHeuristics* heuristics,
    Board *board,
    const Move ttMove,
    const Move* moveStack
)
{
    m_numMoves = numMoves;
    m_moves = moves;
    m_phase = Phase::TT_PHASE;
    m_skipQuiets = false;
    m_ttMove = nullptr;
    m_counter = nullptr;
    m_numKillers = 0;
    m_numCaptures = 0;
    m_numQuiets = 0;
    m_numBadCaptures = 0;
    m_nextBadCapture = 0;

    // If there is only a single move, set it as the TT move to avoid scoring and sorting it
    if(m_numMoves == 1)
    {
        m_ttMove = &m_moves[0];
        return;
    }

    m_heuristics = heuristics;
    m_moveFromTT = ttMove;
    m_moveStack = moveStack;
    m_board = board;
    m_plyFromRoot = plyFromRoot;

    m_scoreMoves();
}

const Move* MoveSelector::getNextMove()
{
    switch (m_phase)
    {
    case Phase::TT_PHASE:
        if(m_ttMove)
        {
            m_phase = Phase::GOOD_CAPTURES_PHASE;
            return m_ttMove;
        }
        [[fallthrough]];

    case Phase::GOOD_CAPTURES_PHASE:
        m_phase = Phase::GOOD_CAPTURES_PHASE;
        while(m_numCaptures > 0)
        {
            MoveAndScore moveAndScore = popBestMoveAndScore(m_captureMovesAndScores, m_numCaptures--);
            // If the capture is a bad capture, move it to the back of the end of the capture array to be played in the bad capture phase
            const Move* move = &m_moves[moveAndScore.index];
            if(move->isUnderPromotion() || !m_board->see(*move))
            {
                // Note: The bad capture array grows backwards, and uses the memory at the end of m_captureMovesAndScores.
                // This is safe since popBestMoveAndScore removes the last element from the m_captureMovesAndScores array.
                m_numBadCaptures++;
                m_badCaptureMovesAndScores[-m_numBadCaptures] = moveAndScore;
            }
            else
            {
                return move;
            }
        }
        [[fallthrough]];

    case Phase::KILLERS_PHASE:
        m_phase = Phase::KILLERS_PHASE;
        if(m_numKillers > 0)
        {
            return m_killers[--m_numKillers];
        }
        [[fallthrough]];

    case Phase::COUNTERS_PHASE:
        if(m_counter)
        {
            m_phase = Phase::QUIETS_PHASE;
            return m_counter;
        }
        [[fallthrough]];

    case Phase::QUIETS_PHASE:
        m_phase = Phase::QUIETS_PHASE;
        if(m_numQuiets > 0 && !m_skipQuiets)
        {
            MoveAndScore moveAndScore = popBestMoveAndScore(m_quietMovesAndScores, m_numQuiets--);
            return m_moves + moveAndScore.index;
        }
        [[fallthrough]];

    case Phase::BAD_CAPTURES_PHASE:
        m_phase = Phase::BAD_CAPTURES_PHASE;
        if(m_nextBadCapture < m_numBadCaptures)
        {
            m_nextBadCapture++;
            MoveAndScore moveAndScore = m_badCaptureMovesAndScores[-m_nextBadCapture];
            return m_moves + moveAndScore.index;
        }
        [[fallthrough]];

    default:
        return nullptr;
    }
}

MoveSelector::MoveAndScore MoveSelector::popBestMoveAndScore(MoveAndScore* list, uint8_t numElements)
{
    MoveAndScore *best = list;
    for(uint8_t i = 1; i < numElements; i++)
    {
        if(best->score < list[i].score)
        {
            best = &list[i];
        }
    }

    // Overwrite the best element with the last
    // to pop the element off the list
    MoveAndScore tmp = *best;
    *best = list[numElements - 1];

    return tmp;
}

MoveSelector::Phase MoveSelector::getPhase() const
{
    return m_phase;
}

bool MoveSelector::isSkippingQuiets()
{
    return m_skipQuiets;
}

// Returns the number of quiet moves left in the move selector
// This exludes TT, killers and counters
uint8_t MoveSelector::getNumQuietsLeft()
{
    return m_numQuiets;
}

void MoveSelector::skipQuiets()
{
    m_skipQuiets = true;
}