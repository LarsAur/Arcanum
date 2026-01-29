#pragma once

#include <move.hpp>
#include <string.h>

namespace Arcanum
{
    class PvTable
    {
        private:
            uint32_t m_maxPvLength;
            uint8_t* m_pvLengths;
            Move* m_pvTable;
            uint32_t m_tableIndex(uint32_t plyFromRoot, uint32_t ply) const;
        public:
            PvTable(uint32_t maxPvLength);
            void updatePv(const Move& move, uint8_t plyFromRoot);
            void updatePvLength(uint8_t plyFromRoot);
            std::string getPvLine();
    };
}