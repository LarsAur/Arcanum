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

    for(uint8_t i = 0; i < m_numMoves; i++)
    {
        const Move& move = m_moves[i];

        // Check if the move is the TT move
        if(move == m_ttMove)
        {
            m_ttIndex = i;
            m_numTTMoves = 1;
            continue;
        }

        if(move.isCapture() || move.isPromotion())
        {
            int32_t score = 0;

            if(move.isCapture())
            {
                score += (s_pieceValues[move.capturedPiece()] - s_pieceValues[move.movedPiece()]) * 16;
                score += m_captureHistory->get(move, turn);
            }

            if(move.isPromotion())
            {
                score += s_pieceValues[move.promotedPiece()] * 16000;
            }

            m_captureScoreIndexPairs[m_numCaptures++] = { .score = score, .index = i };
            continue;
        }

        // Quiet moves

        if(m_killerMoveManager->contains(move, m_plyFromRoot))
        {
            m_killerIndices[m_numKillers++] = i;
            continue;
        }

        if(m_counterMoveManager->contains(move, m_prevMove, turn))
        {
            m_counterIndex = i;
            m_numCounters = 1;
            continue;
        }

        int32_t quietScore = m_history->get(move, turn);
        m_quietScoreIndexPairs[m_numQuiets++] = { .score = quietScore, .index = i };
    }
}

MoveSelector::MoveSelector(
    const Move *moves,
    const uint8_t numMoves,
    int plyFromRoot,
    KillerMoveManager* killerMoveManager,
    History* history,
    CaptureHistory* captureHistory,
    CounterMoveManager* counterMoveManager,
    Board *board,
    const Move ttMove = NULL_MOVE,
    const Move prevMove = NULL_MOVE
)
{
    m_numMoves = numMoves;
    m_moves = moves;
    m_phase = Phase::TT_PHASE;
    m_skipQuiets = false;
    m_numTTMoves = 0;
    m_numKillers = 0;
    m_numCounters = 0;
    m_numCaptures = 0;
    m_numQuiets = 0;

    // If there is only a single move, set it as the TT move to avoid scoring and sorting it
    if(m_numMoves == 1)
    {
        m_ttIndex = 0;
        m_numTTMoves = 1;
        return;
    }

    m_ttMove = ttMove;
    m_prevMove = prevMove;
    m_board = board;
    m_counterMoveManager = counterMoveManager;
    m_killerMoveManager = killerMoveManager;
    m_history = history;
    m_captureHistory = captureHistory;
    m_plyFromRoot = plyFromRoot;

    m_scoreMoves();
}

const Move* MoveSelector::getNextMove()
{
    switch (m_phase)
    {
    case Phase::TT_PHASE:
        if(m_numTTMoves > 0)
        {
            m_numTTMoves = 0;
            return m_moves + m_ttIndex;
        }

    case Phase::SORT_GOOD_CAPTURE_PHASE:
        if(m_numCaptures > 1)
        {
            ScoreIndex* start = m_captureScoreIndexPairs;
            ScoreIndex* end = start + m_numCaptures;
            std::stable_sort(start, end, [](const ScoreIndex& o1, const ScoreIndex& o2){ return o1.score < o2.score; });
        }
        m_numBadCaptures = 0;

    case Phase::GOOD_CAPTURES_PHASE:
        m_phase = Phase::GOOD_CAPTURES_PHASE;
        while(m_numCaptures > 0)
        {
            // If the capture is a bad capture, move it to the back of the end of the capture array to be played in the bad capture phase
            const Move* move = m_moves + m_captureScoreIndexPairs[--m_numCaptures].index;
            if(move->isUnderPromotion() || !m_board->see(*move))
            {
                ++m_numBadCaptures;
                m_captureScoreIndexPairs[MAX_MOVE_COUNT - m_numBadCaptures] = m_captureScoreIndexPairs[m_numCaptures];
            }
            else
            {
                return move;
            }
        }

    case Phase::KILLERS_PHASE:
        m_phase = Phase::KILLERS_PHASE;
        if(m_numKillers > 0)
        {
            return m_moves + m_killerIndices[--m_numKillers];
        }

    case Phase::COUNTERS_PHASE:
        m_phase = Phase::SORT_QUIET_PHASE;
        if(m_numCounters > 0)
        {
            --m_numCounters;
            return m_moves + m_counterIndex;
        }

    case Phase::SORT_QUIET_PHASE:
        if(m_numQuiets > 1 && !m_skipQuiets)
        {
            ScoreIndex* start = m_quietScoreIndexPairs;
            ScoreIndex* end = start + m_numQuiets;
            std::stable_sort(start, end, [](const ScoreIndex& o1, const ScoreIndex& o2){ return o1.score < o2.score; });
        }

    case Phase::QUIETS_PHASE:
        m_phase = Phase::QUIETS_PHASE;
        if(m_numQuiets > 0 && !m_skipQuiets)
        {
            return m_moves + m_quietScoreIndexPairs[--m_numQuiets].index;
        }

    case Phase::SORT_BAD_CAPTURE_PHASE:
        // Sorting is not required as the order is kept after sorting good moves
        m_nextBadCapture = 0;

    case Phase::BAD_CAPTURES_PHASE:
        m_phase = Phase::BAD_CAPTURES_PHASE;
        if(m_nextBadCapture < m_numBadCaptures)
        {
            m_nextBadCapture++;
            return m_moves + m_captureScoreIndexPairs[MAX_MOVE_COUNT - m_nextBadCapture].index;
        }

    default:
        return nullptr;
    }
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
    m_historyScore = new int32_t[TableSize];

    if(m_historyScore == nullptr)
    {
        ERROR("Unable to allocate history table")
    }

    clear();
}

History::~History()
{
    delete[] m_historyScore;
}

inline uint32_t History::m_getIndex(Color turn, square_t from, square_t to)
{
    return turn + 2 * (from + 64 * to);
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
    uint32_t index = m_getIndex(turn, move.from, move.to);
    m_historyScore[index] += bonus - (m_historyScore[index] * std::abs(bonus) / 16384);
}

int32_t History::get(const Move& move, Color turn)
{
    uint32_t index = m_getIndex(turn, move.from, move.to);
    return m_historyScore[index];
}

void History::clear()
{
    memset(m_historyScore, 0, TableSize*sizeof(int32_t));
}

CaptureHistory::CaptureHistory()
{
    m_historyScore = new int32_t[TableSize];

    if(m_historyScore == nullptr)
    {
        ERROR("Unable to allocate capture history table")
    }

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

void CaptureHistory::updateHistory(const Move& bestMove, const std::array<Move, MAX_MOVE_COUNT>& captures, uint8_t numCaptures, uint8_t depth, Color turn)
{
    int32_t bonus = m_getBonus(depth);

    m_addBonus(bestMove, turn, bonus);

    for(uint8_t i = 0; i < numCaptures; i++)
    {
        m_addBonus(captures[i], turn, -bonus);
    }
}

void CaptureHistory::m_addBonus(const Move& move, Color turn, int32_t bonus)
{
    uint32_t index = m_getIndex(turn, move.from, move.movedPiece(), move.capturedPiece());
    m_historyScore[index] += bonus - (m_historyScore[index] * std::abs(bonus) / 16384);
}

int32_t CaptureHistory::get(const Move& move, Color turn)
{
    return m_historyScore[m_getIndex(turn, move.from, move.movedPiece(), move.capturedPiece())];
}

void CaptureHistory::clear()
{
    memset(m_historyScore, 0, TableSize*sizeof(int32_t));
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
