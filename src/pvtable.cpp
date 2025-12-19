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
    std::string str;
    for(uint8_t i = 0; i < m_pvLengths[0] - 1; i++)
    {
        str += m_table[0][i].toString() + " ";
    }
    str += m_table[0][m_pvLengths[0] - 1].toString();
    return str;
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