#include <pvtable.hpp>
#include <sstream>
using namespace Arcanum;

PvTable::PvTable() : m_pvLengths{}
{

};

void PvTable::updatePv(const Move& move, uint8_t plyFromRoot)
{
    if(plyFromRoot >= MAX_PV_LENGTH)
    {
        WARNING("Ply from root is too large when updating PV: " << plyFromRoot)
        return;
    }

    m_table[plyFromRoot][plyFromRoot] = move;
    for (int ply = plyFromRoot + 1; ply < m_pvLengths[plyFromRoot + 1]; ply++) {
        m_table[plyFromRoot][ply] = m_table[plyFromRoot + 1][ply];
    }
    m_pvLengths[plyFromRoot] = m_pvLengths[plyFromRoot + 1];
}

std::string PvTable::getPvLine()
{
    std::stringstream ss;
    for(uint8_t i = 0; i < m_pvLengths[0]; i++)
        ss << m_table[0][i] << " ";
    return ss.str();
}

void PvTable::updatePvLength(uint8_t length)
{
    if(length >= MAX_PV_LENGTH)
    {
        WARNING("Length is too large when updating PV length: " << length)
        return;
    }

    m_pvLengths[length] = length;
}