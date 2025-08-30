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
        if(m_packedTTMove == move)
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
                score += m_captureHistory->get(move, turn);
            }

            if(move.isPromotion())
            {
                score += s_pieceValues[move.promotedPiece()] * 16000;
            }

            m_movesAndScores[m_numCaptures].score = score;
            m_movesAndScores[m_numCaptures].move = &m_moves[i];
            m_numCaptures++;
            continue;
        }

        // Quiet moves

        if(m_killerMoveManager->contains(move, m_plyFromRoot))
        {
            m_killers[m_numKillers++] = &m_moves[i];
            continue;
        }

        if(m_counterMoveManager->contains(move, m_prevMove, turn))
        {
            m_counter = &m_moves[i];
            continue;
        }

        int32_t quietScore = m_history->get(move, turn);
        m_numQuiets++;
        m_movesAndScores[MAX_MOVE_COUNT - m_numQuiets].score = quietScore;
        m_movesAndScores[MAX_MOVE_COUNT - m_numQuiets].move = &m_moves[i];
    }

    m_captureMovesAndScores = &m_movesAndScores[0];
    m_quietMovesAndScores   = &m_movesAndScores[MAX_MOVE_COUNT - m_numQuiets];
    m_badCaptureMovesAndScores = &m_movesAndScores[m_numCaptures];
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
    const PackedMove ttMove,
    const Move prevMove
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

    m_packedTTMove = ttMove;
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
            const Move* move = moveAndScore.move;
            if(move->isUnderPromotion() || !m_board->see(*move))
            {
                m_badCaptureMovesAndScores[m_numBadCaptures++] = moveAndScore;
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
            return moveAndScore.move;
        }
        [[fallthrough]];

    case Phase::BAD_CAPTURES_PHASE:
        m_phase = Phase::BAD_CAPTURES_PHASE;
        if(m_nextBadCapture < m_numBadCaptures)
        {
            return m_badCaptureMovesAndScores[m_nextBadCapture++].move;
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

KillerMoveManager::KillerMoveManager()
{
    m_killerMoves = new Move[TableSize];

    if(m_killerMoves == nullptr)
    {
        ERROR("Unable to allocate killer moves table")
    }

    clear();
}

KillerMoveManager::~KillerMoveManager()
{
    delete[] m_killerMoves;
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
    if(move == m_killerMoves[m_getIndex(plyFromRoot, 0)]
    || move == m_killerMoves[m_getIndex(plyFromRoot, 1)])
    {
        return;
    }

    // Implementation of a queue with only 2 elements
    m_killerMoves[m_getIndex(plyFromRoot, 1)] = m_killerMoves[m_getIndex(plyFromRoot, 0)];
    m_killerMoves[m_getIndex(plyFromRoot, 0)] = move;
}

bool KillerMoveManager::contains(Move move, uint8_t plyFromRoot) const
{
    if(plyFromRoot >= KILLER_MOVES_MAX_PLY)
    {
        WARNING("Cannot check for killer move at " << unsigned(plyFromRoot) << " plyFromRoot")
        return false;
    }

    if(move == m_killerMoves[m_getIndex(plyFromRoot, 0)]
    || move == m_killerMoves[m_getIndex(plyFromRoot, 1)])
    {
        return true;
    }

    return false;
}

void KillerMoveManager::clearPly(uint8_t plyFromRoot)
{
    if(plyFromRoot >= KILLER_MOVES_MAX_PLY)
    {
        WARNING("Cannot clear for killer move at " << unsigned(plyFromRoot) << " plyFromRoot")
        return;
    }

    for(int j = 0; j < 2; j++)
    {
        m_killerMoves[m_getIndex(plyFromRoot, j)] = NULL_MOVE;
    }
}

inline uint32_t KillerMoveManager::m_getIndex(uint8_t plyFromRoot, uint8_t offset) const
{
    return (2 * plyFromRoot) + offset;
}

void KillerMoveManager::clear()
{
    for(uint32_t i = 0; i < TableSize; i++)
    {
        m_killerMoves[i] = NULL_MOVE;
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

void History::updateHistory(const Move& bestMove, const Move* quiets, uint8_t numQuiets, uint8_t depth, Color turn)
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

void CaptureHistory::updateHistory(const Move& bestMove, const Move* captures, uint8_t numCaptures, uint8_t depth, Color turn)
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
    m_counterMoves = new Move[TableSize];

    if(m_counterMoves == nullptr)
    {
        ERROR("Unable to allocate counter move table")
    }

    clear();
}

CounterMoveManager::~CounterMoveManager()
{
    delete[] m_counterMoves;
}

inline uint32_t CounterMoveManager::m_getIndex(Color turn, square_t prevFrom, square_t prevTo)
{
    return turn + 2 * (prevFrom + 64 * prevTo);
}

void CounterMoveManager::setCounter(const Move& counterMove, const Move& prevMove, Color turn)
{
    uint32_t index = m_getIndex(turn, prevMove.from, prevMove.to);
    m_counterMoves[index] = counterMove;
}

bool CounterMoveManager::contains(const Move& move, const Move& prevMove, Color turn)
{
    uint32_t index = m_getIndex(turn, prevMove.from, prevMove.to);
    return m_counterMoves[index] == move;
}

void CounterMoveManager::clear()
{
    for(uint32_t i = 0; i < TableSize; i++)
    {
        m_counterMoves[i] = NULL_MOVE;
    }
}
