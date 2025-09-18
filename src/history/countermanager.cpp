#include <history/countermanager.hpp>

using namespace Arcanum;

CounterManager::CounterManager()
{
    m_counterMoves = new Move[TableSize];

    if(m_counterMoves == nullptr)
    {
        ERROR("Unable to allocate counter move table")
    }

    clear();
}

CounterManager::~CounterManager()
{
    delete[] m_counterMoves;
}

inline uint32_t CounterManager::m_getIndex(Color turn, square_t prevFrom, square_t prevTo)
{
    return turn + 2 * (prevFrom + 64 * prevTo);
}

void CounterManager::setCounter(const Move& counterMove, const Move& prevMove, Color turn)
{
    uint32_t index = m_getIndex(turn, prevMove.from, prevMove.to);
    m_counterMoves[index] = counterMove;
}

bool CounterManager::contains(const Move& move, const Move& prevMove, Color turn)
{
    uint32_t index = m_getIndex(turn, prevMove.from, prevMove.to);
    return m_counterMoves[index] == move;
}

void CounterManager::clear()
{
    for(uint32_t i = 0; i < TableSize; i++)
    {
        m_counterMoves[i] = NULL_MOVE;
    }
}