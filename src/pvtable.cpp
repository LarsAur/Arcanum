#include <pvtable.hpp>
#include <sstream>
using namespace Arcanum;

PvTable::PvTable(uint32_t maxPvLength) : m_maxPvLength(maxPvLength)
{
    m_pvLengths = new uint8_t[maxPvLength]();
    m_pvTable = new Move[maxPvLength * maxPvLength]();

    ASSERT_OR_EXIT(m_pvLengths != nullptr, "Failed to allocate memory for PV lengths");
    ASSERT_OR_EXIT(m_pvTable != nullptr, "Failed to allocate memory for PV table");

    // Zero initialize the arrays. This is not strictly necessary.
    memset(m_pvLengths, 0, sizeof(uint8_t) * maxPvLength);
    memset(m_pvTable, 0, sizeof(Move) * maxPvLength * maxPvLength);
};

inline uint32_t PvTable::m_tableIndex(uint32_t plyFromRoot, uint32_t ply) const
{
    return plyFromRoot * m_maxPvLength + ply;
}

void PvTable::updatePv(const Move& move, uint8_t plyFromRoot)
{
    if(plyFromRoot >= m_maxPvLength)
    {
        WARNING("Ply from root is too large when updating PV: " << plyFromRoot)
        return;
    }

    m_pvTable[m_tableIndex(plyFromRoot, plyFromRoot)] = move;
    for (int ply = plyFromRoot + 1; ply < m_pvLengths[plyFromRoot + 1]; ply++) {
        m_pvTable[m_tableIndex(plyFromRoot, ply)] = m_pvTable[m_tableIndex(plyFromRoot + 1, ply)];
    }
    m_pvLengths[plyFromRoot] = m_pvLengths[plyFromRoot + 1];
}

std::string PvTable::getPvLine()
{
    std::string str;
    for(uint8_t i = 0; i < m_pvLengths[0] - 1; i++)
    {
        str += m_pvTable[m_tableIndex(0, i)].toString() + " ";
    }
    str += m_pvTable[m_tableIndex(0, m_pvLengths[0] - 1)].toString();
    return str;
}

void PvTable::updatePvLength(uint8_t plyFromRoot)
{
    if(plyFromRoot >= m_maxPvLength)
    {
        WARNING("Length is too large when updating PV length: " << plyFromRoot)
        return;
    }

    m_pvLengths[plyFromRoot] = plyFromRoot;
}