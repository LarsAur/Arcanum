#include <history/killermanager.hpp>
#include <utils.hpp>

using namespace Arcanum;

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
    if(plyFromRoot >= KillerMoveMaxPly)
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
    if(plyFromRoot >= KillerMoveMaxPly)
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
    if(plyFromRoot >= KillerMoveMaxPly)
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
