#pragma once

#include <move.hpp>
#include <string.h>

namespace Arcanum
{

    #define MAX_PV_LENGTH 96

    class PvTable
    {
        private:
            uint8_t m_pvLengths[MAX_PV_LENGTH];
            Move m_table[MAX_PV_LENGTH][MAX_PV_LENGTH];
        public:
            PvTable();
            void updatePv(const Move& move, uint8_t plyFromRoot);
            void updatePvLength(uint8_t length);
            std::string getPvLine();
    };
}